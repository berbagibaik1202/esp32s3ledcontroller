#pragma once
#include "framebuffer.h"
#include "panel_profile.h"
#include <cstdint>
namespace display {
bool beginHub75(uint8_t scan = 32, unsigned mapping = 4);
uint8_t hub75Scan();
bool hub75Ready();
float hub75Fps();
uint32_t hub75TransferMicros();
void pauseHub75ForWifiTest();
bool hub75WifiTestActive();
void updateHub75(const Framebuffer& canvas, RgbOrder order);
void nextHub75Mapping();
unsigned hub75Mapping();
void toggleHub75Blank();
bool hub75Blanked();
constexpr uint8_t kHub75Brightness = 1;
}
