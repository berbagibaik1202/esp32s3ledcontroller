#include "animation.h"
#include <algorithm>
#include <cstring>

namespace media {
namespace {
const char* const kEffects[] = {"none", "fade", "slide-left", "slide-right", "slide-up",
    "slide-down", "blink", "scroll-left", "scroll-right", "scroll-up", "scroll-down"};
// Original compact 5x7 uppercase/digit font. Each byte is a five-bit row.
const uint8_t kLetters[][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};
const uint8_t kDigits[][7] = {
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}
};
uint8_t glyphRow(char c, unsigned row) {
  if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  if (c >= 'A' && c <= 'Z') return kLetters[c - 'A'][row];
  if (c >= '0' && c <= '9') return kDigits[c - '0'][row];
  switch (c) {
    case ' ': return 0;
    case ':': return row == 2 || row == 5 ? 4 : 0;
    case '.': return row == 6 ? 4 : 0;
    case ',': return row == 5 ? 4 : row == 6 ? 8 : 0;
    case '-': return row == 3 ? 14 : 0;
    case '_': return row == 6 ? 31 : 0;
    case '+': return row == 3 ? 31 : row >= 1 && row <= 5 ? 4 : 0;
    case '/': return static_cast<uint8_t>(1U << std::min(4U, row * 5 / 7));
    case '!': return row < 4 || row == 6 ? 4 : 0;
    default: { const uint8_t question[] = {14,17,1,2,4,0,4}; return question[row]; }
  }
}
bool validMotion(const Motion& m) {
  return effectName(m.effect) && m.durationMs >= 200 && m.durationMs <= 60000 && m.speed >= 1 && m.speed <= 200;
}
}

const char* effectName(Effect effect) {
  const unsigned index = static_cast<unsigned>(effect);
  return index < sizeof(kEffects) / sizeof(kEffects[0]) ? kEffects[index] : nullptr;
}
bool parseEffect(const char* name, Effect& effect) {
  if (!name) return false;
  for (unsigned i = 0; i < sizeof(kEffects) / sizeof(kEffects[0]); ++i)
    if (!std::strcmp(name, kEffects[i])) { effect = static_cast<Effect>(i); return true; }
  return false;
}

uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha) {
  const unsigned r = (((fg >> 11) & 31) * alpha + ((bg >> 11) & 31) * (255 - alpha) + 127) / 255;
  const unsigned g = (((fg >> 5) & 63) * alpha + ((bg >> 5) & 63) * (255 - alpha) + 127) / 255;
  const unsigned b = ((fg & 31) * alpha + (bg & 31) * (255 - alpha) + 127) / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

bool normalizeText(const char* text, char* output, size_t capacity) {
  if (!text || !output || !capacity) return false;
  const auto* p = reinterpret_cast<const uint8_t*>(text);
  size_t n = 0;
  while (*p) {
    uint32_t cp = *p++;
    unsigned count = 0;
    uint32_t minimum = 0;
    if (cp >= 0xc2 && cp <= 0xdf) { count = 1; minimum = 0x80; cp &= 31; }
    else if (cp >= 0xe0 && cp <= 0xef) { count = 2; minimum = 0x800; cp &= 15; }
    else if (cp >= 0xf0 && cp <= 0xf4) { count = 3; minimum = 0x10000; cp &= 7; }
    else if (cp >= 0x80) return false;
    for (unsigned i = 0; i < count; ++i) {
      if ((*p & 0xc0) != 0x80) return false;
      cp = (cp << 6) | (*p++ & 63);
    }
    if (count && (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))) return false;
    if (n + 1 >= capacity) return false;
    output[n++] = cp >= 32 && cp <= 126 ? static_cast<char>(cp) : '?';
  }
  output[n] = 0;
  return true;
}

Pose motionPose(const Motion& m, uint64_t elapsed, int cw, int ch, int w, int h) {
  Pose p;
  p.x = (cw - w) / 2; p.y = (ch - h) / 2;
  if (!validMotion(m) || cw <= 0 || ch <= 0 || w <= 0 || h <= 0) { p.opacity = 0; return p; }
  if (m.effect == Effect::None) return p;
  if (m.effect >= Effect::ScrollLeft) {
    const bool horizontal = m.effect == Effect::ScrollLeft || m.effect == Effect::ScrollRight;
    const uint64_t distance = (horizontal ? cw + w : ch + h) + 8;
    const uint64_t cycle = (distance * 1000 + m.speed - 1) / m.speed;
    const uint64_t t = m.loop ? elapsed % cycle : std::min(elapsed, cycle);
    const int step = static_cast<int>(t * m.speed / 1000);
    if (m.effect == Effect::ScrollLeft) p.x = cw - step;
    else if (m.effect == Effect::ScrollRight) p.x = -w + step;
    else if (m.effect == Effect::ScrollUp) p.y = ch - step;
    else p.y = -h + step;
    return p;
  }
  const uint64_t t = m.loop ? elapsed % m.durationMs : std::min<uint64_t>(elapsed, m.durationMs);
  // Repeating entrance/hold/exit = 25% / 50% / 25%; one-shot ends fully visible.
  uint32_t visible = 1000;
  if (!m.loop) visible = static_cast<uint32_t>(t * 1000 / m.durationMs);
  else if (t * 4 < m.durationMs) visible = static_cast<uint32_t>(t * 4000 / m.durationMs);
  else if (t * 4 > m.durationMs * 3) visible = static_cast<uint32_t>((m.durationMs - t) * 4000 / m.durationMs);
  if (m.effect == Effect::Fade) p.opacity = static_cast<uint8_t>(visible * 255 / 1000);
  else if (m.effect == Effect::Blink) p.opacity = t >= m.durationMs / 2 ? 255 : 0;
  else {
    const int hidden = 1000 - static_cast<int>(visible);
    const bool exiting = m.loop && t * 4 > m.durationMs * 3;
    if (m.effect == Effect::SlideLeft) p.x += (exiting ? -(w + p.x) : cw - p.x) * hidden / 1000;
    else if (m.effect == Effect::SlideRight) p.x += (exiting ? cw - p.x : -(w + p.x)) * hidden / 1000;
    else if (m.effect == Effect::SlideUp) p.y += (exiting ? -(h + p.y) : ch - p.y) * hidden / 1000;
    else if (m.effect == Effect::SlideDown) p.y += (exiting ? ch - p.y : -(h + p.y)) * hidden / 1000;
  }
  return p;
}

