#pragma once
#include "framebuffer.h"

namespace display {
enum class Pattern { Red, Green, Blue, RgbBars, Rows, Columns, Checkerboard, Gradient, PixelWalker };
const char* patternName(Pattern pattern);
bool drawDiagnostic(Framebuffer& canvas, Pattern pattern, uint32_t step = 0);
}  // namespace display
