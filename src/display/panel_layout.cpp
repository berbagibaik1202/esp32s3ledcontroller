#include "panel_layout.h"
#include "json_input.h"
#include <cstring>

namespace display {
namespace {
bool fail(const char*& error, const char* message) { error = message; return false; }
uint16_t placedWidth(const PanelPlacement& p, const PanelProfile& profile) {
  return p.rotation == 90 || p.rotation == 270 ? profile.height : profile.width;
}
uint16_t placedHeight(const PanelPlacement& p, const PanelProfile& profile) {
  return p.rotation == 90 || p.rotation == 270 ? profile.width : profile.height;
}
bool knownKey(const char* key, bool placement) {
  const char* root[] = {"schema_version", "panel_profile_id", "canvas_width", "canvas_height", "panels"};
  const char* panel[] = {"chain_index", "x", "y", "rotation"};
  const size_t count = placement ? 4 : 5;
  for (size_t i = 0; i < count; ++i)
    if (std::strcmp(key, placement ? panel[i] : root[i]) == 0) return true;
  return false;
}
}

bool validateLayout(const PanelLayout& layout, const PanelProfile& profile, const char*& error) {
  if (!validateProfile(profile, error)) return false;
  if (layout.schemaVersion != 1) return fail(error, "Unsupported layout schema_version");
  if (!std::memchr(layout.panelProfileId, 0, sizeof layout.panelProfileId) ||
      std::strcmp(layout.panelProfileId, profile.id) != 0)
    return fail(error, "Layout references a different profile");
  if (!layout.canvasWidth || !layout.canvasHeight || layout.canvasWidth > 128 || layout.canvasHeight > 64)
    return fail(error, "Canvas exceeds current 128x64 limit");
  if (!layout.panelCount || layout.panelCount > kMaxPanels)
    return fail(error, "Layout must contain 1..4 panels");
  unsigned seen = 0;
  for (size_t i = 0; i < layout.panelCount; ++i) {
    const auto& p = layout.panels[i];
    if (p.chainIndex >= layout.panelCount || (seen & (1U << p.chainIndex)))
      return fail(error, "Chain indices must be unique and contiguous from zero");
    seen |= 1U << p.chainIndex;
    if (p.rotation != 0 && p.rotation != 90 && p.rotation != 180 && p.rotation != 270)
      return fail(error, "Rotation must be 0/90/180/270 degrees clockwise");
    const uint32_t right = static_cast<uint32_t>(p.x) + placedWidth(p, profile);
    const uint32_t bottom = static_cast<uint32_t>(p.y) + placedHeight(p, profile);
    if (right > layout.canvasWidth || bottom > layout.canvasHeight)
      return fail(error, "Panel extends outside canvas");
    for (size_t j = 0; j < i; ++j) {
      const auto& previous = layout.panels[j];
      if (p.x < static_cast<uint32_t>(previous.x) + placedWidth(previous, profile) &&
          right > previous.x && p.y < static_cast<uint32_t>(previous.y) + placedHeight(previous, profile) &&
          bottom > previous.y) return fail(error, "Panel areas overlap");
    }
  }
  error = nullptr;
  return true;
}

bool makeGridLayout(const PanelProfile& profile, uint8_t columns, uint8_t rows,
                    bool serpentine, PanelLayout& output, const char*& error) {
  if (!validateProfile(profile, error)) return false;
  const unsigned count = static_cast<unsigned>(columns) * rows;
  if (!count || count > kMaxPanels || static_cast<unsigned>(columns) * profile.width > 128 ||
      static_cast<unsigned>(rows) * profile.height > 64)
    return fail(error, "Grid exceeds panel count or canvas limit");
  PanelLayout candidate;
  std::memcpy(candidate.panelProfileId, profile.id, sizeof candidate.panelProfileId);
  candidate.canvasWidth = static_cast<uint16_t>(columns * profile.width);
  candidate.canvasHeight = static_cast<uint16_t>(rows * profile.height);
  candidate.panelCount = static_cast<uint8_t>(count);
  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned column = 0; column < columns; ++column) {
      auto& p = candidate.panels[row * columns + column];
      p.chainIndex = static_cast<uint8_t>(row * columns +
          (serpentine && row % 2 ? columns - 1 - column : column));
      p.x = static_cast<uint16_t>(column * profile.width);
      p.y = static_cast<uint16_t>(row * profile.height);
    }
  }
  if (!validateLayout(candidate, profile, error)) return false;
  output = candidate;
  return true;
}

