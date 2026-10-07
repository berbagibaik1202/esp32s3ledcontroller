#pragma once
#include "image_source.h"
#include "display/framebuffer.h"

namespace media {
enum class ScaleMode { Fit, Fill, Crop, Center, Stretch };
struct ImageRenderOptions {
  ScaleMode mode = ScaleMode::Fit;
  int x = 0, y = 0;
  uint16_t width = 0, height = 0;  // Must be explicit; target rectangle.
  uint16_t cropX = 0, cropY = 0;
  uint16_t background = 0;
  bool preserveBackground = false;
};
const char* scaleModeName(ScaleMode mode);
// Validates before writing; writes back buffer only, caller presents completed frame.
// A valid source must support all in-range samples for the duration of rendering.
bool renderImage(const ImageSource& image, display::Framebuffer& canvas,
                 const ImageRenderOptions& options, const char*& error);
}  // namespace media
