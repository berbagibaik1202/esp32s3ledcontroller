#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "prayer_types.h"
#include "clock_service.h"

namespace prayer {

class PrayerSchedule {
 public:
  static PrayerSchedule& instance();

  bool begin();
  void update();

  const PrayerScheduleConfig& config() const { return config_; }
  bool setConfig(const PrayerScheduleConfig& newConfig);
  bool setCorrections(const int corrections[PRAYER_COUNT]);
  bool setBaseTimes(const PrayerTimePair baseTimes[PRAYER_COUNT]);

  // Evaluates effective times and countdown
  PrayerStatus currentStatus() const;

  // Compute astronomical times for given date and coordinates
  static void calculateAstronomicalTimes(
      int year, int month, int day,
      double latitude, double longitude, double tzOffsetHours,
      int method,
      PrayerTimePair& subuh, PrayerTimePair& imsak, PrayerTimePair& syuruq,
      PrayerTimePair& zuhur, PrayerTimePair& asar,
      PrayerTimePair& magrib, PrayerTimePair& isya);

  // Persistence
  bool load();
  bool save();

  // Test Mode Trigger (for manual preview of Adzan, Iqomah, Shalat)
  void setTestMode(MosqueState state, int prayerIdx = PRAYER_ZUHUR, int durationSecs = 60);
  void clearTestMode();
  bool isTestMode() const { return testState_ != MOSQUE_STATE_NORMAL; }

 private:
  PrayerSchedule();
  PrayerScheduleConfig config_;
  mutable uint32_t lastCalcEpoch_{0};
  mutable PrayerStatus cachedStatus_;

  mutable MosqueState testState_{MOSQUE_STATE_NORMAL};
  mutable int testPrayerIdx_{PRAYER_ZUHUR};
  mutable uint32_t testExpiryMillis_{0};
  mutable uint32_t testDurationSecs_{60};

  void recalculateStatus() const;
};

inline PrayerSchedule& prayerSchedule() { return PrayerSchedule::instance(); }

}  // namespace prayer
