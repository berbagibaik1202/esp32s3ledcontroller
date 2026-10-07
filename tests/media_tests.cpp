#include "media/bmp.h"
#include "media/image_renderer.h"
#include "media/builtin_image.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include "test_assert.h"

namespace {
void put32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[at + i] = static_cast<uint8_t>(value >> (8 * i));
}
std::vector<uint8_t> paddedBmp(bool topDown) {
  std::vector<uint8_t> bytes(media::kExampleBmp, media::kExampleBmp + sizeof media::kExampleBmp);
  put32(bytes, 18, 3);  // 3 pixels + 3 padding bytes, same 12-byte stride.
  if (topDown) {
    put32(bytes, 22, 0U - 2);
    for (unsigned i = 0; i < 12; ++i) std::swap(bytes[54 + i], bytes[66 + i]);
  }
  return bytes;
}

void rejectedBmp(const std::vector<uint8_t>& bytes, media::BmpImage& image) {
  const char* error = nullptr;
  const auto width = image.width(), height = image.height();
  const auto color = image.pixel(0, 0);
  assert(!image.open(bytes.data(), bytes.size(), error) && error);
  assert(image.width() == width && image.height() == height && image.pixel(0, 0) == color);
}

void preview(const display::Framebuffer& canvas, const char* mode) {
  std::ofstream file(std::string(".build-temp/previews/media_") + mode + ".ppm", std::ios::binary);
  assert(file);
  file << "P6\n" << canvas.width() << ' ' << canvas.height() << "\n255\n";
  for (size_t i = 0; i < canvas.bufferBytes() / 2; ++i) {
    const uint16_t color = canvas.front()[i];
    const char rgb[] = {static_cast<char>(((color >> 11) & 31) * 255 / 31),
                        static_cast<char>(((color >> 5) & 63) * 255 / 63),
                        static_cast<char>((color & 31) * 255 / 31)};
    file.write(rgb, 3);
  }
  assert(file.good());
}

struct CoordinateImage {
  static uint16_t sample(const void*, uint16_t x, uint16_t y) {
    assert(x < 4 && y < 2);
    return static_cast<uint16_t>(1 + y * 4 + x);
  }
};
}

