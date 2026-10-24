#include "audio_player.h"
#include "../storage/sd_card.h"
#include <LittleFS.h>
#include <algorithm>

namespace audio {

namespace {
constexpr size_t kAudioBufferSize = 2048;

struct WavHeader {
  char riff[4];
  uint32_t fileSize;
  char wave[4];
  char fmt[4];
  uint32_t fmtSize;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};
}

AudioPlayer::AudioPlayer() = default;
AudioPlayer::~AudioPlayer() { stop(); }

AudioPlayer& AudioPlayer::instance() {
  static AudioPlayer s_instance;
  return s_instance;
}

bool AudioPlayer::begin(int bclkPin, int lrcPin, int doutPin) {
  bclk_ = bclkPin;
  lrc_ = lrcPin;
  dout_ = doutPin;

  i2s_config_t i2s_config = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = bclk_,
    .ws_io_num = lrc_,
    .data_out_num = dout_,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, nullptr);
  if (err != ESP_OK) {
    lastError_ = "Gagal install driver I2S";
    Serial.printf("[AUDIO] %s: err=%d\n", lastError_.c_str(), err);
    return false;
  }

  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    lastError_ = "Gagal konfigurasi pin I2S";
    Serial.printf("[AUDIO] %s: err=%d\n", lastError_.c_str(), err);
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  i2sReady_ = true;
  lastError_ = "Siap";
  Serial.printf("[AUDIO] PASS: I2S Driver siap (BCLK=%d LRC=%d DOUT=%d)\n", bclk_, lrc_, dout_);
  return true;
}

bool AudioPlayer::play(const String& path, uint8_t volume) {
  if (playing_.load()) {
    stop();
  }

  activeFile_ = path;
  volume_.store(std::min<uint8_t>(100, std::max<uint8_t>(1, volume)));
  stopRequested_.store(false);

  // Spawn playback task pinned to Core 0 (background core)
  BaseType_t ret = xTaskCreatePinnedToCore(
      taskWorker, "audioWorker", 8192, this, 1, &taskHandle_, 0);

  if (ret != pdPASS) {
    lastError_ = "Gagal membuat task worker audio";
    return false;
  }
  return true;
}

