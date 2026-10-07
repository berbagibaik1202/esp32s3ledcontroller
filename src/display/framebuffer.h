#pragma once
#include <cstddef>
#include <cstdint>

namespace display {
class Framebuffer {
 public:
  using Allocate = void* (*)(size_t);
  using Release = void (*)(void*);
  Framebuffer(Allocate allocate, Release release) : allocate_(allocate), release_(release) {}
  ~Framebuffer();
  Framebuffer(const Framebuffer&) = delete;
  Framebuffer& operator=(const Framebuffer&) = delete;
  bool configure(uint16_t width, uint16_t height);
  void clear(uint16_t color = 0);
  bool setPixel(int x, int y, uint16_t color);
  uint16_t backPixel(int x, int y) const;
  const uint16_t* front() const { return front_; }
  // CPU-only handoff. Backend synchronization must precede use from another task.
  bool present();
  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  size_t bufferBytes() const { return static_cast<size_t>(width_) * height_ * sizeof(uint16_t); }
  uint32_t generation() const { return generation_; }
 private:
  Allocate allocate_;
  Release release_;
  uint16_t* front_ = nullptr;
  uint16_t* back_ = nullptr;
  uint16_t width_ = 0, height_ = 0;
  uint32_t generation_ = 0;
};
}  // namespace display
