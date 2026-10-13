#include "display/builtin_profiles.h"
#include "display/hub75_mapping.h"
#include "board/button.h"
#include "display/diagnostics.h"
#include "display/panel_profile.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include "test_assert.h"

namespace {
int allocationCount = 0, releaseCount = 0, failAllocation = -1;
void* allocate(size_t bytes) {
  ++allocationCount;
  return allocationCount == failAllocation ? nullptr : std::malloc(bytes);
}
void release(void* value) { ++releaseCount; std::free(value); }

std::string replaceOnce(std::string value, const std::string& from, const std::string& to) {
  const size_t at = value.find(from);
  assert(at != std::string::npos);
  value.replace(at, from.size(), to);
  return value;
}

void expectRejected(const std::string& json, display::PanelProfile& profile) {
  const display::PanelProfile old = profile;
  const char* error = nullptr;
  assert(!display::parseProfile(json.data(), json.size(), profile, error));
  assert(error != nullptr);
  assert(profile.width == old.width && profile.height == old.height &&
         std::strcmp(profile.id, old.id) == 0 && profile.rgbOrder == old.rgbOrder);
}

void testProfiles() {
  display::PanelProfile profile;
  const char* error = nullptr;
  const std::string source = display::kDevelopmentProfileJson;
  assert(display::parseProfile(source.data(), source.size(), profile, error));
  assert(profile.width == 64 && profile.height == 32 && profile.scan == 16);
  assert(profile.pitch == 2.5f && error == nullptr);
  expectRejected(replaceOnce(source, "\"schema_version\": 1", "\"schema_version\": 2"), profile);
  expectRejected(replaceOnce(source, "\"width\": 64", "\"width\": -1"), profile);
  expectRejected(replaceOnce(source, "\"width\": 64", "\"width\": \"64\""), profile);
  expectRejected(replaceOnce(source, "\"width\": 64", "\"width\": 129"), profile);
  expectRejected(replaceOnce(source, "\"scan\": 16", "\"scan\": 32"), profile);
  expectRejected(replaceOnce(source, "\"height\": 32", "\"height\": 33"), profile);
  expectRejected(replaceOnce(source, "GENERIC", "ICN2053"), profile);
  expectRejected(replaceOnce(source, "STANDARD", "CUSTOM"), profile);
  expectRejected(replaceOnce(source, "SAFE", "FAST"), profile);
  expectRejected(replaceOnce(source, "p25_64x32_scan16", "../profile"), profile);
  expectRejected(replaceOnce(source, "\"pitch\": 2.5", "\"pitch\": -2.5"), profile);
  expectRejected(replaceOnce(source, "\"width\"", "\"widht\""), profile);
  expectRejected(source + " garbage", profile);
  expectRejected(source.substr(0, source.size() - 2), profile);
  expectRejected("[]", profile);
  expectRejected(std::string(2049, ' '), profile);

  const std::string full = replaceOnce(source, "\"pitch\": 2.5,",
      "\"pitch\": 2.5, \"manufacturer\": \"Example\", \"model\": \"Sample\","
      "\"description\": \"Untested\", \"physical_width_mm\": 160, \"physical_height_mm\": 80,");
  assert(display::parseProfile(full.data(), full.size(), profile, error));
  char exported[2049];
  const size_t bytes = display::exportProfile(profile, exported, sizeof exported);
  assert(bytes > 0 && bytes == std::strlen(exported));
  display::PanelProfile restored;
  assert(display::parseProfile(exported, bytes, restored, error));
  assert(restored.physicalWidthMm == 160 && restored.physicalHeightMm == 80);
  assert(std::strcmp(restored.manufacturer, "Example") == 0);
  assert(std::strcmp(restored.description, "Untested") == 0);
  char small[2] = {'x', 'x'};
  assert(display::exportProfile(profile, small, sizeof small) == bytes && small[0] == 0);
  assert(display::exportProfile(profile, nullptr, 0) == bytes);

  const uint16_t red[] = {0xF800, 0xF800, 0x07E0, 0x001F, 0x07E0, 0x001F};
  for (int i = 0; i < 6; ++i) {
    assert(display::mapRgb565(0xF800, static_cast<display::RgbOrder>(i)) == red[i]);
    assert(display::mapRgb565(0xFFFF, static_cast<display::RgbOrder>(i)) == 0xFFFF);
    assert(display::mapRgb565(0, static_cast<display::RgbOrder>(i)) == 0);
  }
  for (unsigned color = 0; color <= 0xFFFF; ++color)
    assert(display::mapRgb565(static_cast<uint16_t>(color), display::RgbOrder::RGB) == color);
  std::cout << "PASS: profile validation, rejected-input preservation, export round-trip, RGB orders\n";
}

void testBuffers() {
  const int allocationsBefore = allocationCount, releasesBefore = releaseCount;
  {
    display::Framebuffer canvas(allocate, release);
    assert(!canvas.present() && !canvas.setPixel(0, 0, 1));
    assert(!canvas.configure(0, 32) && !canvas.configure(129, 32));
    assert(canvas.configure(64, 32) && canvas.bufferBytes() == 4096);
    assert(canvas.front()[0] == 0);
    canvas.clear(0x001F);
    assert(canvas.front()[0] == 0 && canvas.backPixel(0, 0) == 0x001F);
    assert(canvas.setPixel(63, 31, 0xF800));
    assert(!canvas.setPixel(-1, 0, 1) && !canvas.setPixel(64, 0, 1) && !canvas.setPixel(0, 32, 1));
    assert(canvas.present() && canvas.generation() == 1);
    const auto* active = canvas.front();
    assert(active[0] == 0x001F && active[2047] == 0xF800);
    canvas.clear(0x07E0);
    assert(active[0] == 0x001F);
    failAllocation = allocationCount + 1;
    assert(!canvas.configure(128, 64));
    assert(canvas.front() == active && canvas.width() == 64 && canvas.generation() == 1);
    failAllocation = allocationCount + 2;
    const int released = releaseCount;
    assert(!canvas.configure(128, 64) && releaseCount == released + 1);
    assert(canvas.front() == active && canvas.height() == 32);
    failAllocation = -1;
    assert(canvas.configure(128, 64) && canvas.bufferBytes() == 16384);
    assert(canvas.generation() == 0 && canvas.front()[8191] == 0);
  }
  // Two intentionally failed allocations have no storage to release.
  assert(allocationCount - allocationsBefore - 2 == releaseCount - releasesBefore);
  std::cout << "PASS: clipping, buffer isolation, swap, allocation rollback, release ownership\n";
}

void preview(const display::Framebuffer& canvas, int index) {
  std::ofstream file(".build-temp/previews/pattern_" + std::to_string(index) + ".ppm", std::ios::binary);
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

void testDiagnostics() {
  display::Framebuffer canvas(allocate, release);
  assert(!display::drawDiagnostic(canvas, display::Pattern::Red));
  assert(canvas.configure(64, 32));
  for (int index = 0; index < 9; ++index) {
    const auto pattern = static_cast<display::Pattern>(index);
    assert(display::drawDiagnostic(canvas, pattern, 7));
    assert(canvas.present());
    preview(canvas, index);
  }
  assert(display::drawDiagnostic(canvas, display::Pattern::RgbBars));
  assert(canvas.backPixel(0, 0) == 0xF800 && canvas.backPixel(32, 0) == 0x07E0 &&
         canvas.backPixel(63, 0) == 0x001F);
  assert(display::drawDiagnostic(canvas, display::Pattern::Rows, 33));
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x) assert(canvas.backPixel(x, y) == (y == 1 ? 0xFFFF : 0));
  assert(display::drawDiagnostic(canvas, display::Pattern::Columns, 65));
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x) assert(canvas.backPixel(x, y) == (x == 1 ? 0xFFFF : 0));
  assert(display::drawDiagnostic(canvas, display::Pattern::PixelWalker, 2050));
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x) assert(canvas.backPixel(x, y) == (y == 0 && x == 2 ? 0xFFFF : 0));
  assert(display::drawDiagnostic(canvas, display::Pattern::Checkerboard, 1));
  assert(canvas.backPixel(0, 0) == 0xFFFF && canvas.backPixel(1, 0) == 0 &&
         canvas.backPixel(0, 1) == 0);
  assert(canvas.configure(1, 1));
  assert(display::drawDiagnostic(canvas, display::Pattern::Gradient));
  assert(canvas.backPixel(0, 0) == 0);
  assert(!display::drawDiagnostic(canvas, static_cast<display::Pattern>(99)));
  std::cout << "PASS: diagnostic coordinates, wraparound, tiny canvas, 9 PPM previews\n";
}
}

