#include "prayer_schedule.h"
#include <Preferences.h>
#include <cmath>

namespace prayer {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

double fixAngle(double a) {
  a = a - 360.0 * std::floor(a / 360.0);
  return a < 0 ? a + 360.0 : a;
}

double fixHour(double a) {
  a = a - 24.0 * std::floor(a / 24.0);
  return a < 0 ? a + 24.0 : a;
}

double dSin(double d) { return std::sin(d * kDegToRad); }
double dCos(double d) { return std::cos(d * kDegToRad); }
double dTan(double d) { return std::tan(d * kDegToRad); }
double dAsin(double x) { return std::asin(x) * kRadToDeg; }
double dAcos(double x) { return std::acos(x) * kRadToDeg; }
double dAtan2(double y, double x) { return std::atan2(y, x) * kRadToDeg; }
double dAcot(double x) { return std::atan2(1.0, x) * kRadToDeg; }
}

PrayerSchedule::PrayerSchedule() {
  // Default baseline prayer schedule (approximate WIB times)
  config_.baseTimes[PRAYER_SUBUH] = PrayerTimePair(4, 35);
  config_.baseTimes[PRAYER_ZUHUR] = PrayerTimePair(11, 45);
  config_.baseTimes[PRAYER_ASAR]  = PrayerTimePair(15, 5);
  config_.baseTimes[PRAYER_MAGRIB] = PrayerTimePair(17, 50);
  config_.baseTimes[PRAYER_ISYA]  = PrayerTimePair(19, 0);
}

PrayerSchedule& PrayerSchedule::instance() {
  static PrayerSchedule s_instance;
  return s_instance;
}

bool PrayerSchedule::begin() {
  load();
  recalculateStatus();
  return true;
}

void PrayerSchedule::update() {
  // Recalculate status every second
  recalculateStatus();
}

void PrayerSchedule::calculateAstronomicalTimes(
    int year, int month, int day,
    double latitude, double longitude, double tzOffsetHours,
    int method,
    PrayerTimePair& subuh, PrayerTimePair& imsak, PrayerTimePair& syuruq,
    PrayerTimePair& zuhur, PrayerTimePair& asar,
    PrayerTimePair& magrib, PrayerTimePair& isya) {

  // Julian date
  int y = year, m = month, d = day;
  if (m <= 2) { y -= 1; m += 12; }
  double a = std::floor(y / 100.0);
  double b = 2.0 - a + std::floor(a / 4.0);
  double jd = std::floor(365.25 * (y + 4716.0)) + std::floor(30.6001 * (m + 1.0)) + d + b - 1524.5;
  double dDays = jd - 2451545.0;

  // Solar coordinates
  double g = fixAngle(357.529 + 0.98560028 * dDays);
  double q = fixAngle(280.459 + 0.98564736 * dDays);
  double l = fixAngle(q + 1.915 * dSin(g) + 0.020 * dSin(2.0 * g));
  double e = 23.439 - 0.00000036 * dDays;
  double ra = fixHour(dAtan2(dCos(e) * dSin(l), dCos(l)) / 15.0);
  double decl = dAsin(dSin(e) * dSin(l));
  double eqt = q / 15.0 - ra;

  // Solar noon
  double noon = fixHour(12.0 + tzOffsetHours - (longitude / 15.0) - eqt);

  // Method parameters (Fajr angle, Isha angle)
  // Default: Kemenag RI (Subuh -20°, Isya -18°)
  double fajrAngle = -20.0;
  double ishaAngle = -18.0;
  if (method == 1) { // Muslim World League
    fajrAngle = -18.0; ishaAngle = -17.0;
  } else if (method == 2) { // Egyptian General Authority
    fajrAngle = -19.5; ishaAngle = -17.5;
  } else if (method == 3) { // Umm al-Qura (Isha 90 mins after Maghrib)
    fajrAngle = -18.5; ishaAngle = 0.0;
  } else if (method == 4) { // Karachi
    fajrAngle = -18.0; ishaAngle = -18.0;
  }

  auto getHourAngle = [&](double angle) -> double {
    double val = (dSin(angle) - dSin(latitude) * dSin(decl)) / (dCos(latitude) * dCos(decl));
    if (val > 1.0) val = 1.0;
    if (val < -1.0) val = -1.0;
    return dAcos(val) / 15.0;
  };

  // Zuhur: solar noon + 2-3 minutes precaution (ihtiyat)
  double dhuhrDec = noon + (2.0 / 60.0);

  // Sunset & Sunrise (angle = -0.8333°)
  double hSunset = getHourAngle(-0.8333);
  double sunriseDec = noon - hSunset;
  double maghribDec = noon + hSunset + (2.0 / 60.0);

  // Asr (Shafi'i / Kemenag: shadow factor = 1)
  // Solar altitude angle is positive above the horizon:
  double asrAngle = dAcot(1.0 + dTan(std::abs(latitude - decl)));
  double asrDec = noon + getHourAngle(asrAngle) + (2.0 / 60.0);

  // Fajr (Subuh)
  double fajrDec = noon - getHourAngle(fajrAngle) + (2.0 / 60.0);

  // Isha
  double ishaDec = (method == 3) ? (maghribDec + 1.5) : (noon + getHourAngle(ishaAngle) + (2.0 / 60.0));

  // Imsak = Fajr - 10 minutes
  double imsakDec = fajrDec - (10.0 / 60.0);

  auto toPair = [](double h) -> PrayerTimePair {
    h = fixHour(h);
    int totalMins = static_cast<int>(std::round(h * 60.0));
    return PrayerTimePair::fromMinutes(totalMins);
  };

  subuh = toPair(fajrDec);
  imsak = toPair(imsakDec);
  syuruq = toPair(sunriseDec);
  zuhur = toPair(dhuhrDec);
  asar = toPair(asrDec);
  magrib = toPair(maghribDec);
  isya = toPair(ishaDec);
}

