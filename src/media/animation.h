#pragma once
#include "display/framebuffer.h"

namespace media {
enum class Effect { None, Fade, SlideLeft, SlideRight, SlideUp, SlideDown, Blink,
                    ScrollLeft, ScrollRight, ScrollUp, ScrollDown };
const char* effectName(Effect effect);
bool parseEffect(const char* name, Effect& effect);
struct Motion {
  Effect effect = Effect::None;
  uint32_t durationMs = 3000;
  uint16_t speed = 30;  // pixels/second; used by scroll effects
  bool loop = true;
};
struct AnimationSettings {
  bool imageVisible = true;
  Motion image;
  char text[193] = {};
  Motion textMotion;
  uint8_t textScale = 1;
  uint16_t textColor = 0xffff;
  uint16_t background = 0;
};
struct Pose { int x = 0, y = 0; uint8_t opacity = 255; };
Pose motionPose(const Motion& motion, uint64_t elapsedMs, int canvasWidth,
                int canvasHeight, int contentWidth, int contentHeight);
uint16_t blend565(uint16_t foreground, uint16_t background, uint8_t opacity);
// UTF-8 is validated; unsupported codepoints become one '?' glyph each.
bool normalizeText(const char* text, char* output, size_t capacity);

class AnimationPlayer {
 public:
  using Allocate = display::Framebuffer::Allocate;
  using Release = display::Framebuffer::Release;
  AnimationPlayer(Allocate allocate, Release release) : allocate_(allocate), release_(release) {}
  ~AnimationPlayer() { if (image_) release_(image_); }
  AnimationPlayer(const AnimationPlayer&) = delete;
  AnimationPlayer& operator=(const AnimationPlayer&) = delete;
  bool captureImage(const display::Framebuffer& canvas);
  bool configure(const AnimationSettings& settings);
  const AnimationSettings& settings() const { return settings_; }
  bool render(display::Framebuffer& canvas) const;
  void advance(uint32_t deltaMs) { if (!paused_) elapsedMs_ += deltaMs; }
  void restart() { elapsedMs_ = 0; paused_ = false; }
  void setPaused(bool paused) { paused_ = paused; }
  bool paused() const { return paused_; }
  uint64_t elapsedMs() const { return elapsedMs_; }
  bool hasImage() const { return image_ != nullptr; }
  bool needsFrames(uint16_t canvasWidth, uint16_t canvasHeight) const;
 private:
  Allocate allocate_;
  Release release_;
  uint16_t* image_ = nullptr;
  uint16_t imageWidth_ = 0, imageHeight_ = 0;
  AnimationSettings settings_;
  char glyphs_[193] = {};
  uint64_t elapsedMs_ = 0;
  bool paused_ = false;
};
}  // namespace media
