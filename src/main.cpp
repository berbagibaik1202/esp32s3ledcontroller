#include <Arduino.h>
#include <atomic>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <soc/esp32s3/spiram.h>
#include "board/board_profile.h"
#include "board/button.h"
#include "display/builtin_profiles.h"
#include "display/diagnostics.h"
#include "display/hub75_output.h"
#include "display/hub75_mapping.h"
#include "display/panel_profile.h"
#include "display/panel_layout.h"
#include "media/bmp.h"
#include "media/builtin_image.h"
#include "media/image_renderer.h"
#include "media/jpeg.h"
#include "media/builtin_jpeg.h"
#include "media/file_image.h"
#include "media/mjpeg_player.h"
#include "media/animation.h"
#include "wifi_config.h"
#include "web_ui.h"
#include "storage/sd_card.h"
#include "prayer/prayer_types.h"
#include "prayer/hijri.h"
#include "prayer/clock_service.h"
#include "prayer/prayer_schedule.h"
#include "prayer/audio_scheduler.h"
#include "audio/audio_player.h"
#include "prayer/prayer_renderer.h"

namespace {
constexpr size_t kPsramTestBytes = 64U * 1024U;
bool memoryPassed = false;
bool wifiScanPending = false;
bool wifiConnecting = false;
bool wifiConnected = false;
bool wifiApReady = false;
std::atomic<uint32_t> apJoins{0}, apLeaves{0}, staDisconnects{0};
std::atomic<unsigned> lastStaReason{0};
std::atomic<bool> apRunning{false};
std::atomic<uint32_t> apStarts{0}, apStops{0};
uint32_t apWatchdogAt = 0;
uint32_t wifiRetryAt = 0;
constexpr uint32_t kWifiRetryMs = 30000;
void wifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_AP_START) {
    apRunning = true; ++apStarts; Serial.println("[WIFI] AP START");
  } else if (event == ARDUINO_EVENT_WIFI_AP_STOP) {
    apRunning = false; ++apStops; Serial.println("[WIFI] AP STOP");
  } else if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    ++apJoins; Serial.println("[WIFI] AP client joined");
  } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    ++apLeaves; Serial.println("[WIFI] AP client left");
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    ++staDisconnects; lastStaReason = info.wifi_sta_disconnected.reason;
    Serial.printf("[WIFI] STA disconnected reason=%u\n", lastStaReason.load());
  }
}
uint32_t wifiConnectStarted = 0;
constexpr uint32_t kWifiConnectTimeoutMs = 15000U;
constexpr uint32_t kWifiScanTimeoutMs = 15000U;
uint32_t wifiScanStarted = 0;
struct DiscoveredNetwork {
  String ssid;
  int32_t rssi;
  uint8_t channel;
  bool secure;
};
std::vector<DiscoveredNetwork> discoveredNetworks;
bool userScanPending = false;
WebServer webServer(80);
String staSsid = LED_STA_SSID;
String staPassword = LED_STA_PASSWORD;
bool wifiApplyPending = false;
uint32_t wifiApplyAt = 0;
display::PanelProfile panelProfile;
bool profileReady = false;
display::PanelLayout panelLayout;
display::LayoutMapper layoutMapper;
bool layoutReady = false;
unsigned layoutPreset = 0;
uint8_t selectedScan = 32;
unsigned selectedMapping = 4;
bool panelRestartPending = false;
uint32_t panelRestartAt = 0;
board::DebouncedButton confirmButton;
bool panelPreviewActive = false;
uint32_t panelPreviewStarted = 0;
constexpr uint32_t kPanelPreviewMs = 30000;
void* allocateCanvas(size_t bytes) {
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
display::Framebuffer canvas(allocateCanvas, heap_caps_free);
media::AnimationPlayer animation(allocateCanvas, heap_caps_free);
bool animationActive = false;
uint32_t animationLastTick = 0;
uint32_t animationLastFrame = 0;
constexpr uint32_t kAnimationFrameMs = 16; // ~60 FPS target; actual FPS measured at DMA handoff.
unsigned diagnosticIndex = 0;
uint32_t diagnosticStep = 0;
media::BmpImage demoImage;
bool imageReady = false;
unsigned imageModeIndex = 0;
media::JpegImage demoJpeg(allocateCanvas, heap_caps_free);
bool jpegReady = false;
unsigned jpegModeIndex = 0;
bool filesystemReady = false;
media::FileImage flashImage(allocateCanvas, heap_caps_free);
media::MjpegPlayer mjpegPlayer(allocateCanvas, heap_caps_free);
uint32_t mjpegLastFrame = 0;
String selectedMedia;

enum DisplayMode : uint8_t {
  MODE_AUTO = 0,       // Prioritas: Stream > Animasi > Gambar > Jam Masjid
  MODE_STREAM = 1,     // Hanya Video Stream
  MODE_ANIMATION = 2,  // Hanya Animasi Teks
  MODE_IMAGE = 3,      // Hanya Gambar Flash / SD
  MODE_MOSQUE = 4      // Jam Masjid & Waktu Salat
};
DisplayMode displayMode = MODE_AUTO;
String streamUrl = "";
uint8_t streamFps = 15;
std::atomic<bool> streamRunning{false};
std::atomic<uint32_t> streamFrameCount{0};
std::atomic<uint32_t> streamLastFrameAt{0};
String streamStatus = "Siap";
TaskHandle_t streamTaskHandle = nullptr;
constexpr size_t kStreamBufferSize = 64U * 1024U;
constexpr size_t kMaxMediaBytes = 4096U * 1024U;
constexpr size_t kStorageReserveBytes = 32768U;
constexpr unsigned kMaxMediaFiles = 64;
constexpr const char* kUploadTempPath = "/.media-upload.tmp";
fs::File uploadFile;
String uploadPath;
String uploadError;
int uploadErrorCode = 400;
bool uploadSeen = false;
bool uploadValidated = false;
size_t uploadBytes = 0;
class FlashReader : public media::FileReader {
 public:
  explicit FlashReader(fs::File& file) : file_(file) {}
  size_t size() const override { return file_.size(); }
  size_t read(uint8_t* output, size_t count) override { return file_.read(output, count); }
 private:
  fs::File& file_;
};
void printStorage() {
  if (!filesystemReady) { Serial.println("LittleFS unavailable; upload filesystem image first"); return; }
  Serial.printf("LittleFS: used=%u total=%u bytes\n", static_cast<unsigned>(LittleFS.usedBytes()),
      static_cast<unsigned>(LittleFS.totalBytes()));
  auto directory = LittleFS.open("/media");
  if (!directory || !directory.isDirectory()) { Serial.println("/media unavailable"); return; }
  auto file = directory.openNextFile();
  while (file) {
    Serial.printf("  %s %u bytes\n", file.name(), static_cast<unsigned>(file.size()));
    file.close(); file = directory.openNextFile();
  }
}
bool renderFlashImage(const char* path) {
  if (!media::FileImage::validPath(path)) { Serial.println("[FAIL] Invalid media path"); return false; }
  if (!memoryPassed) { Serial.println("[FAIL] Storage/PSRAM unavailable"); return false; }
  fs::File file;
  bool isSd = (strncmp(path, "/sd", 3) == 0);
  if (isSd) {
    if (!storage::sd().isMounted()) { Serial.println("[FAIL] microSD belum terpasang"); return false; }
    file = storage::sd().open(path, "r");
  } else {
    if (!filesystemReady) { Serial.println("[FAIL] LittleFS belum siap"); return false; }
    file = LittleFS.open(path, "r");
  }
  if (!file || file.isDirectory()) { Serial.println("[FAIL] Media file unavailable"); return false; }
  FlashReader reader(file);
  const char* error = nullptr;
  if (!flashImage.load(path, reader, error)) { Serial.printf("[FAIL] File image: %s\n", error); return false; }
  file.close();
  media::ImageRenderOptions options;
  options.width = canvas.width(); options.height = canvas.height();
  if (!media::renderImage(flashImage.source(), canvas, options, error)) {
    Serial.printf("[FAIL] File render: %s\n", error); return false;
  }
  canvas.present();
  Serial.printf("[INFO] Rendered %s from %s; HUB75 transfer pending\n", path, isSd ? "microSD" : "LittleFS");
  return true;
}

bool parseStreamUrl(const String& url, String& host, uint16_t& port, String& path, bool& isHttps) {
  int hostStart = 0;
  if (url.startsWith("http://")) {
    isHttps = false;
    hostStart = 7;
    port = 80;
  } else if (url.startsWith("https://")) {
    isHttps = true;
    hostStart = 8;
    port = 443;
  } else {
    return false;
  }
  int slashIdx = url.indexOf('/', hostStart);
  String hostPort = (slashIdx < 0) ? url.substring(hostStart) : url.substring(hostStart, slashIdx);
  path = (slashIdx < 0) ? "/" : url.substring(slashIdx);
  int colonIdx = hostPort.indexOf(':');
  if (colonIdx >= 0) {
    host = hostPort.substring(0, colonIdx);
    port = static_cast<uint16_t>(hostPort.substring(colonIdx + 1).toInt());
  } else {
    host = hostPort;
  }
  return host.length() > 0 && port > 0;
}

void streamWorkerTask(void* pvParameters) {
  uint8_t* jpegBuf = static_cast<uint8_t*>(heap_caps_malloc(kStreamBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!jpegBuf) {
    streamStatus = "Alokasi memori stream gagal";
    vTaskDelete(nullptr);
    return;
  }
  media::JpegImage streamDecoder(allocateCanvas, heap_caps_free);
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  while (true) {
    const bool shouldRun = streamRunning.load() &&
                           (displayMode == MODE_STREAM || displayMode == MODE_AUTO) &&
                           (WiFi.status() == WL_CONNECTED) &&
                           streamUrl.length() > 0;
    if (!shouldRun) {
      if (plainClient.connected()) plainClient.stop();
      if (secureClient.connected()) secureClient.stop();
      if (WiFi.status() != WL_CONNECTED) {
        streamStatus = "Menunggu Wi-Fi STA terhubung";
      } else if (!streamRunning.load()) {
        streamStatus = "Stream dinonaktifkan";
      } else if (streamUrl.length() == 0) {
        streamStatus = "URL stream belum diatur";
      } else {
        streamStatus = "Mode non-stream aktif";
      }
      vTaskDelay(pdMS_TO_TICKS(300));
      continue;
    }

    String host, path;
    uint16_t port = 80;
    bool isHttps = false;
    if (!parseStreamUrl(streamUrl, host, port, path, isHttps)) {
      streamStatus = "Format URL tidak valid (harus http:// atau https://)";
      vTaskDelay(pdMS_TO_TICKS(1500));
      continue;
    }

    WiFiClient* client = isHttps ? static_cast<WiFiClient*>(&secureClient) : &plainClient;
    if (client->connected()) client->stop();

    streamStatus = String("Menghubungkan ke ") + host + (isHttps ? " (HTTPS)..." : "...");
    client->setTimeout(5);
    if (!client->connect(host.c_str(), port)) {
      streamStatus = "Gagal terhubung ke host stream";
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    client->printf("GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: ESP32-HUB75/1.0\r\nConnection: keep-alive\r\n\r\n",
                   path.c_str(), host.c_str());

    String line;
    bool inHeaders = true;
    uint32_t headerTimeout = millis();
    bool httpOk = false;
    while (client->connected() && inHeaders && millis() - headerTimeout < 4000U) {
      if (client->available()) {
        line = client->readStringUntil('\n');
        line.trim();
        if (line.startsWith("HTTP/1.") && (line.indexOf("200") > 0 || line.indexOf("OK") > 0)) {
          httpOk = true;
        }
        if (line.length() == 0) {
          inHeaders = false;
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }

    if (!httpOk || inHeaders) {
      client->stop();
      streamStatus = "Respons HTTP bukan 200 OK";
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    streamStatus = "Streaming aktif";

    while (client->connected() && streamRunning.load() &&
           (displayMode == MODE_STREAM || displayMode == MODE_AUTO)) {
      int prev = -1;
      bool foundSoi = false;
      uint32_t soiTimeout = millis();
      while (client->connected() && !foundSoi && millis() - soiTimeout < 3000U) {
        if (client->available()) {
          int b = client->read();
          if (prev == 0xFF && b == 0xD8) {
            foundSoi = true;
            break;
          }
          prev = b;
        } else {
          vTaskDelay(pdMS_TO_TICKS(5));
        }
      }

      if (!foundSoi) break;

      size_t jpegLen = 2;
      jpegBuf[0] = 0xFF;
      jpegBuf[1] = 0xD8;
      bool foundEoi = false;
      prev = -1;
      uint32_t eoiTimeout = millis();
      while (client->connected() && !foundEoi && jpegLen < kStreamBufferSize && millis() - eoiTimeout < 1500U) {
        if (client->available()) {
          int b = client->read();
          jpegBuf[jpegLen++] = static_cast<uint8_t>(b);
          if (prev == 0xFF && b == 0xD9) {
            foundEoi = true;
            break;
          }
          prev = b;
        } else {
          vTaskDelay(pdMS_TO_TICKS(2));
        }
      }

      if (foundEoi && jpegLen > 4) {
        const char* err = nullptr;
        if (streamDecoder.open(jpegBuf, jpegLen, err)) {
          media::ImageRenderOptions opts;
          opts.width = canvas.width();
          opts.height = canvas.height();
          opts.mode = media::ScaleMode::Fit;
          if (media::renderImage(streamDecoder.source(), canvas, opts, err)) {
            canvas.present();
            streamFrameCount++;
            streamLastFrameAt.store(millis());
          }
        }
      }

      const uint32_t targetDelay = 1000 / (streamFps ? streamFps : 15);
      vTaskDelay(pdMS_TO_TICKS(targetDelay));
    }

    client->stop();
    if (streamRunning.load()) {
      streamStatus = "Stream terputus; menghubungkan ulang...";
      vTaskDelay(pdMS_TO_TICKS(1500));
    }
  }
}

void printMedia() {
  Serial.printf("Media: builtin BMP, ready=%s source=%ux%u next_mode=%s\n",
      imageReady ? "YES" : "NO", demoImage.width(), demoImage.height(),
      media::scaleModeName(static_cast<media::ScaleMode>(imageModeIndex)));
  Serial.printf("JPEG: decoded=%s source=%ux%u next_mode=%s workspace_bytes=%u\n",
      jpegReady ? "YES" : "NO", demoJpeg.width(), demoJpeg.height(),
      media::scaleModeName(static_cast<media::ScaleMode>(jpegModeIndex)),
      static_cast<unsigned>(media::JpegImage::decoderWorkspaceBytes()));
}

void renderDemoJpeg() {
  if (!memoryPassed) { Serial.println("[FAIL] JPEG requires valid PSRAM baseline"); return; }
  const char* error = nullptr;
  if (!jpegReady) {
    jpegReady = demoJpeg.open(media::kExampleJpeg, sizeof media::kExampleJpeg, error);
    if (!jpegReady) { Serial.printf("[FAIL] JPEG decode: %s\n", error); return; }
    Serial.println("[PASS] Builtin JPEG decoded to owned RGB565 pixels in PSRAM");
  }
  media::ImageRenderOptions options;
  options.width = canvas.width(); options.height = canvas.height();
  options.mode = static_cast<media::ScaleMode>(jpegModeIndex);
  if (!media::renderImage(demoJpeg.source(), canvas, options, error)) {
    Serial.printf("[FAIL] JPEG render: %s\n", error); return;
  }
  if (!canvas.present()) { Serial.println("[FAIL] Canvas unavailable"); return; }
  Serial.printf("[INFO] Rendered builtin JPEG with %s; HUB75 transfer pending\n", media::scaleModeName(options.mode));
  jpegModeIndex = (jpegModeIndex + 1) % 5;
}

void renderDemoImage() {
  if (!imageReady) { Serial.println("[FAIL] BMP demo unavailable"); return; }
  media::ImageRenderOptions options;
  options.width = canvas.width(); options.height = canvas.height();
  options.mode = static_cast<media::ScaleMode>(imageModeIndex);
  const char* error = nullptr;
  if (!media::renderImage(demoImage.source(), canvas, options, error)) {
    Serial.printf("[FAIL] Image render: %s\n", error); return;
  }
  if (!canvas.present()) { Serial.println("[FAIL] Canvas unavailable"); return; }
  Serial.printf("[INFO] Rendered builtin BMP with %s to canvas; HUB75 transfer pending\n",
      media::scaleModeName(options.mode));
  imageModeIndex = (imageModeIndex + 1) % 5;
}

void printProfile() {
  if (!profileReady) { Serial.println("Profile unavailable"); return; }
  Serial.printf("Panel=%s id=%s %ux%u scan=1/%u address_lines=%u rgb=%s UNVERIFIED\n",
                panelProfile.name, panelProfile.id, panelProfile.width, panelProfile.height,
                panelProfile.scan, panelProfile.addressLines,
                display::rgbOrderName(panelProfile.rgbOrder));
}

void printCanvas() {
  Serial.printf("Canvas=%ux%u double_buffer_bytes=%u generation=%lu\n",
                canvas.width(), canvas.height(), static_cast<unsigned>(canvas.bufferBytes() * 2),
                static_cast<unsigned long>(canvas.generation()));
}

void printLayout() {
  if (!layoutReady) { Serial.println("Layout unavailable"); return; }
  Serial.printf("Layout=%ux%u panels=%u chain_pixels=%u preset=%u\n",
      panelLayout.canvasWidth, panelLayout.canvasHeight, panelLayout.panelCount,
      static_cast<unsigned>(layoutMapper.chainPixels()), layoutPreset);
  for (unsigned i = 0; i < panelLayout.panelCount; ++i) {
    const auto& p = panelLayout.panels[i];
    Serial.printf("  chain=%u position=(%u,%u) clockwise=%u\n", p.chainIndex, p.x, p.y, p.rotation);
  }
}

bool activateLayout(unsigned preset) {
  if (!profileReady || !memoryPassed) { Serial.println("[FAIL] Profile/memory baseline unavailable"); return false; }
  const uint8_t columns[] = {1, 2, 1, 2, 2};
  const uint8_t rows[] = {1, 1, 2, 2, 2};
  if (preset != 0) { Serial.println("[FAIL] Physical 128x64 output supports one panel only"); return false; }
  display::PanelLayout candidate;
  display::LayoutMapper candidateMapper;
  const char* error = nullptr;
  if (!display::makeGridLayout(panelProfile, columns[preset], rows[preset], preset == 4, candidate, error) ||
      !candidateMapper.configure(panelProfile, candidate, error)) {
    Serial.printf("[FAIL] Layout: %s\n", error); return false;
  }
  if (!canvas.configure(candidate.canvasWidth, candidate.canvasHeight)) {
    Serial.println("[FAIL] Canvas allocation; previous layout retained"); return false;
  }
  panelLayout = candidate; layoutMapper = candidateMapper;
  layoutReady = true; layoutPreset = preset;
  diagnosticIndex = 0; diagnosticStep = 0;
  printLayout();
  return true;
}

void nextDiagnostic() {
  const auto pattern = static_cast<display::Pattern>(diagnosticIndex);
  if (!display::drawDiagnostic(canvas, pattern, diagnosticStep) || !canvas.present()) {
    Serial.println("[FAIL] Diagnostic canvas unavailable"); return;
  }
  Serial.printf("[INFO] Rendered %s, step=%lu in memory; HUB75 transfer pending\n",
                display::patternName(pattern), static_cast<unsigned long>(diagnosticStep));
  printCanvas();
  diagnosticIndex = (diagnosticIndex + 1) % 9;
  if (diagnosticIndex == 0) ++diagnosticStep;
}

void initializeCanvas() {
  const char* error = nullptr;
  profileReady = display::parseProfile(display::kHardwareProfileJson,
      sizeof(display::kHardwareProfileJson) - 1, panelProfile, error);
  if (!profileReady) { Serial.printf("[FAIL] Profile: %s\n", error); return; }
  panelProfile.scan = selectedScan;
  panelProfile.addressLines = selectedScan == 32 ? 5 : selectedScan == 16 ? 4 : selectedScan == 8 ? 3 : 2;
  panelProfile.hub75e = selectedScan == 32;
  snprintf(panelProfile.id, sizeof panelProfile.id, "p25_128x64_scan%u", selectedScan);
  snprintf(panelProfile.name, sizeof panelProfile.name, "P2.5 RGB 128x64 1/%u", selectedScan);
  printProfile();
  if (!activateLayout(0)) {
    Serial.println("[FAIL] Canvas initialization; memory baseline must pass"); return;
  }
  Serial.println("[PASS] RGB565 double buffer allocated in PSRAM");
  nextDiagnostic();
}

void printHelp() {
  Serial.println("s=status, w=Wi-Fi scan, p=profile, j=profile JSON, d=diagnostic, l=layout, n=next layout, k=layout JSON");
  Serial.println("b=render BMP/next scaler mode, i=render JPEG/next scaler mode, m=media info, h=help");
  Serial.println("f=LittleFS info/list, u=cek & baca microSD, o=flash test_bars.bmp, v=flash test_bars.jpg");
  Serial.println("HUB75 output: q=next mapping, x=blank/resume, +=tambah kecerahan, -=kurangi kecerahan.");
}

bool testPsram() {
  auto* buffer = static_cast<uint8_t*>(
      heap_caps_malloc(kPsramTestBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buffer) {
    Serial.println("[FAIL] PSRAM test allocation (64 KiB)");
    return false;
  }
  // Sample allocation test, not an exhaustive test of the entire PSRAM.
  bool passed = true;
  for (unsigned pass = 0; pass < 2; ++pass) {
    for (size_t i = 0; i < kPsramTestBytes; ++i)
      buffer[i] = static_cast<uint8_t>((i * 37U + (i >> 8U)) ^
                                       (pass ? 0xAAU : 0x55U));
    for (size_t i = 0; i < kPsramTestBytes; ++i) {
      const auto expected = static_cast<uint8_t>(
          (i * 37U + (i >> 8U)) ^ (pass ? 0xAAU : 0x55U));
      if (buffer[i] != expected) { passed = false; break; }
    }
    if (!passed) break;
  }
  heap_caps_free(buffer);
  Serial.printf("[%s] PSRAM sample write/read: 64 KiB, two patterns\n",
                passed ? "PASS" : "FAIL");
  return passed;
}

void printStatus() {
  Serial.printf("Animation output: %.1f FPS transfer=%lu us (target ~60 FPS, HUB75 clock 4MHz)\n", display::hub75Fps(), static_cast<unsigned long>(display::hub75TransferMicros()));
  Serial.printf("AP radio: running=%u starts=%lu stops=%lu reset_reason=%d\n", apRunning.load(), static_cast<unsigned long>(apStarts.load()), static_cast<unsigned long>(apStops.load()), static_cast<int>(esp_reset_reason()));
  Serial.printf("WiFi diagnostic: mode=%u channel=%d sleep=%u AP joins=%lu leaves=%lu STA disconnects=%lu reason=%u\n", static_cast<unsigned>(WiFi.getMode()), WiFi.channel(), static_cast<unsigned>(WiFi.getSleep()), static_cast<unsigned long>(apJoins.load()), static_cast<unsigned long>(apLeaves.load()), static_cast<unsigned long>(staDisconnects.load()), lastStaReason.load());
  Serial.printf("HUB75=%s brightness=%u/255 mapping=%u\n", display::hub75Ready() ? "READY" : "OFF", display::hub75Blanked() ? 0 : display::hub75Brightness(), display::hub75Mapping());
  Serial.printf("uptime_ms=%lu memory=%s internal_free=%u internal_min=%u "
                "psram_free=%u psram_largest=%u\n",
                millis(), memoryPassed ? "PASS" : "FAIL",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
  Serial.printf("Wi-Fi: STA=%s AP=%s\n",
      WiFi.status() == WL_CONNECTED ? "CONNECTED" : (wifiConnecting ? "CONNECTING" : "DISCONNECTED"),
      wifiApReady ? "READY" : "OFF");
  if (WiFi.status() == WL_CONNECTED) Serial.printf("STA IP=%s\n", WiFi.localIP().toString().c_str());
  if (wifiApReady) Serial.printf("AP SSID=%s IP=%s clients=%u\n", LED_AP_SSID,
      WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum());
}

void startFallbackAp(const char* reason) {
  wifiConnecting = false;
  wifiRetryAt = millis();
  if (wifiApReady) { WiFi.disconnect(); return; }
  Serial.printf("[INFO] AP fallback: %s\n", reason);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  wifiApReady = WiFi.mode(staSsid.length() ? WIFI_AP_STA : WIFI_AP) &&
      WiFi.softAP(LED_AP_SSID, LED_AP_PASSWORD, 1, false, 4);
  WiFi.setSleep(false);
  Serial.printf("[%s] AP SSID=%s IP=%s\n", wifiApReady ? "PASS" : "FAIL",
      LED_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void scanWifi() {
  if (wifiConnecting) { Serial.println("Wi-Fi STA connection pending; scan deferred"); return; }
  if (!(WiFi.getMode() & WIFI_MODE_STA)) WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  Serial.println("Wi-Fi scan started (asynchronous, no credentials required)");
  const int result = WiFi.scanNetworks(true, true);
  wifiScanStarted = millis();
  wifiScanPending = result != WIFI_SCAN_FAILED;
  if (!wifiScanPending) {
    Serial.println("[FAIL] Could not start Wi-Fi scan");
    if (WiFi.status() != WL_CONNECTED) startFallbackAp("scan failed");
  }
}

void updateWifi() {
  // Recover an actual AP-stop/mode loss, not missing clients or HTTP timeouts.
  // Rate-limit recovery so a healthy AP is never periodically restarted.
  if (millis() - apWatchdogAt >= 5000U) {
    apWatchdogAt = millis();
    wifi_mode_t actualMode = WIFI_MODE_NULL;
    const bool modeKnown = esp_wifi_get_mode(&actualMode) == ESP_OK;
    if (!apRunning.load() || (modeKnown && !(actualMode & WIFI_MODE_AP))) {
      Serial.println("[WIFI] AP watchdog: restoring stopped hotspot");
      wifiApReady = WiFi.mode(staSsid.length() ? WIFI_AP_STA : WIFI_AP) &&
          WiFi.softAP(LED_AP_SSID, LED_AP_PASSWORD, 1, false, 4);
      WiFi.setSleep(false);
    }
  }
  const int networks = WiFi.scanComplete();
  if (wifiScanPending && networks >= 0) {
    discoveredNetworks.clear();
    for (int i = 0; i < networks; ++i) {
      String s = WiFi.SSID(i);
      s.trim();
      if (s.length() > 0) {
        bool exists = false;
        for (auto& item : discoveredNetworks) {
          if (item.ssid == s) {
            if (WiFi.RSSI(i) > item.rssi) {
              item.rssi = WiFi.RSSI(i);
              item.channel = WiFi.channel(i);
            }
            exists = true;
            break;
          }
        }
        if (!exists && discoveredNetworks.size() < 30) {
          discoveredNetworks.push_back({
            s,
            WiFi.RSSI(i),
            static_cast<uint8_t>(WiFi.channel(i)),
            WiFi.encryptionType(i) != WIFI_AUTH_OPEN
          });
        }
      }
    }
    std::sort(discoveredNetworks.begin(), discoveredNetworks.end(),
              [](const DiscoveredNetwork& a, const DiscoveredNetwork& b) {
                return a.rssi > b.rssi;
              });

    bool targetFound = false;
    bool hiddenFound = false;
    for (int i = 0; i < networks; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid == staSsid && ssid.length()) targetFound = true;
      if (!ssid.length()) hiddenFound = true;
    }
    Serial.printf("[INFO] Wi-Fi scan complete: %d networks found (%u unique); target=%s\n",
        networks, static_cast<unsigned>(discoveredNetworks.size()),
        targetFound ? "FOUND" : "NOT FOUND");
    WiFi.scanDelete();
    wifiScanPending = false;
    const bool wasUserScan = userScanPending;
    userScanPending = false;
    if (!wasUserScan && WiFi.status() != WL_CONNECTED && staSsid.length()) {
      if (targetFound || hiddenFound) {
        WiFi.setAutoReconnect(false);
        WiFi.begin(staSsid.c_str(), staPassword.c_str());
        wifiConnectStarted = millis();
        wifiConnecting = true;
        Serial.println("[INFO] STA connecting (15 second timeout)");
      } else startFallbackAp("target SSID not found");
    }
  } else if (wifiScanPending && (networks == WIFI_SCAN_FAILED ||
             millis() - wifiScanStarted >= kWifiScanTimeoutMs)) {
    wifiScanPending = false;
    userScanPending = false;
    WiFi.scanDelete();
    if (WiFi.status() != WL_CONNECTED && staSsid.length()) startFallbackAp("scan failed or timed out");
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) Serial.printf("[PASS] STA connected IP=%s\n", WiFi.localIP().toString().c_str());
    wifiConnected = true;
    wifiConnecting = false;
  } else if (wifiConnected) {
    wifiConnected = false;
    startFallbackAp("STA lost; keeping local access");
  } else if (wifiConnecting && millis() - wifiConnectStarted >= kWifiConnectTimeoutMs) {
    startFallbackAp("STA connection timed out");
  }
  if (staSsid.length() && WiFi.status() != WL_CONNECTED && !wifiConnecting &&
      !wifiScanPending && !wifiApplyPending && millis() - wifiRetryAt >= kWifiRetryMs) {
    wifiRetryAt = millis();
    WiFi.begin(staSsid.c_str(), staPassword.c_str());
    WiFi.setSleep(false);
    wifiConnectStarted = millis(); wifiConnecting = true;
    Serial.println("[WIFI] STA retry without full-channel scan");
  }
}

void sendJson(int code, const JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(code, "application/json", body);
}

void webReply(int code, const char* message) {
  JsonDocument doc;
  doc[code < 400 ? "message" : "error"] = message;
  sendJson(code, doc);
}

unsigned mediaFileCount() {
  unsigned count = 0;
  auto directory = LittleFS.open("/media");
  if (!directory || !directory.isDirectory()) return 0;
  auto file = directory.openNextFile();
  while (file) {
    if (!file.isDirectory()) ++count;
    file.close();
    file = directory.openNextFile();
  }
  return count;
}

bool uploadToSd = false;
constexpr const char* kSdUploadTempPath = "/.upload.tmp";

void failUpload(int code, const char* message) {
  if (!uploadError.length()) { uploadErrorCode = code; uploadError = message; }
  uploadValidated = false;
  uploadFile.close();
  if (uploadToSd) {
    if (storage::sd().isMounted()) storage::sd().remove(kSdUploadTempPath);
  } else {
    if (filesystemReady) LittleFS.remove(kUploadTempPath);
  }
}

void resetUpload() {
  uploadFile.close();
  if (uploadToSd) {
    if (storage::sd().isMounted()) storage::sd().remove(kSdUploadTempPath);
  } else {
    if (filesystemReady) LittleFS.remove(kUploadTempPath);
  }
  uploadPath = "";
  uploadError = "";
  uploadSeen = false;
  uploadValidated = false;
  uploadBytes = 0;
  uploadToSd = false;
}

void handleMediaUpload() {
  auto& upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (uploadSeen) { failUpload(400, "Upload satu file per permintaan"); return; }
    resetUpload();
    uploadSeen = true;

    bool isAudio = storage::SdStorage::isAudioFile(upload.filename.c_str());
    bool isVideo = storage::SdStorage::isVideoFile(upload.filename.c_str());
    bool isImage = storage::SdStorage::isImageFile(upload.filename.c_str());

    if (!isAudio && !isVideo && !isImage) {
      failUpload(400, "Format tidak didukung. Gunakan .bmp, .jpg, .jpeg, .mjpeg, .wav, atau .mp3"); return;
    }

    uploadToSd = (webServer.hasArg("storage") && webServer.arg("storage") == "sd") ||
                 (isAudio && storage::sd().isMounted());

    if (uploadToSd) {
      if (!storage::sd().isMounted()) { failUpload(503, "microSD belum siap"); return; }
      uploadPath = String("/sd/media/") + (isAudio ? "audio/" : "") + upload.filename;
      if (storage::sd().exists(uploadPath.c_str())) { failUpload(409, "Nama file sudah ada di microSD"); return; }
      if (storage::sd().status().freeBytes < 1048576) { failUpload(507, "microSD penuh"); return; }
      uploadFile = storage::sd().open(kSdUploadTempPath, "w");
      if (!uploadFile) failUpload(500, "File sementara di microSD gagal dibuat");
    } else {
      uploadPath = String("/media/") + upload.filename;
      if (!filesystemReady || !memoryPassed) { failUpload(503, "Storage/PSRAM belum siap"); return; }
      if (upload.name != "file" || uploadPath.length() != strlen(uploadPath.c_str()) ||
          !media::FileImage::validPath(uploadPath.c_str())) {
        failUpload(400, "Nama file hanya huruf, angka, _, -, dan titik; ekstensi .bmp/.jpg/.jpeg/.mjpeg"); return;
      }
      if (LittleFS.exists(uploadPath)) { failUpload(409, "Nama file sudah ada. Gunakan nama lain."); return; }
      if (mediaFileCount() >= kMaxMediaFiles) { failUpload(507, "Batas 64 file tercapai; hapus media terlebih dahulu"); return; }
      if (LittleFS.totalBytes() - LittleFS.usedBytes() <= kStorageReserveBytes) {
        failUpload(507, "Storage penuh; hapus media terlebih dahulu"); return;
      }
      if (!LittleFS.exists("/media") && !LittleFS.mkdir("/media")) {
        failUpload(500, "Folder media gagal dibuat"); return;
      }
      uploadFile = LittleFS.open(kUploadTempPath, "w");
      if (!uploadFile) failUpload(500, "File sementara gagal dibuat");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadError.length()) return;
    if (!uploadSeen || !uploadFile) { failUpload(400, "Upload belum dimulai"); return; }
    if (!uploadToSd && upload.currentSize > kMaxMediaBytes - uploadBytes) {
      failUpload(413, "File LittleFS maksimal 4 MiB"); return;
    }
    if (uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
      failUpload(507, "Penulisan gagal atau storage penuh"); return;
    }
    uploadBytes += upload.currentSize;
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadError.length()) return;
    uploadFile.flush();
    uploadFile.close();

    if (uploadToSd) {
      if (!uploadBytes) { failUpload(422, "File kosong"); return; }
      if (storage::sd().exists(uploadPath.c_str())) storage::sd().remove(uploadPath.c_str());
      if (!storage::sd().rename(kSdUploadTempPath, uploadPath.c_str())) {
        failUpload(500, "Gagal memindahkan file ke direktori tujuan di microSD"); return;
      }
      uploadValidated = true;
    } else {
      auto candidateFile = LittleFS.open(kUploadTempPath, "r");
      if (!candidateFile || !uploadBytes || candidateFile.size() != uploadBytes || upload.totalSize != uploadBytes) {
        candidateFile.close(); failUpload(422, "File kosong atau upload tidak lengkap"); return;
      }
      FlashReader reader(candidateFile);
      media::FileImage candidate(allocateCanvas, heap_caps_free);
      const char* error = nullptr;
      const bool valid = candidate.load(uploadPath.c_str(), reader, error);
      candidateFile.close();
      if (!valid) { failUpload(422, error ? error : "Media tidak valid"); return; }
      uploadValidated = true;
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    resetUpload();
  }
}

bool saveDisplaySettings(unsigned preset, const String& path) {
  JsonDocument doc;
  doc["layout"] = preset;
  doc["media"] = path;
  doc["brightness"] = display::hub75Brightness();
  doc["display_mode"] = static_cast<uint8_t>(displayMode);
  doc["stream_url"] = streamUrl;
  doc["stream_fps"] = streamFps;
  doc["stream_running"] = streamRunning.load();
  const auto& settings = animation.settings();
  auto motion = doc["animation"].to<JsonObject>();
  motion["text"] = settings.text;
  motion["text_scale"] = settings.textScale;
  motion["text_color"] = settings.textColor;
  motion["background"] = settings.background;
  motion["image_visible"] = settings.imageVisible;
  motion["image_effect"] = media::effectName(settings.image.effect);
  motion["text_effect"] = media::effectName(settings.textMotion.effect);
  motion["duration_ms"] = settings.image.durationMs;
  motion["speed"] = settings.textMotion.speed;
  motion["loop"] = settings.image.loop;
  motion["active"] = animationActive;
  String saved;
  serializeJson(doc, saved);
  Preferences prefs;
  if (!prefs.begin("led-display", false)) return false;
  const bool ok = prefs.putString("config", saved) == saved.length();
  prefs.end();
  return ok;
}

void restoreDisplaySettings() {
  Preferences prefs;
  if (!prefs.begin("led-display", true)) return;
  const String saved = prefs.getString("config", "");
  prefs.end();
  JsonDocument doc;
  if (!saved.length()) return;
  if (deserializeJson(doc, saved) || !doc["layout"].is<unsigned>() || doc["layout"].as<unsigned>() > 4 ||
      !doc["media"].is<String>()) {
    Serial.println("[FAIL] Saved display settings invalid; keeping default"); return;
  }
  if (doc["brightness"].is<uint8_t>()) {
    const uint8_t b = doc["brightness"].as<uint8_t>();
    if (b >= 1) {
      display::setHub75Brightness(b);
      Serial.printf("[INFO] Restored brightness: %u/255\n", b);
    }
  }
  if (doc["display_mode"].is<uint8_t>()) {
    displayMode = static_cast<DisplayMode>(doc["display_mode"].as<uint8_t>());
  }
  if (doc["stream_url"].is<String>()) {
    streamUrl = doc["stream_url"].as<String>();
  }
  if (doc["stream_fps"].is<uint8_t>()) {
    streamFps = doc["stream_fps"].as<uint8_t>();
    if (streamFps < 1 || streamFps > 30) streamFps = 15;
  }
  if (doc["stream_running"].is<bool>()) {
    streamRunning.store(doc["stream_running"].as<bool>());
  }
  const String path = doc["media"].as<String>();
  if (path.length() && !media::FileImage::validPath(path.c_str())) return;
  if (!activateLayout(0)) return;
  nextDiagnostic();
  if (path.length()) {
    if (media::MjpegPlayer::isMjpegPath(path.c_str())) {
      if (mjpegPlayer.open(path.c_str(), 15)) {
        selectedMedia = path;
        mjpegPlayer.renderNextFrame(canvas);
      }
    } else if (renderFlashImage(path.c_str())) {
      selectedMedia = path;
    }
  }
  if (doc["animation"].is<JsonObject>()) {
    const auto stored = doc["animation"].as<JsonObject>();
    media::AnimationSettings settings;
    const String text = stored["text"] | "";
    if (text.length() <= 192 && text.length() == strlen(text.c_str()) &&
        stored["duration_ms"].is<uint32_t>() && stored["speed"].is<uint16_t>() &&
        stored["text_scale"].is<uint8_t>() && stored["text_color"].is<uint16_t>() &&
        stored["background"].is<uint16_t>() && stored["loop"].is<bool>() &&
        stored["image_visible"].is<bool>() && stored["active"].is<bool>() &&
        media::parseEffect(stored["image_effect"] | "", settings.image.effect) &&
        media::parseEffect(stored["text_effect"] | "", settings.textMotion.effect)) {
      memcpy(settings.text, text.c_str(), text.length() + 1);
      settings.textScale = stored["text_scale"];
      settings.textColor = stored["text_color"];
      settings.background = stored["background"];
      settings.imageVisible = stored["image_visible"];
      settings.image.durationMs = settings.textMotion.durationMs = stored["duration_ms"];
      settings.image.speed = settings.textMotion.speed = stored["speed"];
      settings.image.loop = settings.textMotion.loop = stored["loop"];
      if (animation.configure(settings) && animation.captureImage(canvas)) {
        animationActive = stored["active"];
        animationLastTick = animationLastFrame = millis();
        if (animationActive && animation.render(canvas)) canvas.present();
      }
    }
  }
  Serial.println("[INFO] Saved display settings restored");
}

void loadWifiSettings() {
  Preferences prefs;
  if (!prefs.begin("led-network", true)) return;
  const String saved = prefs.getString("config", "");
  prefs.end();
  JsonDocument doc;
  if (saved.length() && !deserializeJson(doc, saved) &&
      doc["ssid"].is<String>() && doc["password"].is<String>()) {
    staSsid = doc["ssid"].as<String>();
    staPassword = doc["password"].as<String>();
  }
}

void applyWifiSettings() {
  wifiApplyPending = false;
  // Preserve AP access while trying new credentials.
  wifiConnecting = false;
  wifiConnected = false;
  if (wifiScanPending) {
    esp_wifi_scan_stop();
    wifiScanPending = false;
    WiFi.scanDelete();
  }
  WiFi.disconnect();
  if (!staSsid.length()) WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  if (staSsid.length()) scanWifi();
  else startFallbackAp("STA SSID cleared");
}

void loadPanelSettings() {
  Preferences prefs;
  if (!prefs.begin("led-panel", false)) return;
  const String saved = prefs.getString("config", "");
  const String pending = prefs.getString("preview", "");
  // Consume a one-shot candidate before activation. A subsequent reboot always
  // returns to the last committed record, including if begin() fails/crashes.
  const bool consumed = !pending.length() || prefs.remove("preview");
  prefs.end();
  JsonDocument doc;
  if (!deserializeJson(doc, saved) && doc["scan"].is<unsigned>() &&
      doc["mapping"].is<unsigned>() && display::validHub75Settings(doc["scan"], doc["mapping"])) {
    selectedScan = doc["scan"]; selectedMapping = doc["mapping"];
  }
  doc.clear();
  if (consumed && !deserializeJson(doc, pending) && doc["scan"].is<unsigned>() &&
      doc["mapping"].is<unsigned>() && display::validHub75Settings(doc["scan"], doc["mapping"])) {
    selectedScan = doc["scan"]; selectedMapping = doc["mapping"];
    panelPreviewActive = true;
    Serial.println("[PANEL] One-shot preview loaded; confirm within 30 seconds");
  }
}

bool confirmPanelPreview() {
  if (!panelPreviewActive || panelRestartPending ||
      millis() - panelPreviewStarted >= kPanelPreviewMs || !display::hub75Ready()) return false;
  JsonDocument doc; doc["scan"] = selectedScan; doc["mapping"] = selectedMapping;
  String saved; serializeJson(doc, saved);
  Preferences prefs;
  if (!prefs.begin("led-panel", false)) return false;
  const bool ok = prefs.putString("config", saved) == saved.length(); prefs.end();
  if (!ok) return false;
  panelPreviewActive = false;
  Serial.printf("[PANEL] Preview confirmed and saved: scan=1/%u mapping=%u\n", selectedScan, selectedMapping);
  return true;
}

void updateConfirmButton() {
  if (!confirmButton.update(digitalRead(board::profile().confirmButton) == LOW, millis())) return;
  if (!panelPreviewActive || panelRestartPending) {
    Serial.println("[BUTTON] GPIO17 pressed; no active preview to confirm"); return;
  }
  if (!confirmPanelPreview()) Serial.println("[BUTTON] Confirm failed; preview timeout remains active");
}

void startWebUi() {
  webServer.on("/api/panel", HTTP_GET, [] {
    JsonDocument doc;
    doc["scan"] = selectedScan; doc["mapping"] = selectedMapping;
    doc["active_scan"] = display::hub75Scan();
    doc["active_mapping"] = display::hub75Mapping();
    doc["restart_pending"] = panelRestartPending;
    doc["preview_active"] = panelPreviewActive;
    doc["preview_remaining_ms"] = panelPreviewActive ?
        (millis() - panelPreviewStarted < kPanelPreviewMs ? kPanelPreviewMs - (millis() - panelPreviewStarted) : 0) : 0;
    sendJson(200, doc);
  });
  webServer.on("/api/panel", HTTP_POST, [] {
    if (panelRestartPending) { webReply(409, "Restart sedang disiapkan"); return; }
    const String command = webServer.arg("command");
    if (command == "cancel") {
      if (!panelPreviewActive) { webReply(409, "Tidak ada preview aktif"); return; }
      panelRestartPending = true; panelRestartAt = millis();
      webReply(200, "Preview dibatalkan; restart kembali ke konfigurasi tersimpan"); return;
    }
    if (command == "confirm") {
      if (!panelPreviewActive || millis() - panelPreviewStarted >= kPanelPreviewMs || !display::hub75Ready()) {
        webReply(409, "Preview tidak aktif, kedaluwarsa, atau output gagal"); return;
      }
      if (!confirmPanelPreview()) {
        webReply(500, "Konfirmasi gagal disimpan; rollback tetap berlaku"); return;
      }
      webReply(200, "Hasil panel dikonfirmasi; konfigurasi tersimpan"); return;
    }
    if ((command != "preview" && command != "save") || (command == "preview" && panelPreviewActive)) {
      webReply(409, "Pilih preview; konfirmasi atau batalkan preview aktif terlebih dahulu"); return;
    }
    const String scanText = webServer.arg("scan"), mappingText = webServer.arg("mapping");
    if ((scanText != "4" && scanText != "8" && scanText != "16" && scanText != "32") ||
        mappingText.length() != 1 || mappingText[0] < '0' || mappingText[0] > '5' ||
        !display::validHub75Settings(scanText.toInt(), mappingText.toInt())) {
      webReply(400, "Scan/mapping tidak valid: 1/4 atau 1/8 bank linear, 1/16 mapping 0-3, 1/32 langsung"); return;
    }
    JsonDocument doc;
    doc["scan"] = scanText.toInt(); doc["mapping"] = mappingText.toInt();
    String saved; serializeJson(doc, saved);
    Preferences prefs;
    if (!prefs.begin("led-panel", false)) { webReply(500, "Penyimpanan panel gagal"); return; }
    // A direct save must not leave a stale one-shot candidate for next boot.
    if (command == "save" && prefs.isKey("preview") && !prefs.remove("preview")) {
      prefs.end(); webReply(500, "Candidate lama gagal dihapus; pilihan belum disimpan"); return;
    }
    const bool ok = prefs.putString(command == "save" ? "config" : "preview", saved) == saved.length();
    prefs.end();
    if (!ok) { webReply(500, "Penyimpanan panel gagal; konfigurasi aktif tetap"); return; }
    panelRestartPending = true; panelRestartAt = millis();
    if (command == "save") {
      panelPreviewActive = false;
      webReply(200, "Scan tersimpan permanen. Controller restart untuk menerapkan.");
    } else {
      webReply(200, "Preview siap. Controller restart; sambungkan kembali lalu konfirmasi dalam 30 detik.");
    }
  });
  webServer.on("/", HTTP_GET, [] {
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send_P(200, "text/html; charset=utf-8", kWebUi);
  });
  webServer.on("/api/status", HTTP_GET, [] {
    JsonDocument doc;
    doc["uptime_ms"] = millis();
    doc["memory_ok"] = memoryPassed;
    doc["storage_ok"] = filesystemReady;
    doc["sta_connected"] = WiFi.status() == WL_CONNECTED;
    doc["sta_connecting"] = wifiConnecting;
    doc["sta_ip"] = WiFi.localIP().toString();
    doc["ap_ready"] = wifiApReady;
    doc["ap_ip"] = WiFi.softAPIP().toString();
    doc["ap_ssid"] = LED_AP_SSID;
    doc["target_ssid"] = staSsid;
    doc["ap_running"] = apRunning.load();
    doc["ap_starts"] = apStarts.load(); doc["ap_stops"] = apStops.load();
    doc["reset_reason"] = static_cast<int>(esp_reset_reason());
    doc["ap_clients"] = wifiApReady ? WiFi.softAPgetStationNum() : 0;
    doc["wifi_channel"] = WiFi.channel();
    doc["wifi_sleep"] = static_cast<unsigned>(WiFi.getSleep());
    doc["ap_joins"] = apJoins.load(); doc["ap_leaves"] = apLeaves.load();
    doc["sta_disconnects"] = staDisconnects.load(); doc["sta_last_reason"] = lastStaReason.load();
    doc["sta_rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    doc["width"] = canvas.width();
    doc["height"] = canvas.height();
    doc["generation"] = canvas.generation();
    doc["layout"] = layoutPreset;
    doc["rf_test_active"] = display::hub75WifiTestActive();
    doc["output_ready"] = display::hub75Ready();
    doc["output_blanked"] = display::hub75Blanked();
    doc["brightness"] = display::hub75Brightness();
    doc["hub75_mapping"] = display::hub75Mapping();
    doc["scan"] = display::hub75Scan();
    doc["output_fps"] = display::hub75Fps();
    doc["transfer_us"] = display::hub75TransferMicros();
    doc["selected_media"] = selectedMedia;
    doc["mjpeg_playing"] = mjpegPlayer.isPlaying();
    doc["mjpeg_path"] = mjpegPlayer.currentPath();
    doc["mjpeg_frames"] = mjpegPlayer.frameCount();
    doc["animation_active"] = animationActive;
    doc["animation_paused"] = animation.paused();
    doc["animation_moving"] = animationActive && animation.needsFrames(canvas.width(), canvas.height());
    doc["elapsed_ms"] = animation.elapsedMs();
    doc["display_mode"] = static_cast<int>(displayMode);
    doc["stream_url"] = streamUrl;
    doc["stream_fps"] = streamFps;
    doc["stream_running"] = streamRunning.load();
    doc["stream_status"] = streamStatus;
    doc["stream_frames"] = streamFrameCount.load();

    // SD Card status
    const auto sdSt = storage::sd().status();
    doc["sd_mounted"] = sdSt.mounted;
    doc["sd_type"] = sdSt.cardTypeName;
    doc["sd_total"] = static_cast<double>(sdSt.totalBytes);
    doc["sd_used"] = static_cast<double>(sdSt.usedBytes);
    doc["sd_free"] = static_cast<double>(sdSt.freeBytes);

    // Prayer & Clock status
    const auto clkSt = prayer::clockService().status();
    doc["clock_synced"] = clkSt.synced;
    doc["clock_time"] = clkSt.timeString;
    doc["clock_date"] = clkSt.dateString;
    doc["clock_hijri"] = clkSt.hijriString;
    doc["timezone"] = clkSt.timezone;

    const auto pSt = prayer::prayerSchedule().currentStatus();
    doc["next_prayer"] = pSt.nextPrayerName;
    doc["next_prayer_time"] = pSt.nextPrayerTime.toString();
    doc["countdown_seconds"] = pSt.countdownSeconds;
    doc["mosque_state"] = prayer::mosqueStateName(pSt.mosqueState);
    doc["mosque_state_code"] = static_cast<int>(pSt.mosqueState);
    doc["active_prayer"] = pSt.activePrayerName;
    doc["state_remaining_seconds"] = pSt.stateRemainingSeconds;

    // Audio status
    const auto aSt = audio::player().status();
    doc["audio_playing"] = (aSt.state == audio::AUDIO_PLAYING);
    doc["audio_file"] = aSt.activeFile;
    doc["audio_volume"] = aSt.volume;

    sendJson(200, doc);
  });
  webServer.on("/api/brightness", HTTP_GET, [] {
    JsonDocument doc;
    doc["brightness"] = display::hub75Brightness();
    doc["blanked"] = display::hub75Blanked();
    sendJson(200, doc);
  });
  webServer.on("/api/brightness", HTTP_POST, [] {
    if (webServer.hasArg("blank")) {
      const String blankStr = webServer.arg("blank");
      if (blankStr == "toggle") display::toggleHub75Blank();
      else if (blankStr == "1" || blankStr == "true") display::setHub75Blank(true);
      else if (blankStr == "0" || blankStr == "false") display::setHub75Blank(false);
      webReply(200, display::hub75Blanked() ? "Panel dimatikan sementara (blank)" : "Panel dihidupkan");
      return;
    }
    if (!webServer.hasArg("value")) {
      webReply(400, "Parameter 'value' diperlukan (1-255)");
      return;
    }
    const int val = webServer.arg("value").toInt();
    if (val < 1 || val > 255) {
      webReply(400, "Nilai kecerahan harus antara 1 sampai 255");
      return;
    }
    display::setHub75Brightness(static_cast<uint8_t>(val));
    const bool shouldSave = !webServer.hasArg("save") || webServer.arg("save") == "1" || webServer.arg("save") == "true";
    if (shouldSave) {
      if (!saveDisplaySettings(layoutPreset, selectedMedia)) {
        webReply(500, "Kecerahan diterapkan, tetapi penyimpanan gagal");
        return;
      }
      webReply(200, "Kecerahan berhasil diatur dan disimpan");
    } else {
      webReply(200, "Kecerahan berhasil diterapkan");
    }
  });
  webServer.on("/api/display", HTTP_GET, [] {
    JsonDocument doc;
    doc["mode"] = static_cast<int>(displayMode);
    doc["stream_url"] = streamUrl;
    doc["stream_fps"] = streamFps;
    doc["stream_running"] = streamRunning.load();
    doc["stream_status"] = streamStatus;
    doc["stream_frames"] = streamFrameCount.load();
    sendJson(200, doc);
  });
  webServer.on("/api/display", HTTP_POST, [] {
    const String command = webServer.arg("command");
    if (command == "mode") {
      const int mode = webServer.arg("mode").toInt();
      if (mode < 0 || mode > 4) {
        webReply(400, "Mode tampilan tidak valid (0=Auto, 1=Stream, 2=Animasi, 3=Gambar, 4=Jam Masjid)");
        return;
      }
      displayMode = static_cast<DisplayMode>(mode);
      saveDisplaySettings(layoutPreset, selectedMedia);
      const char* modeNames[] = {
        "Mode Otomatis (Stream > Animasi > Gambar > Jam Masjid)",
        "Hanya Video Stream",
        "Hanya Teks & Animasi",
        "Hanya Gambar Flash / SD",
        "Jam Masjid & Waktu Salat"
      };
      webReply(200, modeNames[mode]);
      return;
    }
    if (command == "stream_config") {
      const String url = webServer.arg("url");
      const int fps = webServer.arg("fps").toInt();
      if (url.length() > 255) {
        webReply(400, "URL maksimal 255 karakter"); return;
      }
      streamUrl = url;
      if (fps >= 1 && fps <= 30) streamFps = static_cast<uint8_t>(fps);
      if (webServer.hasArg("start")) {
        streamRunning.store(webServer.arg("start") == "1");
      }
      saveDisplaySettings(layoutPreset, selectedMedia);
      webReply(200, "Pengaturan stream video disimpan");
      return;
    }
    if (command == "stream_start") {
      if (streamUrl.length() == 0) {
        webReply(400, "Masukkan URL stream terlebih dahulu"); return;
      }
      streamRunning.store(true);
      saveDisplaySettings(layoutPreset, selectedMedia);
      webReply(200, "Streaming video dimulai");
      return;
    }
    if (command == "stream_stop") {
      streamRunning.store(false);
      saveDisplaySettings(layoutPreset, selectedMedia);
      webReply(200, "Streaming video dihentikan");
      return;
    }
    webReply(400, "Perintah tampilan tidak dikenal");
  });
  webServer.on("/api/animation", HTTP_GET, [] {
    const auto& settings = animation.settings();
    JsonDocument doc;
    doc["text"] = settings.text;
    doc["text_scale"] = settings.textScale;
    doc["text_color"] = settings.textColor;
    doc["background"] = settings.background;
    doc["image_visible"] = settings.imageVisible;
    doc["image_effect"] = media::effectName(settings.image.effect);
    doc["text_effect"] = media::effectName(settings.textMotion.effect);
    doc["duration_ms"] = settings.image.durationMs;
    doc["speed"] = settings.textMotion.speed;
    doc["loop"] = settings.image.loop;
    doc["active"] = animationActive;
    doc["paused"] = animation.paused();
    sendJson(200, doc);
  });
  webServer.on("/api/animation", HTTP_POST, [] {
    const String command = webServer.arg("command");
    if (!memoryPassed || !canvas.front()) { webReply(503, "Canvas belum siap"); return; }
    if (command == "pause" || command == "resume" || command == "restart") {
      if (!animationActive) { webReply(409, "Terapkan pengaturan animasi dahulu"); return; }
      if (command == "restart") {
        animation.restart();
        if (animation.render(canvas)) canvas.present();
      }
      else animation.setPaused(command == "pause");
      animationLastTick = millis();
      webReply(200, command == "pause" ? "Animasi dijeda" : "Animasi berjalan"); return;
    }
    if (command != "configure") { webReply(400, "Perintah animasi tidak dikenal"); return; }
    JsonDocument doc;
    const String payload = webServer.arg("settings");
    if (payload.length() > 1536 || deserializeJson(doc, payload) || !doc.is<JsonObject>()) {
      webReply(400, "Pengaturan animasi tidak valid"); return;
    }
    media::AnimationSettings settings;
    const String text = doc["text"] | "";
    if (!doc["text"].is<String>() || text.length() > 192 || text.length() != strlen(text.c_str()) ||
        !doc["text_scale"].is<uint8_t>() || !doc["text_color"].is<uint16_t>() ||
        !doc["background"].is<uint16_t>() || !doc["image_visible"].is<bool>() ||
        !doc["duration_ms"].is<uint32_t>() || !doc["speed"].is<uint16_t>() || !doc["loop"].is<bool>() ||
        !media::parseEffect(doc["image_effect"] | "", settings.image.effect) ||
        !media::parseEffect(doc["text_effect"] | "", settings.textMotion.effect)) {
      webReply(400, "Teks maksimal 192 byte; parameter animasi tidak valid"); return;
    }
    memcpy(settings.text, text.c_str(), text.length() + 1);
    settings.textScale = doc["text_scale"];
    settings.textColor = doc["text_color"];
    settings.background = doc["background"];
    settings.imageVisible = doc["image_visible"];
    settings.image.durationMs = settings.textMotion.durationMs = doc["duration_ms"];
    settings.image.speed = settings.textMotion.speed = doc["speed"];
    settings.image.loop = settings.textMotion.loop = doc["loop"];
    // Validate first so rejected settings retain the active image/timeline.
    media::AnimationPlayer validator(allocateCanvas, heap_caps_free);
    if (!validator.configure(settings)) {
      webReply(400, "Skala 1-4; durasi 200-60000 ms; kecepatan 1-200 px/detik; UTF-8 harus valid"); return;
    }
    if (!animation.hasImage() && !animation.captureImage(canvas)) {
      webReply(503, "Memori snapshot gambar tidak tersedia"); return;
    }
    animation.configure(settings);
    animationActive = true;
    animationLastTick = animationLastFrame = millis();
    if (animation.render(canvas)) canvas.present();
    if (!saveDisplaySettings(layoutPreset, selectedMedia)) {
      webReply(500, "Animasi berjalan tetapi pengaturan gagal disimpan"); return;
    }
    webReply(200, "Teks dan animasi diterapkan serta disimpan");
  });
  auto handleMediaList = [](const String& filterType) {
    JsonDocument doc;
    doc["ready"] = filesystemReady;
    doc["used"] = filesystemReady ? LittleFS.usedBytes() : 0;
    doc["total"] = filesystemReady ? LittleFS.totalBytes() : 0;
    doc["max_file_bytes"] = kMaxMediaBytes;
    doc["max_files"] = kMaxMediaFiles;

    const auto sdSt = storage::sd().status();
    doc["sd_mounted"] = sdSt.mounted;
    doc["sd_total"] = static_cast<double>(sdSt.totalBytes);
    doc["sd_used"] = static_cast<double>(sdSt.usedBytes);
    doc["sd_free"] = static_cast<double>(sdSt.freeBytes);

    auto files = doc["files"].to<JsonArray>();

    // microSD files
    if (storage::sd().isMounted()) {
      std::vector<storage::FileItem> allSdItems;
      auto scanDir = [&](const char* dir) {
        std::vector<storage::FileItem> dirItems;
        if (storage::sd().listFiles(dir, dirItems)) {
          for (const auto& it : dirItems) {
            bool exists = false;
            for (const auto& prev : allSdItems) {
              if (prev.path == it.path) { exists = true; break; }
            }
            if (!exists) allSdItems.push_back(it);
          }
        }
      };

      // Search root directory, /audio, /media, and /media/audio
      scanDir("/");
      scanDir("/audio");
      scanDir("/media");
      scanDir("/media/audio");

      for (const auto& item : allSdItems) {
        if (item.isDirectory) continue;
        if (filterType == "audio" && !item.isAudio) continue;
        if (filterType == "image" && !item.isImage) continue;
        if (filterType == "video" && !item.isVideo) continue;

        auto itemObj = files.add<JsonObject>();
        itemObj["name"] = item.name;
        itemObj["path"] = item.path;
        itemObj["size"] = item.size;
        itemObj["storage"] = "sd";
        itemObj["is_video"] = item.isVideo;
        itemObj["is_audio"] = item.isAudio;
        itemObj["is_image"] = item.isImage;
      }
    }

    // LittleFS files
    if (filesystemReady) {
      auto directory = LittleFS.open("/media");
      if (directory && directory.isDirectory()) {
        auto file = directory.openNextFile();
        while (file) {
          const String name = file.name();
          if (!file.isDirectory()) {
            bool isAudio = storage::SdStorage::isAudioFile(name.c_str());
            bool isVid = name.endsWith(".mjpeg") || name.endsWith(".mjpg");
            bool isImg = storage::SdStorage::isImageFile(name.c_str());

            bool pass = true;
            if (filterType == "audio" && !isAudio) pass = false;
            if (filterType == "image" && !isImg) pass = false;
            if (filterType == "video" && !isVid) pass = false;

            if (pass) {
              auto itemObj = files.add<JsonObject>();
              itemObj["name"] = name;
              itemObj["path"] = String(file.path());
              itemObj["size"] = file.size();
              itemObj["storage"] = "flash";
              itemObj["is_video"] = isVid;
              itemObj["is_audio"] = isAudio;
              itemObj["is_image"] = isImg;
            }
          }
          file.close();
          file = directory.openNextFile();
        }
      }
    }
    sendJson(200, doc);
  };

  webServer.on("/api/media", HTTP_GET, [handleMediaList] {
    handleMediaList(webServer.arg("type"));
  });

  auto handleMediaUploadFinish = [] {
    if (!uploadSeen || !uploadValidated || uploadError.length()) {
      webReply(uploadError.length() ? uploadErrorCode : 400,
          uploadError.length() ? uploadError.c_str() : "Tidak ada file valid dalam upload");
      resetUpload(); return;
    }
    if (!uploadToSd) {
      if (LittleFS.exists(uploadPath) || !LittleFS.rename(kUploadTempPath, uploadPath)) {
        webReply(500, "File gagal disimpan; file sebelumnya tetap tersedia");
        resetUpload(); return;
      }
    }
    webReply(201, "File tervalidasi dan berhasil tersimpan.");
    resetUpload();
  };

  webServer.on("/api/media/upload", HTTP_POST, handleMediaUploadFinish, handleMediaUpload);
  webServer.on("/api/v1/media/upload", HTTP_POST, handleMediaUploadFinish, handleMediaUpload);

  auto handleMediaDelete = [] {
    String path = webServer.arg("path");
    if (!path.length()) path = webServer.arg("id");
    if (!media::FileImage::validPath(path.c_str()) && !storage::SdStorage::isAudioFile(path.c_str())) {
      webReply(400, "Path media tidak valid"); return;
    }
    bool isSd = (strncmp(path.c_str(), "/sd", 3) == 0);
    if (isSd) {
      if (!storage::sd().isMounted()) { webReply(503, "microSD belum tersedia"); return; }
      if (!storage::sd().exists(path.c_str())) { webReply(404, "File tidak ditemukan di microSD"); return; }
      if (!storage::sd().remove(path.c_str())) { webReply(500, "File gagal dihapus dari microSD"); return; }
    } else {
      if (!filesystemReady) { webReply(503, "Storage belum tersedia"); return; }
      if (!LittleFS.exists(path)) { webReply(404, "File tidak ditemukan"); return; }
      if (path == selectedMedia && !saveDisplaySettings(layoutPreset, "")) {
        webReply(500, "Konfigurasi gagal disimpan; file belum dihapus"); return;
      }
      if (!LittleFS.remove(path)) { webReply(500, "File gagal dihapus"); return; }
    }
    if (path == selectedMedia) selectedMedia = "";
    webReply(200, "File berhasil dihapus");
  };

  webServer.on("/api/media/delete", HTTP_POST, handleMediaDelete);
  webServer.on("/api/v1/media", HTTP_DELETE, handleMediaDelete);
  webServer.on("/api/v1/media", HTTP_POST, [handleMediaDelete] {
    if (webServer.hasArg("_method") && webServer.arg("_method") == "DELETE") {
      handleMediaDelete();
    } else {
      webReply(405, "Method not allowed");
    }
  });

  // --- PRD Section 7.2 APIs ---
  // 1. GET /api/v1/storage/status
  webServer.on("/api/v1/storage/status", HTTP_GET, [] {
    JsonDocument doc;
    const auto sdSt = storage::sd().status();
    doc["sd_mounted"] = sdSt.mounted;
    doc["sd_card_type"] = sdSt.cardTypeName;
    doc["sd_total_bytes"] = static_cast<double>(sdSt.totalBytes);
    doc["sd_used_bytes"] = static_cast<double>(sdSt.usedBytes);
    doc["sd_free_bytes"] = static_cast<double>(sdSt.freeBytes);
    doc["sd_error"] = sdSt.lastError;
    doc["littlefs_ready"] = filesystemReady;
    doc["littlefs_total_bytes"] = filesystemReady ? LittleFS.totalBytes() : 0;
    doc["littlefs_used_bytes"] = filesystemReady ? LittleFS.usedBytes() : 0;
    sendJson(200, doc);
  });

  // POST /api/v1/storage/remount
  webServer.on("/api/v1/storage/remount", HTTP_POST, [] {
    bool ok = storage::sd().remount();
    JsonDocument doc;
    const auto sdSt = storage::sd().status();
    doc["success"] = ok;
    doc["sd_mounted"] = sdSt.mounted;
    doc["sd_card_type"] = sdSt.cardTypeName;
    doc["sd_total_bytes"] = static_cast<double>(sdSt.totalBytes);
    doc["sd_free_bytes"] = static_cast<double>(sdSt.freeBytes);
    doc["sd_error"] = sdSt.lastError;
    sendJson(ok ? 200 : 503, doc);
  });

  // 2. GET /api/v1/media
  webServer.on("/api/v1/media", HTTP_GET, [handleMediaList] {
    handleMediaList(webServer.arg("type"));
  });

  // 3. GET /api/v1/prayer-times
  webServer.on("/api/v1/prayer-times", HTTP_GET, [] {
    JsonDocument doc;
    const auto& cfg = prayer::prayerSchedule().config();
    const auto st = prayer::prayerSchedule().currentStatus();
    doc["source"] = cfg.source;
    doc["latitude"] = cfg.latitude;
    doc["longitude"] = cfg.longitude;
    doc["timezone"] = cfg.timezoneName;
    doc["calc_method"] = cfg.calcMethod;
    doc["imsak"] = st.imsak.toString();
    doc["syuruq"] = st.syuruq.toString();

    auto schedObj = doc["schedule"].to<JsonObject>();
    for (int i = 0; i < prayer::PRAYER_COUNT; ++i) {
      auto pObj = schedObj[prayer::prayerIdName(static_cast<prayer::PrayerId>(i))].to<JsonObject>();
      pObj["name"] = prayer::prayerLabel(static_cast<prayer::PrayerId>(i));
      pObj["base"] = st.base[i].toString();
      pObj["correction"] = st.correction[i];
      pObj["effective"] = st.effective[i].toString();
    }

    auto nextObj = doc["next_prayer"].to<JsonObject>();
    nextObj["index"] = st.nextPrayerIndex;
    nextObj["name"] = st.nextPrayerName;
    nextObj["time"] = st.nextPrayerTime.toString();
    nextObj["countdown_seconds"] = st.countdownSeconds;
    nextObj["is_today"] = st.isTodayNext;

    auto currObj = doc["current_prayer"].to<JsonObject>();
    currObj["index"] = st.currentPrayerIndex;
    currObj["name"] = st.currentPrayerName;

    const auto& pcfg = prayer::prayerSchedule().config();
    auto colorsObj = doc["colors"].to<JsonObject>();
    colorsObj["name"] = pcfg.nameColor;
    colorsObj["time"] = pcfg.timeColor;
    colorsObj["highlight"] = pcfg.highlightColor;
    colorsObj["card_bg"] = pcfg.cardBgColor;
    colorsObj["next_bg"] = pcfg.nextBgColor;

    auto marqObj = doc["marquee"].to<JsonObject>();
    marqObj["enabled"] = pcfg.marqueeEnabled;
    marqObj["text"] = pcfg.marqueeText;
    marqObj["color"] = pcfg.marqueeColor;
    marqObj["direction"] = pcfg.marqueeDirection;
    marqObj["speed"] = pcfg.marqueeSpeed;
    marqObj["hold_seconds"] = pcfg.marqueeHoldSecs;
    marqObj["font_scale"] = pcfg.marqueeFontScale;

    auto flowObj = doc["flow"].to<JsonObject>();
    flowObj["state"] = prayer::mosqueStateName(st.mosqueState);
    flowObj["state_code"] = static_cast<int>(st.mosqueState);
    flowObj["active_prayer_index"] = st.activePrayerIndex;
    flowObj["active_prayer_name"] = st.activePrayerName;
    flowObj["state_remaining_seconds"] = st.stateRemainingSeconds;
    flowObj["state_total_seconds"] = st.stateTotalSeconds;
    flowObj["is_test_mode"] = prayer::prayerSchedule().isTestMode();
    flowObj["adzan_duration_mins"] = pcfg.adzanDurationMins;
    auto iqArr = flowObj["iqomah_duration_mins"].to<JsonArray>();
    for (int i = 0; i < prayer::PRAYER_COUNT; ++i) {
      iqArr.add(pcfg.iqomahDurationMins[i]);
    }
    flowObj["shalat_duration_mins"] = pcfg.shalatDurationMins;
    flowObj["adzan_image"] = pcfg.adzanImage;
    flowObj["iqomah_image"] = pcfg.iqomahImage;
    flowObj["shalat_image"] = pcfg.shalatImage;
    flowObj["dim_screen_during_shalat"] = pcfg.dimScreenDuringShalat;

    sendJson(200, doc);
  });

  // 4. PUT /api/v1/prayer-times/settings (and POST)
  auto handlePrayerSettings = [] {
    auto cfg = prayer::prayerSchedule().config();
    if (webServer.hasArg("source")) cfg.source = webServer.arg("source");
    if (webServer.hasArg("latitude")) cfg.latitude = webServer.arg("latitude").toDouble();
    if (webServer.hasArg("longitude")) cfg.longitude = webServer.arg("longitude").toDouble();
    if (webServer.hasArg("timezone")) cfg.timezoneName = webServer.arg("timezone");
    if (webServer.hasArg("method")) cfg.calcMethod = webServer.arg("method").toInt();

    for (int i = 0; i < prayer::PRAYER_COUNT; ++i) {
      const char* key = prayer::prayerIdName(static_cast<prayer::PrayerId>(i));
      if (webServer.hasArg(key)) {
        cfg.baseTimes[i] = prayer::PrayerTimePair::fromString(webServer.arg(key));
      }
    }
    if (webServer.hasArg("name_color")) cfg.nameColor = webServer.arg("name_color");
    if (webServer.hasArg("time_color")) cfg.timeColor = webServer.arg("time_color");
    if (webServer.hasArg("highlight_color")) cfg.highlightColor = webServer.arg("highlight_color");
    if (webServer.hasArg("card_bg_color")) cfg.cardBgColor = webServer.arg("card_bg_color");
    if (webServer.hasArg("next_bg_color")) cfg.nextBgColor = webServer.arg("next_bg_color");

    if (!prayer::prayerSchedule().setConfig(cfg)) {
      webReply(500, "Gagal menyimpan konfigurasi waktu salat"); return;
    }
    webReply(200, "Pengaturan waktu salat berhasil disimpan");
  };
  webServer.on("/api/v1/prayer-times/settings", HTTP_POST, handlePrayerSettings);
  webServer.on("/api/v1/prayer-times/settings", HTTP_PUT, handlePrayerSettings);

  // 4b. POST /api/v1/prayer-times/colors
  auto handlePrayerColors = [] {
    auto cfg = prayer::prayerSchedule().config();
    if (webServer.hasArg("name_color")) cfg.nameColor = webServer.arg("name_color");
    if (webServer.hasArg("time_color")) cfg.timeColor = webServer.arg("time_color");
    if (webServer.hasArg("highlight_color")) cfg.highlightColor = webServer.arg("highlight_color");
    if (webServer.hasArg("card_bg_color")) cfg.cardBgColor = webServer.arg("card_bg_color");
    if (webServer.hasArg("next_bg_color")) cfg.nextBgColor = webServer.arg("next_bg_color");
    if (webServer.hasArg("plain")) {
      JsonDocument bodyDoc;
      if (!deserializeJson(bodyDoc, webServer.arg("plain"))) {
        if (bodyDoc["name_color"].is<String>()) cfg.nameColor = bodyDoc["name_color"].as<String>();
        if (bodyDoc["time_color"].is<String>()) cfg.timeColor = bodyDoc["time_color"].as<String>();
        if (bodyDoc["highlight_color"].is<String>()) cfg.highlightColor = bodyDoc["highlight_color"].as<String>();
        if (bodyDoc["card_bg_color"].is<String>()) cfg.cardBgColor = bodyDoc["card_bg_color"].as<String>();
        if (bodyDoc["next_bg_color"].is<String>()) cfg.nextBgColor = bodyDoc["next_bg_color"].as<String>();
      }
    }
    if (!prayer::prayerSchedule().setConfig(cfg)) {
      webReply(500, "Gagal menyimpan warna waktu salat"); return;
    }
    webReply(200, "Warna waktu salat berhasil disimpan");
  };
  webServer.on("/api/v1/prayer-times/colors", HTTP_POST, handlePrayerColors);
  webServer.on("/api/v1/prayer-times/colors", HTTP_PUT, handlePrayerColors);

  // 4c. POST /api/v1/prayer-times/marquee
  auto handlePrayerMarquee = [] {
    auto cfg = prayer::prayerSchedule().config();
    if (webServer.hasArg("enabled")) cfg.marqueeEnabled = (webServer.arg("enabled") == "1" || webServer.arg("enabled") == "true");
    if (webServer.hasArg("text")) cfg.marqueeText = webServer.arg("text");
    if (webServer.hasArg("color")) cfg.marqueeColor = webServer.arg("color");
    if (webServer.hasArg("direction")) cfg.marqueeDirection = webServer.arg("direction");
    if (webServer.hasArg("speed")) cfg.marqueeSpeed = std::max(1, std::min(3, static_cast<int>(webServer.arg("speed").toInt())));
    if (webServer.hasArg("hold_seconds")) cfg.marqueeHoldSecs = std::max(3, std::min(120, static_cast<int>(webServer.arg("hold_seconds").toInt())));
    if (webServer.hasArg("font_scale")) cfg.marqueeFontScale = std::max(1, std::min(2, static_cast<int>(webServer.arg("font_scale").toInt())));

    if (webServer.hasArg("plain")) {
      JsonDocument bodyDoc;
      if (!deserializeJson(bodyDoc, webServer.arg("plain"))) {
        if (bodyDoc["enabled"].is<bool>()) cfg.marqueeEnabled = bodyDoc["enabled"].as<bool>();
        if (bodyDoc["text"].is<String>()) cfg.marqueeText = bodyDoc["text"].as<String>();
        if (bodyDoc["color"].is<String>()) cfg.marqueeColor = bodyDoc["color"].as<String>();
        if (bodyDoc["direction"].is<String>()) cfg.marqueeDirection = bodyDoc["direction"].as<String>();
        if (bodyDoc["speed"].is<int>()) cfg.marqueeSpeed = std::max(1, std::min(3, bodyDoc["speed"].as<int>()));
        if (bodyDoc["hold_seconds"].is<int>()) cfg.marqueeHoldSecs = std::max(3, std::min(120, bodyDoc["hold_seconds"].as<int>()));
        if (bodyDoc["font_scale"].is<int>()) cfg.marqueeFontScale = std::max(1, std::min(2, bodyDoc["font_scale"].as<int>()));
      }
    }
    if (!prayer::prayerSchedule().setConfig(cfg)) {
      webReply(500, "Gagal menyimpan pengaturan marquee"); return;
    }
    webReply(200, "Pengaturan marquee berhasil disimpan");
  };
  webServer.on("/api/v1/prayer-times/marquee", HTTP_POST, handlePrayerMarquee);
  webServer.on("/api/v1/prayer-times/marquee", HTTP_PUT, handlePrayerMarquee);

  // 5. PUT /api/v1/prayer-times/corrections (and POST)
  auto handleCorrections = [] {
    int corrections[prayer::PRAYER_COUNT];
    const auto& cur = prayer::prayerSchedule().config();
    for (int i = 0; i < prayer::PRAYER_COUNT; ++i) {
      const char* key = prayer::prayerIdName(static_cast<prayer::PrayerId>(i));
      corrections[i] = webServer.hasArg(key) ? webServer.arg(key).toInt() : cur.corrections[i];
    }
    if (!prayer::prayerSchedule().setCorrections(corrections)) {
      webReply(500, "Gagal menyimpan koreksi waktu salat"); return;
    }
    webReply(200, "Koreksi waktu salat berhasil disimpan");
  };
  webServer.on("/api/v1/prayer-times/corrections", HTTP_POST, handleCorrections);
  webServer.on("/api/v1/prayer-times/corrections", HTTP_PUT, handleCorrections);

  // 5b. POST /api/v1/prayer-times/flow
  auto handlePrayerFlow = [] {
    auto cfg = prayer::prayerSchedule().config();
    if (webServer.hasArg("adzan_duration_mins")) cfg.adzanDurationMins = std::max(1, static_cast<int>(webServer.arg("adzan_duration_mins").toInt()));
    if (webServer.hasArg("shalat_duration_mins")) cfg.shalatDurationMins = std::max(1, static_cast<int>(webServer.arg("shalat_duration_mins").toInt()));
    if (webServer.hasArg("adzan_image")) cfg.adzanImage = webServer.arg("adzan_image");
    if (webServer.hasArg("iqomah_image")) cfg.iqomahImage = webServer.arg("iqomah_image");
    if (webServer.hasArg("shalat_image")) cfg.shalatImage = webServer.arg("shalat_image");
    if (webServer.hasArg("dim_screen_during_shalat")) {
      cfg.dimScreenDuringShalat = (webServer.arg("dim_screen_during_shalat") == "1" || webServer.arg("dim_screen_during_shalat") == "true");
    }
    for (int i = 0; i < prayer::PRAYER_COUNT; ++i) {
      String key = "iqomah_" + String(prayer::prayerIdName(static_cast<prayer::PrayerId>(i)));
      if (webServer.hasArg(key)) {
        cfg.iqomahDurationMins[i] = std::max(1, static_cast<int>(webServer.arg(key).toInt()));
      }
    }

    if (webServer.hasArg("plain")) {
      JsonDocument bodyDoc;
      if (!deserializeJson(bodyDoc, webServer.arg("plain"))) {
        if (bodyDoc["adzan_duration_mins"].is<int>()) cfg.adzanDurationMins = std::max(1, bodyDoc["adzan_duration_mins"].as<int>());
        if (bodyDoc["shalat_duration_mins"].is<int>()) cfg.shalatDurationMins = std::max(1, bodyDoc["shalat_duration_mins"].as<int>());
        if (bodyDoc["adzan_image"].is<String>()) cfg.adzanImage = bodyDoc["adzan_image"].as<String>();
        if (bodyDoc["iqomah_image"].is<String>()) cfg.iqomahImage = bodyDoc["iqomah_image"].as<String>();
        if (bodyDoc["shalat_image"].is<String>()) cfg.shalatImage = bodyDoc["shalat_image"].as<String>();
        if (bodyDoc["dim_screen_during_shalat"].is<bool>()) cfg.dimScreenDuringShalat = bodyDoc["dim_screen_during_shalat"].as<bool>();
        if (bodyDoc["iqomah_duration_mins"].is<JsonArray>()) {
          auto arr = bodyDoc["iqomah_duration_mins"].as<JsonArray>();
          for (size_t i = 0; i < arr.size() && i < prayer::PRAYER_COUNT; ++i) {
            cfg.iqomahDurationMins[i] = std::max(1, arr[i].as<int>());
          }
        }
      }
    }

    if (!prayer::prayerSchedule().setConfig(cfg)) {
      webReply(500, "Gagal menyimpan konfigurasi alur jam masjid"); return;
    }
    webReply(200, "Pengaturan alur Adzan, Iqomah & Shalat berhasil disimpan");
  };
  webServer.on("/api/v1/prayer-times/flow", HTTP_POST, handlePrayerFlow);
  webServer.on("/api/v1/prayer-times/flow", HTTP_PUT, handlePrayerFlow);

  // 5c. POST /api/v1/prayer-times/test-flow
  webServer.on("/api/v1/prayer-times/test-flow", HTTP_POST, [] {
    String stateStr = webServer.hasArg("state") ? webServer.arg("state") : "normal";
    String prayerStr = webServer.hasArg("prayer") ? webServer.arg("prayer") : "zuhur";
    int durationSecs = webServer.hasArg("duration") ? webServer.arg("duration").toInt() : 60;
    if (webServer.hasArg("plain")) {
      JsonDocument doc;
      if (!deserializeJson(doc, webServer.arg("plain"))) {
        if (doc["state"].is<String>()) stateStr = doc["state"].as<String>();
        if (doc["prayer"].is<String>()) prayerStr = doc["prayer"].as<String>();
        if (doc["duration"].is<int>()) durationSecs = doc["duration"].as<int>();
      }
    }
    stateStr.toLowerCase();
    prayerStr.toLowerCase();

    if (stateStr == "normal" || stateStr == "clear" || stateStr == "0") {
      prayer::prayerSchedule().clearTestMode();
      webReply(200, "Mode uji dibatalkan, kembali ke jam masjid normal");
      return;
    }

    int pIdx = prayer::PRAYER_ZUHUR;
    if (prayerStr == "subuh" || prayerStr == "0") pIdx = prayer::PRAYER_SUBUH;
    else if (prayerStr == "zuhur" || prayerStr == "dzuhur" || prayerStr == "1") pIdx = prayer::PRAYER_ZUHUR;
    else if (prayerStr == "asar" || prayerStr == "ashar" || prayerStr == "2") pIdx = prayer::PRAYER_ASAR;
    else if (prayerStr == "magrib" || prayerStr == "maghrib" || prayerStr == "3") pIdx = prayer::PRAYER_MAGRIB;
    else if (prayerStr == "isya" || prayerStr == "4") pIdx = prayer::PRAYER_ISYA;

    prayer::MosqueState mState = prayer::MOSQUE_STATE_NORMAL;
    if (stateStr == "adzan" || stateStr == "1") mState = prayer::MOSQUE_STATE_ADZAN;
    else if (stateStr == "iqomah" || stateStr == "2") mState = prayer::MOSQUE_STATE_IQOMAH;
    else if (stateStr == "shalat" || stateStr == "solat" || stateStr == "3") mState = prayer::MOSQUE_STATE_SHALAT;

    prayer::prayerSchedule().setTestMode(mState, pIdx, durationSecs);
    Serial.printf("[TEST FLOW] State: %s, Salat: %s, Durasi: %d detik\n",
                  prayer::mosqueStateName(mState), prayer::prayerLabel(static_cast<prayer::PrayerId>(pIdx)), durationSecs);
    String replyMsg = "Uji tampilan " + String(prayer::mosqueStateName(mState)) + " aktif selama " + String(durationSecs) + "s";
    webReply(200, replyMsg.c_str());
  });

  // 6. GET /api/v1/prayer-audio
  webServer.on("/api/v1/prayer-audio", HTTP_GET, [] {
    JsonDocument doc;
    const auto& cfg = prayer::audioScheduler().config();
    auto itemsObj = doc["audio_items"].to<JsonObject>();
    for (int i = 0; i < prayer::PRAYER_COUNT; ++i) {
      auto pObj = itemsObj[prayer::prayerIdName(static_cast<prayer::PrayerId>(i))].to<JsonObject>();
      pObj["name"] = prayer::prayerLabel(static_cast<prayer::PrayerId>(i));
      pObj["enabled"] = cfg.items[i].enabled;
      pObj["file"] = cfg.items[i].file;
      pObj["lead_minutes"] = cfg.items[i].leadMinutes;
      pObj["volume"] = cfg.items[i].volume;
    }

    const auto next = prayer::audioScheduler().nextEvent();
    auto nextObj = doc["next_event"].to<JsonObject>();
    nextObj["active"] = next.active;
    nextObj["prayer_name"] = next.prayerName;
    nextObj["trigger_time"] = next.triggerTime.toString();
    nextObj["countdown_seconds"] = next.countdownSeconds;
    nextObj["file"] = next.file;

    const auto aSt = audio::player().status();
    auto statusObj = doc["audio_status"].to<JsonObject>();
    statusObj["playing"] = (aSt.state == audio::AUDIO_PLAYING);
    statusObj["active_file"] = aSt.activeFile;
    statusObj["progress_seconds"] = aSt.progressSeconds;
    statusObj["duration_seconds"] = aSt.durationSeconds;
    statusObj["volume"] = aSt.volume;
    statusObj["hardware_status"] = aSt.hardwareStatus;

    sendJson(200, doc);
  });

  // 7. PUT /api/v1/prayer-audio (and POST)
  auto handlePrayerAudio = [] {
    const String prayerStr = webServer.arg("prayer");
    int pid = prayer::parsePrayerId(prayerStr);
    if (pid < 0) {
      webReply(400, "Waktu salat tidak valid (pilih subuh, zuhur, asar, magrib, isya)"); return;
    }
    bool enabled = (webServer.arg("enabled") == "1" || webServer.arg("enabled") == "true");
    String file = webServer.arg("file");
    int lead = webServer.hasArg("lead_minutes") ? webServer.arg("lead_minutes").toInt() : 10;
    int vol = webServer.hasArg("volume") ? webServer.arg("volume").toInt() : 80;

    if (!prayer::audioScheduler().setPrayerAudio(static_cast<prayer::PrayerId>(pid), enabled, file, lead, vol)) {
      webReply(500, "Gagal menyimpan konfigurasi audio salat"); return;
    }
    webReply(200, "Pengaturan audio pengingat salat berhasil disimpan");
  };
  webServer.on("/api/v1/prayer-audio", HTTP_POST, handlePrayerAudio);
  webServer.on("/api/v1/prayer-audio", HTTP_PUT, handlePrayerAudio);

  // 8. POST /api/v1/prayer-audio/test
  webServer.on("/api/v1/prayer-audio/test", HTTP_POST, [] {
    const String command = webServer.arg("command");
    if (command == "stop") {
      audio::player().stop();
      webReply(200, "Pemutaran audio dihentikan");
      return;
    }
    String file = webServer.arg("file");
    int vol = webServer.hasArg("volume") ? webServer.arg("volume").toInt() : 80;
    if (webServer.hasArg("prayer")) {
      int pid = prayer::parsePrayerId(webServer.arg("prayer"));
      if (pid >= 0) {
        file = prayer::audioScheduler().config().items[pid].file;
      }
    }
    if (!file.length()) {
      webReply(400, "Tentukan file audio atau nama salat"); return;
    }
    if (!audio::player().play(file, vol)) {
      webReply(422, "Gagal memutar audio (pastikan file WAV PCM ada)"); return;
    }
    webReply(200, "Pemutaran audio uji dimulai");
  });

  // 9. GET /api/v1/clock/status
  webServer.on("/api/v1/clock/status", HTTP_GET, [] {
    JsonDocument doc;
    const auto clk = prayer::clockService().status();
    doc["synced"] = clk.synced;
    doc["epoch"] = static_cast<uint32_t>(clk.epoch);
    doc["time"] = clk.timeString;
    doc["date"] = clk.dateString;
    doc["hijri"] = clk.hijriString;
    doc["timezone"] = clk.timezone;
    doc["timezone_offset"] = clk.timezoneOffsetMinutes;
    doc["year"] = clk.year;
    doc["month"] = clk.month;
    doc["day"] = clk.day;
    doc["hour"] = clk.hour;
    doc["minute"] = clk.minute;
    doc["second"] = clk.second;
    doc["hijri_adjustment"] = prayer::prayerSchedule().config().hijriAdjustment;
    sendJson(200, doc);
  });

  // 10. POST /api/v1/clock/sync (and /api/v1/clock, /api/clock/sync)
  auto handleClockSync = [] {
    time_t epoch = 0;
    int year = -1, month = -1, day = -1, hour = -1, minute = -1, second = 0;
    int tzOffset = 420;
    String tzName = "WIB";

    if (webServer.hasArg("plain")) {
      JsonDocument doc;
      if (!deserializeJson(doc, webServer.arg("plain"))) {
        if (doc["epoch"].is<double>()) epoch = static_cast<time_t>(doc["epoch"].as<double>());
        if (doc["year"].is<int>()) year = doc["year"].as<int>();
        if (doc["month"].is<int>()) month = doc["month"].as<int>();
        if (doc["day"].is<int>()) day = doc["day"].as<int>();
        if (doc["hour"].is<int>()) hour = doc["hour"].as<int>();
        if (doc["minute"].is<int>()) minute = doc["minute"].as<int>();
        if (doc["second"].is<int>()) second = doc["second"].as<int>();
        if (doc["timezone_offset"].is<int>()) tzOffset = doc["timezone_offset"].as<int>();
        if (doc["timezone"].is<String>()) tzName = doc["timezone"].as<String>();
      }
    }
    if (webServer.hasArg("epoch")) epoch = static_cast<time_t>(webServer.arg("epoch").toDouble());
    if (webServer.hasArg("year")) year = webServer.arg("year").toInt();
    if (webServer.hasArg("month")) month = webServer.arg("month").toInt();
    if (webServer.hasArg("day")) day = webServer.arg("day").toInt();
    if (webServer.hasArg("hour")) hour = webServer.arg("hour").toInt();
    if (webServer.hasArg("minute")) minute = webServer.arg("minute").toInt();
    if (webServer.hasArg("second")) second = webServer.arg("second").toInt();
    if (webServer.hasArg("timezone_offset")) tzOffset = webServer.arg("timezone_offset").toInt();
    if (webServer.hasArg("timezone")) tzName = webServer.arg("timezone");

    prayer::clockService().syncFromBrowser(epoch, year, month, day, hour, minute, second, tzOffset, tzName);
    prayer::prayerSchedule().update();

    JsonDocument out;
    const auto clk = prayer::clockService().status();
    out["status"] = "ok";
    out["time"] = clk.timeString;
    out["date"] = clk.dateString;
    out["hijri"] = clk.hijriString;
    out["synced"] = clk.synced;
    out["message"] = String("Jam disinkronkan ke waktu browser: ") + clk.timeString + " " + tzName;
    sendJson(200, out);
  };

  webServer.on("/api/v1/clock/sync", HTTP_POST, handleClockSync);
  webServer.on("/api/v1/clock", HTTP_POST, handleClockSync);
  webServer.on("/api/clock/sync", HTTP_POST, handleClockSync);

  // 10b. POST /api/v1/clock/manual
  webServer.on("/api/v1/clock/manual", HTTP_POST, [] {
    int year = -1, month = -1, day = -1, hour = -1, minute = -1, second = 0;
    int hijriAdj = 0;
    bool hasHijriAdj = false;

    if (webServer.hasArg("plain")) {
      JsonDocument doc;
      if (!deserializeJson(doc, webServer.arg("plain"))) {
        if (doc["year"].is<int>()) year = doc["year"].as<int>();
        if (doc["month"].is<int>()) month = doc["month"].as<int>();
        if (doc["day"].is<int>()) day = doc["day"].as<int>();
        if (doc["hour"].is<int>()) hour = doc["hour"].as<int>();
        if (doc["minute"].is<int>()) minute = doc["minute"].as<int>();
        if (doc["second"].is<int>()) second = doc["second"].as<int>();
        if (doc["hijri_adjustment"].is<int>()) {
          hijriAdj = doc["hijri_adjustment"].as<int>();
          hasHijriAdj = true;
        }
      }
    }
    if (webServer.hasArg("year")) year = webServer.arg("year").toInt();
    if (webServer.hasArg("month")) month = webServer.arg("month").toInt();
    if (webServer.hasArg("day")) day = webServer.arg("day").toInt();
    if (webServer.hasArg("hour")) hour = webServer.arg("hour").toInt();
    if (webServer.hasArg("minute")) minute = webServer.arg("minute").toInt();
    if (webServer.hasArg("second")) second = webServer.arg("second").toInt();
    if (webServer.hasArg("hijri_adjustment")) {
      hijriAdj = webServer.arg("hijri_adjustment").toInt();
      hasHijriAdj = true;
    }

    if (year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
      webReply(400, "Format jam atau tanggal manual tidak valid");
      return;
    }

    prayer::clockService().setManualTime(year, month, day, hour, minute, second);
    if (hasHijriAdj) {
      auto cfg = prayer::prayerSchedule().config();
      cfg.hijriAdjustment = hijriAdj;
      prayer::prayerSchedule().setConfig(cfg);
    }
    prayer::prayerSchedule().update();

    JsonDocument out;
    const auto clk = prayer::clockService().status();
    out["status"] = "ok";
    out["time"] = clk.timeString;
    out["date"] = clk.dateString;
    out["hijri"] = clk.hijriString;
    out["message"] = String("Jam & tanggal berhasil diatur: ") + clk.dateString + " " + clk.timeString;
    sendJson(200, out);
  });

  webServer.on("/api/frame", HTTP_GET, [] {
    if (!canvas.front()) { webReply(503, "Canvas belum tersedia"); return; }
    // Explicit little-endian RGB565 payload, prefixed by width and height.
    const uint16_t w = canvas.width(), h = canvas.height();
    uint8_t header[] = {static_cast<uint8_t>(w), static_cast<uint8_t>(w >> 8),
                        static_cast<uint8_t>(h), static_cast<uint8_t>(h >> 8)};
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.setContentLength(4 + canvas.bufferBytes());
    webServer.send(200, "application/octet-stream", "");
    webServer.sendContent(reinterpret_cast<const char*>(header), sizeof header);
    webServer.sendContent(reinterpret_cast<const char*>(canvas.front()), canvas.bufferBytes());
  });
  webServer.on("/api/control", HTTP_POST, [] {
    if (!memoryPassed || !canvas.front()) { webReply(503, "Memori/canvas belum siap"); return; }
    const String command = webServer.arg("command");
    const uint32_t before = canvas.generation();
    if (command == "pattern") { mjpegPlayer.close(); nextDiagnostic(); }
    else if (command == "bmp") renderDemoImage();
    else if (command == "jpeg") renderDemoJpeg();
    else if (command == "layout") {
      const String preset = webServer.arg("preset");
      if (preset.length() != 1 || preset[0] < '0' || preset[0] > '4') {
        webReply(400, "Preset layout tidak valid"); return;
      }
      if (!activateLayout(preset[0] - '0')) { webReply(503, "Layout gagal diterapkan"); return; }
      nextDiagnostic();
    } else if (command == "file") {
      const String path = webServer.arg("path");
      if (media::MjpegPlayer::isMjpegPath(path.c_str())) {
        if (!mjpegPlayer.open(path.c_str(), 15)) {
          webReply(422, "Video MJPEG gagal dibuka atau file rusak"); return;
        }
        animationActive = false;
        selectedMedia = path;
        saveDisplaySettings(layoutPreset, selectedMedia);
        mjpegPlayer.renderNextFrame(canvas);
        webReply(200, "Video MJPEG mulai diputar di panel"); return;
      } else {
        mjpegPlayer.close();
        if (!renderFlashImage(path.c_str())) {
          webReply(422, "Gambar gagal dibaca atau format tidak didukung"); return;
        }
      }
    } else if (command == "mjpeg_stop") {
      mjpegPlayer.close();
      webReply(200, "Pemutaran video dihentikan"); return;
    } else if (command == "brightness") {
      const int val = webServer.arg("value").toInt();
      if (val < 1 || val > 255) {
        webReply(400, "Nilai kecerahan harus antara 1 sampai 255"); return;
      }
      display::setHub75Brightness(static_cast<uint8_t>(val));
      if (!saveDisplaySettings(layoutPreset, selectedMedia)) {
        webReply(500, "Kecerahan diatur tetapi penyimpanan gagal"); return;
      }
      webReply(200, "Kecerahan berhasil diatur dan disimpan");
      return;
    } else { webReply(400, "Perintah tidak dikenal"); return; }
    if (canvas.generation() == before && command != "layout") {
      webReply(422, "Render gagal; gambar sebelumnya dipertahankan"); return;
    }
    const bool keepAnimation = animationActive;
    animationActive = false;
    if (!animation.captureImage(canvas)) {
      webReply(503, "Preview diperbarui, tetapi snapshot animasi gagal dialokasikan"); return;
    }
    animation.restart();
    animationLastTick = animationLastFrame = millis();
    animationActive = keepAnimation;
    if (animationActive && animation.render(canvas)) canvas.present();
    // Layout changes redraw the diagnostic; persist the actual current selection.
    selectedMedia = command == "file" ? webServer.arg("path") : String("");
    if (!saveDisplaySettings(layoutPreset, selectedMedia)) {
      webReply(500, "Preview diperbarui, tetapi konfigurasi gagal disimpan"); return;
    }
    webReply(200, "Preview diperbarui; layout dan pilihan media tersimpan. Frame dikirim ke HUB75 jika inisialisasi output berhasil.");
  });
  webServer.on("/api/wifi/scan", HTTP_POST, [] {
    if (wifiScanPending || WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
      webReply(200, "Pemindaian Wi-Fi sedang berlangsung...");
      return;
    }
    if (!(WiFi.getMode() & WIFI_MODE_STA)) WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    userScanPending = true;
    const int res = WiFi.scanNetworks(true, true);
    wifiScanStarted = millis();
    wifiScanPending = (res != WIFI_SCAN_FAILED);
    if (wifiScanPending) {
      webReply(200, "Pemindaian Wi-Fi dimulai...");
    } else {
      userScanPending = false;
      webReply(500, "Gagal memulai pemindaian Wi-Fi");
    }
  });
  webServer.on("/api/wifi/scan", HTTP_GET, [] {
    JsonDocument doc;
    doc["scanning"] = wifiScanPending || (WiFi.scanComplete() == WIFI_SCAN_RUNNING);
    auto list = doc["networks"].to<JsonArray>();
    for (const auto& net : discoveredNetworks) {
      auto item = list.add<JsonObject>();
      item["ssid"] = net.ssid;
      item["rssi"] = net.rssi;
      item["channel"] = net.channel;
      item["secure"] = net.secure;
    }
    sendJson(200, doc);
  });
  webServer.on("/api/wifi", HTTP_POST, [] {
    if (wifiApplyPending || wifiConnecting || wifiScanPending) {
      webReply(409, "Proses Wi-Fi sedang berjalan, coba lagi setelah selesai"); return;
    }
    const String ssid = webServer.arg("ssid"), password = webServer.arg("password");
    bool hex = password.length() == 64;
    for (unsigned i = 0; i < password.length() && hex; ++i) hex = isxdigit(password[i]);
    if (!webServer.hasArg("ssid") || !webServer.hasArg("password") || ssid.length() > 32 ||
        (password.length() && (password.length() < 8 || password.length() > 64 ||
        (password.length() == 64 && !hex)))) {
      webReply(400, "SSID maksimal 32 byte; password 8–63 karakter, 64 digit hex, atau kosong"); return;
    }
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["password"] = password;
    String saved;
    serializeJson(doc, saved);
    Preferences prefs;
    if (!prefs.begin("led-network", false)) { webReply(500, "Penyimpanan Wi-Fi gagal"); return; }
    const bool written = prefs.putString("config", saved) == saved.length();
    prefs.end();
    if (!written) { webReply(500, "Penyimpanan Wi-Fi gagal"); return; }
    staSsid = ssid;
    staPassword = password;
    wifiApplyPending = true;
    wifiApplyAt = millis();
    webReply(200, "Wi-Fi tersimpan. Koneksi akan dicoba; AP tersedia jika STA gagal.");
  });
  webServer.onNotFound([] { webReply(404, "Halaman tidak ditemukan"); });
  webServer.begin();
  Serial.println("[PASS] Web UI HTTP port 80 ready");
}
}

void setup() {
  board::holdDisplayBlank();
  pinMode(board::profile().confirmButton, INPUT_PULLUP);
  confirmButton.begin(digitalRead(board::profile().confirmButton) == LOW, millis());
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && millis() - waitStart < 2000U) delay(10);
  Serial.println("\nLED Controller / HUB75 panel preview recovery / 0.7.0");
  board::printProfile();
  Serial.printf("Chip=%s revision=%u cores=%u CPU=%u MHz reset_reason=%d\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                ESP.getCpuFreqMHz(), static_cast<int>(esp_reset_reason()));
  const auto flash = ESP.getFlashChipSize();
  // Arduino reports usable heap bytes, which exclude allocator overhead.
  const auto psram = psramFound() ? esp_spiram_get_size() : 0U;
  Serial.printf("Flash=%u bytes; PSRAM=%u bytes; SDK=%s\n",
                flash, psram, ESP.getSdkVersion());
  const bool sizesMatch = flash == board::profile().expectedFlashBytes &&
                          psram == board::profile().expectedPsramBytes;
  const bool samplePassed = psram > 0 && testPsram();
  memoryPassed = sizesMatch && samplePassed;
  Serial.printf("[%s] N16R8 memory baseline\n", memoryPassed ? "PASS" : "FAIL");
  loadPanelSettings();
  initializeCanvas();
  filesystemReady = LittleFS.begin(false);
  Serial.printf("[%s] LittleFS mount (automatic format disabled)\n", filesystemReady ? "PASS" : "FAIL");
  if (filesystemReady) LittleFS.remove(kUploadTempPath);

  // Non-blocking microSD SPI initialization
  const auto& prof = board::profile();
  bool sdMounted = storage::sd().begin(prof.sd.cs, prof.sd.sck, prof.sd.mosi, prof.sd.miso);
  Serial.printf("[%s] microSD SPI initialization (CS=%d SCK=%d MOSI=%d MISO=%d)\n",
                sdMounted ? "PASS" : "INFO: UNMOUNTED/OPTIONAL",
                prof.sd.cs, prof.sd.sck, prof.sd.mosi, prof.sd.miso);

  // Initialize I2S audio player driver
  audio::player().begin(prof.audio.bclk, prof.audio.lrc, prof.audio.dout);

  // Initialize Clock, Prayer Schedule, and Audio Scheduler
  prayer::clockService().begin();
  prayer::prayerSchedule().begin();
  prayer::audioScheduler().begin();

  restoreDisplaySettings();
  const char* imageError = nullptr;
  imageReady = demoImage.open(media::kExampleBmp, sizeof media::kExampleBmp, imageError);
  if (!imageReady) Serial.printf("[FAIL] Builtin BMP: %s\n", imageError);
  printMedia();
  WiFi.persistent(false);
  loadWifiSettings();
  WiFi.onEvent(wifiEvent);
  const bool wifiReady = WiFi.mode(staSsid.length() ? WIFI_AP_STA : WIFI_AP);
  WiFi.setSleep(false);
  Serial.printf("[%s] Wi-Fi STA initialization\n", wifiReady ? "PASS" : "FAIL");
  if (wifiReady && staSsid.length()) scanWifi();
  else startFallbackAp(wifiReady ? "STA SSID not configured" : "STA initialization failed");
  if (memoryPassed && profileReady && layoutReady) {
    display::beginHub75(selectedScan, selectedMapping);
    display::updateHub75(canvas, panelProfile.rgbOrder);
  }
  Serial.println("[BUTTON] GPIO17 INPUT_PULLUP ready: press to confirm active panel preview");
  printHelp();
  printStatus();
  startWebUi();
  xTaskCreatePinnedToCore(streamWorkerTask, "streamWorker", 16384, nullptr, 1, &streamTaskHandle, 0);
  Serial.println("[STREAM] Background stream worker task started on Core 0");
  panelPreviewStarted = millis();
  if (panelPreviewActive && !display::hub75Ready()) {
    panelRestartPending = true; panelRestartAt = millis();
    Serial.println("[PANEL] Preview output failed; rolling back");
  }
}

void loop() {
  updateConfirmButton();
  webServer.handleClient();
  if (panelPreviewActive && !panelRestartPending && millis() - panelPreviewStarted >= kPanelPreviewMs) {
    panelRestartPending = true; panelRestartAt = millis();
    Serial.println("[PANEL] Preview expired; reverting to saved configuration");
  }
  if (panelRestartPending && millis() - panelRestartAt >= 1500U) {
    Serial.println("[SYSTEM] Intentional restart for panel configuration");
    Serial.flush(); ESP.restart();
  }
  // Malformed/disconnected multipart requests may never reach the completion handler.
  if (uploadSeen && !webServer.client().connected()) resetUpload();
  if (wifiApplyPending && millis() - wifiApplyAt >= 1000U) applyWifiSettings();
  if (Serial.available()) {
    const char command = static_cast<char>(Serial.read());
    const bool changesCanvas = command == 'b' || command == 'i' || command == 'o' ||
        command == 'v' || command == 'n' || command == 'd';
    if (changesCanvas) animationActive = false;
    switch (command) {
      case 'z': display::pauseHub75ForWifiTest(); break;
      case 'q': display::nextHub75Mapping(); break;
      case 'x': display::toggleHub75Blank(); break;
      case '+': {
        uint8_t cur = display::hub75Brightness();
        if (cur <= 245) cur += 10; else cur = 255;
        display::setHub75Brightness(cur);
        saveDisplaySettings(layoutPreset, selectedMedia);
        Serial.printf("HUB75 brightness=%u/255\n", cur);
        break;
      }
      case '-': {
        uint8_t cur = display::hub75Brightness();
        if (cur >= 11) cur -= 10; else cur = 1;
        display::setHub75Brightness(cur);
        saveDisplaySettings(layoutPreset, selectedMedia);
        Serial.printf("HUB75 brightness=%u/255\n", cur);
        break;
      }
      case 's': printStatus(); printCanvas(); printLayout(); break;
      case 'b': renderDemoImage(); break;
      case 'i': renderDemoJpeg(); break;
      case 'm': printMedia(); break;
      case 'f': printStorage(); break;
      case 'u': {
        Serial.println("\n[SD DIAGNOSTIC] Memeriksa status koneksi microSD...");
        const auto& prof = board::profile();
        Serial.printf("  Pin: CS=%d, SCK=%d, MOSI=%d, MISO=%d\n",
                      prof.sd.cs, prof.sd.sck, prof.sd.mosi, prof.sd.miso);

        Serial.println("  Pemeriksaan kondisi elektrik pin:");
        auto checkPin = [](int pin, const char* name) {
          pinMode(pin, INPUT_PULLUP);
          delayMicroseconds(500);
          int pu = digitalRead(pin);
          pinMode(pin, INPUT_PULLDOWN);
          delayMicroseconds(500);
          int pd = digitalRead(pin);
          pinMode(pin, INPUT_PULLUP);
          const char* desc = "FLOATING / High-Z";
          if (pu == 0 && pd == 0) desc = "HARD-LOW (Tersambung GND/0V)";
          else if (pu == 1 && pd == 1) desc = "HARD-HIGH (Tersambung VCC/3.3V)";
          else if (pu == 1 && pd == 0) desc = "NORMAL (Tinggi saat idle)";
          Serial.printf("    - %-4s (GPIO %2d): %s [PU=%d, PD=%d]\n", name, pin, desc, pu, pd);
        };
        checkPin(prof.sd.cs, "CS");
        checkPin(prof.sd.sck, "SCK");
        checkPin(prof.sd.mosi, "MOSI");
        checkPin(prof.sd.miso, "MISO");

        // Drive test MOSI as push-pull output
        pinMode(prof.sd.mosi, OUTPUT);
        digitalWrite(prof.sd.mosi, HIGH);
        delay(2);
        int mosiDriveHigh = digitalRead(prof.sd.mosi);
        digitalWrite(prof.sd.mosi, LOW);
        delay(2);
        int mosiDriveLow = digitalRead(prof.sd.mosi);
        Serial.printf("  Uji Daya Dorong MOSI (GPIO %d): Saat didorong HIGH=%d, Saat didorong LOW=%d\n",
                      prof.sd.mosi, mosiDriveHigh, mosiDriveLow);
        if (mosiDriveHigh == 0) {
          Serial.println("  [PERINGATAN] Jalur MOSI korslet langsung ke Ground (0V)! Tidak bisa mengirim data.");
        } else {
          Serial.println("  [OK] Jalur MOSI normal, berhasil didorong ke 3.3V.");
        }

        // Raw SPI CMD0 probe (test pin orientations)
        for (int pAttempt = 0; pAttempt < 2; ++pAttempt) {
          int tMosi = (pAttempt == 0) ? prof.sd.mosi : prof.sd.miso;
          int tMiso = (pAttempt == 0) ? prof.sd.miso : prof.sd.mosi;

          SPIClass testSpi(FSPI);
          testSpi.begin(prof.sd.sck, tMiso, tMosi, prof.sd.cs);
          testSpi.setFrequency(400000);

          pinMode(prof.sd.cs, OUTPUT);
          digitalWrite(prof.sd.cs, HIGH);
          for (int i = 0; i < 16; ++i) testSpi.transfer(0xFF);

          digitalWrite(prof.sd.cs, LOW);
          delayMicroseconds(50);

          testSpi.transfer(0x40);
          testSpi.transfer(0x00);
          testSpi.transfer(0x00);
          testSpi.transfer(0x00);
          testSpi.transfer(0x00);
          testSpi.transfer(0x95);

          uint8_t r1 = 0xFF;
          String hexLog = "";
          for (int i = 0; i < 16; ++i) {
            uint8_t b = testSpi.transfer(0xFF);
            char buf[8];
            snprintf(buf, sizeof(buf), "%02X ", b);
            hexLog += buf;
            if (b != 0xFF && r1 == 0xFF) r1 = b;
          }
          digitalWrite(prof.sd.cs, HIGH);
          testSpi.transfer(0xFF);
          testSpi.end();

          Serial.printf("  [PROBE SPI %s] MOSI=%d, MISO=%d -> Bytes: [ %s] (Respon: 0x%02X)\n",
                        pAttempt == 0 ? "Normal" : "Swapped", tMosi, tMiso, hexLog.c_str(), r1);
        }

        bool ok = storage::sd().remount();
        const auto st = storage::sd().status();
        Serial.printf("  Hasil Mount: %s\n", ok ? "BERHASIL (PASS)" : "GAGAL (FAIL)");
        Serial.printf("  Pesan Driver: %s\n", st.lastError.c_str());
        if (ok) {
          Serial.printf("  Tipe Kartu: %s\n", st.cardTypeName.c_str());
          Serial.printf("  Kapasitas Total: %llu MB\n", st.totalBytes / (1024ULL * 1024ULL));
          Serial.printf("  Kapasitas Bebas: %llu MB\n", st.freeBytes / (1024ULL * 1024ULL));
          std::vector<storage::FileItem> items;
          storage::sd().listFiles("/", items);
          Serial.printf("  File di root (/): %u file ditemukan:\n", (unsigned)items.size());
          for (const auto& f : items) {
            Serial.printf("    - %s [%llu KB] %s\n", f.name.c_str(), (unsigned long long)(f.size / 1024), f.isAudio ? "[AUDIO]" : "");
          }
          std::vector<storage::FileItem> audioItems;
          storage::sd().listFiles("/audio", audioItems);
          if (!audioItems.empty()) {
            Serial.printf("  File di /audio: %u file ditemukan:\n", (unsigned)audioItems.size());
            for (const auto& f : audioItems) {
              Serial.printf("    - %s [%llu KB] %s\n", f.name.c_str(), (unsigned long long)(f.size / 1024), f.isAudio ? "[AUDIO]" : "");
            }
          }
        }
        break;
      }
      case 'o': renderFlashImage("/media/test_bars.bmp"); break;
      case 'v': renderFlashImage("/media/test_bars.jpg"); break;
      case 'l': printLayout(); break;
      case 'n': if (activateLayout(0)) nextDiagnostic(); break;
      case 'k': {
        char json[2049];
        const size_t required = display::exportLayout(panelLayout, panelProfile, json, sizeof json);
        if (layoutReady && required && required < sizeof json) Serial.println(json);
        else Serial.println("[FAIL] Layout export");
        break;
      }
      case 'p': printProfile(); break;
      case 'j': {
        char json[2049];
        const size_t required = display::exportProfile(panelProfile, json, sizeof json);
        if (required && required < sizeof json) Serial.println(json);
        else Serial.println("[FAIL] Profile export");
        break;
      }
      case 'd': nextDiagnostic(); break;
      case 'w':
        if (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
          Serial.println("Wi-Fi scan already running");
        else { WiFi.scanDelete(); scanWifi(); }
        break;
      case '1': {
        Serial.println("[TEST] Mengaktifkan Tampilan Adzan (60 detik)...");
        prayer::prayerSchedule().setTestMode(prayer::MOSQUE_STATE_ADZAN, prayer::PRAYER_ZUHUR, 60);
        break;
      }
      case '2': {
        Serial.println("[TEST] Mengaktifkan Tampilan Iqomah (60 detik)...");
        prayer::prayerSchedule().setTestMode(prayer::MOSQUE_STATE_IQOMAH, prayer::PRAYER_ZUHUR, 60);
        break;
      }
      case '3': {
        Serial.println("[TEST] Mengaktifkan Tampilan Shalat (60 detik)...");
        prayer::prayerSchedule().setTestMode(prayer::MOSQUE_STATE_SHALAT, prayer::PRAYER_ZUHUR, 60);
        break;
      }
      case '0': {
        Serial.println("[TEST] Menghentikan Uji Coba, kembali ke Normal.");
        prayer::prayerSchedule().clearTestMode();
        break;
      }
      case 'h': printHelp(); break;
      default: break;
    }
    if (changesCanvas && canvas.front()) animation.captureImage(canvas);
  }
  prayer::clockService().update();
  prayer::prayerSchedule().update();
  prayer::audioScheduler().update();

  const uint32_t now = millis();
  const bool streamHasRecentFrame = (streamLastFrameAt.load() > 0 && now - streamLastFrameAt.load() < 3000U);
  const bool streamIsActive = streamRunning.load() && streamHasRecentFrame;
  
  const auto mosqueStatus = prayer::prayerSchedule().currentStatus();
  const bool isMosquePriorityEvent = (mosqueStatus.mosqueState != prayer::MOSQUE_STATE_NORMAL);

  bool shouldRenderAnimation = false;
  bool shouldRenderMjpeg = false;
  bool shouldRenderMosque = false;

  if (displayMode == MODE_STREAM) {
    shouldRenderAnimation = false;
    shouldRenderMjpeg = false;
    shouldRenderMosque = false;
  } else if (displayMode == MODE_ANIMATION) {
    shouldRenderAnimation = true;
    shouldRenderMjpeg = false;
    shouldRenderMosque = false;
  } else if (displayMode == MODE_IMAGE) {
    if (mjpegPlayer.isPlaying()) shouldRenderMjpeg = true;
    shouldRenderAnimation = false;
    shouldRenderMosque = false;
  } else if (displayMode == MODE_MOSQUE) {
    shouldRenderMosque = true;
    shouldRenderAnimation = false;
    shouldRenderMjpeg = false;
  } else { // MODE_AUTO
    if (isMosquePriorityEvent) {
      // Prioritas Utama: Saat waktu Adzan, Iqomah, atau Shalat tiba
      shouldRenderMosque = true;
      shouldRenderAnimation = false;
      shouldRenderMjpeg = false;
    } else if (streamIsActive) {
      shouldRenderAnimation = false;
      shouldRenderMjpeg = false;
      shouldRenderMosque = false;
    } else if (mjpegPlayer.isPlaying()) {
      shouldRenderMjpeg = true;
      shouldRenderAnimation = false;
      shouldRenderMosque = false;
    } else if (animationActive) {
      shouldRenderAnimation = true;
      shouldRenderMosque = false;
    } else {
      // Standby / Idle Default: Jam Masjid & Jadwal Shalat
      shouldRenderMosque = true;
    }
  }

  static uint32_t lastMosqueRender = 0;
  if (shouldRenderMosque) {
    const uint32_t interval = prayer::prayerRenderer().refreshInterval();
    if (now - lastMosqueRender >= interval) {
      lastMosqueRender += interval;
      if (now - lastMosqueRender > interval) {
        lastMosqueRender = now;
      }
      prayer::prayerRenderer().render(canvas);
    }
  } else if (shouldRenderMjpeg && mjpegPlayer.isPlaying()) {
    const uint32_t targetMs = 1000 / (mjpegPlayer.fps() ? mjpegPlayer.fps() : 15);
    if (now - mjpegLastFrame >= targetMs) {
      mjpegLastFrame = now;
      mjpegPlayer.renderNextFrame(canvas);
    }
  } else if (shouldRenderAnimation && animationActive) {
    const bool wasMoving = animation.needsFrames(canvas.width(), canvas.height());
    animation.advance(now - animationLastTick);
    const bool moving = animation.needsFrames(canvas.width(), canvas.height());
    const bool finishedNow = wasMoving && !moving;
    if ((moving || finishedNow) && !animation.paused() &&
        (finishedNow || now - animationLastFrame >= kAnimationFrameMs)) {
      animationLastFrame = now;
      if (animation.render(canvas)) canvas.present();
    }
  }
  animationLastTick = now;
  updateWifi();
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus >= 10000U) {
    lastStatus = millis();
    printStatus();
  }
  display::updateHub75(canvas, panelProfile.rgbOrder);
  delay(1);
}
