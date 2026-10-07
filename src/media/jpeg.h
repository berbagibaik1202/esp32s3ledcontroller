#pragma once
#include "image_source.h"
#include <cstddef>

namespace media {
class JpegImage {
 public:
  using Allocate = void* (*)(size_t);
  using Release = void (*)(void*);
  JpegImage(Allocate allocate, Release release) : allocate_(allocate), release_(release) {}
  ~JpegImage();
  void reset();
  JpegImage(const JpegImage&) = delete;
  JpegImage& operator=(const JpegImage&) = delete;
  // Input is needed only during decode; owned RGB565 pixels remain afterwards.
  // Accepts baseline single-scan Huffman JPEG, grayscale or YCbCr, <=128x64.
  // Failure retains the previous image. Allocator must provide ordinary object alignment.
  bool open(const uint8_t* data, size_t length, const char*& error);
  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  uint16_t pixel(uint16_t x, uint16_t y) const;
  ImageSource source() const;
  static size_t decoderWorkspaceBytes();
 private:
  static uint16_t sample(const void* context, uint16_t x, uint16_t y);
  Allocate allocate_;
  Release release_;
  uint16_t* pixels_ = nullptr;
  uint16_t width_ = 0, height_ = 0;
};
}  // namespace media
