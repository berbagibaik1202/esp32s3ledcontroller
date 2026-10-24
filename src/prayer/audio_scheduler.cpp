#include "audio_scheduler.h"
#include "clock_service.h"
#include <Preferences.h>

namespace prayer {

AudioScheduler::AudioScheduler() {
  for (int i = 0; i < PRAYER_COUNT; ++i) {
    config_.items[i].enabled = false;
    config_.items[i].file = "";
    config_.items[i].leadMinutes = 10;
    config_.items[i].volume = 80;
  }
}

AudioScheduler& AudioScheduler::instance() {
  static AudioScheduler s_instance;
  return s_instance;
}

bool AudioScheduler::begin() {
  load();
  return true;
}

void AudioScheduler::update() {
  const uint32_t nowMs = millis();
  if (nowMs - lastCheckMs_ < 1000U) return;
  lastCheckMs_ = nowMs;

  const auto clk = clockService().status();
  if (!clk.synced) return; // Don't trigger if system time is untrusted

  // Reset triggered mask on new day
  char todayBuf[16];
  snprintf(todayBuf, sizeof todayBuf, "%04d-%02d-%02d", clk.year, clk.month, clk.day);
  String todayStr = String(todayBuf);

  if (lastDateString_ != todayStr) {
    lastDateString_ = todayStr;
    triggeredMaskToday_ = 0;
    // On first boot of the day, mark past events as already triggered so they don't fire in a burst
    int nowSecs = (clk.hour * 60 + clk.minute) * 60 + clk.second;
    const auto sched = prayerSchedule().currentStatus();
    for (int i = 0; i < PRAYER_COUNT; ++i) {
      if (config_.items[i].enabled) {
        int effSecs = sched.effective[i].totalMinutes() * 60;
        int triggerSecs = effSecs - (config_.items[i].leadMinutes * 60);
        if (nowSecs > triggerSecs + 60) {
          triggeredMaskToday_ |= (1 << i);
        }
      }
    }
  }

  // Check each prayer for trigger
  const auto sched = prayerSchedule().currentStatus();
  int nowSecs = (clk.hour * 60 + clk.minute) * 60 + clk.second;

  for (int i = 0; i < PRAYER_COUNT; ++i) {
    const auto& item = config_.items[i];
    if (!item.enabled || !item.file.length()) continue;

    // Has it already triggered today?
    if (triggeredMaskToday_ & (1 << i)) continue;

    int effSecs = sched.effective[i].totalMinutes() * 60;
    int triggerSecs = effSecs - (item.leadMinutes * 60);

    // If trigger window is hit (within 0 - 5 seconds window)
    if (nowSecs >= triggerSecs && nowSecs <= triggerSecs + 5) {
      triggeredMaskToday_ |= (1 << i);
      Serial.printf("[AUDIO SCHEDULER] Memicu audio pengingat %s (%s, -%d menit)\n",
                    prayerLabel(static_cast<PrayerId>(i)), item.file.c_str(), item.leadMinutes);
      audio::player().play(item.file, item.volume);
    }
  }
}

NextAudioEvent AudioScheduler::nextEvent() const {
  NextAudioEvent res;
  const auto clk = clockService().status();
  if (!clk.synced) return res;

  const auto sched = prayerSchedule().currentStatus();
  int nowSecs = (clk.hour * 60 + clk.minute) * 60 + clk.second;

  int minDiff = 999999;
  int chosen = -1;

  for (int i = 0; i < PRAYER_COUNT; ++i) {
    const auto& item = config_.items[i];
    if (!item.enabled || !item.file.length()) continue;
    if (triggeredMaskToday_ & (1 << i)) continue;

    int effSecs = sched.effective[i].totalMinutes() * 60;
    int triggerSecs = effSecs - (item.leadMinutes * 60);
    int diff = triggerSecs - nowSecs;
    if (diff > 0 && diff < minDiff) {
      minDiff = diff;
      chosen = i;
    }
  }

  if (chosen >= 0) {
    res.active = true;
    res.prayerIndex = chosen;
    res.prayerName = prayerLabel(static_cast<PrayerId>(chosen));
    res.file = config_.items[chosen].file;
    int effMins = sched.effective[chosen].totalMinutes();
    res.triggerTime = PrayerTimePair::fromMinutes(effMins - config_.items[chosen].leadMinutes);
    res.countdownSeconds = minDiff;
  }
  return res;
}

bool AudioScheduler::testPlay(PrayerId id) {
  if (id < 0 || id >= PRAYER_COUNT) return false;
  const auto& item = config_.items[id];
  if (!item.file.length()) return false;
  return audio::player().play(item.file, item.volume);
}

void AudioScheduler::stopTest() {
  audio::player().stop();
}

bool AudioScheduler::setConfig(const PrayerAudioConfig& newConfig) {
  config_ = newConfig;
  return save();
}

bool AudioScheduler::setPrayerAudio(PrayerId id, bool enabled, const String& file, int leadMinutes, uint8_t volume) {
  if (id < 0 || id >= PRAYER_COUNT) return false;
  config_.items[id].enabled = enabled;
  config_.items[id].file = file;
  config_.items[id].leadMinutes = std::max(1, std::min(60, leadMinutes));
  config_.items[id].volume = std::max<uint8_t>(1, std::min<uint8_t>(100, volume));
  return save();
}

bool AudioScheduler::save() {
  Preferences prefs;
  if (!prefs.begin("prayer-audio", false)) return false;

  JsonDocument doc;
  for (int i = 0; i < PRAYER_COUNT; ++i) {
    auto itemObj = doc[prayerIdName(static_cast<PrayerId>(i))].to<JsonObject>();
    itemObj["enabled"] = config_.items[i].enabled;
    itemObj["file"] = config_.items[i].file;
    itemObj["lead_minutes"] = config_.items[i].leadMinutes;
    itemObj["volume"] = config_.items[i].volume;
  }

  String json;
  serializeJson(doc, json);
  const bool ok = prefs.putString("config", json) == json.length();
  prefs.end();
  return ok;
}

bool AudioScheduler::load() {
  Preferences prefs;
  if (!prefs.begin("prayer-audio", true)) return false;
  String json = prefs.getString("config", "");
  prefs.end();

  if (!json.length()) return false;

  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;

  for (int i = 0; i < PRAYER_COUNT; ++i) {
    const char* key = prayerIdName(static_cast<PrayerId>(i));
    if (doc[key].is<JsonObject>()) {
      auto obj = doc[key].as<JsonObject>();
      config_.items[i].enabled = obj["enabled"] | false;
      config_.items[i].file = obj["file"] | "";
      config_.items[i].leadMinutes = obj["lead_minutes"] | 10;
      config_.items[i].volume = obj["volume"] | 80;
    }
  }
  return true;
}

}  // namespace prayer
