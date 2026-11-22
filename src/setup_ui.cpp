#include "setup_ui.h"

#include <WiFi.h>

#include <algorithm>
#include <vector>

#include "config.h"
#include "settings.h"
#include "touch.h"
#include "ui.h"

namespace {
TFT_eSPI &tft = UI::tft;
uint16_t PAPER, PAPER_DARK, INK, RED, RED_DARK, GRID, GOLD, MUTED;

void initColors() {
  PAPER = tft.color565(250, 243, 224);
  PAPER_DARK = tft.color565(232, 220, 190);
  INK = tft.color565(35, 30, 28);
  RED = UI::accentColor();  // current theme
  RED_DARK = UI::accentDarkColor();
  GRID = tft.color565(205, 190, 160);
  GOLD = tft.color565(255, 214, 90);
  MUTED = tft.color565(150, 135, 110);
}

void waitTap(int16_t &x, int16_t &y) {
  while (!Touch::tapped(x, y)) delay(10);
}

void titleBar(const String &title) {
  tft.fillRect(0, 0, SCREEN_W, 28, RED);
  tft.setTextColor(PAPER);
  UI::drawTitle(title, 8, 14, false);
}

void button(int x, int y, int w, int h, const char *label, uint16_t color) {
  tft.fillRoundRect(x + 3, y + 4, w - 6, h - 8, 6, color);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(PAPER);
  tft.drawString(label, x + w / 2, y + h / 2, 2);
}

// Shortens to fit `maxW` pixels in `font`, ending with "..".
String fit(String s, int maxW, int font) {
  if (tft.textWidth(s, font) <= maxW) return s;
  while (s.length() > 1 && tft.textWidth(s + "..", font) > maxW) s.remove(s.length() - 1);
  return s + "..";
}

// ---------------------------------------------------------------- list chooser

// Returns the chosen index, -1 for CANCEL, -2 for the optional extra button.
int choose(const String &title, const std::vector<String> &items, int current, const char *extra = nullptr) {
  const int ROW_Y = 32, ROW_H = 28, BTN_Y = 272;
  int n = std::min((int)items.size(), (BTN_Y - ROW_Y) / ROW_H);
  tft.fillScreen(PAPER);
  titleBar(title);
  for (int i = 0; i < n; i++) {
    int y = ROW_Y + i * ROW_H;
    if (i == current) tft.fillRect(0, y, SCREEN_W, ROW_H, GOLD);
    tft.drawFastHLine(0, y + ROW_H - 1, SCREEN_W, GRID);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(INK);
    tft.drawString(fit(items[i], SCREEN_W - 16, 2), 8, y + ROW_H / 2, 2);
  }
  if (items.empty()) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(INK);
    tft.drawString("Nothing found", SCREEN_W / 2, 120, 2);
  }
  tft.fillRect(0, BTN_Y, SCREEN_W, SCREEN_H - BTN_Y, PAPER_DARK);
  if (extra) {
    button(0, BTN_Y, SCREEN_W / 2, SCREEN_H - BTN_Y, "CANCEL", RED_DARK);
    button(SCREEN_W / 2, BTN_Y, SCREEN_W / 2, SCREEN_H - BTN_Y, extra, RED);
  } else {
    button(0, BTN_Y, SCREEN_W, SCREEN_H - BTN_Y, "CANCEL", RED_DARK);
  }
  while (true) {
    int16_t x, y;
    waitTap(x, y);
    if (y >= BTN_Y) return (extra && x >= SCREEN_W / 2) ? -2 : -1;
    if (y >= ROW_Y) {
      int i = (y - ROW_Y) / ROW_H;
      if (i < n) return i;
    }
  }
}

// ---------------------------------------------------------------- keyboard

const char *const LAYERS[3][4] = {
    {"1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm,.@"},
    {"1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM,.@"},
    {"!@#$%^&*()", "-_=+/\\|~`'", "[]{}<>\":;?", "&,.!#$%@*/"},
};
const int TEXT_Y = 34, TEXT_H = 52, KB_Y = 94, KEY_W = 24, KEY_H = 34, BTN_Y = KB_Y + 5 * KEY_H + 6;