void PrayerSchedule::recalculateStatus() const {
  const auto clk = clockService().status();

  PrayerTimePair base[PRAYER_COUNT];
  PrayerTimePair imsak, syuruq;

  if ((config_.source == "calc" || config_.source == "auto") && clk.synced) {
    // Astronomical calculation
    double tzHours = config_.timezoneOffsetMinutes / 60.0;
    calculateAstronomicalTimes(
        clk.year, clk.month, clk.day,
        config_.latitude, config_.longitude, tzHours, config_.calcMethod,
        base[PRAYER_SUBUH], imsak, syuruq,
        base[PRAYER_ZUHUR], base[PRAYER_ASAR],
        base[PRAYER_MAGRIB], base[PRAYER_ISYA]);
  } else {
    // Manual schedule baseline
    for (int i = 0; i < PRAYER_COUNT; ++i) {
      base[i] = config_.baseTimes[i];
    }
    imsak = PrayerTimePair::fromMinutes(base[PRAYER_SUBUH].totalMinutes() - 10);
    syuruq = PrayerTimePair::fromMinutes(base[PRAYER_SUBUH].totalMinutes() + 75);
  }

  // Calculate effective times = base + correction
  PrayerTimePair effective[PRAYER_COUNT];
  for (int i = 0; i < PRAYER_COUNT; ++i) {
    int effMin = base[i].totalMinutes() + config_.corrections[i];
    effective[i] = PrayerTimePair::fromMinutes(effMin);
    cachedStatus_.base[i] = base[i];
    cachedStatus_.correction[i] = config_.corrections[i];
    cachedStatus_.effective[i] = effective[i];
  }
  cachedStatus_.imsak = imsak;
  cachedStatus_.syuruq = syuruq;
  cachedStatus_.duha = PrayerTimePair::fromMinutes(syuruq.totalMinutes() + 20);

  // Determine current minutes from midnight
  int nowMins = clk.hour * 60 + clk.minute;
  int nowSecsOfDay = nowMins * 60 + clk.second;

  int nextIdx = -1;
  int minDiffSeconds = 999999;
  bool isToday = true;

  for (int i = 0; i < PRAYER_COUNT; ++i) {
    int prayerSecs = effective[i].totalMinutes() * 60;
    int diff = prayerSecs - nowSecsOfDay;
    if (diff >= 0 && diff < minDiffSeconds) {
      minDiffSeconds = diff;
      nextIdx = i;
    }
  }

  if (nextIdx == -1) {
    // All prayers for today have passed; next prayer is Subuh tomorrow
    nextIdx = PRAYER_SUBUH;
    isToday = false;
    int subuhTomorrowSecs = 86400 + effective[PRAYER_SUBUH].totalMinutes() * 60;
    minDiffSeconds = subuhTomorrowSecs - nowSecsOfDay;
  }

  cachedStatus_.nextPrayerIndex = nextIdx;
  cachedStatus_.nextPrayerName = prayerLabel(static_cast<PrayerId>(nextIdx));
  cachedStatus_.nextPrayerTime = effective[nextIdx];
  cachedStatus_.countdownSeconds = std::max(0, minDiffSeconds);
  cachedStatus_.isTodayNext = isToday;

  // Determine current active prayer (prayer time entered until next prayer)
  int currIdx = PRAYER_ISYA;
  if (nowMins >= effective[PRAYER_ISYA].totalMinutes() || nowMins < effective[PRAYER_SUBUH].totalMinutes()) {
    currIdx = PRAYER_ISYA;
  } else if (nowMins >= effective[PRAYER_MAGRIB].totalMinutes()) {
    currIdx = PRAYER_MAGRIB;
  } else if (nowMins >= effective[PRAYER_ASAR].totalMinutes()) {
    currIdx = PRAYER_ASAR;
  } else if (nowMins >= effective[PRAYER_ZUHUR].totalMinutes()) {
    currIdx = PRAYER_ZUHUR;
  } else if (nowMins >= effective[PRAYER_SUBUH].totalMinutes()) {
    currIdx = PRAYER_SUBUH;
  }
  cachedStatus_.currentPrayerIndex = currIdx;
  cachedStatus_.currentPrayerName = prayerLabel(static_cast<PrayerId>(currIdx));

  // Evaluate Mosque Flow State: Adzan -> Iqomah -> Shalat -> Normal
  if (testState_ != MOSQUE_STATE_NORMAL) {
    if (millis() < testExpiryMillis_) {
      cachedStatus_.mosqueState = testState_;
      cachedStatus_.activePrayerIndex = testPrayerIdx_;
      cachedStatus_.activePrayerName = prayerLabel(static_cast<PrayerId>(testPrayerIdx_));
      int remain = static_cast<int>((testExpiryMillis_ - millis()) / 1000U);
      cachedStatus_.stateRemainingSeconds = std::max(0, remain);
      cachedStatus_.stateTotalSeconds = testDurationSecs_;
      return;
    } else {
      testState_ = MOSQUE_STATE_NORMAL;
    }
  }

  MosqueState detectedState = MOSQUE_STATE_NORMAL;
  int activePrayer = -1;
  int stateRemaining = 0;
  int stateTotal = 0;

  for (int i = 0; i < PRAYER_COUNT; ++i) {
    int pSecs = effective[i].totalMinutes() * 60;
    int adzanSecs = std::max(1, config_.adzanDurationMins) * 60;
    int iqomahSecs = std::max(1, config_.iqomahDurationMins[i]) * 60;
    int shalatSecs = std::max(1, config_.shalatDurationMins) * 60;

    int tAdzanEnd = pSecs + adzanSecs;
    int tIqomahEnd = tAdzanEnd + iqomahSecs;
    int tShalatEnd = tIqomahEnd + shalatSecs;

    if (nowSecsOfDay >= pSecs && nowSecsOfDay < tAdzanEnd) {
      detectedState = MOSQUE_STATE_ADZAN;
      activePrayer = i;
      stateRemaining = tAdzanEnd - nowSecsOfDay;
      stateTotal = adzanSecs;
      break;
    } else if (nowSecsOfDay >= tAdzanEnd && nowSecsOfDay < tIqomahEnd) {
      detectedState = MOSQUE_STATE_IQOMAH;
      activePrayer = i;
      stateRemaining = tIqomahEnd - nowSecsOfDay;
      stateTotal = iqomahSecs;
      break;
    } else if (nowSecsOfDay >= tIqomahEnd && nowSecsOfDay < tShalatEnd) {
      detectedState = MOSQUE_STATE_SHALAT;
      activePrayer = i;
      stateRemaining = tShalatEnd - nowSecsOfDay;
      stateTotal = shalatSecs;
      break;
    }
  }

  cachedStatus_.mosqueState = detectedState;
  cachedStatus_.activePrayerIndex = activePrayer;
  cachedStatus_.activePrayerName = (activePrayer >= 0) ? prayerLabel(static_cast<PrayerId>(activePrayer)) : "";
  cachedStatus_.stateRemainingSeconds = std::max(0, stateRemaining);
  cachedStatus_.stateTotalSeconds = stateTotal;
}