bool parseLayout(const char* json, size_t length, const PanelProfile& profile,
                 PanelLayout& output, const char*& error) {
  error = nullptr;
  JsonDocument doc;
  if (!readJsonObject(json, length, 2048, doc)) return fail(error, "Invalid layout JSON (limit 2048 bytes)");
  for (JsonPairConst pair : doc.as<JsonObjectConst>())
    if (!knownKey(pair.key().c_str(), false)) return fail(error, "Unknown layout field");
  if (!doc["schema_version"].is<uint16_t>() || !doc["canvas_width"].is<uint16_t>() ||
      !doc["canvas_height"].is<uint16_t>() || !doc["panel_profile_id"].is<const char*>() ||
      !doc["panels"].is<JsonArray>()) return fail(error, "Missing or invalid layout field");
  PanelLayout candidate;
  candidate.schemaVersion = doc["schema_version"].as<uint16_t>();
  candidate.canvasWidth = doc["canvas_width"].as<uint16_t>();
  candidate.canvasHeight = doc["canvas_height"].as<uint16_t>();
  const JsonString id = doc["panel_profile_id"].as<JsonString>();
  if (!id.size() || id.size() >= sizeof candidate.panelProfileId || std::strlen(id.c_str()) != id.size())
    return fail(error, "Invalid panel_profile_id");
  std::memcpy(candidate.panelProfileId, id.c_str(), id.size() + 1);
  const JsonArrayConst panels = doc["panels"].as<JsonArrayConst>();
  if (!panels.size() || panels.size() > kMaxPanels) return fail(error, "Layout must contain 1..4 panels");
  candidate.panelCount = static_cast<uint8_t>(panels.size());
  size_t index = 0;
  for (JsonVariantConst entry : panels) {
    if (!entry.is<JsonObjectConst>()) return fail(error, "Panel placement must be an object");
    for (JsonPairConst pair : entry.as<JsonObjectConst>())
      if (!knownKey(pair.key().c_str(), true)) return fail(error, "Unknown placement field");
    if (!entry["chain_index"].is<uint8_t>() || !entry["x"].is<uint16_t>() ||
        !entry["y"].is<uint16_t>() || !entry["rotation"].is<uint16_t>())
      return fail(error, "Missing or invalid placement field");
    auto& p = candidate.panels[index++];
    p.chainIndex = entry["chain_index"].as<uint8_t>();
    p.x = entry["x"].as<uint16_t>(); p.y = entry["y"].as<uint16_t>();
    p.rotation = entry["rotation"].as<uint16_t>();
  }
  if (!validateLayout(candidate, profile, error)) return false;
  output = candidate;
  return true;
}

size_t exportLayout(const PanelLayout& layout, const PanelProfile& profile, char* output, size_t capacity) {
  if (output && capacity) output[0] = 0;
  const char* error = nullptr;
  if (!validateLayout(layout, profile, error)) return 0;
  JsonDocument doc;
  doc["schema_version"] = layout.schemaVersion; doc["panel_profile_id"] = layout.panelProfileId;
  doc["canvas_width"] = layout.canvasWidth; doc["canvas_height"] = layout.canvasHeight;
  auto panels = doc["panels"].to<JsonArray>();
  for (size_t i = 0; i < layout.panelCount; ++i) {
    auto p = panels.add<JsonObject>();
    p["chain_index"] = layout.panels[i].chainIndex;
    p["x"] = layout.panels[i].x; p["y"] = layout.panels[i].y;
    p["rotation"] = layout.panels[i].rotation;
  }
  if (doc.overflowed()) return 0;
  const size_t required = measureJson(doc);
  if (output && capacity > required) serializeJson(doc, output, capacity);
  return required;
}