bool AnimationPlayer::captureImage(const display::Framebuffer& canvas) {
  if (!allocate_ || !release_ || !canvas.front() || !canvas.width() || !canvas.height() || canvas.width() > 128 || canvas.height() > 64) return false;
  auto* next = static_cast<uint16_t*>(allocate_(canvas.bufferBytes()));
  if (!next) return false;
  std::memcpy(next, canvas.front(), canvas.bufferBytes());
  if (image_) release_(image_);
  image_ = next; imageWidth_ = canvas.width(); imageHeight_ = canvas.height();
  return true;
}

bool AnimationPlayer::needsFrames(uint16_t cw, uint16_t ch) const {
  const auto pending = [&](const Motion& m, int width, int height) {
    if (m.effect == Effect::None) return false;
    if (m.loop) return true;
    uint64_t duration = m.durationMs;
    if (m.effect >= Effect::ScrollLeft) {
      const bool horizontal = m.effect == Effect::ScrollLeft || m.effect == Effect::ScrollRight;
      const uint64_t distance = (horizontal ? cw + width : ch + height) + 8;
      duration = (distance * 1000 + m.speed - 1) / m.speed;
    }
    return elapsedMs_ < duration;
  };
  const size_t length = std::strlen(glyphs_);
  return (image_ && settings_.imageVisible && pending(settings_.image, imageWidth_, imageHeight_)) ||
      (length && pending(settings_.textMotion, static_cast<int>(length * 6 - 1) * settings_.textScale, 7 * settings_.textScale));
}

bool AnimationPlayer::configure(const AnimationSettings& settings) {
  char normalized[193];
  if (!std::memchr(settings.text, 0, sizeof settings.text) || !validMotion(settings.image) ||
      !validMotion(settings.textMotion) || settings.textScale < 1 || settings.textScale > 4 ||
      !normalizeText(settings.text, normalized, sizeof normalized)) return false;
  settings_ = settings;
  std::memcpy(glyphs_, normalized, std::strlen(normalized) + 1);
  restart();
  return true;
}

bool AnimationPlayer::render(display::Framebuffer& canvas) const {
  if (!canvas.front() || !canvas.width() || !canvas.height()) return false;
  canvas.clear(settings_.background);
  if (image_ && settings_.imageVisible) {
    const Pose pose = motionPose(settings_.image, elapsedMs_, canvas.width(), canvas.height(), imageWidth_, imageHeight_);
    for (int y = std::max(0, pose.y); y < std::min<int>(canvas.height(), pose.y + imageHeight_); ++y)
      for (int x = std::max(0, pose.x); x < std::min<int>(canvas.width(), pose.x + imageWidth_); ++x)
        canvas.setPixel(x, y, blend565(image_[(y - pose.y) * imageWidth_ + x - pose.x], settings_.background, pose.opacity));
  }
  const size_t length = std::strlen(glyphs_);
  if (!length) return true;
  const int scale = settings_.textScale;
  const int width = static_cast<int>(length * 6 - 1) * scale;
  const Pose pose = motionPose(settings_.textMotion, elapsedMs_, canvas.width(), canvas.height(), width, 7 * scale);
  if (!pose.opacity) return true;
  for (size_t i = 0; i < length; ++i) {
    const int gx = pose.x + static_cast<int>(i) * 6 * scale;
    if (gx + 5 * scale <= 0 || gx >= canvas.width()) continue;
    for (unsigned row = 0; row < 7; ++row) {
      const uint8_t bits = glyphRow(glyphs_[i], row);
      for (int col = 0; col < 5; ++col) if (bits & (1U << (4 - col)))
        for (int dy = 0; dy < scale; ++dy) for (int dx = 0; dx < scale; ++dx) {
          const int x = gx + col * scale + dx, y = pose.y + static_cast<int>(row) * scale + dy;
          canvas.setPixel(x, y, blend565(settings_.textColor, canvas.backPixel(x, y), pose.opacity));
        }
    }
  }
  return true;
}
}  // namespace media