void PrayerSchedule::setTestMode(MosqueState state, int prayerIdx, int durationSecs) {
  testState_ = state;
  testPrayerIdx_ = std::max(0, std::min(4, prayerIdx));
  testDurationSecs_ = std::max(5, durationSecs);
  testExpiryMillis_ = millis() + (testDurationSecs_ * 1000U);
  recalculateStatus();
}

void PrayerSchedule::clearTestMode() {
  testState_ = MOSQUE_STATE_NORMAL;
  recalculateStatus();
}

PrayerStatus PrayerSchedule::currentStatus() const {
  recalculateStatus();
  return cachedStatus_;
}

bool PrayerSchedule::setConfig(const PrayerScheduleConfig& newConfig) {
  config_ = newConfig;
  recalculateStatus();
  return save();
}

bool PrayerSchedule::setCorrections(const int corrections[PRAYER_COUNT]) {
  for (int i = 0; i < PRAYER_COUNT; ++i) {
    // Bound correction between -30 and +30 minutes
    config_.corrections[i] = std::max(-30, std::min(30, corrections[i]));
  }
  recalculateStatus();
  return save();
}

bool PrayerSchedule::setBaseTimes(const PrayerTimePair baseTimes[PRAYER_COUNT]) {
  for (int i = 0; i < PRAYER_COUNT; ++i) {
    config_.baseTimes[i] = baseTimes[i];
  }
  recalculateStatus();
  return save();
}

