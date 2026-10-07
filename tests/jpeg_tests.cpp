#include "media/jpeg.h"
#include "media/builtin_jpeg.h"
#include "media/image_renderer.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include "test_assert.h"

namespace {
unsigned allocations = 0, releases = 0, failedAllocations = 0, failAt = 0;
void* allocate(size_t bytes) {
  ++allocations;
  if (allocations == failAt) { ++failedAllocations; return nullptr; }
  return std::malloc(bytes);
}
void release(void* p) { ++releases; std::free(p); }
std::vector<uint8_t> load(const char* path) {
  std::ifstream file(path, std::ios::binary); assert(file);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}
void nearColor(uint16_t color, int red, int green, int blue) {
  assert(std::abs(static_cast<int>(((color >> 11) & 31) * 255 / 31) - red) <= 12);
  assert(std::abs(static_cast<int>(((color >> 5) & 63) * 255 / 63) - green) <= 12);
  assert(std::abs(static_cast<int>((color & 31) * 255 / 31) - blue) <= 12);
}
void rejected(const std::vector<uint8_t>& bytes, media::JpegImage& image) {
  const auto width = image.width(), height = image.height(), first = image.pixel(0, 0);
  const char* error = nullptr;
  const bool accepted = image.open(bytes.data(), bytes.size(), error);
  if (accepted) std::fprintf(stderr, "Unexpected accepted JPEG: bytes=%zu allocations=%u failAt=%u\n", bytes.size(), allocations, failAt);
  assert(!accepted && error);
  assert(image.width() == width && image.height() == height && image.pixel(0, 0) == first);
}
}

void testJpeg() {
  const auto baseline = load("examples/media/test_bars.jpg");
  assert(baseline.size() == sizeof media::kExampleJpeg);
  for (size_t i = 0; i < baseline.size(); ++i) assert(baseline[i] == media::kExampleJpeg[i]);
  {
    media::JpegImage image(allocate, release);
    const char* error = nullptr;
    assert(!image.source().sample && image.pixel(0, 0) == 0);
    assert(image.open(baseline.data(), baseline.size(), error));
    assert(image.width() == 32 && image.height() == 16);
    nearColor(image.pixel(3, 5), 255, 0, 0);
    nearColor(image.pixel(11, 5), 0, 255, 0);
    nearColor(image.pixel(19, 5), 0, 0, 255);
    nearColor(image.pixel(27, 5), 255, 255, 255);
    assert(image.pixel(32, 0) == 0 && image.pixel(0, 16) == 0);
    for (size_t length = 0; length < baseline.size(); ++length) {
      auto truncated = baseline; truncated.resize(length); rejected(truncated, image);
    }
    rejected(load("examples/media/test_progressive.jpg"), image);
    rejected(load("examples/media/test_oversize.jpg"), image);
    auto invalid = baseline; invalid.push_back(0); rejected(invalid, image);
    invalid = baseline; invalid[0] = 0; rejected(invalid, image);
    invalid = baseline; invalid[4] = 0xFF; invalid[5] = 0xFF; rejected(invalid, image);
    invalid = baseline; invalid.erase(invalid.end() - 65, invalid.end() - 2); rejected(invalid, image);
    failAt = allocations + 1; rejected(baseline, image);
    failAt = allocations + 2; rejected(baseline, image);
    failAt = 0;
    const auto gray = load("examples/media/test_gray.jpg");
    assert(image.open(gray.data(), gray.size(), error));
    assert(image.width() == 7 && image.height() == 5);
    nearColor(image.pixel(6, 4), 128, 128, 128);
    auto odd = load("examples/media/test_420.jpg");
    assert(image.open(odd.data(), odd.size(), error));
    assert(image.width() == 17 && image.height() == 9);
    nearColor(image.pixel(16, 8), 40, 160, 220);
    odd.assign(odd.size(), 0);
    nearColor(image.pixel(16, 8), 40, 160, 220);
    const auto subsampled = load("examples/media/test_422.jpg");
    assert(image.open(subsampled.data(), subsampled.size(), error));
    nearColor(image.pixel(16, 8), 40, 160, 220);
    const auto maximum = load("examples/media/test_max.jpg");
    assert(maximum.size() > 4096);
    assert(image.open(maximum.data(), maximum.size(), error));
    assert(image.width() == 128 && image.height() == 64);
    invalid = maximum; invalid.erase(invalid.end() - 1000, invalid.end() - 2);
    rejected(invalid, image);
    assert(image.open(media::kExampleJpeg, sizeof media::kExampleJpeg, error));
    display::Framebuffer canvas(std::malloc, std::free);
    assert(canvas.configure(64, 32));
    media::ImageRenderOptions options; options.width = 64; options.height = 32;
    assert(media::renderImage(image.source(), canvas, options, error));
    assert(canvas.front()[0] == 0 && canvas.present());
    for (int mode = 0; mode < 5; ++mode) {
      options.mode = static_cast<media::ScaleMode>(mode);
      assert(media::renderImage(image.source(), canvas, options, error) && canvas.present());
    }
    assert(media::JpegImage::decoderWorkspaceBytes() > 0);
  }
  assert(allocations - failedAllocations == releases);
  std::cout << "PASS: baseline JPEG, grayscale/4:2:0 edges, truncation/progressive rejection, allocation rollback and owned pixels\n";
}