void AudioPlayer::stop() {
  if (playing_.load()) {
    stopRequested_.store(true);
    uint32_t startWait = millis();
    while (playing_.load() && millis() - startWait < 1500) {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

bool AudioPlayer::isPlaying() const {
  return playing_.load();
}

void AudioPlayer::setVolume(uint8_t volume) {
  volume_.store(std::min<uint8_t>(100, std::max<uint8_t>(0, volume)));
}

AudioStatus AudioPlayer::status() const {
  AudioStatus s;
  s.state = playing_.load() ? AUDIO_PLAYING : AUDIO_IDLE;
  s.stateName = playing_.load() ? "Memutar" : "Idle";
  s.i2sReady = i2sReady_;
  s.activeFile = activeFile_;
  s.sampleRate = sampleRate_;
  s.channels = channels_;
  s.bitsPerSample = bitsPerSample_;
  s.durationSeconds = durationSeconds_;
  s.progressSeconds = progressSeconds_.load();
  s.volume = volume_.load();
  s.lastError = lastError_;
  return s;
}

void AudioPlayer::taskWorker(void* param) {
  auto* self = static_cast<AudioPlayer*>(param);
  self->runWorker();
  self->taskHandle_ = nullptr;
  vTaskDelete(nullptr);
}

void AudioPlayer::runWorker() {
  playing_.store(true);
  progressSeconds_.store(0);
  lastError_ = "";

  File file;
  if (activeFile_.startsWith("/sd") || storage::sd().exists(activeFile_.c_str())) {
    file = storage::sd().open(activeFile_.c_str(), "r");
  } else if (LittleFS.exists(activeFile_)) {
    file = LittleFS.open(activeFile_, "r");
  }

  if (!file || file.isDirectory()) {
    lastError_ = "File audio tidak dapat dibuka atau tidak ditemukan: " + activeFile_;
    Serial.println("[AUDIO] " + lastError_);
    playing_.store(false);
    return;
  }

  // Parse WAV Header
  WavHeader header;
  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    lastError_ = "Header WAV tidak lengkap";
    file.close();
    playing_.store(false);
    return;
  }

  if (memcmp(header.riff, "RIFF", 4) != 0 || memcmp(header.wave, "WAVE", 4) != 0) {
    lastError_ = "Bukan format WAV PCM yang valid (hanya RIFF/WAVE PCM yang didukung)";
    file.close();
    playing_.store(false);
    return;
  }

  sampleRate_ = header.sampleRate;
  channels_ = header.numChannels;
  bitsPerSample_ = header.bitsPerSample;

  // Search for the "data" subchunk
  char chunkId[4];
  uint32_t chunkSize = 0;
  bool foundData = false;

  // Reposition past format chunk
  file.seek(12);
  while (file.available() >= 8) {
    file.read(reinterpret_cast<uint8_t*>(chunkId), 4);
    file.read(reinterpret_cast<uint8_t*>(&chunkSize), 4);
    if (memcmp(chunkId, "data", 4) == 0) {
      foundData = true;
      break;
    }
    file.seek(file.position() + chunkSize);
  }

  if (!foundData) {
    lastError_ = "Chunk data WAV tidak ditemukan";
    file.close();
    playing_.store(false);
    return;
  }

  uint32_t totalBytes = chunkSize;
  durationSeconds_ = (header.byteRate > 0) ? (totalBytes / header.byteRate) : 0;

  Serial.printf("[AUDIO] Playback started: %s (%u Hz, %d-ch, %d-bit, %u detik)\n",
                activeFile_.c_str(), sampleRate_, channels_, bitsPerSample_, durationSeconds_);

  if (i2sReady_) {
    i2s_set_clk(I2S_NUM_0, sampleRate_,
                bitsPerSample_ == 16 ? I2S_BITS_PER_SAMPLE_16BIT : I2S_BITS_PER_SAMPLE_8BIT,
                channels_ == 2 ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO);
  }

  uint8_t* buffer = static_cast<uint8_t*>(malloc(kAudioBufferSize));
  if (!buffer) {
    lastError_ = "Gagal alokasi buffer audio";
    file.close();
    playing_.store(false);
    return;
  }

  uint32_t bytesPlayed = 0;
  uint32_t lastProgressTick = millis();

  while (file.available() && !stopRequested_.load()) {
    size_t toRead = std::min<size_t>(kAudioBufferSize, file.available());
    size_t bytesRead = file.read(buffer, toRead);
    if (bytesRead == 0) break;

    // Apply software volume scaling
    float vol = volume_.load() / 100.0f;
    if (bitsPerSample_ == 16) {
      int16_t* samples = reinterpret_cast<int16_t*>(buffer);
      size_t numSamples = bytesRead / 2;
      for (size_t i = 0; i < numSamples; ++i) {
        samples[i] = static_cast<int16_t>(samples[i] * vol);
      }
    }

    if (i2sReady_) {
      size_t bytesWritten = 0;
      i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
    } else {
      // Simulate playback timing if external I2S hardware is not active
      vTaskDelay(pdMS_TO_TICKS(bytesRead * 1000 / (header.byteRate ? header.byteRate : 88200)));
    }

    bytesPlayed += bytesRead;
    if (millis() - lastProgressTick >= 1000) {
      lastProgressTick = millis();
      if (header.byteRate > 0) {
        progressSeconds_.store(bytesPlayed / header.byteRate);
      }
    }
  }

  free(buffer);
  file.close();

  if (i2sReady_) {
    i2s_zero_dma_buffer(I2S_NUM_0);
  }

  Serial.println("[AUDIO] Playback selesai: " + activeFile_);
  playing_.store(false);
}

}  // namespace audio
