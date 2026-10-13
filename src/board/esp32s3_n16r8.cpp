#include "board_profile.h"
#include <Arduino.h>

namespace board {
namespace {
// GPIO assignment finalized with the user for PCB wiring; see WIRING.md.
// Panel scan/mapping and electrical operation still require hardware validation.
constexpr BoardProfile kProfile{
    "ESP32S3_DEVKIT_N16R8_PROTOTYPE", 16U * 1024U * 1024U,
    8U * 1024U * 1024U,
    {// R1, G1, B1, R2, G2, B2
     4, 5, 6, 7, 15, 16,
     // A, B, C, D, E (E reserved; use depends on panel addressing)
     8, 9, 10, 11, 12,
     // CLK, LAT, OE
     13, 14, 18}, 19, 20, 17};
static_assert(kProfile.confirmButton != kProfile.usbDm &&
              kProfile.confirmButton != kProfile.usbDp &&
              kProfile.confirmButton != kProfile.hub75.oe &&
              kProfile.confirmButton != kProfile.hub75.clk &&
              kProfile.confirmButton != kProfile.hub75.lat,
              "Button must not overlap control/USB pins");
static_assert(kProfile.hub75.oe != kProfile.usbDm &&
              kProfile.hub75.oe != kProfile.usbDp, "USB pins are reserved");
}

const BoardProfile& profile() { return kProfile; }

void holdDisplayBlank() {
  // OE is active low on the planned backend. An external pull-up is still
  // needed to hold blank during reset, before software can configure GPIO.
  digitalWrite(kProfile.hub75.oe, HIGH);
  pinMode(kProfile.hub75.oe, OUTPUT);
  // HUB75 backend takes over these pins only after successful initialization.
}

void printProfile() {
  Serial.printf("Board profile: %s (pinout UNVERIFIED)\n", kProfile.name);
  Serial.printf("USB reserved: D-=%d D+=%d; HUB75 OE=%d (blank)\n",
                kProfile.usbDm, kProfile.usbDp, kProfile.hub75.oe);
}
}  // namespace board
