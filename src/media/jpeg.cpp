#include "jpeg.h"
#include <JPEGDEC.h>
#include <algorithm>
#include <cstring>
#include <new>
#include <type_traits>

namespace media {
namespace {
bool fail(const char*& error, const char* message) { error = message; return false; }
uint16_t big16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }

bool inspectJpeg(const uint8_t* data, size_t length, uint16_t& width, uint16_t& height,
                 const char*& error) {
  if (!data || length < 4 || length > 1024U * 1024U || data[0] != 0xFF || data[1] != 0xD8)
    return fail(error, "Invalid JPEG input (limit 1 MiB)");
  size_t position = 2;
  bool frameFound = false;
  uint8_t components = 0;
  while (position < length) {
    if (data[position++] != 0xFF) return fail(error, "Invalid JPEG marker");
    while (position < length && data[position] == 0xFF) ++position;
    if (position == length) break;
    const uint8_t marker = data[position++];
    if (marker == 0 || marker == 0xD8 || marker == 0xD9 || marker == 0x01 ||
        (marker >= 0xD0 && marker <= 0xD7)) return fail(error, "Unexpected JPEG marker");
    if (length - position < 2) break;
    const size_t segmentLength = big16(data + position);
    if (segmentLength < 2 || segmentLength > length - position) break;
    const uint8_t* segment = data + position;
    if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4) {
      if (marker != 0xC0 || frameFound) return fail(error, "Only baseline JPEG supported (no progressive/arithmetic)");
      if (segmentLength < 8 || segment[2] != 8) return fail(error, "JPEG must use 8-bit samples");
      height = big16(segment + 3); width = big16(segment + 5); components = segment[7];
      if (!width || !height || width > 128 || height > 64)
        return fail(error, "JPEG dimensions exceed 128x64 limit");
      if ((components != 1 && components != 3) || segmentLength != size_t(8 + 3 * components))
        return fail(error, "Only grayscale or three-component JPEG supported");
      if (components == 3 && (segment[8] != 1 || segment[11] != 2 || segment[14] != 3))
        return fail(error, "Only YCbCr color JPEG supported");
      frameFound = true;
    }
    if (marker == 0xDA) {
      if (!frameFound || segmentLength != size_t(6 + 2 * components) || segment[2] != components ||
          segment[segmentLength - 3] != 0 || segment[segmentLength - 2] != 63 || segment[segmentLength - 1] != 0)
        return fail(error, "Only single-scan sequential JPEG supported");
      position += segmentLength;
      bool entropyFound = false;
      while (position < length) {
        if (data[position++] != 0xFF) { entropyFound = true; continue; }
        while (position < length && data[position] == 0xFF) ++position;
        if (position == length) break;
        const uint8_t code = data[position++];
        if (code == 0) { entropyFound = true; continue; }
        if (code >= 0xD0 && code <= 0xD7) continue;
        if (code == 0xD9 && position == length && entropyFound) return true;
        return fail(error, "JPEG has multiple scans, early EOI, or trailing data");
      }
      return fail(error, "JPEG entropy data or EOI is truncated");
    }
    position += segmentLength;
  }
  return fail(error, "JPEG header is truncated or has no scan");
}

struct DecodeTarget {
  uint16_t* pixels;
  uint8_t* coverage;
  uint16_t width, height;
  size_t written = 0;
  bool failed = false;
  const JPEGIMAGE* decoderState = nullptr;
  DecodeTarget(uint16_t* p, uint8_t* c, uint16_t w, uint16_t h) : pixels(p), coverage(c), width(w), height(h) {}
};

// Pinned JPEGDEC is a standard-layout wrapper whose sole member is JPEGIMAGE.
// That first member is pointer-interconvertible with the wrapper in C++11.
// Revalidate this integration whenever the pinned upstream commit changes.
static_assert(std::is_standard_layout<JPEGDEC>::value && sizeof(JPEGDEC) == sizeof(JPEGIMAGE),
              "JPEGDEC layout changed: revalidate bounded entropy integration");
bool withinEntropy(const JPEGIMAGE* state) {
  return state && state->iVLCOff >= 0 && state->iVLCSize >= 0 &&
      static_cast<size_t>(state->iVLCOff) + (state->bb.ulBitOff + 7U) / 8U <=
      static_cast<size_t>(state->iVLCSize);
}

