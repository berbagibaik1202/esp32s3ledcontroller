#pragma once
#include <cstdint>

namespace display {
inline bool validHub75Settings(unsigned scan, unsigned mapping) {
  return (scan == 32 && mapping == 4) || (scan == 16 && mapping <= 3) ||
      ((scan == 4 || scan == 8) && mapping == 5);
}
inline bool hub75BankCoordinates(int x, int y, unsigned scan, uint16_t& dx, uint16_t& dy) {
  if (x < 0 || x >= 128 || y < 0 || y >= 64 ||
      (scan != 4 && scan != 8 && scan != 16 && scan != 32)) return false;
  dx = x + ((y % 32) / scan) * 128;
  dy = y % scan + (y / 32) * scan;
  return true;
}
// Electrical buffer: 256x32, ABCD addressing (16 addresses), no E.
// Four-scan mapping based on upstream FOUR_SCAN_64PX_HIGH policy.
// Modes 0/1/2 use 16/32/128-pixel blocks; mode 3 uses linear 16-row banks.
inline bool hub75Coordinates(int x, int y, unsigned mode, uint16_t& dx, uint16_t& dy) {
  if (x < 0 || x >= 128 || y < 0 || y >= 64 || mode > 3) return false;
  if (mode == 3) {
    dx = x + ((y & 16) ? 128 : 0);
    dy = (y & 15) + ((y & 32) ? 16 : 0);
    return true;
  }
  const int block = mode == 0 ? 16 : mode == 1 ? 32 : 128;
  if ((y & 8) != ((y & 16) >> 1)) y = ((y & 24) ^ 24) + (y & 103);
  dx = x + (x / block + ((y & 8) == 0 ? 1 : 0)) * block;
  dy = (y >> 4) * 8 + (y & 7);
  return true;
}
}  // namespace display