bool PrayerSchedule::save() {
  Preferences prefs;
  if (!prefs.begin("prayer-cfg", false)) return false;

  JsonDocument doc;
  doc["schemaVersion"] = config_.schemaVersion;
  doc["source"] = config_.source;
  doc["lat"] = config_.latitude;
  doc["lng"] = config_.longitude;
  doc["tz_name"] = config_.timezoneName;
  doc["tz_offset"] = config_.timezoneOffsetMinutes;
  doc["method"] = config_.calcMethod;

  auto baseObj = doc["base"].to<JsonObject>();
  for (int i = 0; i < PRAYER_COUNT; ++i) {
    baseObj[prayerIdName(static_cast<PrayerId>(i))] = config_.baseTimes[i].toString();
  }

  auto corrObj = doc["corrections"].to<JsonObject>();
  for (int i = 0; i < PRAYER_COUNT; ++i) {
    corrObj[prayerIdName(static_cast<PrayerId>(i))] = config_.corrections[i];
  }

  doc["name_color"] = config_.nameColor;
  doc["time_color"] = config_.timeColor;
  doc["highlight_color"] = config_.highlightColor;
  doc["card_bg_color"] = config_.cardBgColor;
  doc["next_bg_color"] = config_.nextBgColor;

  doc["marquee_enabled"] = config_.marqueeEnabled;
  doc["marquee_text"] = config_.marqueeText;
  doc["marquee_color"] = config_.marqueeColor;
  doc["marquee_direction"] = config_.marqueeDirection;
  doc["marquee_speed"] = config_.marqueeSpeed;
  doc["marquee_hold"] = config_.marqueeHoldSecs;
  doc["marquee_scale"] = config_.marqueeFontScale;

  // Mosque Flow Settings
  doc["adzan_mins"] = config_.adzanDurationMins;
  doc["iqomah_subuh"] = config_.iqomahDurationMins[PRAYER_SUBUH];
  doc["iqomah_zuhur"] = config_.iqomahDurationMins[PRAYER_ZUHUR];
  doc["iqomah_asar"] = config_.iqomahDurationMins[PRAYER_ASAR];
  doc["iqomah_magrib"] = config_.iqomahDurationMins[PRAYER_MAGRIB];
  doc["iqomah_isya"] = config_.iqomahDurationMins[PRAYER_ISYA];
  doc["shalat_mins"] = config_.shalatDurationMins;
  doc["adzan_image"] = config_.adzanImage;
  doc["iqomah_image"] = config_.iqomahImage;
  doc["shalat_image"] = config_.shalatImage;
  doc["dim_shalat"] = config_.dimScreenDuringShalat;
  doc["hijri_adj"] = config_.hijriAdjustment;

  String json;
  serializeJson(doc, json);
  const bool ok = prefs.putString("config", json) == json.length();
  prefs.end();
  return ok;
}

