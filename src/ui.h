#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

#include <vector>

#include "store.h"

// Calendar page layout (portrait 240x320), chosen at boot by UI::configureLayout():
//
//   classic (no ad_ fields)            advertising (ad_ fields in config.txt)
//   0..169    picture                  0..139    picture
//                                      140..183  company / addresses / phone
//   170..193  red header < MONTH >     184..203  red header
//   194..205  weekday initials         204..213  weekday initials
//   206..307  6 rows x 17px            214..303  6 rows x 15px
//   308..319  status line              304..319  motto (holidays when a day is tapped)
namespace Layout {
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
// Color theme: "red" (default), "blue" or "green".
void applyTheme(const String &name);
uint16_t accentColor();
uint16_t accentDarkColor();
// Bold title text vertically centered on its capital letters at cy.
void drawTitle(const String &text, int x, int cy, bool centered);
// Sizes the calendar page from the ad_ fields that are set (none = classic layout).
// Call after settings.loadConfig().
void configureLayout();
bool mottoMode();  // bottom line shows the motto instead of cycling holidays

// --- calendar page ---
enum CalHit { HIT_NONE, HIT_PREV, HIT_NEXT, HIT_TITLE, HIT_IMAGE, HIT_AD, HIT_DAY, HIT_STATUS };
// meta == nullptr means the series has no edition for `year`: a placeholder is shown.
void drawCalendarPage(const SeriesInfo &s, const CalendarMeta *meta, int year, int month, const DateYMD &today,
                      int selDay);
void drawCalendarImage(const SeriesInfo &s, const CalendarMeta *meta, int year, int month);
void drawGrid(int year, int month, const DateYMD &today, int selDay);
void drawStatus(const String &text);  // with a motto: temporary text in the motto line
void drawMotto();
CalHit hitCalendar(int16_t x, int16_t y, int year, int month, int &day);

// Time in a small dark tab over the picture's top-left corner (calendar page only).
void drawClock(const String &text);

// --- full-screen calendar (month grid filling the screen) ---
// While on, drawGrid/drawStatus/hitCalendar use the big geometry. Turn off before
// drawing the regular calendar page again.
void setBigCalendar(bool on);
bool bigCalendar();
void drawBigCalendar(int year, int month, const DateYMD &today, int selDay);

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
enum MenuHit { MHIT_NONE, MHIT_CLOSE, MHIT_CAT, MHIT_ROW, MHIT_UP, MHIT_DOWN, MHIT_SYNC, MHIT_SETUP };
void drawMenu(const MenuState &s);
MenuHit hitMenu(int16_t x, int16_t y, int &row);

// --- misc ---
void drawMessage(const String &title, const String &l1 = "", const String &l2 = "", const String &l3 = "");
void drawProgress(const String &title, const String &what, int done, int total,
                  const char *heading = "DOWNLOADING");
// "Thu Nov 26  <names>"
String formatDayLine(int year, int month, int day, const String &names);
}  // namespace UI
