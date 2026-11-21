#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

#include <vector>

#include "store.h"

// Screen layout (portrait 240x320)
//   0..169    pin-up image (cropped from a 240x320 picture)
//   170..193  red header:  <  NOVEMBER 2026  >
//   194..205  weekday initials
//   206..307  6 week rows x 17px
//   308..319  status line (holiday names)
namespace Layout {
const int IMG_H = 170;
const int HDR_Y = 170, HDR_H = 24;
const int DOW_Y = 194, DOW_H = 12;
const int GRID_Y = 206, ROW_H = 17;
const int STATUS_Y = 308, STATUS_H = 12;

// menu
const int MENU_TITLE_H = 28;
const int MENU_CAT_Y = 28, MENU_CAT_H = 26;
const int MENU_LIST_Y = 54, MENU_ROW_H = 36, MENU_ROWS = 6;
const int MENU_BTN_Y = 270, MENU_BTN_H = 50;
}  // namespace Layout

struct DateYMD {
  int y = 0, m = 0, d = 0;
};

namespace UI {
extern TFT_eSPI tft;

void begin();

// --- calendar page ---
enum CalHit { HIT_NONE, HIT_PREV, HIT_NEXT, HIT_TITLE, HIT_IMAGE, HIT_DAY, HIT_STATUS };
// meta == nullptr means the series has no edition for `year`: a placeholder is shown.
void drawCalendarPage(const SeriesInfo &s, const CalendarMeta *meta, int year, int month, const DateYMD &today,
                      int selDay);
void drawCalendarImage(const SeriesInfo &s, const CalendarMeta *meta, int year, int month);
void drawGrid(int year, int month, const DateYMD &today, int selDay);
void drawStatus(const String &text);
CalHit hitCalendar(int16_t x, int16_t y, int year, int month, int &day);

// --- full-screen pin-up ---
void drawFullImage(const SeriesInfo &s, const CalendarMeta *meta, int year, int month);

// --- calendar picker ---
struct MenuState {
  std::vector<SeriesEntry> all;
  std::vector<String> categories;  // "All" + unique categories
  int catIndex = 0;
  int scroll = 0;
  String currentId;
  bool online = false;
  std::vector<int> visible() const;  // indices into `all`
};
enum MenuHit { MHIT_NONE, MHIT_CLOSE, MHIT_CAT, MHIT_ROW, MHIT_UP, MHIT_DOWN, MHIT_SYNC };
void drawMenu(const MenuState &s);
MenuHit hitMenu(int16_t x, int16_t y, int &row);

// --- misc ---
void drawMessage(const String &title, const String &l1 = "", const String &l2 = "", const String &l3 = "");
void drawProgress(const String &title, const String &what, int done, int total,
                  const char *heading = "DOWNLOADING");
// "Thu Nov 26  <names>"
String formatDayLine(int year, int month, int day, const String &names);
}  // namespace UI
