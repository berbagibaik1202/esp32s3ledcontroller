#include "panel_profile.h"
#include "json_input.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstring>

namespace display {
namespace {
bool fail(const char*& error, const char* message) { error = message; return false; }

template <size_t N>
bool copyString(JsonVariantConst value, char (&target)[N], bool required) {
  if (value.isNull()) return !required;
  if (!value.is<const char*>()) return false;
  const JsonString text = value.as<JsonString>();
  if (text.size() >= N || std::strlen(text.c_str()) != text.size() ||
      (required && text.size() == 0)) return false;
  std::memcpy(target, text.c_str(), text.size() + 1);
  return true;
}

bool optionalPositive(JsonVariantConst value, float& target) {
  if (value.isNull()) return true;
  if (!value.is<float>()) return false;
  target = value.as<float>();
  return std::isfinite(target) && target > 0;
}

bool knownKey(const char* key) {
  const char* fields[] = {"schema_version", "id", "name", "width", "height",
      "interface", "scan", "address_lines", "rgb_order", "pixel_mapping",
      "row_mapping", "driver", "timing", "manufacturer", "model",
      "description", "pitch", "physical_width_mm", "physical_height_mm"};
  for (const char* field : fields) if (std::strcmp(key, field) == 0) return true;
  return false;
}

bool isText(JsonVariantConst value, const char* expected) {
  return value.is<const char*>() && std::strcmp(value.as<const char*>(), expected) == 0;
}
}

const char* rgbOrderName(RgbOrder order) {
  switch (order) {
    case RgbOrder::RGB: return "RGB";
    case RgbOrder::RBG: return "RBG";
    case RgbOrder::GRB: return "GRB";
    case RgbOrder::GBR: return "GBR";
    case RgbOrder::BRG: return "BRG";
    case RgbOrder::BGR: return "BGR";
  }
  return nullptr;
}

bool validateProfile(const PanelProfile& p, const char*& error) {
  error = nullptr;
  if (p.schemaVersion != 1) return fail(error, "Unsupported schema_version");
  if (!std::memchr(p.id, 0, sizeof p.id) || p.id[0] == 0)
    return fail(error, "Invalid profile id");
  for (const char* c = p.id; *c; ++c)
    if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
          (*c >= '0' && *c <= '9') || *c == '_' || *c == '-'))
      return fail(error, "Profile id must not contain paths or special characters");
  if (!std::memchr(p.name, 0, sizeof p.name) || p.name[0] == 0 ||
      !std::memchr(p.manufacturer, 0, sizeof p.manufacturer) ||
      !std::memchr(p.model, 0, sizeof p.model) ||
      !std::memchr(p.description, 0, sizeof p.description))
    return fail(error, "Invalid profile text");
  if (!p.width || !p.height || p.width > 128 || p.height > 64)
    return fail(error, "Dimensions exceed current 128x64 limit");
  const uint8_t expected = p.scan == 8 ? 3 : p.scan == 16 ? 4 : p.scan == 32 ? 5 : 0;
  if (!expected || p.addressLines != expected)
    return fail(error, "Invalid scan/address_lines combination");
  if (p.height % (2U * p.scan) != 0)
    return fail(error, "Height is incompatible with two RGB lanes and scan");
  if (!rgbOrderName(p.rgbOrder)) return fail(error, "Invalid RGB order");
  const float metadata[] = {p.pitch, p.physicalWidthMm, p.physicalHeightMm};
  for (float number : metadata)
    if (!std::isfinite(number) || number < 0) return fail(error, "Invalid physical metadata");
  return true;
}

