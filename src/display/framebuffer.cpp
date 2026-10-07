#include "framebuffer.h"
#include <algorithm>
#include <cstring>
#include <utility>

namespace display {
Framebuffer::~Framebuffer() {
  if (release_) {
    if (front_) release_(front_);
    if (back_) release_(back_);
  }
}

bool Framebuffer::configure(uint16_t width, uint16_t height) {
  if (!allocate_ || !release_ || !width || !height || width > 128 || height > 64) return false;
  const size_t bytes = static_cast<size_t>(width) * height * sizeof(uint16_t);
  auto* newFront = static_cast<uint16_t*>(allocate_(bytes));
  if (!newFront) return false;
  auto* newBack = static_cast<uint16_t*>(allocate_(bytes));
  if (!newBack) { release_(newFront); return false; }
  std::memset(newFront, 0, bytes);
  std::memset(newBack, 0, bytes);
  if (front_) release_(front_);
  if (back_) release_(back_);
  front_ = newFront; back_ = newBack;
  width_ = width; height_ = height; generation_ = 0;
  return true;
}

void Framebuffer::clear(uint16_t color) {
  if (back_) std::fill(back_, back_ + static_cast<size_t>(width_) * height_, color);
}

bool Framebuffer::setPixel(int x, int y, uint16_t color) {
  if (!back_ || x < 0 || y < 0 || x >= width_ || y >= height_) return false;
  back_[static_cast<size_t>(y) * width_ + x] = color;
  return true;
}

uint16_t Framebuffer::backPixel(int x, int y) const {
  if (!back_ || x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
  return back_[static_cast<size_t>(y) * width_ + x];
}

bool Framebuffer::present() {
  if (!front_ || !back_) return false;
  std::swap(front_, back_); ++generation_; return true;
}
}  // namespace display
