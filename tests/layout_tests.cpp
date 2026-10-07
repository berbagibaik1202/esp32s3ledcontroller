#include "display/builtin_profiles.h"
#include "display/panel_layout.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "test_assert.h"

namespace {
void checkBijection(const display::PanelProfile& profile, const display::PanelLayout& layout) {
  display::LayoutMapper mapper;
  const char* error = nullptr;
  assert(mapper.configure(profile, layout, error));
  std::vector<bool> visited(mapper.chainPixels(), false);
  size_t count = 0;
  for (int y = 0; y < layout.canvasHeight; ++y) {
    for (int x = 0; x < layout.canvasWidth; ++x) {
      display::PanelPixel pixel;
      if (!mapper.canvasToPanel(x, y, pixel)) continue;
      const size_t slot = (static_cast<size_t>(pixel.chainIndex) * profile.height + pixel.y) * profile.width + pixel.x;
      assert(slot < visited.size() && !visited[slot]); visited[slot] = true; ++count;
      uint16_t restoredX = 0, restoredY = 0;
      assert(mapper.panelToCanvas(pixel, restoredX, restoredY));
      assert(restoredX == x && restoredY == y);
    }
  }
  assert(count == mapper.chainPixels());
  for (bool seen : visited) assert(seen);
  display::PanelPixel invalid;
  assert(!mapper.canvasToPanel(-1, 0, invalid));
  assert(!mapper.canvasToPanel(layout.canvasWidth, 0, invalid));
  assert(!mapper.canvasToPanel(0, layout.canvasHeight, invalid));
  uint16_t x = 123, y = 456;
  invalid.chainIndex = layout.panelCount;
  assert(!mapper.panelToCanvas(invalid, x, y) && x == 123 && y == 456);
  invalid.chainIndex = 0; invalid.x = profile.width;
  assert(!mapper.panelToCanvas(invalid, x, y));
}
}