bool parseProfile(const char* json, size_t length, PanelProfile& output, const char*& error) {
  error = nullptr;
  if (!json || length == 0 || length > 2048) return fail(error, "Profile JSON must be 1..2048 bytes");
  JsonDocument doc;
  if (!readJsonObject(json, length, 2048, doc))
    return fail(error, "Malformed JSON or insufficient parser memory");
  for (JsonPairConst pair : doc.as<JsonObjectConst>())
    if (!knownKey(pair.key().c_str())) return fail(error, "Unknown or unsupported profile field");
  PanelProfile candidate;
  if (!doc["schema_version"].is<uint16_t>() || !doc["width"].is<uint16_t>() ||
      !doc["height"].is<uint16_t>() || !doc["scan"].is<uint8_t>() ||
      !doc["address_lines"].is<uint8_t>()) return fail(error, "Missing or invalid integer field");
  candidate.schemaVersion = doc["schema_version"].as<uint16_t>();
  candidate.width = doc["width"].as<uint16_t>();
  candidate.height = doc["height"].as<uint16_t>();
  candidate.scan = doc["scan"].as<uint8_t>();
  candidate.addressLines = doc["address_lines"].as<uint8_t>();
  if (!copyString(doc["id"], candidate.id, true) ||
      !copyString(doc["name"], candidate.name, true) ||
      !copyString(doc["manufacturer"], candidate.manufacturer, false) ||
      !copyString(doc["model"], candidate.model, false) ||
      !copyString(doc["description"], candidate.description, false))
    return fail(error, "Missing, oversized, or invalid text field");
  if (!optionalPositive(doc["pitch"], candidate.pitch) ||
      !optionalPositive(doc["physical_width_mm"], candidate.physicalWidthMm) ||
      !optionalPositive(doc["physical_height_mm"], candidate.physicalHeightMm))
    return fail(error, "Physical metadata must be positive numbers");
  if (isText(doc["interface"], "HUB75E")) candidate.hub75e = true;
  else if (!isText(doc["interface"], "HUB75")) return fail(error, "Unsupported interface");
  bool foundOrder = false;
  for (int i = 0; i < 6; ++i) {
    const auto order = static_cast<RgbOrder>(i);
    if (isText(doc["rgb_order"], rgbOrderName(order))) {
      candidate.rgbOrder = order; foundOrder = true; break;
    }
  }
  if (!foundOrder) return fail(error, "Unsupported RGB order");
  if (!isText(doc["pixel_mapping"], "STANDARD") || !isText(doc["row_mapping"], "STANDARD") ||
      !isText(doc["driver"], "GENERIC") || !isText(doc["timing"], "SAFE"))
    return fail(error, "Current software supports STANDARD/GENERIC/SAFE only");
  if (!validateProfile(candidate, error)) return false;
  output = candidate;  // Commit only after every check succeeds.
  return true;
}

size_t exportProfile(const PanelProfile& p, char* output, size_t capacity) {
  if (output && capacity) output[0] = 0;
  const char* error = nullptr;
  if (!validateProfile(p, error)) return 0;
  JsonDocument doc;
  doc["schema_version"] = p.schemaVersion; doc["id"] = p.id; doc["name"] = p.name;
  doc["width"] = p.width; doc["height"] = p.height;
  doc["interface"] = p.hub75e ? "HUB75E" : "HUB75";
  doc["scan"] = p.scan; doc["address_lines"] = p.addressLines;
  doc["rgb_order"] = rgbOrderName(p.rgbOrder);
  doc["pixel_mapping"] = "STANDARD"; doc["row_mapping"] = "STANDARD";
  doc["driver"] = "GENERIC"; doc["timing"] = "SAFE";
  if (*p.manufacturer) doc["manufacturer"] = p.manufacturer;
  if (*p.model) doc["model"] = p.model;
  if (*p.description) doc["description"] = p.description;
  if (p.pitch > 0) doc["pitch"] = p.pitch;
  if (p.physicalWidthMm > 0) doc["physical_width_mm"] = p.physicalWidthMm;
  if (p.physicalHeightMm > 0) doc["physical_height_mm"] = p.physicalHeightMm;
  if (doc.overflowed()) return 0;
  const size_t required = measureJson(doc);
  if (output && capacity > required) serializeJson(doc, output, capacity);
  return required;
}

uint16_t mapRgb565(uint16_t color, RgbOrder order) {
  const uint8_t r = static_cast<uint8_t>(((color >> 11) & 31) * 255 / 31);
  const uint8_t g = static_cast<uint8_t>(((color >> 5) & 63) * 255 / 63);
  const uint8_t b = static_cast<uint8_t>((color & 31) * 255 / 31);
  uint8_t red = r, green = g, blue = b;
  switch (order) {
    case RgbOrder::RGB: return color;
    case RgbOrder::RBG: green = b; blue = g; break;
    case RgbOrder::GRB: red = g; green = r; break;
    case RgbOrder::GBR: red = g; green = b; blue = r; break;
    case RgbOrder::BRG: red = b; green = r; blue = g; break;
    case RgbOrder::BGR: red = b; blue = r; break;
  }
  return static_cast<uint16_t>((red >> 3) << 11 | (green >> 2) << 5 | (blue >> 3));
}
}  // namespace display
