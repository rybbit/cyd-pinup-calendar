#include "ui.h"

#include <SD.h>
#include <TJpg_Decoder.h>

#include "config.h"
#include "holidays.h"

using namespace Layout;

namespace {
// Old print-shop palette
uint16_t PAPER, PAPER_DARK, INK, RED, RED_DARK, BLUE, GRIDLINE, GOLD;

const char *MONTHS[] = {"JANUARY", "FEBRUARY", "MARCH",     "APRIL",   "MAY",      "JUNE",
                        "JULY",    "AUGUST",   "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
const char *MON3[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

int jpgClipBottom = SCREEN_H;
bool jpgStoppedEarly = false;

bool jpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (y >= jpgClipBottom) {  // below visible area: stop decoding (reported as JDR_INTR)
    jpgStoppedEarly = true;
    return false;
  }
  UI::tft.pushImage(x, y, w, h, bitmap);  // clipped by the active viewport
  return true;
}

// Decodes a JPEG from SD into the band [0, clipH) with the image shifted up by offsetY.
bool drawJpeg(const String &path, int offsetY, int clipH) {
  File f = SD.open(path);
  if (!f) return false;
  size_t len = f.size();
  if (len == 0 || len > MAX_IMAGE_BYTES) {
    f.close();
    return false;
  }
  uint8_t *buf = (uint8_t *)malloc(len);
  if (!buf) {
    f.close();
    return false;
  }
  size_t got = f.read(buf, len);
  f.close();
  bool ok = false;
  if (got == len && buf[0] == 0xFF && buf[1] == 0xD8) {
    UI::tft.setViewport(0, 0, SCREEN_W, clipH);
    jpgClipBottom = clipH;
    jpgStoppedEarly = false;
    JRESULT r = TJpgDec.drawJpg(0, -offsetY, buf, len);
    ok = r == JDR_OK || (r == JDR_INTR && jpgStoppedEarly);
    if (!ok) Serial.printf("JPEG %s: decode error %d\n", path.c_str(), r);
    UI::tft.resetViewport();
    jpgClipBottom = SCREEN_H;
  }
  free(buf);
  return ok;
}

void drawMissingImage(int h, const String &msg) {
  auto &t = UI::tft;
  t.fillRect(0, 0, SCREEN_W, h, PAPER_DARK);
  for (int i = -h; i < SCREEN_W; i += 16) t.drawLine(i, h, i + h, 0, PAPER);
  t.setTextDatum(MC_DATUM);
  t.setTextColor(INK);
  t.drawString(msg, SCREEN_W / 2, h / 2, 2);
}

void drawCaption(const String &caption, int bottom, int font) {
  if (caption.isEmpty()) return;
  auto &t = UI::tft;
  int h = font == 2 ? 18 : 11;
  t.fillRect(0, bottom - h, SCREEN_W, h, TFT_BLACK);
  t.setTextDatum(MC_DATUM);
  t.setTextColor(GOLD, TFT_BLACK);
  t.drawString(caption, SCREEN_W / 2, bottom - h / 2, font);
}

int colX(int col) { return col * SCREEN_W / 7; }

void button(int x, int y, int w, int h, const char *label, bool enabled = true) {
  auto &t = UI::tft;
  t.fillRoundRect(x + 3, y + 5, w - 6, h - 10, 6, enabled ? RED : PAPER_DARK);
  t.setTextDatum(MC_DATUM);
  t.setTextColor(enabled ? PAPER : GRIDLINE);
  t.drawString(label, x + w / 2, y + h / 2, 2);
}
}  // namespace

namespace UI {
TFT_eSPI tft;

void begin() {
  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  PAPER = tft.color565(250, 243, 224);
  PAPER_DARK = tft.color565(232, 220, 190);
  INK = tft.color565(35, 30, 28);
  RED = tft.color565(196, 32, 38);
  RED_DARK = tft.color565(140, 20, 24);
  BLUE = tft.color565(30, 80, 170);
  GRIDLINE = tft.color565(205, 190, 160);
  GOLD = tft.color565(255, 214, 90);
  tft.fillScreen(PAPER);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(jpgOutput);
}

// ---------------------------------------------------------------- calendar page

void drawCalendarImage(const CalendarMeta &meta, int month) {
  if (!drawJpeg(Store::imagePath(meta.id, month), meta.cropY[month - 1], IMG_H))
    drawMissingImage(IMG_H, "Image missing - open menu to re-download");
  else
    drawCaption(meta.captions[month - 1], IMG_H, 1);
}

static void drawHeader(int year, int month) {
  tft.fillRect(0, HDR_Y, SCREEN_W, HDR_H, RED);
  tft.drawFastHLine(0, HDR_Y, SCREEN_W, RED_DARK);
  int cy = HDR_Y + HDR_H / 2;
  tft.fillTriangle(8, cy, 22, cy - 7, 22, cy + 7, PAPER);
  tft.fillTriangle(SCREEN_W - 8, cy, SCREEN_W - 22, cy - 7, SCREEN_W - 22, cy + 7, PAPER);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(PAPER);
  tft.drawString(String(MONTHS[month - 1]) + " " + year, SCREEN_W / 2, cy);
  tft.setTextFont(1);

  tft.fillRect(0, DOW_Y, SCREEN_W, DOW_H, PAPER_DARK);
  static const char *DOW = "SMTWTFS";
  for (int c = 0; c < 7; c++) {
    tft.setTextColor(c == 0 ? RED : INK);
    tft.drawString(String(DOW[c]), (colX(c) + colX(c + 1)) / 2, DOW_Y + DOW_H / 2 + 1, 1);
  }
}

void drawGrid(int year, int month, const DateYMD &today, int selDay) {
  Holiday hol[16];
  int nh = holidaysInMonth(year, month, hol, 16);
  int first = dayOfWeek(year, month, 1);
  int dim = daysInMonth(year, month);

  tft.fillRect(0, GRID_Y, SCREEN_W, 6 * ROW_H, PAPER);
  for (int r = 1; r < 6; r++) tft.drawFastHLine(0, GRID_Y + r * ROW_H, SCREEN_W, GRIDLINE);
  for (int c = 1; c < 7; c++) tft.drawFastVLine(colX(c), GRID_Y, 6 * ROW_H, GRIDLINE);

  tft.setTextDatum(MC_DATUM);
  for (int d = 1; d <= dim; d++) {
    int idx = first + d - 1;
    int row = idx / 7, col = idx % 7;
    int x = colX(col), w = colX(col + 1) - x, y = GRID_Y + row * ROW_H;

    bool dayOff = false, observance = false;
    for (int i = 0; i < nh; i++)
      if (hol[i].day == d) (hol[i].kind == HOLIDAY_OBSERVANCE ? observance : dayOff) = true;
    bool isToday = today.y == year && today.m == month && today.d == d;

    uint16_t fg = col == 0 ? RED : INK;
    if (d == selDay) {
      tft.fillRect(x + 1, y + 1, w - 1, ROW_H - 1, GOLD);
    }
    if (dayOff) {
      tft.fillRoundRect(x + 3, y + 1, w - 5, ROW_H - 2, 4, RED);
      fg = PAPER;
    }
    if (isToday) {
      tft.drawRect(x + 1, y + 1, w - 1, ROW_H - 1, INK);
      tft.drawRect(x + 2, y + 2, w - 3, ROW_H - 3, INK);
    }
    if (observance) tft.fillCircle(x + w - 5, y + ROW_H - 5, 2, dayOff ? PAPER : BLUE);
    tft.setTextColor(fg);
    tft.drawNumber(d, x + w / 2, y + ROW_H / 2 + 1, 2);
  }
}

void drawStatus(const String &text) {
  tft.fillRect(0, STATUS_Y, SCREEN_W, STATUS_H, PAPER_DARK);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(INK);
  tft.drawString(text, 4, STATUS_Y + STATUS_H / 2 + 1, 1);
}

void drawCalendarPage(const CalendarMeta &meta, int year, int month, const DateYMD &today, int selDay) {
  drawCalendarImage(meta, month);
  drawHeader(year, month);
  drawGrid(year, month, today, selDay);
}

CalHit hitCalendar(int16_t x, int16_t y, int year, int month, int &day) {
  day = 0;
  if (y < IMG_H) return HIT_IMAGE;
  if (y < HDR_Y + HDR_H + 4) {  // header (a little extra slop below it)
    if (x < 60) return HIT_PREV;
    if (x > SCREEN_W - 60) return HIT_NEXT;
    return HIT_TITLE;
  }
  if (y >= GRID_Y && y < GRID_Y + 6 * ROW_H) {
    int row = (y - GRID_Y) / ROW_H;
    int col = min(6, x * 7 / SCREEN_W);
    int d = row * 7 + col - dayOfWeek(year, month, 1) + 1;
    if (d >= 1 && d <= daysInMonth(year, month)) {
      day = d;
      return HIT_DAY;
    }
    return HIT_NONE;
  }
  if (y >= STATUS_Y) return HIT_STATUS;
  return HIT_NONE;
}

// ---------------------------------------------------------------- full screen

void drawFullImage(const CalendarMeta &meta, int month) {
  if (!drawJpeg(Store::imagePath(meta.id, month), 0, SCREEN_H))
    drawMissingImage(SCREEN_H, "Image missing");
  else
    drawCaption(meta.captions[month - 1], SCREEN_H, 2);
}

// ---------------------------------------------------------------- picker

std::vector<int> MenuState::visible() const {
  std::vector<int> v;
  const String *cat = catIndex > 0 && catIndex < (int)categories.size() ? &categories[catIndex] : nullptr;
  for (int i = 0; i < (int)all.size(); i++)
    if (!cat || all[i].category == *cat) v.push_back(i);
  return v;
}

void drawMenu(const MenuState &s) {
  tft.fillScreen(PAPER);

  // title bar
  tft.fillRect(0, 0, SCREEN_W, MENU_TITLE_H, RED);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(PAPER);
  tft.drawString("PIN-UP CALENDARS", 8, MENU_TITLE_H / 2);
  tft.setTextFont(1);
  if (!s.currentId.isEmpty()) {  // close (X)
    int cx = SCREEN_W - 16, cy = MENU_TITLE_H / 2;
    tft.drawLine(cx - 6, cy - 6, cx + 6, cy + 6, PAPER);
    tft.drawLine(cx - 6, cy + 6, cx + 6, cy - 6, PAPER);
    tft.drawLine(cx - 5, cy - 6, cx + 7, cy + 6, PAPER);
    tft.drawLine(cx - 5, cy + 6, cx + 7, cy - 6, PAPER);
  }

  // category selector
  tft.fillRect(0, MENU_CAT_Y, SCREEN_W, MENU_CAT_H, PAPER_DARK);
  String cat = s.categories.empty() ? "All" : s.categories[s.catIndex];
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(INK);
  tft.drawString("Category:  " + cat + "  >", SCREEN_W / 2, MENU_CAT_Y + MENU_CAT_H / 2, 2);

  // list
  auto vis = s.visible();
  if (vis.empty()) {
    tft.setTextColor(INK);
    tft.drawString(s.all.empty() ? "No calendars yet." : "Nothing in this category.", SCREEN_W / 2, 120, 2);
    tft.drawString(s.online ? "Tap SYNC to fetch the list." : "Tap WI-FI to get online.", SCREEN_W / 2, 145, 2);
  }
  for (int r = 0; r < MENU_ROWS && s.scroll + r < (int)vis.size(); r++) {
    const CatalogEntry &e = s.all[vis[s.scroll + r]];
    int y = MENU_LIST_Y + r * MENU_ROW_H;
    bool current = e.id == s.currentId;
    if (current) tft.fillRect(0, y, SCREEN_W, MENU_ROW_H, GOLD);
    tft.drawFastHLine(0, y + MENU_ROW_H - 1, SCREEN_W, GRIDLINE);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(INK);
    String title = e.title;
    while (tft.textWidth(title, 2) > SCREEN_W - 40 && title.length() > 3) title.remove(title.length() - 1);
    tft.drawString(title, 8, y + 3, 2);
    tft.setTextColor(RED_DARK);
    String sub = e.category;
    if (e.year) sub += "  -  " + String(e.year);
    tft.drawString(sub, 8, y + 22, 1);
    // status badge
    tft.setTextDatum(MR_DATUM);
    if (e.updateAvailable() && s.online) {
      tft.setTextColor(RED);
      tft.drawString("UPDATE", SCREEN_W - 8, y + MENU_ROW_H / 2, 1);
    } else if (e.downloaded) {
      tft.setTextColor(BLUE);
      tft.drawString("SAVED", SCREEN_W - 8, y + MENU_ROW_H / 2, 1);
    } else {
      tft.setTextColor(RED);
      tft.drawString(s.online ? "GET" : "--", SCREEN_W - 8, y + MENU_ROW_H / 2, 1);
    }
  }

  // buttons
  tft.fillRect(0, MENU_BTN_Y, SCREEN_W, MENU_BTN_H, PAPER_DARK);
  int bw = SCREEN_W / 4;
  button(0 * bw, MENU_BTN_Y, bw, MENU_BTN_H, "UP", s.scroll > 0);
  button(1 * bw, MENU_BTN_Y, bw, MENU_BTN_H, "DOWN", s.scroll + MENU_ROWS < (int)vis.size());
  button(2 * bw, MENU_BTN_Y, bw, MENU_BTN_H, "WI-FI");
  button(3 * bw, MENU_BTN_Y, bw, MENU_BTN_H, "SYNC", s.online);
}

MenuHit hitMenu(int16_t x, int16_t y, int &row) {
  row = -1;
  if (y < MENU_TITLE_H) return x > SCREEN_W - 50 ? MHIT_CLOSE : MHIT_NONE;
  if (y < MENU_CAT_Y + MENU_CAT_H) return MHIT_CAT;
  if (y >= MENU_BTN_Y) {
    switch (x * 4 / SCREEN_W) {
      case 0: return MHIT_UP;
      case 1: return MHIT_DOWN;
      case 2: return MHIT_WIFI;
      default: return MHIT_SYNC;
    }
  }
  if (y >= MENU_LIST_Y && y < MENU_LIST_Y + MENU_ROWS * MENU_ROW_H) {
    row = (y - MENU_LIST_Y) / MENU_ROW_H;
    return MHIT_ROW;
  }
  return MHIT_NONE;
}

// ---------------------------------------------------------------- misc

void drawMessage(const String &title, const String &l1, const String &l2, const String &l3) {
  tft.fillScreen(PAPER);
  tft.fillRect(0, 0, SCREEN_W, 40, RED);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(PAPER);
  tft.drawString(title, SCREEN_W / 2, 20);
  tft.setTextFont(1);
  tft.setTextColor(INK);
  const String *lines[] = {&l1, &l2, &l3};
  for (int i = 0; i < 3; i++)
    if (lines[i]->length()) tft.drawString(*lines[i], SCREEN_W / 2, 120 + i * 26, 2);
}

void drawProgress(const String &title, const String &what, int done, int total, const char *heading) {
  if (done == 0) drawMessage(heading, title, what);
  int x = 20, y = 220, w = SCREEN_W - 40, h = 18;
  tft.drawRect(x, y, w, h, INK);
  int fill = total > 0 ? (w - 4) * done / total : 0;
  tft.fillRect(x + 2, y + 2, fill, h - 4, RED);
  tft.fillRect(0, y + 26, SCREEN_W, 18, PAPER);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(INK);
  tft.drawString(String(done) + " / " + total, SCREEN_W / 2, y + 35, 2);
}

String formatDayLine(int year, int month, int day, const String &names) {
  static const char *DOWN[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return String(DOWN[dayOfWeek(year, month, day)]) + " " + MON3[month - 1] + " " + day + "  " + names;
}

}  // namespace UI
