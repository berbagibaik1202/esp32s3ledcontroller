#pragma once

#include <Arduino.h>

namespace prayer {

enum PrayerId : uint8_t {
  PRAYER_SUBUH = 0,
  PRAYER_ZUHUR = 1,
  PRAYER_ASAR = 2,
  PRAYER_MAGRIB = 3,
  PRAYER_ISYA = 4,
  PRAYER_COUNT = 5
};

inline const char* prayerIdName(PrayerId id) {
  switch (id) {
    case PRAYER_SUBUH: return "subuh";
    case PRAYER_ZUHUR: return "zuhur";
    case PRAYER_ASAR: return "asar";
    case PRAYER_MAGRIB: return "magrib";
    case PRAYER_ISYA: return "isya";
    default: return "unknown";
  }
}

inline const char* prayerLabel(PrayerId id) {
  switch (id) {
    case PRAYER_SUBUH: return "SUBUH";
    case PRAYER_ZUHUR: return "ZUHUR";
    case PRAYER_ASAR: return "ASAR";
    case PRAYER_MAGRIB: return "MAGRIB";
    case PRAYER_ISYA: return "ISYA";
    default: return "?";
  }
}

inline int parsePrayerId(const String& name) {
  String s = name;
  s.toLowerCase();
  s.trim();
  if (s == "subuh" || s == "fajr") return PRAYER_SUBUH;
  if (s == "zuhur" || s == "dhuhr" || s == "dzuhur") return PRAYER_ZUHUR;
  if (s == "asar" || s == "asr") return PRAYER_ASAR;
  if (s == "magrib" || s == "maghrib") return PRAYER_MAGRIB;
  if (s == "isya" || s == "isha") return PRAYER_ISYA;
  return -1;
}

struct PrayerTimePair {
  int hour{0};
  int minute{0};

  PrayerTimePair() = default;
  PrayerTimePair(int h, int m) : hour(h), minute(m) {}

  String toString() const {
    char buf[8];
    snprintf(buf, sizeof buf, "%02d:%02d", hour, minute);
    return String(buf);
  }

  int totalMinutes() const {
    return hour * 60 + minute;
  }

  static PrayerTimePair fromString(const String& str) {
    PrayerTimePair pt;
    int colon = str.indexOf(':');
    if (colon > 0) {
      pt.hour = str.substring(0, colon).toInt();
      pt.minute = str.substring(colon + 1).toInt();
    }
    return pt;
  }

  static PrayerTimePair fromMinutes(int mins) {
    while (mins < 0) mins += 1440;
    mins %= 1440;
    PrayerTimePair pt;
    pt.hour = mins / 60;
    pt.minute = mins % 60;
    return pt;
  }
};

struct PrayerScheduleConfig {
  uint32_t schemaVersion{1};
  String source{"auto"}; // "auto" or "manual"
  double latitude{-6.2088}; // Jakarta default
  double longitude{106.8456};
  String timezoneName{"Asia/Jakarta"};
  int timezoneOffsetMinutes{420}; // UTC+7 = +420 mins
  int calcMethod{0}; // 0 = Kemenag RI, 1 = MWL, 2 = Egyptian, 3 = Umm al-Qura, 4 = Karachi

  // Base schedule for the 5 prayers in "HH:MM"
  PrayerTimePair baseTimes[PRAYER_COUNT];

  // Independent correction in minutes (-30 to +30)
  int corrections[PRAYER_COUNT]{0, 0, 0, 0, 0};

  // Display Colors for LED Panel (Hex #RRGGBB)
  String nameColor{"#00FFFF"};       // Default bright cyan
  String timeColor{"#FFFFFF"};       // Default bright white
  String highlightColor{"#FFD100"};  // Default bright gold/yellow for next prayer
  String cardBgColor{"#000000"};     // Default pure black (LEDs OFF, zero pink/purple glow)
  String nextBgColor{"#081824"};     // Default subtle deep navy for highlight card

