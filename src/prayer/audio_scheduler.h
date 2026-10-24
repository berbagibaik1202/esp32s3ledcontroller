#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "prayer_types.h"
#include "prayer_schedule.h"
#include "../audio/audio_player.h"

namespace prayer {

struct NextAudioEvent {
  bool active{false};
  int prayerIndex{-1};
  String prayerName{""};
  String file{""};
  PrayerTimePair triggerTime;
  int countdownSeconds{0};
};

class AudioScheduler {
 public:
  static AudioScheduler& instance();

  bool begin();
  void update();

  const PrayerAudioConfig& config() const { return config_; }
  bool setConfig(const PrayerAudioConfig& newConfig);
  bool setPrayerAudio(PrayerId id, bool enabled, const String& file, int leadMinutes, uint8_t volume);

  NextAudioEvent nextEvent() const;

  bool testPlay(PrayerId id);
  void stopTest();

  // Persistence
  bool load();
  bool save();

 private:
  AudioScheduler();
  PrayerAudioConfig config_;

  // Deduplication tracking: date string + bitmask of triggered prayers today
  String lastDateString_{""};
  uint8_t triggeredMaskToday_{0};

  uint32_t lastCheckMs_{0};
};

inline AudioScheduler& audioScheduler() { return AudioScheduler::instance(); }

}  // namespace prayer
