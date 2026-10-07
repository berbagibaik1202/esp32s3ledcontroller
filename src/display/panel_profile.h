#pragma once
#include <cstddef>
#include <cstdint>

namespace display {
enum class RgbOrder { RGB, RBG, GRB, GBR, BRG, BGR };
struct PanelProfile {
  uint16_t schemaVersion = 1;
  char id[65] = {};
  char name[129] = {};
  uint16_t width = 0, height = 0;
  uint8_t scan = 0, addressLines = 0;
  bool hub75e = false;
  RgbOrder rgbOrder = RgbOrder::RGB;
  char manufacturer[65] = {}, model[65] = {}, description[257] = {};
  float pitch = 0, physicalWidthMm = 0, physicalHeightMm = 0;
};

// Current software subset: GENERIC + STANDARD mappings + SAFE timing only.
// Successful parsing is not a claim of panel/driver compatibility.
bool validateProfile(const PanelProfile& profile, const char*& error);
bool parseProfile(const char* json, size_t length, PanelProfile& output,
                  const char*& error);
// Returns required bytes excluding NUL; insufficient capacity leaves output empty.
size_t exportProfile(const PanelProfile& profile, char* output, size_t capacity);
const char* rgbOrderName(RgbOrder order);
uint16_t mapRgb565(uint16_t color, RgbOrder order);
}  // namespace display
