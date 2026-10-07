#pragma once
#include "image_source.h"
#include <cstddef>

namespace media {
class BmpImage {
 public:
  // Borrowed immutable bytes must remain alive through rendering.
  // Failed opens retain the previous valid image. No heap allocation.
  bool open(const uint8_t* data, size_t length, const char*& error);
  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  bool topDown() const { return topDown_; }
  uint16_t pixel(uint16_t x, uint16_t y) const;
  ImageSource source() const;
 private:
  static uint16_t sample(const void* context, uint16_t x, uint16_t y);
  const uint8_t* pixels_ = nullptr;
  size_t stride_ = 0;
  uint16_t width_ = 0, height_ = 0;
  bool topDown_ = false;
};
}  // namespace media
