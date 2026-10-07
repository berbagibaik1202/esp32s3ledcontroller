#pragma once
#include "panel_profile.h"

namespace display {
constexpr size_t kMaxPanels = 4;
struct PanelPlacement {
  uint8_t chainIndex = 0;
  uint16_t x = 0, y = 0, rotation = 0;
};
struct PanelLayout {
  uint16_t schemaVersion = 1;
  char panelProfileId[65] = {};
  uint16_t canvasWidth = 0, canvasHeight = 0;
  uint8_t panelCount = 0;
  PanelPlacement panels[kMaxPanels];
};
struct PanelPixel {
  uint8_t chainIndex = 0;
  uint16_t x = 0, y = 0;
};

bool validateLayout(const PanelLayout& layout, const PanelProfile& profile, const char*& error);
bool makeGridLayout(const PanelProfile& profile, uint8_t columns, uint8_t rows,
                    bool serpentine, PanelLayout& output, const char*& error);
bool parseLayout(const char* json, size_t length, const PanelProfile& profile,
                 PanelLayout& output, const char*& error);
size_t exportLayout(const PanelLayout& layout, const PanelProfile& profile,
                    char* output, size_t capacity);

class LayoutMapper {
 public:
  // Invalid configuration retains the previous mapping.
  bool configure(const PanelProfile& profile, const PanelLayout& layout, const char*& error);
  bool canvasToPanel(int x, int y, PanelPixel& output) const;
  bool panelToCanvas(const PanelPixel& pixel, uint16_t& x, uint16_t& y) const;
  size_t chainPixels() const;
  // Output is logical module-row-major in chain order, not DMA/scan data.
  // RGB order is applied exactly once here. Buffers must be disjoint.
  bool copyCanvasToChain(const uint16_t* canvas, size_t canvasPixels,
                         uint16_t* chain, size_t chainCapacity) const;
 private:
  bool ready_ = false;
  PanelProfile profile_;
  PanelLayout layout_;
};
}  // namespace display
