#include "clock_service.h"
#include "prayer_schedule.h"
#include <esp_sntp.h>
#include <sys/time.h>

namespace prayer {

ClockService::ClockService() = default;

ClockService& ClockService::instance() {
  static ClockService s_instance;
  return s_instance;
}

void ClockService::begin(const char* timezone, const char* ntpServer1, const char* ntpServer2) {
  // Use POSIX timezone string for WIB (UTC+7): "WIB-7"
  configTzTime(timezone ? timezone : "WIB-7", ntpServer1, ntpServer2, "time.google.com");
  Serial.printf("[CLOCK] NTP configured: %s, %s (TZ: %s)\n", ntpServer1, ntpServer2, timezone);
}

void ClockService::update() {
  const uint32_t nowMs = millis();
  if (nowMs - lastSyncCheck_ < 1000U) return;
  lastSyncCheck_ = nowMs;

  time_t now = ::time(nullptr);
  if (now > 1700000000) { // Valid time after Nov 2023
    if (!synced_) {
      synced_ = true;
      Serial.printf("[CLOCK] PASS: Waktu tersinkronisasi (%ld)\n", static_cast<long>(now));
    }
  }
}

time_t ClockService::nowEpoch() const {
  return ::time(nullptr);
}

tm ClockService::nowTm() const {
  time_t now = nowEpoch();
  tm t;
  localtime_r(&now, &t);
  return t;
}

void ClockService::setTimezone(const String& tzName, int offsetMinutes) {
  timezoneName_ = tzName;
  timezoneOffsetMinutes_ = offsetMinutes;

  char tzBuf[32];
  // Calculate POSIX format: inverted sign for hours ahead of UTC
  int hoursAhead = offsetMinutes / 60;
  snprintf(tzBuf, sizeof tzBuf, "UTC%s%d", hoursAhead >= 0 ? "-" : "+", abs(hoursAhead));
  setenv("TZ", tzBuf, 1);
  tzset();
}

void ClockService::setManualTime(int year, int month, int day, int hour, int minute, int second) {
  tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;
  time_t epoch = mktime(&t);

  timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
  synced_ = true;
  Serial.printf("[CLOCK] Waktu manual diterapkan: %04d-%02d-%02d %02d:%02d:%02d\n",
                year, month, day, hour, minute, second);
}

void ClockService::syncFromBrowser(time_t epoch, int year, int month, int day, int hour, int minute, int second, int tzOffsetMinutes, const String& tzName) {
  if (tzName.length()) timezoneName_ = tzName;
  timezoneOffsetMinutes_ = tzOffsetMinutes;

  char tzBuf[32];
  int hoursAhead = tzOffsetMinutes / 60;
  snprintf(tzBuf, sizeof tzBuf, "%s%s%d", timezoneName_.c_str(), hoursAhead >= 0 ? "-" : "+", abs(hoursAhead));
  setenv("TZ", tzBuf, 1);
  tzset();

  if (epoch > 1000000000) {
    timeval tv = {epoch, 0};
    settimeofday(&tv, nullptr);
  } else if (year >= 2024 && month >= 1 && day >= 1) {
    setManualTime(year, month, day, hour, minute, second);
  }

  // Verify what localtime_r produces and ensure hour/minute match exactly what browser sent
  time_t cur = ::time(nullptr);
  tm t = {};
  localtime_r(&cur, &t);

  if (hour >= 0 && (t.tm_hour != hour || t.tm_min != minute)) {
    int targetSecOfDay = hour * 3600 + minute * 60 + second;
    int actualSecOfDay = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
    int diffSec = targetSecOfDay - actualSecOfDay;
    if (diffSec > 43200) diffSec -= 86400;
    else if (diffSec < -43200) diffSec += 86400;

    cur += diffSec;
    timeval adjTv = {cur, 0};
    settimeofday(&adjTv, nullptr);
    localtime_r(&cur, &t);
  }

  synced_ = true;
  Serial.printf("[CLOCK] Sinkronisasi jam browser berhasil: %04d-%02d-%02d %02d:%02d:%02d (%s, offset=%d mnt)\n",
                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
                timezoneName_.c_str(), timezoneOffsetMinutes_);
}


ClockStatus ClockService::status() const {
  ClockStatus s;
  s.synced = synced_;
  s.epoch = nowEpoch();
  s.timezone = timezoneName_;
  s.timezoneOffsetMinutes = timezoneOffsetMinutes_;

  tm t = nowTm();
  s.year = t.tm_year + 1900;
  s.month = t.tm_mon + 1;
  s.day = t.tm_mday;
  s.hour = t.tm_hour;
  s.minute = t.tm_min;
  s.second = t.tm_sec;
  s.dayOfWeek = t.tm_wday;

  char timeBuf[16];
  snprintf(timeBuf, sizeof timeBuf, "%02d:%02d:%02d", s.hour, s.minute, s.second);
  s.timeString = String(timeBuf);

  char dateBuf[64];
  snprintf(dateBuf, sizeof dateBuf, "%s, %d %s %04d",
           indonesianDayName(s.dayOfWeek), s.day, indonesianMonthName(s.month), s.year);
  s.dateString = String(dateBuf);

  HijriDate h = gregorianToHijri(s.year, s.month, s.day, prayerSchedule().config().hijriAdjustment);
  s.hijriString = h.formatted;

  return s;
}

}  // namespace prayer
