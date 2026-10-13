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
}
bool hub75Ready() { return ready; }
float hub75Fps() { return measuredFps; }
uint32_t hub75TransferMicros() { return transferMicros; }
bool hub75Blanked() { return blanked; }
unsigned hub75Mapping() { return mapping; }
uint8_t hub75Scan() { return activeScan; }
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
  output->setBrightness8(kHub75Brightness);
  output->clearScreen();
  output->flipDMABuffer();
  output->clearScreen();
  ready = true; dirty = true;
  Serial.printf("[PASS] HUB75 DMA: 128x64 scan 1/%u clock=4MHz brightness=1/255 mapping=%u\n", activeScan, mapping);
  return true;
}
bool hub75WifiTestActive() { return wifiTestActive; }
void pauseHub75ForWifiTest() {
  if (!ready || wifiTestActive) return;
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
    output->setBrightness8(blanked ? 0 : kHub75Brightness);
    wifiTestActive = false; dirty = true;
    Serial.println("[RF TEST] HUB75 DMA resumed; brightness restored");
  }
  if (!ready || !canvas.front() || canvas.width() != 128 || canvas.height() != 64) return;
  if (!dirty && lastGeneration == canvas.generation()) return;
  const uint32_t transferStarted = micros();
  // Only Arduino loop touches the CPU canvas and DMA back buffer; DMA refreshes
  // its own front buffer independently, including during HTTP/upload/decode.
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 128; ++x) {
      uint16_t dx, dy;
      if (activeScan == 32) { dx = x; dy = y; }
      else if (activeScan == 16) hub75Coordinates(x, y, mapping, dx, dy);
      else hub75BankCoordinates(x, y, activeScan, dx, dy);
      const uint16_t color = mapRgb565(canvas.front()[y * 128 + x], order);
      output->drawPixelRGB888(dx, dy, ((color >> 11) & 31) * 255 / 31,
          ((color >> 5) & 63) * 255 / 63, (color & 31) * 255 / 31);
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
void toggleHub75Blank() {
  if (!ready || wifiTestActive) return;
  blanked = !blanked;
  output->setBrightness8(blanked ? 0 : kHub75Brightness);
  Serial.printf("HUB75 brightness=%u/255\n", blanked ? 0 : kHub75Brightness);
}
}  // namespace display