bool PrayerSchedule::load() {
  Preferences prefs;
  if (!prefs.begin("prayer-cfg", true)) return false;
  String json = prefs.getString("config", "");
  prefs.end();

  if (!json.length()) return false;

  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;

  config_.schemaVersion = doc["schemaVersion"] | 1;
  config_.source = doc["source"] | "manual";
  config_.latitude = doc["lat"] | -6.2088;
  config_.longitude = doc["lng"] | 106.8456;
  config_.timezoneName = doc["tz_name"] | "Asia/Jakarta";
  config_.timezoneOffsetMinutes = doc["tz_offset"] | 420;
  config_.calcMethod = doc["method"] | 0;

  if (doc["base"].is<JsonObject>()) {
    auto baseObj = doc["base"].as<JsonObject>();
    for (int i = 0; i < PRAYER_COUNT; ++i) {
      const char* key = prayerIdName(static_cast<PrayerId>(i));
      if (baseObj[key].is<String>()) {
        config_.baseTimes[i] = PrayerTimePair::fromString(baseObj[key].as<String>());
      }
    }
  }

  if (doc["corrections"].is<JsonObject>()) {
    auto corrObj = doc["corrections"].as<JsonObject>();
    for (int i = 0; i < PRAYER_COUNT; ++i) {
      const char* key = prayerIdName(static_cast<PrayerId>(i));
      if (corrObj[key].is<int>()) {
        config_.corrections[i] = corrObj[key].as<int>();
      }
    }
  }

  config_.nameColor = doc["name_color"] | "#00FFFF";
  config_.timeColor = doc["time_color"] | "#FFFFFF";
  config_.highlightColor = doc["highlight_color"] | "#FFD100";
  config_.cardBgColor = doc["card_bg_color"] | "#000000";
  config_.nextBgColor = doc["next_bg_color"] | "#081824";

  config_.marqueeEnabled = doc["marquee_enabled"] | true;
  config_.marqueeText = doc["marquee_text"] | "LURUS DAN RAPATKAN SHAF · MOHON NONAKTIFKAN NADA DERING HP";
  config_.marqueeColor = doc["marquee_color"] | "#FFD100";
  config_.marqueeDirection = doc["marquee_direction"] | "left";
  if (config_.marqueeDirection == "right") {
    config_.marqueeDirection = "left";
  }
  config_.marqueeSpeed = doc["marquee_speed"] | 2;
  config_.marqueeHoldSecs = doc["marquee_hold"] | 10;
  config_.marqueeFontScale = doc["marquee_scale"] | 2;
  if (config_.marqueeFontScale < 2) {
    config_.marqueeFontScale = 2;
  }

  // Mosque Flow Settings
  config_.adzanDurationMins = doc["adzan_mins"] | 3;
  config_.iqomahDurationMins[PRAYER_SUBUH] = doc["iqomah_subuh"] | 10;
  config_.iqomahDurationMins[PRAYER_ZUHUR] = doc["iqomah_zuhur"] | 8;
  config_.iqomahDurationMins[PRAYER_ASAR] = doc["iqomah_asar"] | 7;
  config_.iqomahDurationMins[PRAYER_MAGRIB] = doc["iqomah_magrib"] | 5;
  config_.iqomahDurationMins[PRAYER_ISYA] = doc["iqomah_isya"] | 7;
  config_.shalatDurationMins = doc["shalat_mins"] | 10;
  config_.adzanImage = doc["adzan_image"] | "/media/adzan.jpg";
  config_.iqomahImage = doc["iqomah_image"] | "/media/iqomah.jpg";
  config_.shalatImage = doc["shalat_image"] | "/media/shalat.jpg";
  config_.dimScreenDuringShalat = doc["dim_shalat"] | true;
  config_.hijriAdjustment = doc["hijri_adj"] | 0;

  clockService().setTimezone(config_.timezoneName, config_.timezoneOffsetMinutes);
  return true;
}

}  // namespace prayer