void drawText(const String &v) {
  tft.fillRect(4, TEXT_Y, SCREEN_W - 8, TEXT_H, PAPER);
  tft.drawRect(4, TEXT_Y, SCREEN_W - 8, TEXT_H, INK);
  // Show the end of the text (where typing happens) followed by a cursor.
  String shown = v;
  int maxW = SCREEN_W - 24;
  bool cut = false;
  while (shown.length() && tft.textWidth(shown + "_", 2) > maxW) {
    shown.remove(0, 1);
    cut = true;
  }
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(INK);
  tft.drawString((cut ? ".." : "") + shown + "_", 10, TEXT_Y + 18, 2);
  tft.setTextColor(MUTED);
  tft.drawString(String(v.length()) + " characters", 10, TEXT_Y + 40, 1);
}

void drawKey(int x, int y, int w, const String &label, bool pressed) {
  tft.fillRect(x + 1, y + 1, w - 2, KEY_H - 2, pressed ? GOLD : PAPER);
  tft.drawRoundRect(x + 1, y + 1, w - 2, KEY_H - 2, 3, GRID);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(INK);
  tft.drawString(label, x + w / 2, y + KEY_H / 2, 2);
}

// Control row: [shift 0-47] [symbols 48-95] [space 96-191] [backspace 192-239]
void drawControls(int layer) {
  int y = KB_Y + 4 * KEY_H;
  drawKey(0, y, 48, layer == 1 ? "abc" : "ABC", false);
  drawKey(48, y, 48, layer == 2 ? "abc" : "#+=", false);
  drawKey(96, y, 96, "space", false);
  drawKey(192, y, 48, "DEL", false);
}

void drawKeyboard(int layer) {
  tft.fillRect(0, KB_Y, SCREEN_W, 5 * KEY_H, PAPER_DARK);
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 10; c++) drawKey(c * KEY_W, KB_Y + r * KEY_H, KEY_W, String(LAYERS[layer][r][c]), false);
  drawControls(layer);
}

// Edits `value` with an on-screen keyboard. Returns true on OK.
bool editText(const String &title, String &value, int maxLen = 120) {
  String v = value;
  int layer = 0;
  tft.fillScreen(PAPER);
  titleBar(title);
  drawText(v);
  drawKeyboard(layer);
  tft.fillRect(0, BTN_Y, SCREEN_W, SCREEN_H - BTN_Y, PAPER);
  button(0, BTN_Y, SCREEN_W / 3, SCREEN_H - BTN_Y, "CANCEL", RED_DARK);
  button(SCREEN_W / 3, BTN_Y, SCREEN_W / 3, SCREEN_H - BTN_Y, "CLEAR", RED_DARK);
  button(2 * SCREEN_W / 3, BTN_Y, SCREEN_W / 3, SCREEN_H - BTN_Y, "OK", RED);

  while (true) {
    int16_t x, y;
    waitTap(x, y);
    if (y >= BTN_Y) {
      int b = x * 3 / SCREEN_W;
      if (b == 0) return false;
      if (b == 2) {
        value = v;
        return true;
      }
      v = "";
      drawText(v);
      continue;
    }
    if (y < KB_Y || y >= KB_Y + 5 * KEY_H) continue;
    int r = (y - KB_Y) / KEY_H;
    int ky = KB_Y + r * KEY_H;
    if (r < 4) {
      int c = std::min(9, x / KEY_W);
      char ch = LAYERS[layer][r][c];
      if ((int)v.length() < maxLen) v += ch;
      drawKey(c * KEY_W, ky, KEY_W, String(ch), true);
      drawText(v);
      delay(60);
      drawKey(c * KEY_W, ky, KEY_W, String(ch), false);
      if (layer == 1) {  // shift applies to one letter, like a phone keyboard
        layer = 0;
        drawKeyboard(layer);
      }
      continue;
    }
    if (x < 48) {
      layer = layer == 1 ? 0 : 1;
      drawKeyboard(layer);
    } else if (x < 96) {
      layer = layer == 2 ? 0 : 2;
      drawKeyboard(layer);
    } else if (x < 192) {
      if ((int)v.length() < maxLen) v += ' ';
      drawText(v);
    } else if (v.length()) {
      v.remove(v.length() - 1);
      drawText(v);
    }
  }
}

