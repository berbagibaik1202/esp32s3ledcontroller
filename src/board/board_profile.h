#pragma once

#include <cstddef>
#include <cstdint>

namespace board {
struct Hub75Pins {
  int r1, g1, b1, r2, g2, b2;
  int a, b, c, d, e;
  int clk, lat, oe;
};

struct BoardProfile {
  const char* name;
  uint32_t expectedFlashBytes;
  uint32_t expectedPsramBytes;
  Hub75Pins hub75;
  int usbDm, usbDp;
};

const BoardProfile& profile();
void holdDisplayBlank();
void printProfile();
}  // namespace board
