#include "touch.h"

#include <Preferences.h>

#include <utility>
#include <TFT_eSPI.h>

#include "config.h"

namespace {
const int Z_THRESHOLD = 400;

struct Cal {
  int16_t a0, a1;  // raw value on the "x" raw axis at screen x=20 and x=220
  int16_t b0, b2;  // raw value on the "y" raw axis at screen y=20 and y=300
  uint8_t swap;    // 1 if raw X drives screen Y
  uint8_t valid;
} cal;

bool wasDown = false;
uint32_t lastChange = 0;

uint16_t xfer(uint8_t cmd) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(TOUCH_MOSI, (cmd >> i) & 1);
    digitalWrite(TOUCH_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(TOUCH_CLK, LOW);
    delayMicroseconds(1);
  }
  digitalWrite(TOUCH_MOSI, LOW);
  uint16_t r = 0;
  for (int i = 0; i < 16; i++) {
    digitalWrite(TOUCH_CLK, HIGH);
    delayMicroseconds(1);
    r = (r << 1) | digitalRead(TOUCH_MISO);
    digitalWrite(TOUCH_CLK, LOW);
    delayMicroseconds(1);
  }
  return (r >> 3) & 0x0FFF;
}

int16_t median3(int16_t a, int16_t b, int16_t c) {
  if (a > b) std::swap(a, b);
  if (b > c) std::swap(b, c);
  if (a > b) std::swap(a, b);
  return b;
}

// Raw sample. Returns false if not pressed hard enough.
bool readRaw(int16_t &rx, int16_t &ry) {
  digitalWrite(TOUCH_CS, LOW);
  int z1 = xfer(0xB1);
  int z2 = xfer(0xC1);
  int z = z1 + 4095 - z2;
  bool down = z >= Z_THRESHOLD;
  if (down) {
    xfer(0x91);  // throw away first (settling) sample
    int16_t y1 = xfer(0x91), y2 = xfer(0x91), y3 = xfer(0x91);
    int16_t x1 = xfer(0xD1), x2 = xfer(0xD1), x3 = xfer(0xD1);
    rx = median3(x1, x2, x3);
    ry = median3(y1, y2, y3);
  }
  xfer(0xD0);  // power down, keep PENIRQ enabled
  digitalWrite(TOUCH_CS, HIGH);
  return down;
}

bool readRawStable(int16_t &rx, int16_t &ry) {
  int16_t ax, ay, bx, by;
  if (!readRaw(ax, ay)) return false;
  delay(2);
  if (!readRaw(bx, by)) return false;
  if (abs(ax - bx) > 80 || abs(ay - by) > 80) return false;  // still settling
  rx = (ax + bx) / 2;
  ry = (ay + by) / 2;
  return true;
}

void saveCal() {
  Preferences p;
  p.begin("touch", false);
  p.putBytes("cal", &cal, sizeof(cal));
  p.end();
}
}  // namespace

namespace Touch {

void begin() {
  pinMode(TOUCH_CLK, OUTPUT);
  pinMode(TOUCH_MOSI, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(TOUCH_MISO, INPUT);
  pinMode(TOUCH_IRQ, INPUT);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(TOUCH_CLK, LOW);

  Preferences p;
  p.begin("touch", true);
  if (p.getBytes("cal", &cal, sizeof(cal)) != sizeof(cal)) cal.valid = 0;
  p.end();
  if (!cal.valid) {
    // Typical defaults for a CYD in portrait (rotation 0); calibration refines these.
    cal = Cal{3700, 380, 300, 3750, 0, 0};
  }
}

bool hasCalibration() { return cal.valid; }

bool read(int16_t &x, int16_t &y) {
  int16_t rx, ry;
  if (!readRawStable(rx, ry)) return false;
  int16_t a = cal.swap ? ry : rx;
  int16_t b = cal.swap ? rx : ry;
  long sx = 20 + (long)(a - cal.a0) * 200 / (cal.a1 - cal.a0);
  long sy = 20 + (long)(b - cal.b0) * 280 / (cal.b2 - cal.b0);
  x = constrain(sx, 0, SCREEN_W - 1);
  y = constrain(sy, 0, SCREEN_H - 1);
  return true;
}

bool tapped(int16_t &x, int16_t &y) {
  bool down = read(x, y);
  uint32_t now = millis();
  if (down != wasDown && now - lastChange > 60) {
    wasDown = down;
    lastChange = now;
    return down;  // report on the press edge only
  }
  return false;
}

void waitRelease() {
  int16_t x, y;
  uint32_t upSince = millis();
  while (millis() - upSince < 80) {
    if (read(x, y)) upSince = millis();
    delay(5);
  }
  wasDown = false;
  lastChange = millis();
}

void calibrate(TFT_eSPI &tft) {
  const int16_t px[3] = {20, 220, 20};
  const int16_t py[3] = {20, 20, 300};
  int16_t rx[3], ry[3];

  waitRelease();
  for (int i = 0; i < 3; i++) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Touch calibration", SCREEN_W / 2, 130, 2);
    tft.drawString("Tap the crosshair with the stylus", SCREEN_W / 2, 155, 2);
    tft.drawString(String(i + 1) + " / 3", SCREEN_W / 2, 180, 2);
    tft.drawLine(px[i] - 12, py[i], px[i] + 12, py[i], TFT_RED);
    tft.drawLine(px[i], py[i] - 12, px[i], py[i] + 12, TFT_RED);
    tft.drawCircle(px[i], py[i], 6, TFT_YELLOW);

    // Average a number of stable samples while the stylus is held
    long sx = 0, sy = 0;
    int n = 0;
    while (n < 16) {
      int16_t x, y;
      if (readRawStable(x, y)) {
        sx += x;
        sy += y;
        n++;
      }
      delay(10);
    }
    rx[i] = sx / n;
    ry[i] = sy / n;
    tft.fillCircle(px[i], py[i], 6, TFT_GREEN);
    waitRelease();
    delay(200);
  }

  // Moving right (point 0 -> 1) changes screen x: whichever raw axis moved most is the x axis.
  bool swap = abs(ry[1] - ry[0]) > abs(rx[1] - rx[0]);
  cal.swap = swap;
  cal.a0 = swap ? ry[0] : rx[0];
  cal.a1 = swap ? ry[1] : rx[1];
  cal.b0 = swap ? rx[0] : ry[0];
  cal.b2 = swap ? rx[2] : ry[2];
  cal.valid = (abs(cal.a1 - cal.a0) > 400 && abs(cal.b2 - cal.b0) > 400);
  if (cal.valid) saveCal();

  tft.fillScreen(TFT_BLACK);
  tft.drawString(cal.valid ? "Calibration saved" : "Calibration failed - using defaults", SCREEN_W / 2,
                 SCREEN_H / 2, 2);
  delay(1000);
  if (!cal.valid) cal = Cal{3700, 380, 300, 3750, 0, 0};
}

}  // namespace Touch
