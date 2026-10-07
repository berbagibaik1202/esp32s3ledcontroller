#pragma once
#include <cstdint>

namespace media {
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
struct ImageSource {
  using Sample = uint16_t (*)(const void*, uint16_t, uint16_t);
  const void* context = nullptr;
  uint16_t width = 0, height = 0;
  Sample sample = nullptr;
};
}  // namespace media
