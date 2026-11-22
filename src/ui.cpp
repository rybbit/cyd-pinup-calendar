#include "ui.h"

#include <SD.h>
#include <TJpg_Decoder.h>

#include "config.h"
#include "holidays.h"

#include "settings.h"

using namespace Layout;

namespace {
// Calendar page geometry; see the table in ui.h. Set by UI::configureLayout().
int IMG_H = 170;
int AD_Y = 170, AD_H = 0;
int HDR_Y = 170, HDR_H = 24;
int DOW_Y = 194, DOW_H = 12;
int GRID_Y = 206, ROW_H = 17;
int STATUS_Y = 308, STATUS_H = 12;
int COMPANY_CY = 0, ADDR_CY = 0, CONTACT_CY = 0;  // ad line centers, relative to AD_Y
bool mottoLine = false;
const int CLASSIC_IMG_H = 170;  // crop_y values in calendar.json are tuned for this window

// Full-screen calendar: the same drawing code with this geometry swapped in.
bool bigCal = false;
struct Geometry {
  int imgH, adY, adH, hdrY, hdrH, dowY, dowH, gridY, rowH, statusY, statusH;
};
Geometry pageGeometry;
Geometry currentGeometry() {
  return {IMG_H, AD_Y, AD_H, HDR_Y, HDR_H, DOW_Y, DOW_H, GRID_Y, ROW_H, STATUS_Y, STATUS_H};
}
void applyGeometry(const Geometry &g) {
  IMG_H = g.imgH, AD_Y = g.adY, AD_H = g.adH, HDR_Y = g.hdrY, HDR_H = g.hdrH, DOW_Y = g.dowY, DOW_H = g.dowH;
  GRID_Y = g.gridY, ROW_H = g.rowH, STATUS_Y = g.statusY, STATUS_H = g.statusH;
}
// The header is the regular page's (same font and arrows); the extra room goes to the rows.
//                            img ad    header  weekdays grid     status
const Geometry BIG_CAL = {0, 0, 0, 0, 24, 24, 18, 42, 40, 282, 38};
}  // namespace

