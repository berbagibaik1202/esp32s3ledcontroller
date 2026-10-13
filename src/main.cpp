#include <Arduino.h>
#include <atomic>
#include <WiFi.h>
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
#include "media/animation.h"
#include "wifi_config.h"
#include "web_ui.h"

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
String selectedMedia;
constexpr size_t kMaxMediaBytes = 1024U * 1024U;
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
  if (!filesystemReady || !memoryPassed) { Serial.println("[FAIL] Storage/PSRAM unavailable"); return false; }
  auto file = LittleFS.open(path, "r");
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
  Serial.printf("[INFO] Rendered %s from LittleFS; HUB75 transfer pending\n", path);
  return true;
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
  Serial.println("f=LittleFS info/list, o=flash test_bars.bmp, v=flash test_bars.jpg");
  Serial.println("HUB75 output enabled after successful DMA initialization; q=next mapping, x=blank/resume (1/255).");
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
  Serial.printf("HUB75=%s brightness=%u/255 mapping=%u\n", display::hub75Ready() ? "READY" : "OFF", display::hub75Blanked() ? 0 : display::kHub75Brightness, display::hub75Mapping());
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
    bool targetFound = false;
    bool hiddenFound = false;
    for (int i = 0; i < networks; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid == staSsid && ssid.length()) targetFound = true;
      if (!ssid.length()) hiddenFound = true;
    }
    Serial.printf("[INFO] Wi-Fi scan complete: %d networks; target=%s\n", networks,
        targetFound ? "FOUND" : "NOT FOUND");
    WiFi.scanDelete();
    wifiScanPending = false;
    if (WiFi.status() != WL_CONNECTED) {
      if (staSsid.length() && (targetFound || hiddenFound)) {
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
    WiFi.scanDelete();
    if (WiFi.status() != WL_CONNECTED) startFallbackAp("scan failed or timed out");
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

void failUpload(int code, const char* message) {
  if (!uploadError.length()) { uploadErrorCode = code; uploadError = message; }
  uploadValidated = false;
  uploadFile.close();
  if (filesystemReady) LittleFS.remove(kUploadTempPath);
}

void resetUpload() {
  uploadFile.close();
  if (filesystemReady) LittleFS.remove(kUploadTempPath);
  uploadPath = "";
  uploadError = "";
  uploadSeen = false;
  uploadValidated = false;
  uploadBytes = 0;
}

void handleMediaUpload() {
  auto& upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (uploadSeen) { failUpload(400, "Upload satu file per permintaan"); return; }
    resetUpload();
    uploadSeen = true;
    uploadPath = String("/media/") + upload.filename;
    if (!filesystemReady || !memoryPassed) { failUpload(503, "Storage/PSRAM belum siap"); return; }
    if (upload.name != "file" || uploadPath.length() != strlen(uploadPath.c_str()) ||
        !media::FileImage::validPath(uploadPath.c_str())) {
      failUpload(400, "Nama file hanya huruf, angka, _, -, dan titik; ekstensi .bmp/.jpg/.jpeg"); return;
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
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadError.length()) return;
    if (!uploadSeen || !uploadFile) { failUpload(400, "Upload belum dimulai"); return; }
    if (upload.currentSize > kMaxMediaBytes - uploadBytes) {
      failUpload(413, "File maksimal 1 MiB"); return;
    }
    if (LittleFS.totalBytes() - LittleFS.usedBytes() < upload.currentSize + kStorageReserveBytes) {
      failUpload(507, "Storage penuh; file tidak disimpan"); return;
    }
    if (uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
      failUpload(507, "Penulisan gagal atau storage penuh"); return;
    }
    uploadBytes += upload.currentSize;
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadError.length()) return;
    uploadFile.flush();
    uploadFile.close();
    auto candidateFile = LittleFS.open(kUploadTempPath, "r");
    if (!candidateFile || !uploadBytes || candidateFile.size() != uploadBytes || upload.totalSize != uploadBytes) {
      candidateFile.close(); failUpload(422, "File kosong atau upload tidak lengkap"); return;
    }
    FlashReader reader(candidateFile);
    media::FileImage candidate(allocateCanvas, heap_caps_free);
    const char* error = nullptr;
    const bool valid = candidate.load(uploadPath.c_str(), reader, error);
    candidateFile.close();
    if (!valid) { failUpload(422, error ? error : "Gambar tidak valid"); return; }
    uploadValidated = true;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    resetUpload();
  }
}

