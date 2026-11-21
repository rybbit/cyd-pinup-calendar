// Pin-Up Calendar for the ESP32-2432S028R "Cheap Yellow Display"
//
// Calendar page:  tap < / > in the red bar to change month, tap the month name for the
//                 calendar picker, tap the picture to see it full screen, tap a day to see
//                 its holidays, tap the bottom line to jump back to today.
// Boot:           hold a finger/stylus on the screen while "Hold to recalibrate" is shown
//                 to redo touch calibration.
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

CalendarMeta meta;
bool haveMeta = false;
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
  UI::drawCalendarPage(meta, viewY, viewM, today, selDay);
  resetStatus();
}

void goToToday() {
  if (today.y) {
    viewY = today.y;
    viewM = today.m;
  } else {
    viewY = meta.year ? meta.year : 2026;
    viewM = 1;
  }
  selDay = 0;
}

// Rotates through this month's holidays on the bottom line.
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
  if (monthHolN == 0) {
    setStatus(today.y ? meta.title : meta.title + "  (clock not set)");
    return;
  }
  const Holiday &h = monthHol[statusIdx % monthHolN];
  statusIdx++;
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

bool loadCalendar(const String &id) {
  CalendarMeta m;
  if (!Store::isDownloaded(id) || !Store::loadMeta(id, m)) return false;
  meta = m;
  haveMeta = true;
  if (settings.calId != id) {
    settings.calId = id;
    settings.save();
  }
  goToToday();
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
  menu.currentId = haveMeta ? meta.id : "";
  menu.online = Net::online();
  int n = menu.visible().size();
  menu.scroll = constrain(menu.scroll, 0, max(0, n - Layout::MENU_ROWS));
  UI::drawMenu(menu);
}

// Re-reads /config/wifi.conf and (re)connects.
void wifiSetup() {
  bool haveConf = settings.loadWifiConf();
  setenv("TZ", settings.tz.c_str(), 1);
  tzset();
  if (!haveConf) {
    UI::drawMessage("NO WI-FI CONFIG", settings.confFound ? "No ssid= line in" : "Missing file on SD card:",
                    WIFI_CONF_PATH, "ssid=...  and  pass=...");
    delay(3500);
    return;
  }
  UI::drawMessage("WI-FI", "Connecting to", settings.ssid);
  bool ok = Net::reconnect();
  if (ok) {
    UI::drawMessage("WI-FI", "Connected!", "Setting clock & fetching list...");
    Net::syncTime();
    today = getToday();
    Store::refreshCatalog();
  } else {
    UI::drawMessage("WI-FI", "Could not connect to", settings.ssid, "Check " WIFI_CONF_PATH);
    delay(2000);
  }
}

void pickCalendar(const CatalogEntry &e) {
  if (!e.downloaded) {
    if (!Net::online()) {
      UI::drawMessage("OFFLINE", "Connect Wi-Fi to download", e.title);
      delay(2000);
      openMenu(false);
      return;
    }
    bool ok = Store::downloadCalendar(e, [&](int done, int total) {
      String what = e.category;
      if (e.year) what += " " + String(e.year);
      UI::drawProgress(e.title, what, done, total);
    });
    if (!ok) {
      UI::drawMessage("DOWNLOAD FAILED", e.title, "Check Wi-Fi and try again.", "Saved images are kept.");
      delay(2500);
      openMenu(true);
      return;
    }
  }
  if (!loadCalendar(e.id)) {
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
      if (haveMeta) showCalendar();
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
    case UI::MHIT_WIFI:
      wifiSetup();
      openMenu(true);
      break;
    case UI::MHIT_SYNC:
      if (Net::online()) {
        UI::drawMessage("SYNC", "Fetching calendar list...");
        if (!Store::refreshCatalog()) {
          UI::drawMessage("SYNC FAILED", "Could not download", "catalog.json", "Check catalog= in wifi.conf");
          delay(2500);
        }
        openMenu(true);
      }
      break;
    case UI::MHIT_ROW:
      if (menu.scroll + row < (int)vis.size()) pickCalendar(menu.all[vis[menu.scroll + row]]);
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
      UI::drawFullImage(meta, viewM);
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

void checkDateChange() {
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  DateYMD t = getToday();
  if (t.y == today.y && t.m == today.m && t.d == today.d) return;
  bool followToday = !today.y || (viewY == today.y && viewM == today.m);
  today = t;
  if (!haveMeta) return;
  if (followToday && t.y) {
    viewY = t.y;
    viewM = t.m;
  }
  if (screen == SCR_CAL) showCalendar();
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

  if (!settings.calId.isEmpty() && loadCalendar(settings.calId))
    showCalendar();
  else
    openMenu(true);
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
  checkDateChange();
  if (screen == SCR_CAL) updateStatus();
  delay(10);
}
