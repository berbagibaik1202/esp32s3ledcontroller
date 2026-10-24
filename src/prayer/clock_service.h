#pragma once

#include <Arduino.h>
#include <time.h>
#include "hijri.h"

namespace prayer {

struct ClockStatus {
  bool synced{false};
  time_t epoch{0};
  int year{2026};
  int month{1};
  int day{1};
  int hour{0};
  int minute{0};
  int second{0};
  int dayOfWeek{0}; // 0 = Ahad/Sunday
  String timeString; // "04:35:12"
  String dateString; // "Sabtu, 24 Oktober 2026"
  String hijriString; // "12 Rabiul Akhir 1448 H"
  String timezone; // "Asia/Jakarta"
  int timezoneOffsetMinutes{420};
};

class ClockService {
 public:
  static ClockService& instance();

  void begin(const char* timezone = "WIB-7", const char* ntpServer1 = "pool.ntp.org", const char* ntpServer2 = "id.pool.ntp.org");
  void update();

  bool isSynced() const { return synced_; }
  time_t nowEpoch() const;
  tm nowTm() const;

  ClockStatus status() const;

  void setTimezone(const String& tzName, int offsetMinutes);
  void setManualTime(int year, int month, int day, int hour, int minute, int second);
  void syncFromBrowser(time_t epoch, int year, int month, int day, int hour, int minute, int second, int tzOffsetMinutes = 420, const String& tzName = "WIB");

 private:
  ClockService();
  bool synced_{false};
  uint32_t lastSyncCheck_{0};
  String timezoneName_{"Asia/Jakarta"};
  int timezoneOffsetMinutes_{420};
};

inline ClockService& clockService() { return ClockService::instance(); }

}  // namespace prayer
