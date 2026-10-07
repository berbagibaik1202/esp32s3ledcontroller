#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "board/board_profile.h"
#include "display/builtin_profiles.h"
#include "display/diagnostics.h"
#include "display/panel_profile.h"
#include "display/panel_layout.h"
#include "media/bmp.h"
#include "media/builtin_image.h"
#include "media/image_renderer.h"
#include "media/jpeg.h"
#include "media/builtin_jpeg.h"
#include "media/file_image.h"

namespace {
constexpr size_t kPsramTestBytes = 64U * 1024U;
bool memoryPassed = false;
bool wifiScanPending = false;
display::PanelProfile panelProfile;
bool profileReady = false;
display::PanelLayout panelLayout;
display::LayoutMapper layoutMapper;
bool layoutReady = false;
unsigned layoutPreset = 0;
void* allocateCanvas(size_t bytes) {
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
display::Framebuffer canvas(allocateCanvas, heap_caps_free);
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
void renderFlashImage(const char* path) {
  if (!filesystemReady || !memoryPassed) { Serial.println("[FAIL] Storage/PSRAM unavailable"); return; }
  auto file = LittleFS.open(path, "r");
  if (!file || file.isDirectory()) { Serial.println("[FAIL] Media file unavailable"); return; }
  FlashReader reader(file);
  const char* error = nullptr;
  if (!flashImage.load(path, reader, error)) { Serial.printf("[FAIL] File image: %s\n", error); return; }
  file.close();
  media::ImageRenderOptions options;
  options.width = canvas.width(); options.height = canvas.height();
  if (!media::renderImage(flashImage.source(), canvas, options, error)) {
    Serial.printf("[FAIL] File render: %s\n", error); return;
  }
  canvas.present();
  Serial.printf("[INFO] Rendered %s from LittleFS; HUB75 remains blank\n", path);
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
  Serial.printf("[INFO] Rendered builtin JPEG with %s; HUB75 remains blank\n", media::scaleModeName(options.mode));
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
  Serial.printf("[INFO] Rendered builtin BMP with %s to canvas; HUB75 remains blank\n",
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
  Serial.printf("Canvas=%ux%u double_buffer_bytes=%u generation=%lu (CPU only)\n",
                canvas.width(), canvas.height(), static_cast<unsigned>(canvas.bufferBytes() * 2),
                static_cast<unsigned long>(canvas.generation()));
}

void printLayout() {
  if (!layoutReady) { Serial.println("Layout unavailable"); return; }
  Serial.printf("Layout=%ux%u panels=%u chain_pixels=%u preset=%u (CPU only)\n",
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
  if (preset >= 5) return false;
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
  Serial.printf("[INFO] Rendered %s, step=%lu in memory; HUB75 remains blank\n",
                display::patternName(pattern), static_cast<unsigned long>(diagnosticStep));
  printCanvas();
  diagnosticIndex = (diagnosticIndex + 1) % 9;
  if (diagnosticIndex == 0) ++diagnosticStep;
}

void initializeCanvas() {
  const char* error = nullptr;
  profileReady = display::parseProfile(display::kDevelopmentProfileJson,
      sizeof(display::kDevelopmentProfileJson) - 1, panelProfile, error);
  if (!profileReady) { Serial.printf("[FAIL] Profile: %s\n", error); return; }
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
  Serial.println("Diagnostics render to memory only. HUB75 remains blank.");
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
  Serial.printf("uptime_ms=%lu memory=%s internal_free=%u internal_min=%u "
                "psram_free=%u psram_largest=%u\n",
                millis(), memoryPassed ? "PASS" : "FAIL",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
}

void scanWifi() {
  Serial.println("Wi-Fi scan started (asynchronous, no credentials required)");
  const int result = WiFi.scanNetworks(true);
  wifiScanPending = result != WIFI_SCAN_FAILED;
  if (!wifiScanPending)
    Serial.println("[FAIL] Could not start Wi-Fi scan");
}
}

void setup() {
  board::holdDisplayBlank();
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && millis() - waitStart < 2000U) delay(10);
  Serial.println("\nLED Controller / LittleFS media preparation / 0.6.0");
  board::printProfile();
  Serial.printf("Chip=%s revision=%u cores=%u CPU=%u MHz reset_reason=%d\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                ESP.getCpuFreqMHz(), static_cast<int>(esp_reset_reason()));
  const auto flash = ESP.getFlashChipSize();
  const auto psram = ESP.getPsramSize();
  Serial.printf("Flash=%u bytes; PSRAM=%u bytes; SDK=%s\n",
                flash, psram, ESP.getSdkVersion());
  const bool sizesMatch = flash == board::profile().expectedFlashBytes &&
                          psram == board::profile().expectedPsramBytes;
  const bool samplePassed = psram > 0 && testPsram();
  memoryPassed = sizesMatch && samplePassed;
  Serial.printf("[%s] N16R8 memory baseline\n", memoryPassed ? "PASS" : "FAIL");
  initializeCanvas();
  filesystemReady = LittleFS.begin(false);
  Serial.printf("[%s] LittleFS mount (automatic format disabled)\n", filesystemReady ? "PASS" : "FAIL");
  const char* imageError = nullptr;
  imageReady = demoImage.open(media::kExampleBmp, sizeof media::kExampleBmp, imageError);
  if (!imageReady) Serial.printf("[FAIL] Builtin BMP: %s\n", imageError);
  printMedia();
  const bool wifiReady = WiFi.mode(WIFI_STA);
  Serial.printf("[%s] Wi-Fi STA initialization\n", wifiReady ? "PASS" : "FAIL");
  if (wifiReady) scanWifi();
  printHelp();
  printStatus();
}

void loop() {
  if (Serial.available()) {
    switch (Serial.read()) {
      case 's': printStatus(); printCanvas(); printLayout(); break;
      case 'b': renderDemoImage(); break;
      case 'i': renderDemoJpeg(); break;
      case 'm': printMedia(); break;
      case 'f': printStorage(); break;
      case 'o': renderFlashImage("/media/test_bars.bmp"); break;
      case 'v': renderFlashImage("/media/test_bars.jpg"); break;
      case 'l': printLayout(); break;
      case 'n': if (activateLayout((layoutPreset + 1) % 5)) nextDiagnostic(); break;
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
  }
  const int networks = WiFi.scanComplete();
  if (wifiScanPending && networks >= 0) {
    Serial.printf("[INFO] Wi-Fi scan complete: %d networks; zero is valid without nearby APs\n", networks);
    WiFi.scanDelete();
    wifiScanPending = false;
  } else if (wifiScanPending && networks == WIFI_SCAN_FAILED) {
    Serial.println("[FAIL] Wi-Fi scan failed; press w to retry");
    wifiScanPending = false;
  }
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus >= 10000U) {
    lastStatus = millis();
    printStatus();
  }
  delay(10);
}
