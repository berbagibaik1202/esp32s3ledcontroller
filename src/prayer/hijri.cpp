#include "hijri.h"
#include <cmath>

namespace prayer {

namespace {
const char* const kHijriMonths[] = {
  "", "Muharram", "Safar", "Rabiul Awal", "Rabiul Akhir",
  "Jumadil Awal", "Jumadil Akhir", "Rajab", "Sya'ban",
  "Ramadhan", "Syawal", "Zulkaidah", "Zulhijjah"
};

const char* const kIndoDays[] = {
  "Ahad", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"
};

const char* const kIndoMonths[] = {
  "", "Januari", "Februari", "Maret", "April", "Mei", "Juni",
  "Juli", "Agustus", "September", "Oktober", "November", "Desember"
};

bool isHijriLeapYear(int year) {
  // Standard 30-year cycle leap years: 2, 5, 7, 10, 13, 16, 18, 21, 24, 26, 29
  int r = year % 30;
  return (r == 2 || r == 5 || r == 7 || r == 10 || r == 13 ||
          r == 16 || r == 18 || r == 21 || r == 24 || r == 26 || r == 29);
}

int hijriYearDays(int year) {
  return isHijriLeapYear(year) ? 355 : 354;
}

int hijriMonthDays(int year, int month) {
  if (month == 1 || month == 3 || month == 5 || month == 7 || month == 9 || month == 11) {
    return 30;
  } else if (month == 2 || month == 4 || month == 6 || month == 8 || month == 10) {
    return 29;
  } else if (month == 12) {
    return isHijriLeapYear(year) ? 30 : 29;
  }
  return 30;
}
} // namespace

const char* hijriMonthName(int month) {
  if (month >= 1 && month <= 12) return kHijriMonths[month];
  return "";
}

const char* indonesianDayName(int wday) {
  if (wday >= 0 && wday <= 6) return kIndoDays[wday];
  return "";
}

const char* indonesianMonthName(int month) {
  if (month >= 1 && month <= 12) return kIndoMonths[month];
  return "";
}

HijriDate gregorianToHijri(int gYear, int gMonth, int gDay, int adjustmentDays) {
  int m = gMonth;
  int y = gYear;
  if (m <= 2) {
    y -= 1;
    m += 12;
  }

  int a = y / 100;
  int b = 2 - a + (a / 4);
  long jd = static_cast<long>(std::floor(365.25 * (y + 4716))) +
            static_cast<long>(std::floor(30.6001 * (m + 1))) +
            gDay + b - 1524;
  jd += adjustmentDays;

  // Reference epoch: 1 Muharram 1 AH = Julian Day 1948440
  constexpr long kEpochJd = 1948440L;
  long days = jd - kEpochJd;

  long cycle = days / 10631L;
  long remDays = days % 10631L;
  while (remDays < 0) {
    remDays += 10631L;
    cycle -= 1;
  }

  int hYear = static_cast<int>(cycle * 30 + 1);
  while (true) {
    int yd = hijriYearDays(hYear);
    if (remDays >= yd) {
      remDays -= yd;
      hYear += 1;
    } else {
      break;
    }
  }

  int hMonth = 1;
  while (true) {
    int md = hijriMonthDays(hYear, hMonth);
    if (remDays >= md) {
      remDays -= md;
      hMonth += 1;
    } else {
      break;
    }
  }

  int hDay = static_cast<int>(remDays + 1);

  HijriDate res;
  res.year = hYear;
  res.month = hMonth;
  res.day = hDay;

  char buf[48];
  snprintf(buf, sizeof buf, "%d %s %d H", res.day, hijriMonthName(res.month), res.year);
  res.formatted = String(buf);
  return res;
}

}  // namespace prayer