namespace {
// Old print-shop palette
uint16_t PAPER, PAPER_DARK, INK, GRIDLINE, GOLD, GREEN, NAVY;
// Theme colors (UI::applyTheme). The names come from the original Red theme:
//   RED      accent: month bar, Sundays, federal holidays, buttons
//   RED_DARK darker accent: bar edge, secondary text
//   BLUE     contrast: observance dots and labels, SD badge
uint16_t RED, RED_DARK, BLUE;

const char *MONTHS[] = {"JANUARY", "FEBRUARY", "MARCH",     "APRIL",   "MAY",      "JUNE",
                        "JULY",    "AUGUST",   "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
const char *MON3[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

int jpgClipBottom = SCREEN_H;
bool jpgStoppedEarly = false;

bool jpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
#if JPEG_SWAP_RB
  // This panel shows decoded JPEG pixels with red and blue exchanged (UI colors are fine),
  // so swap them back. Pixels are byte-swapped for SPI (TJpgDec.setSwapBytes(true)).
  for (uint32_t i = 0, n = (uint32_t)w * h; i < n; i++) {
    uint16_t p = (bitmap[i] >> 8) | (bitmap[i] << 8);
    p = (p & 0x07E0) | (p >> 11) | ((p & 0x001F) << 11);
    bitmap[i] = (p >> 8) | (p << 8);
  }
#endif
  if (y >= jpgClipBottom) {  // below visible area: stop decoding (reported as JDR_INTR)
    jpgStoppedEarly = true;
    return false;
  }
  UI::tft.pushImage(x, y, w, h, bitmap);  // clipped by the active viewport
  return true;
}

// Decodes a JPEG from SD into the band [0, clipH) with the image shifted up by offsetY.
bool drawJpeg(const String &path, int offsetY, int clipH) {
  if (!SD.exists(path)) return false;
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

// Retro sunburst with the category and year, for years with no edition.
void drawSunburst(const SeriesInfo &s, int year, int h) {
  auto &t = UI::tft;
  t.fillRect(0, 0, SCREEN_W, h, PAPER);
  int cx = SCREEN_W / 2, cy = h / 2;
  const float R = 400, STEP = PI / 12;  // 24 rays
  for (int k = 0; k < 24; k += 2) {
    float a0 = k * STEP, a1 = a0 + STEP;
    t.fillTriangle(cx, cy, cx + R * cos(a0), cy + R * sin(a0), cx + R * cos(a1), cy + R * sin(a1), PAPER_DARK);
  }
  int r = min(64, h / 2 - 14);
  t.fillCircle(cx, cy, r, PAPER);
  t.drawCircle(cx, cy, r, INK);
  t.drawCircle(cx, cy, r - 1, INK);
  t.setTextDatum(MC_DATUM);
  String cat = s.category;
  cat.toUpperCase();
  t.setTextColor(RED);
  t.drawString(cat, cx, cy - 18, 2);
  t.setTextColor(INK);
  t.drawNumber(year, cx, cy + 4, 4);
  t.setTextColor(RED_DARK);
  t.drawString("COMING SOON", cx, cy + 26, 1);
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

// Two-line labels (max 5 characters per line) for the big calendar's day boxes.
const char *shortHolidayName(const char *name) {
  static const char *const MAP[][2] = {
      {"New Year's Day", "New\nYear"},         {"New Year's (observed)", "New\nYear"},
      {"Martin Luther King Jr. Day", "MLK\nDay"}, {"Presidents' Day", "Pres.\nDay"},
      {"Memorial Day", "Mem.\nDay"},           {"Juneteenth", "June\nteen"},
      {"Juneteenth (observed)", "June\nteen"}, {"Independence Day", "July\n4th"},
      {"Independence Day (observed)", "July\n4th"}, {"Labor Day", "Labor\nDay"},
      {"Columbus Day", "Colum\nbus"},          {"Veterans Day", "Vets\nDay"},
      {"Veterans Day (observed)", "Vets\nDay"}, {"Thanksgiving", "Thnks\ngivng"},
      {"Christmas Day", "Xmas"},                {"Christmas (observed)", "Xmas"},
      {"Groundhog Day", "Grnd-\nhog"},         {"Valentine's Day", "Valen\ntine"},
      {"Daylight Saving Time begins", "DST\n+1 hr"}, {"Daylight Saving Time ends", "DST\n-1 hr"},
      {"St. Patrick's Day", "St.\nPat's"},     {"Easter Sunday", "Eas-\nter"},
      {"Tax Day", "Tax\nDay"},                 {"Mother's Day", "Mom's\nDay"},
      {"Flag Day", "Flag\nDay"},               {"Father's Day", "Dad's\nDay"},
      {"Halloween", "Hallo\nween"},            {"Election Day", "Elec.\nDay"},
      {"Christmas Eve", "Xmas\nEve"},          {"New Year's Eve", "NYE"},
  };
  for (auto &m : MAP)
    if (strcmp(m[0], name) == 0) return m[1];
  return name;
}

// Draws text centered at (cx, cy) in the first font that fits the width:
// the free font, then built-in font 2, then font 1.
void drawFitted(const String &text, int cx, int cy, const GFXfont *ff, int maxW = SCREEN_W - 8) {
  auto &t = UI::tft;
  t.setTextDatum(MC_DATUM);
  if (ff) {
    t.setFreeFont(ff);
    if (t.textWidth(text) <= maxW) {
      t.drawString(text, cx, cy);
      t.setTextFont(1);
      return;
    }
    t.setTextFont(1);
  }
  t.drawString(text, cx, cy + 1, t.textWidth(text, 2) <= maxW ? 2 : 1);
}

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

void applyTheme(const String &name) {
  String t = name;
  t.toLowerCase();
  if (t == "blue") {
    RED = tft.color565(44, 92, 186);
    RED_DARK = tft.color565(24, 56, 128);
    BLUE = tft.color565(196, 32, 38);
  } else if (t == "green") {
    RED = tft.color565(36, 122, 64);
    RED_DARK = tft.color565(20, 80, 40);
    BLUE = tft.color565(30, 80, 170);
  } else {  // red (default)
    RED = tft.color565(196, 32, 38);
    RED_DARK = tft.color565(140, 20, 24);
    BLUE = tft.color565(30, 80, 170);
  }
}

uint16_t accentColor() { return RED; }
uint16_t accentDarkColor() { return RED_DARK; }

// Bold title text centered on its capital letters: the font's normal centering also
// reserves room for descenders, which made all-caps titles sit low in their bars.
void drawTitle(const String &text, int x, int cy, bool centered) {
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextDatum(centered ? C_BASELINE : L_BASELINE);
  tft.drawString(text, x, cy + 6);  // caps are 13px tall, 12px above the baseline
  tft.setTextFont(1);
  tft.setTextDatum(MC_DATUM);
}

void begin() {
  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  PAPER = tft.color565(250, 243, 224);
  PAPER_DARK = tft.color565(232, 220, 190);
  INK = tft.color565(35, 30, 28);
  applyTheme("red");
  GRIDLINE = tft.color565(205, 190, 160);
  GOLD = tft.color565(255, 214, 90);
  GREEN = tft.color565(60, 170, 80);
  NAVY = tft.color565(28, 36, 88);
  tft.fillScreen(PAPER);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(jpgOutput);
}

// ---------------------------------------------------------------- calendar page

// The series' placeholder.jpg if it has one, otherwise a drawn sunburst.
static void drawPlaceholder(const SeriesInfo &s, int year, int h, const String &note, int font) {
  if (!s.hasPlaceholder || !drawJpeg(Store::placeholderPath(s.id), 0, h)) drawSunburst(s, year, h);
  drawCaption(note, h, font);
}

static void drawArt(const SeriesInfo &s, const CalendarMeta *meta, int year, int month, int h, int font) {
  // A separate calendar-page header (MM_top.jpg) wins over a window of the full picture.
  if (meta && h != SCREEN_H && drawJpeg(Store::topImagePath(s.id, year, month), max(0, (TOP_H - h) / 2), h)) {
    drawCaption(meta->captions[month - 1], h, font);
    return;
  }
  int offset = 0;
  if (meta && h != SCREEN_H) {
    offset = meta->cropY[month - 1];
    // Framed photos stay centered in a shorter window; pin-up art stays top-aligned.
    if (meta->framed[month - 1]) offset += (CLASSIC_IMG_H - h) / 2;
    offset = constrain(offset, 0, SCREEN_H - h);
  }
  if (meta && drawJpeg(Store::imagePath(s.id, year, month), offset, h)) {
    drawCaption(meta->captions[month - 1], h, font);
    return;
  }
  drawPlaceholder(s, year, h, meta ? String("No picture this month") : String(year) + " edition not available", font);
}

void drawCalendarImage(const SeriesInfo &s, const CalendarMeta *meta, int year, int month) {
  drawArt(s, meta, year, month, IMG_H, 1);
}

static void drawHeader(int year, int month) {
  tft.fillRect(0, HDR_Y, SCREEN_W, HDR_H, RED);
  tft.drawFastHLine(0, HDR_Y, SCREEN_W, RED_DARK);
  int cy = HDR_Y + HDR_H / 2;
  tft.fillTriangle(8, cy, 22, cy - 7, 22, cy + 7, PAPER);
  tft.fillTriangle(SCREEN_W - 8, cy, SCREEN_W - 22, cy - 7, SCREEN_W - 22, cy + 7, PAPER);
  tft.setTextColor(PAPER);
  drawTitle(String(MONTHS[month - 1]) + " " + year, SCREEN_W / 2, cy, true);

  tft.fillRect(0, DOW_Y, SCREEN_W, DOW_H, PAPER_DARK);
  static const char *DOW = "SMTWTFS";
  static const char *DOW3[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  for (int c = 0; c < 7; c++) {
    tft.setTextColor(c == 0 ? RED : INK);
    int cx = (colX(c) + colX(c + 1)) / 2;
    if (bigCal) tft.drawString(DOW3[c], cx, DOW_Y + DOW_H / 2 + 1, 2);
    else tft.drawString(String(DOW[c]), cx, DOW_Y + DOW_H / 2 + 1, 1);
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
    const char *label = nullptr;  // first holiday of the day (federal ones sort first)
    for (int i = 0; i < nh; i++)
      if (hol[i].day == d) {
        (hol[i].kind == HOLIDAY_OBSERVANCE ? observance : dayOff) = true;
        if (!label) label = hol[i].name;
      }
    bool isToday = today.y == year && today.m == month && today.d == d;

    // Federal holidays fill the whole square in the theme color; the tapped day is gold
    // (a tapped holiday stays gold, with its text in the theme color).
    bool selected = d == selDay;
    bool filled = dayOff && !selected;  // white text on the theme color
    uint16_t fg = filled ? PAPER : (col == 0 || dayOff) ? RED : INK;
    if (selected || dayOff) tft.fillRect(x + 1, y + 1, w - 1, ROW_H - 1, selected ? GOLD : RED);
    if (isToday) {
      tft.drawRect(x + 1, y + 1, w - 1, ROW_H - 1, INK);
      tft.drawRect(x + 2, y + 2, w - 3, ROW_H - 3, INK);
    }
    if (observance) {
      if (bigCal) tft.fillCircle(x + w - 6, y + 6, 3, filled ? PAPER : BLUE);  // top-right: label below
      else tft.fillCircle(x + w - 5, y + ROW_H - 5, 2, filled ? PAPER : BLUE);
    }
    tft.setTextColor(fg);
    if (bigCal) {  // like a printed wall calendar: small number in the top-left of a big box
      tft.setTextDatum(TL_DATUM);
      tft.drawNumber(d, x + 4, y + 3, 2);
      if (label) {  // holiday name in two short lines under the number
        String l = shortHolidayName(label);
        int nl = l.indexOf('\n');
        tft.setTextColor(filled ? PAPER : dayOff ? RED : BLUE);
        tft.drawString(nl < 0 ? l : l.substring(0, nl), x + 3, y + 21, 1);
        if (nl >= 0) tft.drawString(l.substring(nl + 1), x + 3, y + 30, 1);
      }
      tft.setTextDatum(MC_DATUM);
    } else {
      tft.drawNumber(d, x + w / 2, y + ROW_H / 2 + 1, 2);
    }
  }
}

// Builds the page from whichever ad_ fields are set: each line reserves space only
// when it has text, and the picture takes whatever height is left.
void configureLayout() {
  setBigCalendar(false);
  bool company = settings.showCompany.length();
  bool addr = settings.showAddress1.length() || settings.showAddress2.length();
  bool contact = settings.showContact.length();
  mottoLine = settings.adMotto.length();
  bool any = company || addr || contact || mottoLine;

  int y = 0;
  if (company || addr || contact) {
    y = 2;
    if (company) { COMPANY_CY = y + 9; y += 18; }
    if (addr) { ADDR_CY = y + 5; y += 10; }
    if (contact) { CONTACT_CY = y + 7; y += 14; }
    y += 2;
  }
  AD_H = y;
  if (any) {  // shorter calendar rows to make room
    HDR_H = 20, DOW_H = 10, ROW_H = 15;
  } else {
    HDR_H = 24, DOW_H = 12, ROW_H = 17;
  }
  STATUS_H = mottoLine ? 16 : 12;
  IMG_H = SCREEN_H - AD_H - HDR_H - DOW_H - 6 * ROW_H - STATUS_H;
  AD_Y = IMG_H;
  HDR_Y = AD_Y + AD_H;
  DOW_Y = HDR_Y + HDR_H;
  GRID_Y = DOW_Y + DOW_H;
  STATUS_Y = GRID_Y + 6 * ROW_H;
  Serial.printf("Layout: picture %d px, ad block %d px, motto %s\n", IMG_H, AD_H, mottoLine ? "yes" : "no");
}

bool mottoMode() { return mottoLine; }

// Company name (large), the two address lines side by side (small), contact (medium).
static void drawAd() {
  tft.fillRect(0, AD_Y, SCREEN_W, AD_H, PAPER);
  tft.drawFastHLine(0, AD_Y, SCREEN_W, GRIDLINE);
  tft.setTextColor(INK);
  if (settings.showCompany.length())
    drawFitted(settings.showCompany, SCREEN_W / 2, AD_Y + COMPANY_CY, &FreeSansBold9pt7b);
  String addr = settings.showAddress1;
  if (addr.length() && settings.showAddress2.length()) addr += "    ";
  addr += settings.showAddress2;
  if (addr.length()) {
    tft.setTextDatum(MC_DATUM);
    tft.drawString(addr, SCREEN_W / 2, AD_Y + ADDR_CY, 1);
  }
  if (settings.showContact.length()) drawFitted(settings.showContact, SCREEN_W / 2, AD_Y + CONTACT_CY, nullptr);
}

void drawMotto() {
  tft.fillRect(0, STATUS_Y, SCREEN_W, STATUS_H, PAPER);
  if (settings.adMotto.isEmpty()) return;
  tft.setTextColor(NAVY);
  drawFitted(settings.adMotto, SCREEN_W / 2, STATUS_Y + STATUS_H / 2 - 1, &FreeSerifBoldItalic9pt7b);
}

void drawStatus(const String &text) {
  if (mottoLine || bigCal) {
    tft.fillRect(0, STATUS_Y, SCREEN_W, STATUS_H, PAPER_DARK);
    tft.setTextColor(INK);
    drawFitted(text, SCREEN_W / 2, STATUS_Y + STATUS_H / 2 - 1, nullptr);
    return;
  }
  tft.fillRect(0, STATUS_Y, SCREEN_W, STATUS_H, PAPER_DARK);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(INK);
  tft.drawString(text, 4, STATUS_Y + STATUS_H / 2 + 1, 1);
}

void drawCalendarPage(const SeriesInfo &s, const CalendarMeta *meta, int year, int month, const DateYMD &today,
                      int selDay) {
  setBigCalendar(false);
  drawCalendarImage(s, meta, year, month);
  if (AD_H) drawAd();
  drawHeader(year, month);
  drawGrid(year, month, today, selDay);
  if (mottoLine) drawMotto();
}

CalHit hitCalendar(int16_t x, int16_t y, int year, int month, int &day) {
  day = 0;
  if (y < IMG_H) return HIT_IMAGE;
  if (y < IMG_H + AD_H) return HIT_AD;
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

// ---------------------------------------------------------------- full-screen calendar

void setBigCalendar(bool on) {
  if (on == bigCal) return;
  if (on) {
    pageGeometry = currentGeometry();
    applyGeometry(BIG_CAL);
  } else {
    applyGeometry(pageGeometry);
  }
  bigCal = on;
}

bool bigCalendar() { return bigCal; }

void drawBigCalendar(int year, int month, const DateYMD &today, int selDay) {
  setBigCalendar(true);
  drawHeader(year, month);
  drawGrid(year, month, today, selDay);
}

// ---------------------------------------------------------------- full screen

void drawFullImage(const SeriesInfo &s, const CalendarMeta *meta, int year, int month) {
  drawArt(s, meta, year, month, SCREEN_H, 2);
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
  tft.setTextColor(PAPER);
  drawTitle("PIN-UP CALENDARS", 8, MENU_TITLE_H / 2, false);
  // Wi-Fi status dot
  tft.fillCircle(SCREEN_W - 40, MENU_TITLE_H / 2, 4, s.online ? GREEN : GRIDLINE);
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
    tft.drawString(s.online ? "Tap SYNC to fetch the list." : "Waiting for Wi-Fi...", SCREEN_W / 2, 145, 2);
    if (!s.online) tft.drawString("(check /calendar/config.txt)", SCREEN_W / 2, 170, 2);
  }
  for (int r = 0; r < MENU_ROWS && s.scroll + r < (int)vis.size(); r++) {
    const SeriesEntry &e = s.all[vis[s.scroll + r]];
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
    String years = e.yearsLabel();
    if (years.length()) sub += "  -  " + years;
    tft.drawString(sub, 8, y + 22, 1);
    // status badge
    tft.setTextDatum(MR_DATUM);
    if (e.custom) {
      tft.fillRoundRect(SCREEN_W - 30, y + MENU_ROW_H / 2 - 7, 24, 14, 3, BLUE);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(PAPER);
      tft.drawString("SD", SCREEN_W - 18, y + MENU_ROW_H / 2 + 1, 1);
    } else if (e.updateAvailable() && s.online) {
      tft.setTextColor(RED);
      tft.drawString("UPDATE", SCREEN_W - 8, y + MENU_ROW_H / 2, 1);
    } else if (e.downloaded()) {
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
  button(2 * bw, MENU_BTN_Y, bw, MENU_BTN_H, "SYNC", s.online);
  button(3 * bw, MENU_BTN_Y, bw, MENU_BTN_H, "SETUP");
}

MenuHit hitMenu(int16_t x, int16_t y, int &row) {
  row = -1;
  if (y < MENU_TITLE_H) return x > SCREEN_W - 50 ? MHIT_CLOSE : MHIT_NONE;
  if (y < MENU_CAT_Y + MENU_CAT_H) return MHIT_CAT;
  if (y >= MENU_BTN_Y) {
    switch (x * 4 / SCREEN_W) {
      case 0: return MHIT_UP;
      case 1: return MHIT_DOWN;
      case 2: return MHIT_SYNC;
      default: return MHIT_SETUP;
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
  tft.setTextColor(PAPER);
  drawTitle(title, SCREEN_W / 2, 20, true);
  tft.setTextColor(INK);
  const String *lines[] = {&l1, &l2, &l3};
  for (int i = 0; i < 3; i++)
    if (lines[i]->length()) tft.drawString(*lines[i], SCREEN_W / 2, 120 + i * 26, 2);
}

void drawProgress(const String &title, const String &what, int done, int total, const char *heading) {
  static String lastWhat;
  if (done == 0 || what != lastWhat) drawMessage(heading, title, what);
  lastWhat = what;
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
