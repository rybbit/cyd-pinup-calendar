// Pin-Up Calendar for the ESP32-2432S028R "Cheap Yellow Display"
//
// Calendar page:  tap < / > in the red bar to change month, tap the month name for the
//                 calendar picker, tap the picture to see it full screen, tap a day to see
//                 its holidays, tap the bottom line to jump back to today.
// Boot:           hold a finger/stylus on the screen while "Hold to recalibrate" is shown
//                 to redo touch calibration.
//
// A calendar is a SERIES with one edition per year. Moving into a year the series has no
// edition for shows a placeholder; the grid and holidays still work.
#include <Arduino.h>
#include <time.h>

#include "config.h"
#include "holidays.h"
#include "net.h"
#include "settings.h"
#include "store.h"
#include "touch.h"
#include "ui.h"

namespace {
enum Screen { SCR_CAL, SCR_FULL, SCR_MENU };
Screen screen = SCR_MENU;

SeriesInfo series;
bool haveSeries = false;
CalendarMeta yearMeta;  // edition for metaYear (cached)
bool haveYearMeta = false;
int metaYear = 0;

int viewY = 2026, viewM = 1;
DateYMD today;

int selDay = 0;
uint32_t selUntil = 0;

Holiday monthHol[16];
int monthHolN = 0, statusIdx = 0;
uint32_t statusNext = 0;
String lastStatus;

UI::MenuState menu;

DateYMD getToday() {
  DateYMD t;
  if (!Net::timeValid()) return t;
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  t.y = tm.tm_year + 1900;
  t.m = tm.tm_mon + 1;
  t.d = tm.tm_mday;
  return t;
}

// Edition for the year being viewed, or nullptr if the series has none.
const CalendarMeta *currentMeta() {
  if (metaYear != viewY) {
    haveYearMeta = haveSeries && Store::loadYear(series.id, viewY, yearMeta);
    metaYear = viewY;
  }
  return haveYearMeta ? &yearMeta : nullptr;
}

String editionTitle() {
  const CalendarMeta *m = currentMeta();
  if (m && m->title.length()) return m->title;
  return series.title + " " + viewY;
}

void setStatus(const String &s) {
  if (s == lastStatus) return;
  lastStatus = s;
  UI::drawStatus(s);
}

void resetStatus() {
  monthHolN = holidaysInMonth(viewY, viewM, monthHol, 16);
  statusIdx = 0;
  statusNext = 0;
  lastStatus = "\x01";  // force redraw
}

void showCalendar() {
  screen = SCR_CAL;
  UI::drawCalendarPage(series, currentMeta(), viewY, viewM, today, selDay);
  resetStatus();
}

void goToToday() {
  if (today.y) {
    viewY = today.y;
    viewM = today.m;
  } else {
    viewY = series.years.empty() ? 2026 : series.years.front();
    viewM = 1;
  }
  selDay = 0;
}

// Bottom line: the edition's title, then each of this month's holidays in turn.
void updateStatus() {
  uint32_t now = millis();
  if (selDay) {
    if (now < selUntil) return;
    selDay = 0;
    UI::drawGrid(viewY, viewM, today, 0);
    statusNext = 0;
  }
  if (now < statusNext) return;
  statusNext = now + 3500;
  int item = statusIdx++ % (monthHolN + 1);
  if (item == 0) {
    setStatus(today.y ? editionTitle() : editionTitle() + "  (clock not set)");
    return;
  }
  const Holiday &h = monthHol[item - 1];
  setStatus(UI::formatDayLine(viewY, viewM, h.day, h.name));
}

void selectDay(int d) {
  selDay = d;
  selUntil = millis() + 8000;
  String names;
  for (int i = 0; i < monthHolN; i++)
    if (monthHol[i].day == d) names += (names.length() ? " / " : "") + String(monthHol[i].name);
  UI::drawGrid(viewY, viewM, today, selDay);
  setStatus(UI::formatDayLine(viewY, viewM, d, names));
}

void changeMonth(int delta) {
  viewM += delta;
  if (viewM < 1) { viewM = 12; viewY--; }
  if (viewM > 12) { viewM = 1; viewY++; }
  selDay = 0;
  showCalendar();
}

// Selects a series that is on the SD card. keepView keeps the month being viewed.
bool loadSeries(const String &id, bool keepView = false) {
  SeriesInfo s;
  if (!Store::loadSeries(id, s)) return false;
  series = s;
  haveSeries = true;
  metaYear = 0;  // force the edition to reload
  if (settings.calId != id) {
    settings.calId = id;
    settings.save();
  }
  if (!keepView) goToToday();
  return true;
}

// ---------------------------------------------------------------- menu

void openMenu(bool reload = true) {
  screen = SCR_MENU;
  if (reload) {
    UI::drawMessage("PIN-UP CALENDARS", "Loading...");
    menu.all = Store::loadCatalog();
    menu.categories = {"All"};
    for (auto &e : menu.all) {
      bool seen = false;
      for (auto &c : menu.categories) seen |= (c == e.category);
      if (!seen) menu.categories.push_back(e.category);
    }
    if (menu.catIndex >= (int)menu.categories.size()) menu.catIndex = 0;
  }
  menu.currentId = haveSeries ? series.id : "";
  menu.online = Net::online();
  int n = menu.visible().size();
  menu.scroll = constrain(menu.scroll, 0, max(0, n - Layout::MENU_ROWS));
  UI::drawMenu(menu);
}

bool fetchSeries(const SeriesEntry &e) {
  const char *heading = e.downloaded() ? "UPDATING" : "DOWNLOADING";
  return Store::downloadSeries(
      e, [&](const String &label, int done, int total) { UI::drawProgress(e.title, label, done, total, heading); });
}

// If GitHub has anything newer for the selected series, download it and reload,
// keeping the month being viewed. Returns true if the screen was replaced.
bool updateSelectedSeries() {
  if (!haveSeries || series.custom || !Net::online()) return false;
  SeriesEntry e;
  if (!Store::findEntry(series.id, e) || !e.updateAvailable()) return false;
  Serial.printf("Updating series %s\n", e.id.c_str());
  fetchSeries(e);
  loadSeries(e.id, true);
  return true;
}

void pickSeries(const SeriesEntry &e) {
  bool onCard = e.custom || e.localVersion.length();
  if (!e.custom && (!e.downloaded() || e.updateAvailable())) {
    if (!Net::online()) {
      if (!onCard) {
        UI::drawMessage("OFFLINE", "Wi-Fi is needed to download", e.title);
        delay(2000);
        openMenu(false);
        return;
      }
    } else if (!fetchSeries(e) && !onCard) {
      UI::drawMessage("DOWNLOAD FAILED", e.title, "Check Wi-Fi and try again.", "Saved images are kept.");
      delay(2500);
      openMenu(true);
      return;
    }
  }
  if (!loadSeries(e.id)) {
    UI::drawMessage("ERROR", "Could not open", e.title);
    delay(2000);
    openMenu(true);
    return;
  }
  showCalendar();
}

void handleMenuTap(int16_t x, int16_t y) {
  int row;
  auto vis = menu.visible();
  switch (UI::hitMenu(x, y, row)) {
    case UI::MHIT_CLOSE:
      if (haveSeries) showCalendar();
      break;
    case UI::MHIT_CAT:
      menu.catIndex = (menu.catIndex + 1) % max<int>(1, menu.categories.size());
      menu.scroll = 0;
      openMenu(false);
      break;
    case UI::MHIT_UP:
      if (menu.scroll > 0) {
        menu.scroll = max(0, menu.scroll - Layout::MENU_ROWS);
        openMenu(false);
      }
      break;
    case UI::MHIT_DOWN:
      if (menu.scroll + Layout::MENU_ROWS < (int)vis.size()) {
        menu.scroll += Layout::MENU_ROWS;
        openMenu(false);
      }
      break;
    case UI::MHIT_SYNC:
      if (Net::online()) {
        UI::drawMessage("SYNC", "Fetching calendar list...");
        if (!Store::refreshCatalog()) {
          UI::drawMessage("SYNC FAILED", "Could not download", "catalog.json", "Check catalog= in wifi.conf");
          delay(2500);
        } else {
          updateSelectedSeries();
        }
        openMenu(true);
      }
      break;
    case UI::MHIT_ROW:
      if (menu.scroll + row < (int)vis.size()) pickSeries(menu.all[vis[menu.scroll + row]]);
      break;
    default:
      break;
  }
}

void handleCalendarTap(int16_t x, int16_t y) {
  int day;
  switch (UI::hitCalendar(x, y, viewY, viewM, day)) {
    case UI::HIT_PREV: changeMonth(-1); break;
    case UI::HIT_NEXT: changeMonth(+1); break;
    case UI::HIT_TITLE: openMenu(true); break;
    case UI::HIT_IMAGE:
      screen = SCR_FULL;
      UI::drawFullImage(series, currentMeta(), viewY, viewM);
      break;
    case UI::HIT_DAY: selectDay(day); break;
    case UI::HIT_STATUS:
      if (today.y && (viewY != today.y || viewM != today.m)) {
        goToToday();
        showCalendar();
      }
      break;
    default: break;
  }
}

// Runs when Wi-Fi comes back after being down: set the clock, fetch the catalog,
// update the selected series and refresh whatever is on screen.
void onlineTasks() {
  if (!Net::timeValid()) Net::syncTime(5000);
  bool fresh = Store::refreshCatalog();
  if (screen == SCR_CAL) {
    if (fresh && updateSelectedSeries()) showCalendar();
  } else if (screen == SCR_MENU) {
    openMenu(true);
  }
}

// Background Wi-Fi: retry every 2 minutes while offline (non-blocking), and run
// onlineTasks() when the connection comes back. Replaces the old WI-FI button.
void maintainWifi() {
  static bool first = true, wasOnline = false;
  static uint32_t lastTry = 0;
  bool on = Net::online();
  if (first) {
    first = false;
    wasOnline = on;
    lastTry = millis();
  }
  if (on && !wasOnline) {
    Serial.println("Wi-Fi connected");
    onlineTasks();
  }
  wasOnline = on;
  if (!on && settings.ssid.length() && millis() - lastTry > 120000) {
    lastTry = millis();
    Net::kick();
  }
}

// Every 6 hours, look for a newer catalog and update the selected series.
void checkForUpdates() {
  static uint32_t last = millis();
  if (millis() - last < 6UL * 3600 * 1000 || screen != SCR_CAL) return;
  last = millis();
  if (Net::online() && Store::refreshCatalog() && updateSelectedSeries()) showCalendar();
}

void checkDateChange() {
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  DateYMD t = getToday();
  if (t.y == today.y && t.m == today.m && t.d == today.d) return;
  bool followToday = !today.y || (viewY == today.y && viewM == today.m);
  today = t;
  if (!haveSeries) return;
  if (followToday && t.y) {
    viewY = t.y;
    viewM = t.m;
  }
  if (screen == SCR_CAL) showCalendar();
}

// Older firmware saved selections like "bikinis-2027"; the series is now "bikinis".
String migrateSelection(const String &id) {
  int dash = id.lastIndexOf('-');
  if (dash > 0 && id.length() - dash == 5 && id.substring(dash + 1).toInt() >= 1900) return id.substring(0, dash);
  return id;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  UI::begin();
  Touch::begin();
  settings.load();

  UI::drawMessage("PIN-UP CALENDAR", "Starting up...", "", "Hold screen to recalibrate");
  int16_t tx, ty;
  bool held = false;
  for (int i = 0; i < 15 && !held; i++) {
    held = Touch::read(tx, ty);
    delay(100);
  }
  if (held || !Touch::hasCalibration()) Touch::calibrate(UI::tft);

  while (!Store::mountSD()) {
    UI::drawMessage("NO SD CARD", "Insert a FAT32 microSD card", "Retrying...");
    delay(2000);
  }
  Store::cleanupLegacy();

  bool haveConf = settings.loadWifiConf();
  setenv("TZ", settings.tz.c_str(), 1);
  tzset();
  if (haveConf) {
    UI::drawMessage("PIN-UP CALENDAR", "Connecting to Wi-Fi...", settings.ssid);
    Net::connect();
  } else {
    UI::drawMessage("NO WI-FI CONFIG", "Create " WIFI_CONF_PATH, "on the SD card with lines", "ssid=...  and  pass=...");
    delay(4000);
  }
  if (Net::online()) {
    UI::drawMessage("PIN-UP CALENDAR", "Setting the clock...");
    Net::syncTime();
    UI::drawMessage("PIN-UP CALENDAR", "Checking for new calendars...");
    Store::refreshCatalog();
  }
  today = getToday();

  String id = migrateSelection(settings.calId);
  SeriesEntry entry;
  if (!id.isEmpty() && loadSeries(id)) {
    updateSelectedSeries();
    showCalendar();
  } else if (!id.isEmpty() && Net::online() && Store::findEntry(id, entry)) {
    pickSeries(entry);  // selected series not on the card yet (e.g. after the layout change)
  } else {
    openMenu(true);
  }
}

void loop() {
  int16_t x, y;
  if (Touch::tapped(x, y)) {
    switch (screen) {
      case SCR_CAL: handleCalendarTap(x, y); break;
      case SCR_FULL: showCalendar(); break;
      case SCR_MENU: handleMenuTap(x, y); break;
    }
  }
  maintainWifi();
  checkDateChange();
  checkForUpdates();
  if (screen == SCR_CAL) updateStatus();
  delay(10);
}
