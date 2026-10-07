#include "board_profile.h"
#include <Arduino.h>

namespace board {
namespace {
// Prototype pinout from PRD; validate against the actual board before M1.
constexpr BoardProfile kProfile{
    "ESP32S3_DEVKIT_N16R8_PROTOTYPE", 16U * 1024U * 1024U,
    8U * 1024U * 1024U,
    {4, 5, 6, 7, 15, 16, 8, 9, 10, 11, 12, 13, 14, 18}, 19, 20};
static_assert(kProfile.hub75.oe != kProfile.usbDm &&
              kProfile.hub75.oe != kProfile.usbDp, "USB pins are reserved");
}

const BoardProfile& profile() { return kProfile; }

void holdDisplayBlank() {
  // OE is active low on the planned backend. An external pull-up is still
  // needed to hold blank during reset, before software can configure GPIO.
  digitalWrite(kProfile.hub75.oe, HIGH);
  pinMode(kProfile.hub75.oe, OUTPUT);
  // M0 does not clock/latch pixel data or drive the other HUB75 signals.
}

void printProfile() {
  Serial.printf("Board profile: %s (pinout UNVERIFIED)\n", kProfile.name);
  Serial.printf("USB reserved: D-=%d D+=%d; HUB75 OE=%d (blank)\n",
                kProfile.usbDm, kProfile.usbDp, kProfile.hub75.oe);
}
}  // namespace board
