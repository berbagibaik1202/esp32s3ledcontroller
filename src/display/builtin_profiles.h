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
constexpr char kHardwareProfileJson[] = R"json({
  "schema_version": 1,
  "id": "p25_128x64_scan32",
  "name": "P2.5 RGB 128x64 1/32 TEST - UNVERIFIED",
  "pitch": 2.5,
  "width": 128,
  "height": 64,
  "interface": "HUB75E",
  "scan": 32,
  "address_lines": 5,
  "rgb_order": "RGB",
  "pixel_mapping": "STANDARD",
  "row_mapping": "STANDARD",
  "driver": "GENERIC",
  "timing": "SAFE"
})json";
}  // namespace display