bool saveDisplaySettings(unsigned preset, const String& path) {
  JsonDocument doc;
  doc["layout"] = preset;
  doc["media"] = path;
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
  const String path = doc["media"].as<String>();
  if (path.length() && !media::FileImage::validPath(path.c_str())) return;
  if (!activateLayout(0)) return;
  nextDiagnostic();
  if (path.length() && renderFlashImage(path.c_str())) selectedMedia = path;
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
      webReply(200, "Scan tersimpan permanen. Controller restart untuk menerapkan; brightness tetap 1/255.");
    } else {
      webReply(200, "Preview siap. Controller restart; sambungkan kembali lalu konfirmasi dalam 30 detik. Brightness 1/255.");
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
    doc["brightness"] = display::kHub75Brightness;
    doc["hub75_mapping"] = display::hub75Mapping();
    doc["scan"] = display::hub75Scan();
    doc["output_fps"] = display::hub75Fps();
    doc["transfer_us"] = display::hub75TransferMicros();
    doc["selected_media"] = selectedMedia;
    doc["animation_active"] = animationActive;
    doc["animation_paused"] = animation.paused();
    doc["animation_moving"] = animationActive && animation.needsFrames(canvas.width(), canvas.height());
    doc["elapsed_ms"] = animation.elapsedMs();
    sendJson(200, doc);
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
  webServer.on("/api/media", HTTP_GET, [] {
    JsonDocument doc;
    doc["ready"] = filesystemReady;
    doc["used"] = filesystemReady ? LittleFS.usedBytes() : 0;
    doc["total"] = filesystemReady ? LittleFS.totalBytes() : 0;
    doc["max_file_bytes"] = kMaxMediaBytes;
    doc["max_files"] = kMaxMediaFiles;
    auto files = doc["files"].to<JsonArray>();
    if (filesystemReady) {
      auto directory = LittleFS.open("/media");
      if (directory && directory.isDirectory()) {
        auto file = directory.openNextFile();
        while (file) {
          const String name = file.name();
          if (!file.isDirectory() && (name.endsWith(".bmp") || name.endsWith(".jpg") || name.endsWith(".jpeg"))) {
            auto item = files.add<JsonObject>();
            item["name"] = name;
            item["path"] = String(file.path());
            item["size"] = file.size();
          }
          file.close();
          file = directory.openNextFile();
        }
      }
    }
    sendJson(200, doc);
  });
  webServer.on("/api/media/upload", HTTP_POST, [] {
    if (!uploadSeen || !uploadValidated || uploadError.length()) {
      webReply(uploadError.length() ? uploadErrorCode : 400,
          uploadError.length() ? uploadError.c_str() : "Tidak ada gambar valid dalam upload");
      resetUpload(); return;
    }
    if (LittleFS.exists(uploadPath) || !LittleFS.rename(kUploadTempPath, uploadPath)) {
      webReply(500, "File gagal disimpan; file sebelumnya tetap tersedia");
      resetUpload(); return;
    }
    webReply(201, "Gambar tervalidasi dan tersimpan. Tekan Preview untuk menampilkannya.");
    resetUpload();
  }, handleMediaUpload);
  webServer.on("/api/media/delete", HTTP_POST, [] {
    const String path = webServer.arg("path");
    if (!media::FileImage::validPath(path.c_str()) || path.length() != strlen(path.c_str())) {
      webReply(400, "Path media tidak valid"); return;
    }
    if (!filesystemReady) { webReply(503, "Storage belum tersedia"); return; }
    if (!LittleFS.exists(path)) { webReply(404, "File tidak ditemukan"); return; }
    if (path == selectedMedia && !saveDisplaySettings(layoutPreset, "")) {
      webReply(500, "Konfigurasi gagal disimpan; file belum dihapus"); return;
    }
    if (!LittleFS.remove(path)) { webReply(500, "File gagal dihapus"); return; }
    if (path == selectedMedia) selectedMedia = "";
    webReply(200, "File dihapus. Preview yang sedang tampil tetap di memori.");
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
    if (command == "pattern") nextDiagnostic();
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
      if (!renderFlashImage(webServer.arg("path").c_str())) {
        webReply(422, "Gambar gagal dibaca atau format tidak didukung"); return;
      }
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
      case 's': printStatus(); printCanvas(); printLayout(); break;
      case 'b': renderDemoImage(); break;
      case 'i': renderDemoJpeg(); break;
      case 'm': printMedia(); break;
      case 'f': printStorage(); break;
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
      case 'h': printHelp(); break;
      default: break;
    }
    if (changesCanvas && canvas.front()) animation.captureImage(canvas);
  }
  const uint32_t now = millis();
  if (animationActive) {
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
