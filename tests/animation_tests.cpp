#include "media/animation.h"
#include "test_assert.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
int allocations = 0, releases = 0;
bool failAllocation = false;
void* allocate(size_t bytes) { if (failAllocation) return nullptr; ++allocations; return std::malloc(bytes); }
void release(void* value) { if (value) ++releases; std::free(value); }
}

void testAnimation() {
  using namespace media;
  Effect parsed = Effect::None;
  for (unsigned i = 0; i <= static_cast<unsigned>(Effect::ScrollDown); ++i) {
    assert(parseEffect(effectName(static_cast<Effect>(i)), parsed));
    assert(parsed == static_cast<Effect>(i));
  }
  assert(!parseEffect("unknown", parsed));
  assert(!effectName(static_cast<Effect>(99)));
  assert(blend565(0xf800, 0x001f, 0) == 0x001f);
  assert(blend565(0xf800, 0x001f, 255) == 0xf800);
  assert(blend565(0xffff, 0, 128) == 0x8410);
  char normalized[193];
  assert(normalizeText("A\xc3\xa9\xf0\x9f\x98\x80", normalized, sizeof normalized));
  assert(!std::strcmp(normalized, "A??"));
  for (const char* bad : {"\xc0\xaf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"})
    assert(!normalizeText(bad, normalized, sizeof normalized));
  assert(!normalizeText("AB", normalized, 2));
  Motion motion;
  motion.durationMs = 1000;
  motion.effect = Effect::Fade;
  assert(motionPose(motion, 0, 64, 32, 5, 7).opacity == 0);
  assert(motionPose(motion, 250, 64, 32, 5, 7).opacity == 255);
  assert(motionPose(motion, 500, 64, 32, 5, 7).opacity == 255);
  assert(motionPose(motion, 999, 64, 32, 5, 7).opacity < 3);
  assert(motionPose(motion, 1000, 64, 32, 5, 7).opacity == 0);
  motion.loop = false;
  assert(motionPose(motion, 100000, 64, 32, 5, 7).opacity == 255);
  motion.loop = true;
  for (Effect effect : {Effect::SlideLeft, Effect::SlideRight, Effect::SlideUp, Effect::SlideDown}) {
    motion.effect = effect;
    auto start = motionPose(motion, 0, 64, 32, 5, 7);
    auto middle = motionPose(motion, 500, 64, 32, 5, 7);
    auto end = motionPose(motion, 990, 64, 32, 5, 7);
    assert(middle.x == 29 && middle.y == 12);
    if (effect == Effect::SlideLeft) assert(start.x == 64 && end.x < 0);
    if (effect == Effect::SlideRight) assert(start.x == -5 && end.x >= 60);
    if (effect == Effect::SlideUp) assert(start.y == 32 && end.y < 0);
    if (effect == Effect::SlideDown) assert(start.y == -7 && end.y >= 30);
  }
  motion.effect = Effect::ScrollLeft;
  motion.speed = 10;
  assert(motionPose(motion, 0, 64, 32, 5, 7).x == 64);
  assert(motionPose(motion, 1000, 64, 32, 5, 7).x == 54);
  assert(motionPose(motion, 7700, 64, 32, 5, 7).x == 64);
  motion.loop = false;
  assert(motionPose(motion, 8000, 64, 32, 5, 7).x == -13);
  motion.effect = Effect::ScrollRight;
  assert(motionPose(motion, 1000, 64, 32, 5, 7).x == 5);
  motion.effect = Effect::ScrollUp;
  assert(motionPose(motion, 1000, 64, 32, 5, 7).y == 22);
  motion.effect = Effect::ScrollDown;
  assert(motionPose(motion, 1000, 64, 32, 5, 7).y == 3);
  motion.effect = Effect::Blink;
  assert(motionPose(motion, 499, 64, 32, 5, 7).opacity == 0);
  assert(motionPose(motion, 500, 64, 32, 5, 7).opacity == 255);
  assert(motionPose(motion, 500, 0, 32, 5, 7).opacity == 0);

  {
    display::Framebuffer canvas(allocate, release);
    assert(canvas.configure(64, 32));
    canvas.clear(0xf800); assert(canvas.present());
    AnimationPlayer player(allocate, release);
    assert(player.captureImage(canvas));
    AnimationSettings settings;
    std::strcpy(settings.text, "A");
    assert(player.configure(settings));
    assert(!player.needsFrames(64, 32));
    assert(player.render(canvas));
    assert(canvas.front()[12 * 64 + 30] == 0xf800);  // back-only rendering
    assert(canvas.backPixel(30, 12) == 0xffff);
    assert(canvas.backPixel(29, 12) == 0xf800);  // transparent glyph background
    settings.textMotion.effect = Effect::Fade;
    settings.textMotion.loop = false;
    settings.textMotion.durationMs = 1000;
    assert(player.configure(settings));
    assert(player.needsFrames(64, 32));
    player.advance(500);
    assert(player.render(canvas));
    assert(canvas.backPixel(30, 12) == blend565(0xffff, 0xf800, 127));
    player.setPaused(true); player.advance(1000); assert(player.elapsedMs() == 500);
    player.setPaused(false); player.advance(100); assert(player.elapsedMs() == 600);
    player.advance(400); assert(!player.needsFrames(64, 32));
    player.restart(); assert(!player.paused() && player.elapsedMs() == 0);
    const int count = allocations;
    for (int i = 0; i < 20; ++i) { player.advance(50); assert(player.render(canvas)); }
    assert(allocations == count);  // no per-frame allocations or JPEG decoding
    settings.textScale = 0;
    assert(!player.configure(settings)); assert(player.settings().textScale == 1);
    settings.textScale = 1; settings.image.durationMs = 199;
    assert(!player.configure(settings));
    settings.image.durationMs = 3000; settings.image.speed = 201;
    assert(!player.configure(settings));
    settings.image.speed = 30;
    std::memset(settings.text, 'A', sizeof settings.text);
    assert(!player.configure(settings));
    failAllocation = true;
    assert(!player.captureImage(canvas));
    failAllocation = false;
    assert(player.render(canvas));
    assert(canvas.backPixel(0, 0) == 0xf800);  // previous snapshot survives allocation failure
    settings = AnimationSettings();
    settings.imageVisible = false;
    settings.background = 0x001f;
    std::strcpy(settings.text, "ABC");
    settings.textScale = 4;
    assert(player.configure(settings));
    assert(canvas.configure(1, 1));
    assert(player.render(canvas));  // clipped large text on tiny canvas

    // Export real renderer frames for a standalone browser preview.
    assert(canvas.configure(64, 32));
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x)
      canvas.setPixel(x, y, x < 21 ? 0x001f : x < 42 ? 0x07e0 : 0xf800);
    assert(canvas.present()); assert(player.captureImage(canvas));
    settings = AnimationSettings();
    settings.image.effect = Effect::Fade;
    settings.image.durationMs = 4000;
    settings.textMotion.effect = Effect::ScrollLeft;
    settings.textMotion.speed = 24;
    std::strcpy(settings.text, "ESP32 LED ANIMASI");
    assert(player.configure(settings));
    std::ofstream frames(".build-temp/previews/animation-frames.rgb", std::ios::binary);
    assert(frames.good());
    for (int i = 0; i < 60; ++i) {
      assert(player.render(canvas)); assert(canvas.present());
      for (unsigned pixel = 0; pixel < 64 * 32; ++pixel) {
        const uint16_t c = canvas.front()[pixel];
        const unsigned char rgb[] = {static_cast<unsigned char>(((c >> 11) & 31) * 255 / 31),
            static_cast<unsigned char>(((c >> 5) & 63) * 255 / 63), static_cast<unsigned char>((c & 31) * 255 / 31)};
        frames.write(reinterpret_cast<const char*>(rgb), 3);
      }
      player.advance(100);
    }
  }
  assert(allocations == releases);
  std::cout << "PASS: animation timing/directions, UTF-8, glyphs, opacity, pause, clipping, rollback, allocation-free frames\n";
}