bool LayoutMapper::configure(const PanelProfile& profile, const PanelLayout& layout, const char*& error) {
  if (!validateLayout(layout, profile, error)) return false;
  profile_ = profile; layout_ = layout; ready_ = true; return true;
}

bool LayoutMapper::canvasToPanel(int x, int y, PanelPixel& output) const {
  if (!ready_ || x < 0 || y < 0 || x >= layout_.canvasWidth || y >= layout_.canvasHeight) return false;
  for (size_t i = 0; i < layout_.panelCount; ++i) {
    const auto& p = layout_.panels[i];
    const int dx = x - p.x, dy = y - p.y;
    if (dx < 0 || dy < 0 || dx >= placedWidth(p, profile_) || dy >= placedHeight(p, profile_)) continue;
    PanelPixel pixel;
    pixel.chainIndex = p.chainIndex;
    switch (p.rotation) {
      case 0: pixel.x = static_cast<uint16_t>(dx); pixel.y = static_cast<uint16_t>(dy); break;
      case 90: pixel.x = static_cast<uint16_t>(dy); pixel.y = static_cast<uint16_t>(profile_.height - 1 - dx); break;
      case 180: pixel.x = static_cast<uint16_t>(profile_.width - 1 - dx); pixel.y = static_cast<uint16_t>(profile_.height - 1 - dy); break;
      case 270: pixel.x = static_cast<uint16_t>(profile_.width - 1 - dy); pixel.y = static_cast<uint16_t>(dx); break;
    }
    output = pixel; return true;
  }
  return false;  // Gap in a custom layout; no physical pixel at this location.
}

bool LayoutMapper::panelToCanvas(const PanelPixel& pixel, uint16_t& x, uint16_t& y) const {
  if (!ready_ || pixel.x >= profile_.width || pixel.y >= profile_.height) return false;
  for (size_t i = 0; i < layout_.panelCount; ++i) {
    const auto& p = layout_.panels[i];
    if (p.chainIndex != pixel.chainIndex) continue;
    uint16_t dx = 0, dy = 0;
    switch (p.rotation) {
      case 0: dx = pixel.x; dy = pixel.y; break;
      case 90: dx = profile_.height - 1 - pixel.y; dy = pixel.x; break;
      case 180: dx = profile_.width - 1 - pixel.x; dy = profile_.height - 1 - pixel.y; break;
      case 270: dx = pixel.y; dy = profile_.width - 1 - pixel.x; break;
    }
    x = p.x + dx; y = p.y + dy; return true;
  }
  return false;
}

size_t LayoutMapper::chainPixels() const {
  return ready_ ? static_cast<size_t>(profile_.width) * profile_.height * layout_.panelCount : 0;
}

bool LayoutMapper::copyCanvasToChain(const uint16_t* canvas, size_t canvasPixels,
                                    uint16_t* chain, size_t chainCapacity) const {
  const size_t neededCanvas = static_cast<size_t>(layout_.canvasWidth) * layout_.canvasHeight;
  const size_t neededChain = chainPixels();
  if (!ready_ || !canvas || !chain || canvasPixels < neededCanvas || chainCapacity < neededChain) return false;
  const uintptr_t source = reinterpret_cast<uintptr_t>(canvas), target = reinterpret_cast<uintptr_t>(chain);
  if (source <= target ? target - source < neededCanvas * sizeof(uint16_t)
                       : source - target < neededChain * sizeof(uint16_t)) return false;
  const size_t modulePixels = static_cast<size_t>(profile_.width) * profile_.height;
  for (uint8_t index = 0; index < layout_.panelCount; ++index) {
    for (uint16_t py = 0; py < profile_.height; ++py) {
      for (uint16_t px = 0; px < profile_.width; ++px) {
        PanelPixel pixel; pixel.chainIndex = index; pixel.x = px; pixel.y = py;
        uint16_t x = 0, y = 0;
        if (!panelToCanvas(pixel, x, y)) return false;
        chain[index * modulePixels + static_cast<size_t>(py) * profile_.width + px] =
            mapRgb565(canvas[static_cast<size_t>(y) * layout_.canvasWidth + x], profile_.rgbOrder);
      }
    }
  }
  return true;
}
}  // namespace display