  // Middle Section (Zone 2) Marquee / Running Text
  bool marqueeEnabled{true};
  String marqueeText{"LURUS DAN RAPATKAN SHAF · MOHON NONAKTIFKAN NADA DERING HP"};
  String marqueeColor{"#FFD100"};
  String marqueeDirection{"left"};  // "left" = ke sebelah kiri, "right" = ke kanan
  int marqueeSpeed{2};              // 1 = Lambat, 2 = Sedang, 3 = Cepat
  int marqueeHoldSecs{10};          // Durasi tampil info salat sebelum bergantian ke marquee (detik)
  uint8_t marqueeFontScale{2};      // 1 = Normal (5x7), 2 = Besar (10x14 - default)

  // Mosque Flow Settings (Adzan, Iqomah, Shalat)
  int adzanDurationMins{3};                          // Durasi tampilan Adzan (menit, default 3)
  int iqomahDurationMins[PRAYER_COUNT]{10, 8, 7, 5, 7}; // Durasi hitung mundur Iqomah per salat: Subuh(10), Zuhur(8), Asar(7), Magrib(5), Isya(7)
  int shalatDurationMins{10};                        // Durasi tampilan Shalat (menit, default 10)
  String adzanImage{"/media/adzan.jpg"};             // Background image untuk tampilan Adzan
  String iqomahImage{"/media/iqomah.jpg"};           // Background image untuk tampilan Iqomah
  String shalatImage{"/media/shalat.jpg"};           // Background image untuk tampilan Shalat
  bool dimScreenDuringShalat{true};                  // Meredupkan panel setelah pesan shalat tampil agar tidak silau
  int hijriAdjustment{0};                            // Koreksi tanggal Hijriah (-3 s.d. +3 hari)
};

enum MosqueState : uint8_t {
  MOSQUE_STATE_NORMAL = 0, // Jam & Jadwal Salat Standar
  MOSQUE_STATE_ADZAN  = 1, // Tampilan Adzan (Background + Teks Saatnya Adzan)
  MOSQUE_STATE_IQOMAH = 2, // Tampilan Iqomah (Background + Countdown Iqomah)
  MOSQUE_STATE_SHALAT = 3  // Tampilan Shalat (Background + Teks Saatnya Shalat)
};

inline const char* mosqueStateName(MosqueState s) {
  switch (s) {
    case MOSQUE_STATE_ADZAN:  return "ADZAN";
    case MOSQUE_STATE_IQOMAH: return "IQOMAH";
    case MOSQUE_STATE_SHALAT: return "SHALAT";
    default:                  return "NORMAL";
  }
}

inline uint16_t hexToRgb565(const String& hex, uint16_t defaultColor) {
  String h = hex;
  if (h.startsWith("#")) h = h.substring(1);
  if (h.length() != 6) return defaultColor;
  long val = strtol(h.c_str(), nullptr, 16);
  uint8_t r = (val >> 16) & 0xFF;
  uint8_t g = (val >> 8) & 0xFF;
  uint8_t b = val & 0xFF;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

inline String rgb565ToHex(uint16_t c) {
  uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((c >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (c & 0x1F) * 255 / 31;
  char buf[8];
  snprintf(buf, sizeof buf, "#%02X%02X%02X", r, g, b);
  return String(buf);
}

struct PrayerAudioItem {
  bool enabled{false};
  String file{""};
  int leadMinutes{10}; // minutes before effective prayer time (1-60)
  uint8_t volume{80};  // 1-100%
};

struct PrayerAudioConfig {
  PrayerAudioItem items[PRAYER_COUNT];
};

struct PrayerStatus {
  PrayerTimePair base[PRAYER_COUNT];
  int correction[PRAYER_COUNT];
  PrayerTimePair effective[PRAYER_COUNT];

  int nextPrayerIndex{-1};
  String nextPrayerName{""};
  PrayerTimePair nextPrayerTime;
  int countdownSeconds{0}; // seconds to next prayer
  bool isTodayNext{true};

  int currentPrayerIndex{-1};
  String currentPrayerName{""};

  // Additional times
  PrayerTimePair imsak;
  PrayerTimePair syuruq;
  PrayerTimePair duha;

  // Mosque Flow State
  MosqueState mosqueState{MOSQUE_STATE_NORMAL};
  int activePrayerIndex{-1};
  String activePrayerName{""};
  int stateRemainingSeconds{0}; // Detik tersisa untuk state saat ini (hitung mundur adzan, iqomah, shalat)
  int stateTotalSeconds{0};     // Total durasi state saat ini (detik)
};

}  // namespace prayer
