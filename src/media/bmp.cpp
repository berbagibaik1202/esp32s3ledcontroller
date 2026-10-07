#include "bmp.h"

namespace media {
namespace {
uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8)); }
uint32_t u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
bool fail(const char*& error, const char* message) { error = message; return false; }
}

bool BmpImage::open(const uint8_t* data, size_t length, const char*& error) {
  error = nullptr;
  if (!data || length < 54 || length > 1024U * 1024U) return fail(error, "BMP size must be 54 bytes..1 MiB");
  if (data[0] != 'B' || data[1] != 'M' || u16(data + 6) || u16(data + 8))
    return fail(error, "Invalid BMP file header");
  if (u32(data + 2) != length) return fail(error, "BMP declared file size does not match input");
  if (u32(data + 14) != 40) return fail(error, "Only 40-byte BITMAPINFOHEADER supported");
  if (u16(data + 26) != 1 || u16(data + 28) != 24 || u32(data + 30) != 0 || u32(data + 46) != 0)
    return fail(error, "Only uncompressed 24-bit BMP without a palette supported");
  const uint32_t width = u32(data + 18), rawHeight = u32(data + 22);
  const bool topDown = (rawHeight & 0x80000000U) != 0;
  const uint32_t height = topDown ? 0U - rawHeight : rawHeight;
  if (!width || width > 128 || !height || height > 64)
    return fail(error, "BMP dimensions must be 1..128 x 1..64");
  const size_t stride = (static_cast<size_t>(width) * 3 + 3) & ~size_t(3);
  const size_t pixelBytes = stride * height;
  const size_t offset = u32(data + 10);
  if (offset < 54 || offset > length || pixelBytes > length - offset)
    return fail(error, "BMP pixel data is truncated or overlaps header");
  const uint32_t declaredPixels = u32(data + 34);
  if (declaredPixels && declaredPixels != pixelBytes)
    return fail(error, "BMP image size does not match row stride");
  pixels_ = data + offset; stride_ = stride;
  width_ = static_cast<uint16_t>(width); height_ = static_cast<uint16_t>(height);
  topDown_ = topDown;
  return true;
}

uint16_t BmpImage::pixel(uint16_t x, uint16_t y) const {
  if (!pixels_ || x >= width_ || y >= height_) return 0;
  const size_t row = topDown_ ? y : height_ - 1 - y;
  const uint8_t* p = pixels_ + row * stride_ + static_cast<size_t>(x) * 3;
  return rgb565(p[2], p[1], p[0]);
}

uint16_t BmpImage::sample(const void* context, uint16_t x, uint16_t y) {
  return static_cast<const BmpImage*>(context)->pixel(x, y);
}

ImageSource BmpImage::source() const {
  ImageSource result;
  result.context = this; result.width = width_; result.height = height_;
  result.sample = pixels_ ? sample : nullptr;
  return result;
}
}  // namespace media
