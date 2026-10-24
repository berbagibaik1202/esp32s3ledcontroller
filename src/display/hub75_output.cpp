#include "hub75_output.h"
#include "hub75_mapping.h"
#include "../board/board_profile.h"
#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <new>

namespace display {
namespace {
class PanelOutput : public MatrixPanel_I2S_DMA {
 public:
  explicit PanelOutput(const HUB75_I2S_CFG& config) : MatrixPanel_I2S_DMA(config) {}
  void resumeDma() { dma_bus.dma_transfer_start(); }
};
PanelOutput* output = nullptr;
bool wifiTestActive = false;
uint32_t wifiTestStarted = 0;
bool ready = false, blanked = false, dirty = true;
unsigned mapping = 4;
uint32_t lastGeneration = 0;
uint32_t fpsWindowStarted = 0, windowFrames = 0, transferMicros = 0;
float measuredFps = 0;
uint8_t activeScan = 32;
uint8_t activeBrightness = kDefaultBrightness;
}
bool hub75Ready() { return ready; }
float hub75Fps() { return measuredFps; }
uint32_t hub75TransferMicros() { return transferMicros; }
bool hub75Blanked() { return blanked; }
unsigned hub75Mapping() { return mapping; }
uint8_t hub75Scan() { return activeScan; }
uint8_t hub75Brightness() { return activeBrightness; }
bool beginHub75(uint8_t scan, unsigned selectedMapping) {
  if (!validHub75Settings(scan, selectedMapping)) return false;
  if (ready) return true;
  activeScan = scan; mapping = selectedMapping;
  const auto& p = board::profile().hub75;
  HUB75_I2S_CFG config(128 * (32 / scan), 2 * scan, 1);
  config.gpio = {static_cast<int8_t>(p.r1), static_cast<int8_t>(p.g1),
      static_cast<int8_t>(p.b1), static_cast<int8_t>(p.r2), static_cast<int8_t>(p.g2),
      static_cast<int8_t>(p.b2), static_cast<int8_t>(p.a), static_cast<int8_t>(p.b),
      static_cast<int8_t>(scan >= 8 ? p.c : -1), static_cast<int8_t>(scan >= 16 ? p.d : -1), static_cast<int8_t>(scan == 32 ? p.e : -1),
      static_cast<int8_t>(p.lat), static_cast<int8_t>(p.oe), static_cast<int8_t>(p.clk)};
  config.driver = HUB75_I2S_CFG::SHIFTREG;
  // Pin the S3 LCD divider to 40 in platformio.ini: 160 MHz / 40 = 4 MHz.
  // Library's HZ_8M selector otherwise uses a hard-coded divider of 16 (10 MHz).
  config.i2sspeed = static_cast<HUB75_I2S_CFG::clk_speed>(4000000);
  config.min_refresh_rate = 100;
  config.clkphase = false;
  config.latch_blanking = 2;
  config.double_buff = true;
  config.setPixelColorDepthBits(6);
  output = new (std::nothrow) PanelOutput(config);
  // begin() initializes black buffers. Set minimum brightness immediately
  // afterwards, before copying any non-black pixels into either DMA buffer.
  if (!output || !output->begin()) {
    delete output; output = nullptr;
    board::holdDisplayBlank();
    Serial.println("[FAIL] HUB75 DMA initialization; output held blank");
    return false;
  }
  output->setBrightness8(blanked ? 0 : activeBrightness);
  output->clearScreen();
  output->flipDMABuffer();
  output->clearScreen();
  ready = true; dirty = true;
  Serial.printf("[PASS] HUB75 DMA: 128x64 scan 1/%u clock=4MHz brightness=%u/255 mapping=%u\n", activeScan, activeBrightness, mapping);
  return true;
}
bool hub75WifiTestActive() { return wifiTestActive; }
void pauseHub75ForWifiTest() {
  if (!ready || !output || wifiTestActive) return;
  output->setBrightness8(0);
  delay(10); // Let DMA send OE blank before stopping the peripheral.
  output->stopDMAoutput();
  wifiTestStarted = millis(); wifiTestActive = true;
  Serial.println("[RF TEST] HUB75 DMA stopped for 30 seconds; Wi-Fi unchanged");
}
void updateHub75(const Framebuffer& canvas, RgbOrder order) {
  const uint32_t now = millis();
  if (!fpsWindowStarted) fpsWindowStarted = now;
  if (now - fpsWindowStarted >= 2000U) {
    measuredFps = windowFrames * 1000.0f / (now - fpsWindowStarted);
    windowFrames = 0; fpsWindowStarted = now;
  }
  if (wifiTestActive) {
    if (millis() - wifiTestStarted < 30000U) return;
    output->resumeDma();
    output->setBrightness8(blanked ? 0 : activeBrightness);
    wifiTestActive = false; dirty = true;
    Serial.println("[RF TEST] HUB75 DMA resumed; brightness restored");
  }
  if (!ready || !canvas.front() || canvas.width() != 128 || canvas.height() != 64) return;
  if (!dirty && lastGeneration == canvas.generation()) return;
  const uint32_t transferStarted = micros();
  const uint16_t* front = canvas.front();
  if (activeScan == 32) {
    for (int y = 0; y < 64; ++y) {
      const uint16_t* row = &front[y * 128];
      for (int x = 0; x < 128; ++x) {
        uint16_t color = row[x];
        if (order != RgbOrder::RGB) color = mapRgb565(color, order);
        const uint8_t r = (color >> 11) & 0x1F;
        const uint8_t g = (color >> 5) & 0x3F;
        const uint8_t b = color & 0x1F;
        output->drawPixelRGB888(x, y, (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
      }
    }
  } else if (activeScan == 16) {
    for (int y = 0; y < 64; ++y) {
      const uint16_t* row = &front[y * 128];
      for (int x = 0; x < 128; ++x) {
        uint16_t dx, dy;
        hub75Coordinates(x, y, mapping, dx, dy);
        uint16_t color = row[x];
        if (order != RgbOrder::RGB) color = mapRgb565(color, order);
        const uint8_t r = (color >> 11) & 0x1F;
        const uint8_t g = (color >> 5) & 0x3F;
        const uint8_t b = color & 0x1F;
        output->drawPixelRGB888(dx, dy, (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
      }
    }
  } else {
    for (int y = 0; y < 64; ++y) {
      const uint16_t* row = &front[y * 128];
      for (int x = 0; x < 128; ++x) {
        uint16_t dx, dy;
        hub75BankCoordinates(x, y, activeScan, dx, dy);
        uint16_t color = row[x];
        if (order != RgbOrder::RGB) color = mapRgb565(color, order);
        const uint8_t r = (color >> 11) & 0x1F;
        const uint8_t g = (color >> 5) & 0x3F;
        const uint8_t b = color & 0x1F;
        output->drawPixelRGB888(dx, dy, (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
      }
    }
  }
  output->flipDMABuffer();
  transferMicros = micros() - transferStarted;
  ++windowFrames;
  lastGeneration = canvas.generation(); dirty = false;
}
void nextHub75Mapping() {
  if (activeScan != 16) { Serial.println("Mapping fixed for this scan; choose scan 1/16 for block mapping tests"); return; }
  mapping = (mapping + 1) % 4; dirty = true;
  Serial.printf("HUB75 temporary mapping=%u; save via Web UI for persistence\n", mapping);
}
void setHub75Brightness(uint8_t brightness) {
  if (brightness < 1) brightness = 1;
  activeBrightness = brightness;
  if (ready && output && !wifiTestActive) {
    output->setBrightness8(blanked ? 0 : activeBrightness);
  }
}
void setHub75Blank(bool blank) {
  if (!ready || !output || wifiTestActive) return;
  blanked = blank;
  output->setBrightness8(blanked ? 0 : activeBrightness);
  Serial.printf("HUB75 %s (brightness=%u/255)\n", blanked ? "BLANKED" : "UNBLANKED", blanked ? 0 : activeBrightness);
}
void toggleHub75Blank() {
  setHub75Blank(!blanked);
}
}  // namespace display
