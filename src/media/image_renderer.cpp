#include "image_renderer.h"
#include <algorithm>

namespace media {
const char* scaleModeName(ScaleMode mode) {
  switch (mode) {
    case ScaleMode::Fit: return "FIT";
    case ScaleMode::Fill: return "FILL";
    case ScaleMode::Crop: return "CROP";
    case ScaleMode::Center: return "CENTER";
    case ScaleMode::Stretch: return "STRETCH";
  }
  return nullptr;
}

bool renderImage(const ImageSource& image, display::Framebuffer& canvas,
                 const ImageRenderOptions& o, const char*& error) {
  error = nullptr;
  if (!image.context || !image.sample || !image.width || !image.height ||
      image.width > 128 || image.height > 64 || !canvas.width() || !canvas.height() ||
      !o.width || !o.height || o.width > 128 || o.height > 64 ||
      o.x < -128 || o.x > 128 || o.y < -64 || o.y > 64 || !scaleModeName(o.mode)) {
    error = "Invalid image, canvas, target rectangle, or scale mode"; return false;
  }
  if (o.mode == ScaleMode::Crop && (o.cropX >= image.width || o.cropY >= image.height)) {
    error = "Crop origin is outside source image"; return false;
  }
  int scaledWidth = image.width, scaledHeight = image.height;
  if (o.mode == ScaleMode::Fit || o.mode == ScaleMode::Fill) {
    const bool widthLimits = static_cast<uint32_t>(o.width) * image.height <=
                             static_cast<uint32_t>(o.height) * image.width;
    const bool useWidth = o.mode == ScaleMode::Fit ? widthLimits : !widthLimits;
    if (useWidth) {
      scaledWidth = o.width;
      const unsigned numerator = static_cast<unsigned>(image.height) * o.width;
      scaledHeight = o.mode == ScaleMode::Fill ? (numerator + image.width - 1) / image.width
                                             : std::max(1U, numerator / image.width);
    } else {
      scaledHeight = o.height;
      const unsigned numerator = static_cast<unsigned>(image.width) * o.height;
      scaledWidth = o.mode == ScaleMode::Fill ? (numerator + image.height - 1) / image.height
                                            : std::max(1U, numerator / image.height);
    }
  } else if (o.mode == ScaleMode::Stretch) {
    scaledWidth = o.width; scaledHeight = o.height;
  }
  // Odd spare/cropped pixels go on the right/bottom, consistently across modes.
  const int offsetX = scaledWidth <= o.width ? (o.width - scaledWidth) / 2 : -(scaledWidth - o.width) / 2;
  const int offsetY = scaledHeight <= o.height ? (o.height - scaledHeight) / 2 : -(scaledHeight - o.height) / 2;
  const int left = std::max(0, o.x), top = std::max(0, o.y);
  const int right = std::min(static_cast<int>(canvas.width()), o.x + o.width);
  const int bottom = std::min(static_cast<int>(canvas.height()), o.y + o.height);
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      int sx = 0, sy = 0;
      bool visible = false;
      if (o.mode == ScaleMode::Crop) {
        sx = x - o.x + o.cropX; sy = y - o.y + o.cropY;
        visible = sx < image.width && sy < image.height;
      } else {
        const int dx = x - o.x - offsetX, dy = y - o.y - offsetY;
        visible = dx >= 0 && dy >= 0 && dx < scaledWidth && dy < scaledHeight;
        if (visible) {
          sx = static_cast<int>(static_cast<uint32_t>(dx) * image.width / scaledWidth);
          sy = static_cast<int>(static_cast<uint32_t>(dy) * image.height / scaledHeight);
        }
      }
      if (visible) canvas.setPixel(x, y, image.sample(image.context, static_cast<uint16_t>(sx), static_cast<uint16_t>(sy)));
      else if (!o.preserveBackground) canvas.setPixel(x, y, o.background);
    }
  }
  return true;
}
}  // namespace media
