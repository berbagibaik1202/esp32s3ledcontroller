#include "diagnostics.h"

namespace display {
const char* patternName(Pattern p) {
  switch (p) {
    case Pattern::Red: return "Solid Red";
    case Pattern::Green: return "Solid Green";
    case Pattern::Blue: return "Solid Blue";
    case Pattern::RgbBars: return "RGB Bars";
    case Pattern::Rows: return "Row Test";
    case Pattern::Columns: return "Column Test";
    case Pattern::Checkerboard: return "Checkerboard";
    case Pattern::Gradient: return "Gradient";
    case Pattern::PixelWalker: return "Pixel Walker";
  }
  return nullptr;
}

bool drawDiagnostic(Framebuffer& canvas, Pattern p, uint32_t step) {
  if (!canvas.width() || !canvas.height() || !patternName(p)) return false;
  canvas.clear();
  for (uint16_t y = 0; y < canvas.height(); ++y) {
    for (uint16_t x = 0; x < canvas.width(); ++x) {
      uint16_t color = 0;
      switch (p) {
        case Pattern::Red: color = 0xF800; break;
        case Pattern::Green: color = 0x07E0; break;
        case Pattern::Blue: color = 0x001F; break;
        case Pattern::RgbBars: {
          const unsigned bar = static_cast<unsigned>(x) * 3 / canvas.width();
          color = bar == 0 ? 0xF800 : bar == 1 ? 0x07E0 : 0x001F; break;
        }
        case Pattern::Rows: color = y == step % canvas.height() ? 0xFFFF : 0; break;
        case Pattern::Columns: color = x == step % canvas.width() ? 0xFFFF : 0; break;
        case Pattern::Checkerboard: color = ((x + y + (step & 1U)) & 1U) ? 0xFFFF : 0; break;
        case Pattern::Gradient: {
          const uint16_t red = canvas.width() > 1 ? x * 31 / (canvas.width() - 1) : 0;
          const uint16_t green = canvas.height() > 1 ? y * 63 / (canvas.height() - 1) : 0;
          color = static_cast<uint16_t>((red << 11) | (green << 5)); break;
        }
        case Pattern::PixelWalker:
          color = static_cast<uint32_t>(y) * canvas.width() + x ==
              step % (static_cast<uint32_t>(canvas.width()) * canvas.height()) ? 0xFFFF : 0;
          break;
      }
      canvas.setPixel(x, y, color);
    }
  }
  return true;
}
}  // namespace display
