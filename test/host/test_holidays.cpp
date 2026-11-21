// Host-side test for the holiday engine:
//   g++ -std=c++17 -I src test/host/test_holidays.cpp src/holidays.cpp -o /tmp/th && /tmp/th
#include <cstdio>
#include <cstring>
#include "holidays.h"

static int failures = 0;

static bool has(int y, int m, int d, const char *name) {
  Holiday h[16];
  int n = holidaysInMonth(y, m, h, 16);
  for (int i = 0; i < n; i++)
    if (h[i].day == d && strcmp(h[i].name, name) == 0) return true;
  return false;
}

#define EXPECT(y, m, d, name)                                                \
  do {                                                                       \
    if (!has(y, m, d, name)) {                                               \
      printf("FAIL: expected %04d-%02d-%02d %s\n", y, m, d, name);           \
      failures++;                                                            \
    }                                                                        \
  } while (0)

int main() {
  // 2026 federal calendar (opm.gov)
  EXPECT(2026, 1, 1, "New Year's Day");
  EXPECT(2026, 1, 19, "Martin Luther King Jr. Day");
  EXPECT(2026, 2, 16, "Presidents' Day");
  EXPECT(2026, 5, 25, "Memorial Day");
  EXPECT(2026, 6, 19, "Juneteenth");
  EXPECT(2026, 7, 3, "Independence Day (observed)");  // Jul 4 2026 is a Saturday
  EXPECT(2026, 9, 7, "Labor Day");
  EXPECT(2026, 10, 12, "Columbus Day");
  EXPECT(2026, 11, 11, "Veterans Day");
  EXPECT(2026, 11, 26, "Thanksgiving");
  EXPECT(2026, 12, 25, "Christmas Day");
  EXPECT(2026, 11, 3, "Election Day");
  EXPECT(2026, 3, 8, "Daylight Saving Time begins");
  EXPECT(2026, 11, 1, "Daylight Saving Time ends");
  EXPECT(2026, 4, 5, "Easter Sunday");

  // 2027: Jun 19 Sat -> Fri 18, Jul 4 Sun -> Mon 5, Dec 25 Sat -> Fri 24, Jan 1 2028 Sat -> Dec 31 2027
  EXPECT(2027, 6, 18, "Juneteenth (observed)");
  EXPECT(2027, 7, 5, "Independence Day (observed)");
  EXPECT(2027, 12, 24, "Christmas (observed)");
  EXPECT(2027, 12, 31, "New Year's (observed)");
  EXPECT(2027, 11, 25, "Thanksgiving");
  EXPECT(2027, 3, 28, "Easter Sunday");
  EXPECT(2027, 5, 9, "Mother's Day");
  EXPECT(2027, 6, 20, "Father's Day");

  // 2028: Nov 11 Sat -> Fri 10; Election Day Nov 7
  EXPECT(2028, 11, 10, "Veterans Day (observed)");
  EXPECT(2028, 11, 7, "Election Day");
  EXPECT(2028, 4, 16, "Easter Sunday");

  // Sanity: day-of-week and leap years
  if (dayOfWeek(2026, 11, 20) != 5) { puts("FAIL: 2026-11-20 should be Friday"); failures++; }
  if (daysInMonth(2028, 2) != 29 || daysInMonth(2100, 2) != 28) { puts("FAIL: leap years"); failures++; }

  // Ordering is by day
  Holiday h[16];
  for (int y = 2020; y <= 2040; y++)
    for (int m = 1; m <= 12; m++) {
      int n = holidaysInMonth(y, m, h, 16);
      for (int i = 1; i < n; i++)
        if (h[i].day < h[i - 1].day) { printf("FAIL: unsorted %d-%d\n", y, m); failures++; }
      for (int i = 0; i < n; i++)
        if (h[i].day < 1 || h[i].day > daysInMonth(y, m)) { printf("FAIL: bad day %d-%d\n", y, m); failures++; }
    }

  printf(failures ? "%d failure(s)\n" : "All holiday tests passed\n", failures);
  return failures ? 1 : 0;
}
