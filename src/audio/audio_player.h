#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>
#include <atomic>

namespace audio {

enum AudioState : uint8_t {
  AUDIO_IDLE = 0,
  AUDIO_PLAYING = 1,
  AUDIO_STOPPED = 2,
  AUDIO_ERROR = 3
};

struct AudioStatus {
  AudioState state{AUDIO_IDLE};
  String stateName{"Idle"};
  bool i2sReady{false};
  String activeFile{""};
  uint32_t sampleRate{0};
  uint8_t channels{0};
  uint8_t bitsPerSample{0};
  uint32_t durationSeconds{0};
  uint32_t progressSeconds{0};
  uint8_t volume{80};
  String lastError{""};
  String hardwareStatus{"Software-implemented (I2S driver configured; requires external I2S DAC amplifier)"};
};

class AudioPlayer {
 public:
  static AudioPlayer& instance();

  // Initializes I2S peripheral with configured pins
  bool begin(int bclkPin, int lrcPin, int doutPin);

  // Play audio file (from SD e.g. "/sd/audio/subuh.wav" or LittleFS)
  bool play(const String& path, uint8_t volume = 80);
  void stop();

  bool isPlaying() const;
  AudioStatus status() const;

  void setVolume(uint8_t volume);

 private:
  AudioPlayer();
  ~AudioPlayer();

  static void taskWorker(void* param);
  void runWorker();

  int bclk_{-1}, lrc_{-1}, dout_{-1};
  bool i2sReady_{false};
  std::atomic<bool> playing_{false};
  std::atomic<bool> stopRequested_{false};
  std::atomic<uint8_t> volume_{80};

  String activeFile_{""};
  uint32_t sampleRate_{0};
  uint8_t channels_{0};
  uint8_t bitsPerSample_{0};
  uint32_t durationSeconds_{0};
  std::atomic<uint32_t> progressSeconds_{0};
  String lastError_{""};

  TaskHandle_t taskHandle_{nullptr};
};

inline AudioPlayer& player() { return AudioPlayer::instance(); }

}  // namespace audio