// ---------------------------------------------------------------- special fields

struct Zone {
  const char *name, *posix;
};
const Zone ZONES[] = {
    {"Eastern", "EST5EDT,M3.2.0,M11.1.0"},   {"Central", "CST6CDT,M3.2.0,M11.1.0"},
    {"Mountain", "MST7MDT,M3.2.0,M11.1.0"},  {"Arizona (no DST)", "MST7"},
    {"Pacific", "PST8PDT,M3.2.0,M11.1.0"},   {"Alaska", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"Hawaii", "HST10"},
};
const int NZONES = sizeof(ZONES) / sizeof(ZONES[0]);

String zoneName(const String &posix) {
  for (int i = 0; i < NZONES; i++)
    if (posix == ZONES[i].posix) return ZONES[i].name;
  return posix;
}

void editZone(AppSettings &e) {
  std::vector<String> items;
  int cur = -1;
  for (int i = 0; i < NZONES; i++) {
    items.push_back(ZONES[i].name);
    if (e.tz == ZONES[i].posix) cur = i;
  }
  int r = choose("TIME ZONE", items, cur, "CUSTOM");
  if (r >= 0) e.tz = ZONES[r].posix;
  if (r == -2) {
    String v = e.tz;
    if (editText("POSIX TIME ZONE", v) && v.length()) e.tz = v;
  }
}

void editTheme(AppSettings &e) {
  static const char *const THEMES[] = {"red", "blue", "green"};
  int cur = 0;
  for (int i = 0; i < 3; i++)
    if (e.theme.equalsIgnoreCase(THEMES[i])) cur = i;
  int r = choose("COLOR THEME", {"Red", "Blue", "Green"}, cur);
  if (r >= 0) e.theme = THEMES[r];
}

void editEnv(AppSettings &e) {
  int r = choose("ENVIRONMENT", {"prod  (ad block does nothing)", "dev  (ad block opens picker)"}, e.isDev() ? 1 : 0);
  if (r == 0) e.env = "prod";
  if (r == 1) e.env = "dev";
}

void editWifi(AppSettings &e) {
  tft.fillScreen(PAPER);
  titleBar("WI-FI NETWORK");
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(INK);
  tft.drawString("Scanning...", SCREEN_W / 2, 140, 2);
  if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();

  struct Net {
    String ssid;
    int rssi;
    bool open;
  };
  std::vector<Net> nets;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.isEmpty()) continue;
    auto it = std::find_if(nets.begin(), nets.end(), [&](const Net &x) { return x.ssid == s; });
    if (it == nets.end()) nets.push_back({s, WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN});
    else it->rssi = std::max(it->rssi, (int)WiFi.RSSI(i));
  }
  WiFi.scanDelete();
  std::sort(nets.begin(), nets.end(), [](const Net &a, const Net &b) { return a.rssi > b.rssi; });

  std::vector<String> items;
  int cur = -1;
  for (auto &x : nets) {
    int quality = constrain(2 * (x.rssi + 100), 0, 100);
    if (x.ssid == e.ssid) cur = items.size();
    items.push_back(x.ssid + "  " + quality + "%" + (x.open ? " open" : ""));
  }
  int r = choose("WI-FI NETWORK", items, cur, "TYPE NAME");
  if (r == -2) {
    String v = e.ssid;
    if (!editText("WI-FI NAME", v) || v.isEmpty()) return;
    e.ssid = v;
  } else if (r >= 0) {
    if (nets[r].ssid != e.ssid) e.pass = "";
    e.ssid = nets[r].ssid;
    if (nets[r].open) {
      e.pass = "";
      return;
    }
  } else {
    return;
  }
  String p = e.pass;
  if (editText("PASSWORD: " + e.ssid, p, 64)) e.pass = p;
}

// ---------------------------------------------------------------- settings list

