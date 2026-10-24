#pragma once

#include <Arduino.h>
#include "../display/framebuffer.h"
#include "prayer_schedule.h"
#include "clock_service.h"

namespace prayer {

class PrayerRenderer {
 public:
  static PrayerRenderer& instance();

  // Renders the Mosque Digital Clock & Prayer Times into canvas (128x64)
  bool render(display::Framebuffer& canvas);
  bool isTransitioning() const { return inTransition_; }
  bool isMarqueeRunning() const { return middleMode_ == 1; }
  bool needsFastRefresh() const { return inTransition_ || (middleMode_ == 1); }
  uint32_t refreshInterval() const;

 private:
  PrayerRenderer();
  uint32_t lastCycleChange_{0};
  uint8_t dateCycle_{0}; // 0 = Masehi, 1 = Hijriah

  uint32_t lastBatchSwitch_{0};
  uint8_t batchIndex_{0}; // 0 = Pagi (Imsak, Shubuh, Terbit, Duha), 1 = Siang-Malam (Dzuhur, Ashar, Magrib, Isya)
  uint8_t batchPrevious_{0};
  bool inTransition_{false};
  uint32_t transitionStart_{0};

  // Middle section alternating modes (0 = Next prayer & countdown, 1 = Marquee text)
  uint8_t middleMode_{0};
  uint32_t middleModeChangeTime_{0};
  int marqueeX_{128};
  bool marqueeInitialized_{false};

  // Mosque Flow Dedicated Renderers
  void renderAdzan(display::Framebuffer& canvas, const PrayerStatus& sched);
  void renderIqomah(display::Framebuffer& canvas, const PrayerStatus& sched);
  void renderShalat(display::Framebuffer& canvas, const PrayerStatus& sched);
  bool tryRenderCustomBackground(display::Framebuffer& canvas, const String& imagePath);
};

inline PrayerRenderer& prayerRenderer() { return PrayerRenderer::instance(); }

}  // namespace prayer