void testMedia() {
  media::BmpImage image;
  const char* error = nullptr;
  assert(image.width() == 0 && image.pixel(0, 0) == 0 && !image.source().sample);
  assert(image.open(media::kExampleBmp, sizeof media::kExampleBmp, error));
  assert(image.width() == 4 && image.height() == 2 && !image.topDown());
  const uint16_t expected[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0xFFE0, 0x07FF, 0xF81F, 0};
  for (unsigned i = 0; i < 8; ++i) assert(image.pixel(i % 4, i / 4) == expected[i]);
  assert(image.pixel(4, 0) == 0 && image.pixel(0, 2) == 0);
  std::ifstream fixture("examples/media/test_bars.bmp", std::ios::binary);
  assert(fixture);
  const std::vector<uint8_t> fileBytes((std::istreambuf_iterator<char>(fixture)), std::istreambuf_iterator<char>());
  assert(fileBytes.size() == sizeof media::kExampleBmp);
  assert(std::equal(fileBytes.begin(), fileBytes.end(), media::kExampleBmp));
  for (bool topDown : {false, true}) {
    auto bytes = paddedBmp(topDown);
    media::BmpImage padded;
    assert(padded.open(bytes.data(), bytes.size(), error));
    assert(padded.width() == 3 && padded.topDown() == topDown);
    for (unsigned y = 0; y < 2; ++y)
      for (unsigned x = 0; x < 3; ++x) assert(padded.pixel(x, y) == expected[y * 4 + x]);
  }
  const std::vector<uint8_t> valid(media::kExampleBmp, media::kExampleBmp + sizeof media::kExampleBmp);
  const auto mutate = [&](size_t offset, uint32_t value) {
    auto bytes = valid; put32(bytes, offset, value); rejectedBmp(bytes, image);
  };
  for (size_t size = 0; size < valid.size(); ++size) {
    auto truncated = valid; truncated.resize(size); rejectedBmp(truncated, image);
  }
  mutate(2, 1000); mutate(10, 40); mutate(10, 0xFFFFFFFFU);
  mutate(14, 108); mutate(18, 129); mutate(18, 0xFFFFFFFFU); mutate(18, 0);
  mutate(22, 65); mutate(22, 0x80000000U); mutate(22, 0);
  mutate(30, 1); mutate(34, 1); mutate(46, 2);
  auto wrong = valid; wrong[0] = 'X'; rejectedBmp(wrong, image);
  wrong = valid; wrong[6] = 1; rejectedBmp(wrong, image);
  wrong = valid; wrong[26] = 2; rejectedBmp(wrong, image);
  wrong = valid; wrong[28] = 32; rejectedBmp(wrong, image);
  wrong = valid; put32(wrong, 34, 0); assert(image.open(wrong.data(), wrong.size(), error));
  assert(image.open(media::kExampleBmp, sizeof media::kExampleBmp, error));

  display::Framebuffer canvas(std::malloc, std::free);
  assert(canvas.configure(4, 4));
  media::ImageRenderOptions options;
  options.width = 4; options.height = 4; options.background = 99;
  CoordinateImage context;
  media::ImageSource coordinates;
  coordinates.context = &context; coordinates.width = 4; coordinates.height = 2;
  coordinates.sample = CoordinateImage::sample;
  assert(media::renderImage(coordinates, canvas, options, error));
  assert(canvas.backPixel(0, 0) == 99 && canvas.backPixel(0, 1) == 1 && canvas.backPixel(3, 2) == 8 &&
         canvas.backPixel(0, 3) == 99 && canvas.front()[0] == 0);
  options.mode = media::ScaleMode::Fill;
  assert(media::renderImage(coordinates, canvas, options, error));
  assert(canvas.backPixel(0, 0) == 2 && canvas.backPixel(3, 3) == 7);
  options.mode = media::ScaleMode::Stretch;
  assert(media::renderImage(coordinates, canvas, options, error));
  assert(canvas.backPixel(0, 0) == 1 && canvas.backPixel(3, 3) == 8);
  options.mode = media::ScaleMode::Center;
  assert(media::renderImage(coordinates, canvas, options, error));
  assert(canvas.backPixel(0, 0) == 99 && canvas.backPixel(0, 1) == 1 && canvas.backPixel(3, 2) == 8);
  options.mode = media::ScaleMode::Crop; options.cropX = 1;
  assert(media::renderImage(coordinates, canvas, options, error));
  assert(canvas.backPixel(0, 0) == 2 && canvas.backPixel(2, 1) == 8 && canvas.backPixel(3, 0) == 99);
  canvas.clear(77); options.preserveBackground = true;
  assert(media::renderImage(coordinates, canvas, options, error));
  assert(canvas.backPixel(3, 0) == 77);
  options.cropX = 4;
  const auto before = canvas.backPixel(0, 0);
  assert(!media::renderImage(coordinates, canvas, options, error) && canvas.backPixel(0, 0) == before);
  options.cropX = 0; options.mode = static_cast<media::ScaleMode>(99);
  assert(!media::renderImage(coordinates, canvas, options, error));
  options.mode = media::ScaleMode::Stretch; options.x = -1; options.y = -1;
  canvas.clear(77);
  assert(media::renderImage(coordinates, canvas, options, error));
  assert(canvas.backPixel(0, 0) == 2 && canvas.backPixel(2, 2) == 8 && canvas.backPixel(3, 0) == 77);
  options.x = 0; options.y = 0; options.width = 0;
  assert(!media::renderImage(coordinates, canvas, options, error));
  options.width = 4;
  auto badSource = coordinates; badSource.sample = nullptr;
  assert(!media::renderImage(badSource, canvas, options, error));
  assert(canvas.configure(1, 1)); options.width = 1; options.height = 1;
  for (int mode = 0; mode < 5; ++mode) {
    options.mode = static_cast<media::ScaleMode>(mode);
    assert(media::renderImage(coordinates, canvas, options, error));
  }
  assert(canvas.configure(64, 64)); options.width = 64; options.height = 64;
  options.x = 0; options.y = 0; options.background = 0; options.preserveBackground = false;
  for (int mode = 0; mode < 5; ++mode) {
    options.mode = static_cast<media::ScaleMode>(mode);
    assert(media::renderImage(image.source(), canvas, options, error) && canvas.present());
    preview(canvas, media::scaleModeName(options.mode));
  }
  std::cout << "PASS: BMP validation, padding/orientation, RGB565, scaler modes, clipping and background\n";
}
