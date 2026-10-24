#pragma once

#include <Arduino.h>

namespace prayer {

struct HijriDate {
  int year;
  int month;
  int day;
  String formatted;
};

// Converts Gregorian (year, month 1-12, day 1-31) to Hijri date
HijriDate gregorianToHijri(int gYear, int gMonth, int gDay, int adjustmentDays = 0);

const char* hijriMonthName(int month);
const char* indonesianDayName(int wday); // 0=Minggu/Ahad, 1=Senin, ...
const char* indonesianMonthName(int month); // 1=Januari, ...

}  // namespace prayer
