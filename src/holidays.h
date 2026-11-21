#pragma once
// US holiday calculator. Pure C++ (no Arduino deps) so it can be unit-tested on a PC.
#include <stdint.h>

enum HolidayKind : uint8_t {
  HOLIDAY_FEDERAL = 0,   // federal holiday (banks / post office closed)
  HOLIDAY_OBSERVED = 1,  // weekday a federal holiday is observed on when it falls on a weekend
  HOLIDAY_OBSERVANCE = 2 // popular non-federal day (Valentine's, Easter, Halloween...)
};

struct Holiday {
  uint8_t day;           // 1..31
  HolidayKind kind;
  const char *name;
};

// 0 = Sunday .. 6 = Saturday
int dayOfWeek(int year, int month, int day);
int daysInMonth(int year, int month);
bool isLeapYear(int year);

// Fills `out` with the holidays in the given month, sorted by day.
// Returns the number written (never more than maxOut).
int holidaysInMonth(int year, int month, Holiday *out, int maxOut);

// Easter Sunday (Gregorian). Writes month (3 or 4) and day.
void easterSunday(int year, int &month, int &day);