enum Kind { TEXT, SECRET, WIFI, ZONE, ENV, THEME };
struct Field {
  const char *label;
  String AppSettings::*member;
  Kind kind;
};
const Field FIELDS[] = {
    {"Wi-Fi", &AppSettings::ssid, WIFI},
    {"Password", &AppSettings::pass, SECRET},
    {"Time zone", &AppSettings::tz, ZONE},
    {"Organization", &AppSettings::adCompany, TEXT},
    {"Address 1", &AppSettings::adAddress1, TEXT},
    {"Address 2", &AppSettings::adAddress2, TEXT},
    {"Contact", &AppSettings::adContact, TEXT},
    {"Motto", &AppSettings::adMotto, TEXT},
    {"Env", &AppSettings::env, ENV},
    {"Theme", &AppSettings::theme, THEME},
    {"Location", &AppSettings::location, TEXT},
    {"Catalog URL", &AppSettings::catalogUrl, TEXT},
};
const int NFIELDS = sizeof(FIELDS) / sizeof(FIELDS[0]);
const int LIST_Y = 30, LIST_ROW = 21, SAVE_Y = LIST_Y + NFIELDS * LIST_ROW + 4;

String shownValue(const AppSettings &e, const Field &f) {
  const String &v = e.*(f.member);
  switch (f.kind) {
    case SECRET: return v.isEmpty() ? "" : String("********");
    case ZONE: return zoneName(v);
    case ENV: return v.isEmpty() ? "prod (default)" : v;
    case THEME: {
      String t = v.isEmpty() ? String("red") : v;
      t.toLowerCase();
      t.setCharAt(0, toupper(t[0]));
      return t;
    }
    default: return v;
  }
}

void drawList(const AppSettings &e, bool dirty) {
  tft.fillScreen(PAPER);
  titleBar(dirty ? "SETTINGS *" : "SETTINGS");
  for (int i = 0; i < NFIELDS; i++) {
    int y = LIST_Y + i * LIST_ROW;
    tft.drawFastHLine(0, y + LIST_ROW - 1, SCREEN_W, GRID);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(RED_DARK);
    tft.drawString(FIELDS[i].label, 4, y + LIST_ROW / 2, 1);
    String v = shownValue(e, FIELDS[i]);
    tft.setTextColor(v.isEmpty() ? MUTED : INK);
    tft.drawString(fit(v.isEmpty() ? "(not set)" : v, SCREEN_W - 84, 2), 80, y + LIST_ROW / 2, 2);
  }
  tft.fillRect(0, SAVE_Y, SCREEN_W, SCREEN_H - SAVE_Y, PAPER_DARK);
  button(0, SAVE_Y, SCREEN_W / 2, SCREEN_H - SAVE_Y, "CANCEL", RED_DARK);
  button(SCREEN_W / 2, SAVE_Y, SCREEN_W / 2, SCREEN_H - SAVE_Y, "SAVE", RED);
}

bool sameSettings(const AppSettings &a, const AppSettings &b) {
  for (int i = 0; i < NFIELDS; i++)
    if (a.*(FIELDS[i].member) != b.*(FIELDS[i].member)) return false;
  return true;
}
}  // namespace

namespace Setup {

bool run() {
  initColors();
  AppSettings e = settings;
  while (true) {
    drawList(e, !sameSettings(e, settings));
    int16_t x, y;
    waitTap(x, y);
    if (y >= SAVE_Y) {
      if (x < SCREEN_W / 2) return false;  // CANCEL
      UI::drawMessage("SETTINGS", "Saving " CONFIG_PATH "...");
      if (!e.saveConfig()) {
        UI::drawMessage("SAVE FAILED", "Could not write", CONFIG_PATH, "Is the SD card OK?");
        delay(2500);
        continue;
      }
      settings.loadConfig();
      return true;
    }
    if (y < LIST_Y) continue;
    int i = (y - LIST_Y) / LIST_ROW;
    if (i >= NFIELDS) continue;
    const Field &f = FIELDS[i];
    switch (f.kind) {
      case WIFI: editWifi(e); break;
      case ZONE: editZone(e); break;
      case ENV: editEnv(e); break;
      case THEME: editTheme(e); break;
      default: {
        String v = e.*(f.member);
        if (editText(f.label, v, f.kind == SECRET ? 64 : 120)) e.*(f.member) = v;
      }
    }
  }
}

}  // namespace Setup
