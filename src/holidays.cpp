#include "holidays.h"

bool isLeapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
  static const uint8_t d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (m == 2 && isLeapYear(y)) ? 29 : d[m - 1];
}

int dayOfWeek(int y, int m, int d) {  // Sakamoto
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// n-th (1-based) weekday `wd` of the month
static int nthWeekday(int y, int m, int wd, int n) {
  int first = dayOfWeek(y, m, 1);
  return 1 + (wd - first + 7) % 7 + (n - 1) * 7;
}

static int lastWeekday(int y, int m, int wd) {
  int last = daysInMonth(y, m);
  return last - (dayOfWeek(y, m, last) - wd + 7) % 7;
}

void easterSunday(int y, int &month, int &day) {  // Anonymous Gregorian algorithm
  int a = y % 19, b = y / 100, c = y % 100, d = b / 4, e = b % 4;
  int f = (b + 8) / 25, g = (b - f + 1) / 3;
  int h = (19 * a + b - d - g + 15) % 30;
  int i = c / 4, k = c % 4;
  int l = (32 + 2 * e + 2 * i - h - k) % 7;
  int m = (a + 11 * h + 22 * l) / 451;
  month = (h + l - 7 * m + 114) / 31;
  day = ((h + l - 7 * m + 114) % 31) + 1;
}

namespace {
struct Builder {
  Holiday *out;
  int max, n;
  Builder(Holiday *o, int m) : out(o), max(m), n(0) {}
  void add(int day, HolidayKind k, const char *name) {
    if (n >= max) return;
    // insertion sort by day; federal sorts before observance on the same day
    int i = n++;
    while (i > 0 && (out[i - 1].day > day || (out[i - 1].day == day && out[i - 1].kind > k))) {
      out[i] = out[i - 1];
      --i;
    }
    out[i] = Holiday{(uint8_t)day, k, name};
  }
};

// Fixed-date federal holiday plus its weekend "observed" day if that day lands in this month.
void fixedFederal(Builder &b, int y, int m, int fm, int fd, const char *name, const char *obsName) {
  if (m == fm) b.add(fd, HOLIDAY_FEDERAL, name);
  int dow = dayOfWeek(y, fm, fd);
  if (dow == 0 && m == fm) b.add(fd + 1, HOLIDAY_OBSERVED, obsName);  // Sun -> Mon
  if (dow == 6) {                                                     // Sat -> Fri
    if (fd > 1 && m == fm) b.add(fd - 1, HOLIDAY_OBSERVED, obsName);
  }
}
}  // namespace

int holidaysInMonth(int y, int m, Holiday *out, int maxOut) {
  Builder b(out, maxOut);

  // ---- Federal holidays (5 U.S.C. 6103) ----
  fixedFederal(b, y, m, 1, 1, "New Year's Day", "New Year's (observed)");
  // Jan 1 on a Saturday is observed Friday Dec 31 of the previous year.
  if (m == 12 && dayOfWeek(y + 1, 1, 1) == 6) b.add(31, HOLIDAY_OBSERVED, "New Year's (observed)");
  if (m == 1 && y >= 1986) b.add(nthWeekday(y, 1, 1, 3), HOLIDAY_FEDERAL, "Martin Luther King Jr. Day");
  if (m == 2) b.add(nthWeekday(y, 2, 1, 3), HOLIDAY_FEDERAL, "Presidents' Day");
  if (m == 5) b.add(lastWeekday(y, 5, 1), HOLIDAY_FEDERAL, "Memorial Day");
  if (y >= 2021) fixedFederal(b, y, m, 6, 19, "Juneteenth", "Juneteenth (observed)");
  fixedFederal(b, y, m, 7, 4, "Independence Day", "Independence Day (observed)");
  if (m == 9) b.add(nthWeekday(y, 9, 1, 1), HOLIDAY_FEDERAL, "Labor Day");
  if (m == 10) b.add(nthWeekday(y, 10, 1, 2), HOLIDAY_FEDERAL, "Columbus Day");
  fixedFederal(b, y, m, 11, 11, "Veterans Day", "Veterans Day (observed)");
  if (m == 11) b.add(nthWeekday(y, 11, 4, 4), HOLIDAY_FEDERAL, "Thanksgiving");
  fixedFederal(b, y, m, 12, 25, "Christmas Day", "Christmas (observed)");

  // ---- Popular observances ----
  switch (m) {
    case 2:
      b.add(2, HOLIDAY_OBSERVANCE, "Groundhog Day");
      b.add(14, HOLIDAY_OBSERVANCE, "Valentine's Day");
      break;
    case 3:
      b.add(nthWeekday(y, 3, 0, 2), HOLIDAY_OBSERVANCE, "Daylight Saving Time begins");
      b.add(17, HOLIDAY_OBSERVANCE, "St. Patrick's Day");
      break;
    case 4:
      b.add(15, HOLIDAY_OBSERVANCE, "Tax Day");
      break;
    case 5:
      b.add(nthWeekday(y, 5, 0, 2), HOLIDAY_OBSERVANCE, "Mother's Day");
      break;
    case 6:
      b.add(14, HOLIDAY_OBSERVANCE, "Flag Day");
      b.add(nthWeekday(y, 6, 0, 3), HOLIDAY_OBSERVANCE, "Father's Day");
      break;
    case 10:
      b.add(31, HOLIDAY_OBSERVANCE, "Halloween");
      break;
    case 11:
      b.add(nthWeekday(y, 11, 0, 1), HOLIDAY_OBSERVANCE, "Daylight Saving Time ends");
      // Election Day: Tuesday after the first Monday
      b.add(nthWeekday(y, 11, 1, 1) + 1, HOLIDAY_OBSERVANCE, "Election Day");
      break;
    case 12:
      b.add(24, HOLIDAY_OBSERVANCE, "Christmas Eve");
      b.add(31, HOLIDAY_OBSERVANCE, "New Year's Eve");
      break;
  }
  int em, ed;
  easterSunday(y, em, ed);
  if (m == em) b.add(ed, HOLIDAY_OBSERVANCE, "Easter Sunday");

  return b.n;
}