int drawBlock(JPEGDRAW* block) {
  if (!block || !block->pUser) return 0;
  auto& target = *static_cast<DecodeTarget*>(block->pUser);
  if (!withinEntropy(target.decoderState)) { target.failed = true; return 0; }
  if (!block->pPixels || block->iBpp != 16 || block->x < 0 || block->y < 0 ||
      block->x >= target.width || block->y >= target.height || block->iWidth <= 0 ||
      block->iHeight <= 0 || block->iWidthUsed <= 0 || block->iWidthUsed > block->iWidth) {
    target.failed = true; return 0;
  }
  const int width = std::min(block->iWidthUsed, target.width - block->x);
  const int height = std::min(block->iHeight, target.height - block->y);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t slot = static_cast<size_t>(block->y + y) * target.width + block->x + x;
      const uint8_t bit = static_cast<uint8_t>(1U << (slot % 8));
      if (target.coverage[slot / 8] & bit) { target.failed = true; return 0; }
      target.coverage[slot / 8] |= bit;
      target.pixels[slot] = block->pPixels[static_cast<size_t>(y) * block->iWidth + x];
      ++target.written;
    }
  }
  return 1;
}
}

JpegImage::~JpegImage() { reset(); }
void JpegImage::reset() {
  if (pixels_ && release_) release_(pixels_);
  pixels_ = nullptr; width_ = height_ = 0;
}
size_t JpegImage::decoderWorkspaceBytes() { return sizeof(JPEGDEC); }

bool JpegImage::open(const uint8_t* data, size_t length, const char*& error) {
  error = nullptr;
  if (!allocate_ || !release_) return fail(error, "JPEG allocator unavailable");
  uint16_t width = 0, height = 0;
  if (!inspectJpeg(data, length, width, height, error)) return false;
  const size_t count = static_cast<size_t>(width) * height;
  const size_t bytes = count * sizeof(uint16_t), coverageBytes = (count + 7) / 8;
  auto* candidate = static_cast<uint16_t*>(allocate_(bytes + coverageBytes));
  if (!candidate) return fail(error, "JPEG pixel allocation failed");
  auto* coverage = reinterpret_cast<uint8_t*>(candidate) + bytes;
  std::memset(coverage, 0, coverageBytes);
  void* workspace = allocate_(sizeof(JPEGDEC));
  if (!workspace) { release_(candidate); return fail(error, "JPEG workspace allocation failed"); }
  auto* decoder = new (workspace) JPEGDEC;
  DecodeTarget target(candidate, coverage, width, height);
  target.decoderState = reinterpret_cast<const JPEGIMAGE*>(decoder);
  const bool opened = decoder->openFLASH(data, static_cast<int>(length), drawBlock) != 0;
  bool decoded = false;
  if (opened && decoder->getJPEGType() == JPEG_MODE_BASELINE &&
      decoder->getWidth() == width && decoder->getHeight() == height) {
    decoder->setPixelType(RGB565_LITTLE_ENDIAN);
    decoder->setUserPointer(&target);
    decoded = decoder->decode(0, 0, 0) != 0 && decoder->getLastError() == 0 &&
              !target.failed && target.written == count && withinEntropy(target.decoderState);
  }
  if (opened) decoder->close();
  decoder->~JPEGDEC(); release_(workspace);
  if (!decoded) { release_(candidate); return fail(error, "JPEG decode failed or incomplete pixel coverage"); }
  if (pixels_) release_(pixels_);
  pixels_ = candidate; width_ = width; height_ = height;
  return true;
}

uint16_t JpegImage::pixel(uint16_t x, uint16_t y) const {
  return pixels_ && x < width_ && y < height_ ? pixels_[static_cast<size_t>(y) * width_ + x] : 0;
}
uint16_t JpegImage::sample(const void* context, uint16_t x, uint16_t y) {
  return static_cast<const JpegImage*>(context)->pixel(x, y);
}
ImageSource JpegImage::source() const {
  ImageSource result;
  result.context = this; result.width = width_; result.height = height_;
  result.sample = pixels_ ? sample : nullptr;
  return result;
}
}  // namespace media
