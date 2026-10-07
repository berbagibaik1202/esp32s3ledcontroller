#pragma once
namespace display {
constexpr char kDevelopmentProfileJson[] = R"json({
  "schema_version": 1,
  "id": "p25_64x32_scan16",
  "name": "P2.5 RGB 64x32 1/16 - UNVERIFIED",
  "pitch": 2.5,
  "width": 64,
  "height": 32,
  "interface": "HUB75",
  "scan": 16,
  "address_lines": 4,
  "rgb_order": "RGB",
  "pixel_mapping": "STANDARD",
  "row_mapping": "STANDARD",
  "driver": "GENERIC",
  "timing": "SAFE"
})json";
}  // namespace display