void testLayouts();
void testMedia();
void testJpeg();
void testFileImages();
void testAnimation();
int main() {
  testProfiles(); testBuffers(); testDiagnostics();
  board::DebouncedButton button;
  button.begin(false,0);
  assert(!button.update(true,10));
  assert(!button.update(false,20));
  assert(!button.update(true,25));
  assert(!button.update(true,64));
  assert(button.update(true,65));
  assert(!button.update(true,1000));
  assert(!button.update(false,1001));
  assert(!button.update(false,1041));
  assert(!button.update(true,1050));
  assert(button.update(true,1090));
  button.begin(true,0); // Held at boot cannot confirm by itself.
  assert(!button.update(true,100));
  assert(!button.update(false,110));
  assert(!button.update(false,150));
  assert(!button.update(true,160));
  assert(button.update(true,200));
  button.begin(false,0xfffffff0U);
  assert(!button.update(true,0xfffffff8U));
  assert(button.update(true,0x20U));
  std::cout << "PASS: GPIO button debounce, held-at-boot, one press event, millis wrap\n";
  display::PanelProfile hardware;
  const char* error = nullptr;
  assert(display::parseProfile(display::kHardwareProfileJson, sizeof(display::kHardwareProfileJson)-1, hardware, error));
  assert(hardware.width == 128 && hardware.height == 64 && hardware.scan == 32 && hardware.addressLines == 5 && hardware.hub75e);
  for (unsigned mode=0; mode<4; ++mode) {
    bool seen[8192] = {};
    for (int y=0; y<64; ++y) for (int x=0; x<128; ++x) {
      uint16_t dx, dy;
      assert(display::hub75Coordinates(x, y, mode, dx, dy));
      assert(dx<256 && dy<32);
      assert(!seen[dy*256+dx]); seen[dy*256+dx]=true;
    }
    for (bool pixel : seen) assert(pixel);
  }
  for (unsigned scan : {4U,8U,16U,32U}) {
    bool seen[8192] = {};
    for (int y=0;y<64;++y) for (int x=0;x<128;++x) {
      uint16_t dx,dy;
      assert(display::hub75BankCoordinates(x,y,scan,dx,dy));
      const unsigned width=128*(32/scan);
      assert(dx<width && dy<2*scan);
      assert(!seen[dy*width+dx]); seen[dy*width+dx]=true;
    }
    for (bool pixel : seen) assert(pixel);
  }
  assert(display::validHub75Settings(4,5) && display::validHub75Settings(8,5));
  assert(!display::validHub75Settings(4,0) && !display::validHub75Settings(32,5));
  uint16_t dx, dy;
  assert(!display::hub75Coordinates(-1, 0, 0, dx, dy));
  assert(!display::hub75Coordinates(128, 0, 0, dx, dy));
  assert(!display::hub75Coordinates(0, 64, 0, dx, dy));
  assert(!display::hub75Coordinates(0, 0, 4, dx, dy));
  std::cout << "PASS: HUB75 four mapping modes cover all 8192 electrical pixels without overlap\n";
  testLayouts();
  testMedia();
  testJpeg();
  testFileImages();
  testAnimation();
  std::cout << "All software tests passed. Hardware output not tested.\n";
}
