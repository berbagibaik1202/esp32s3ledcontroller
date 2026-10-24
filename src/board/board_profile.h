#pragma once

#include <cstddef>
#include <cstdint>

namespace board {
struct Hub75Pins {
  int r1, g1, b1, r2, g2, b2;
  int a, b, c, d, e;
  int clk, lat, oe;
};

struct SdPins {
  int cs;    // GPIO 21
  int sck;   // GPIO 38
  int mosi;  // GPIO 39
  int miso;  // GPIO 40
};

struct AudioPins {
  int bclk;  // GPIO 41
  int lrc;   // GPIO 42
  int dout;  // GPIO 2
};

struct BoardProfile {
  const char* name;
  uint32_t expectedFlashBytes;
  uint32_t expectedPsramBytes;
  Hub75Pins hub75;
  int usbDm, usbDp;
  int confirmButton;
  SdPins sd;
  AudioPins audio;
};

const BoardProfile& profile();
void holdDisplayBlank();
void printProfile();
}  // namespace board