void testLayouts() {
  display::PanelProfile profile;
  const char* error = nullptr;
  assert(display::parseProfile(display::kDevelopmentProfileJson,
      sizeof(display::kDevelopmentProfileJson) - 1, profile, error));
  display::PanelLayout layout;
  display::LayoutMapper mapper;
  display::PanelPixel pixel;
  assert(mapper.chainPixels() == 0 && !mapper.canvasToPanel(0, 0, pixel));
  assert(display::makeGridLayout(profile, 2, 1, false, layout, error));
  assert(layout.canvasWidth == 128 && layout.canvasHeight == 32);
  checkBijection(profile, layout);
  assert(display::makeGridLayout(profile, 1, 2, false, layout, error));
  assert(layout.canvasWidth == 64 && layout.canvasHeight == 64);
  checkBijection(profile, layout);
  assert(display::makeGridLayout(profile, 2, 2, false, layout, error));
  assert(mapper.configure(profile, layout, error));
  assert(mapper.canvasToPanel(0, 32, pixel) && pixel.chainIndex == 2);
  checkBijection(profile, layout);
  assert(display::makeGridLayout(profile, 2, 2, true, layout, error));
  assert(mapper.configure(profile, layout, error));
  assert(mapper.canvasToPanel(0, 32, pixel) && pixel.chainIndex == 3 && pixel.x == 0 && pixel.y == 0);
  assert(mapper.canvasToPanel(127, 63, pixel) && pixel.chainIndex == 2 && pixel.x == 63 && pixel.y == 31);
  checkBijection(profile, layout);

  const auto good = layout;
  auto shuffled = good;
  std::swap(shuffled.panels[0], shuffled.panels[3]);
  checkBijection(profile, shuffled);
  auto bad = good;
  bad.panels[0].chainIndex = 1;
  assert(!mapper.configure(profile, bad, error));
  assert(mapper.canvasToPanel(0, 32, pixel) && pixel.chainIndex == 3);
  bad = good; bad.panels[0].chainIndex = 4; assert(!display::validateLayout(bad, profile, error));
  bad = good; bad.panels[1].x = 63; assert(!display::validateLayout(bad, profile, error));
  bad = good; bad.panels[0].x = 65535; assert(!display::validateLayout(bad, profile, error));
  bad = good; bad.panels[0].rotation = 45; assert(!display::validateLayout(bad, profile, error));
  bad = good; bad.panelCount = 0; assert(!display::validateLayout(bad, profile, error));
  bad = good; bad.panelCount = 5; assert(!display::validateLayout(bad, profile, error));
  bad = good; bad.schemaVersion = 2; assert(!display::validateLayout(bad, profile, error));
  bad = good; bad.canvasWidth = 129; assert(!display::validateLayout(bad, profile, error));
  bad = good; std::strcpy(bad.panelProfileId, "other"); assert(!display::validateLayout(bad, profile, error));
  assert(!display::makeGridLayout(profile, 3, 1, false, layout, error));
  assert(layout.canvasWidth == good.canvasWidth && layout.panels[2].chainIndex == 3);

  char json[2049];
  const size_t length = display::exportLayout(good, profile, json, sizeof json);
  assert(length && length == std::strlen(json));
  display::PanelLayout restored;
  assert(display::parseLayout(json, length, profile, restored, error));
  checkBijection(profile, restored);
  std::ifstream fixture("examples/layouts/serpentine_2x2.json");
  assert(fixture);
  const std::string fixtureJson((std::istreambuf_iterator<char>(fixture)), std::istreambuf_iterator<char>());
  assert(display::parseLayout(fixtureJson.data(), fixtureJson.size(), profile, restored, error));
  checkBijection(profile, restored);
  char tiny[2] = {'x', 'x'};
  assert(display::exportLayout(good, profile, tiny, sizeof tiny) == length && tiny[0] == 0);
  const std::string input(json);
  std::vector<std::string> rejected = {"[]", input + "{}", input.substr(0, length - 2),
      "{\"schema_version\":1}", std::string(2049, ' ')};
  auto replaced = [&](const std::string& from, const std::string& to) {
    std::string value = input;
    const size_t index = value.find(from); assert(index != std::string::npos);
    value.replace(index, from.size(), to); rejected.push_back(value);
  };
  replaced("\"rotation\":0", "\"rotation\":90");  // Rotated area no longer fits.
  replaced("\"x\":0", "\"x\":-1");
  replaced("\"x\":0", "\"x\":\"0\"");
  replaced("\"rotation\":0", "\"rotation\":0,\"extra\":1");
  replaced("\"schema_version\":1", "\"schema_version\":1,\"extra\":1");
  for (const auto& invalid : rejected) {
    assert(!display::parseLayout(invalid.data(), invalid.size(), profile, restored, error));
    assert(restored.canvasWidth == 128 && restored.panelCount == 4 && restored.panels[2].chainIndex == 3);
  }

  // Explicit corners verify clockwise rotation independently of round-trip.
  for (uint16_t rotation : {0, 90, 180, 270}) {
    assert(display::makeGridLayout(profile, 1, 1, false, layout, error));
    layout.panels[0].rotation = rotation;
    layout.canvasWidth = rotation == 90 || rotation == 270 ? 32 : 64;
    layout.canvasHeight = rotation == 90 || rotation == 270 ? 64 : 32;
    assert(mapper.configure(profile, layout, error));
    display::PanelPixel origin;
    uint16_t x = 0, y = 0;
    assert(mapper.panelToCanvas(origin, x, y));
    assert(x == (rotation == 90 ? 31 : rotation == 180 ? 63 : 0));
    assert(y == (rotation == 180 ? 31 : rotation == 270 ? 63 : 0));
    checkBijection(profile, layout);
  }
  assert(display::makeGridLayout(profile, 1, 1, false, layout, error));
  layout.canvasWidth = 128; layout.canvasHeight = 64;
  layout.panels[0].x = 7; layout.panels[0].y = 3;
  assert(mapper.configure(profile, layout, error));
  assert(!mapper.canvasToPanel(0, 0, pixel));
  checkBijection(profile, layout);

  assert(display::makeGridLayout(profile, 2, 2, true, layout, error));
  assert(mapper.configure(profile, layout, error));
  std::vector<uint16_t> canvas(8192), chain(8193, 0xABCD);
  for (size_t i = 0; i < canvas.size(); ++i) canvas[i] = static_cast<uint16_t>(i);
  assert(mapper.copyCanvasToChain(canvas.data(), canvas.size(), chain.data(), chain.size()));
  assert(chain[0] == 0 && chain[2047] == 31 * 128 + 63);
  assert(chain[2048] == 64 && chain[4096] == 32 * 128 + 64 && chain[6144] == 32 * 128);
  assert(chain[8192] == 0xABCD);
  const auto old = chain;
  assert(!mapper.copyCanvasToChain(canvas.data(), 8191, chain.data(), chain.size()));
  assert(!mapper.copyCanvasToChain(canvas.data(), canvas.size(), chain.data(), 8191));
  assert(!mapper.copyCanvasToChain(nullptr, canvas.size(), chain.data(), chain.size()));
  assert(chain == old);
  assert(!mapper.copyCanvasToChain(canvas.data(), canvas.size(), canvas.data(), canvas.size()));
  assert(!mapper.copyCanvasToChain(canvas.data(), canvas.size(), canvas.data() + 1, canvas.size()));
  profile.rgbOrder = display::RgbOrder::BGR;
  assert(mapper.configure(profile, layout, error));
  canvas.assign(8192, 0xF800);
  assert(mapper.copyCanvasToChain(canvas.data(), canvas.size(), chain.data(), chain.size()));
  for (size_t i = 0; i < 8192; ++i) assert(chain[i] == 0x001F);
  std::cout << "PASS: layouts, serpentine, four rotations, holes, JSON rollback, chain transfer and RGB order\n";
}
