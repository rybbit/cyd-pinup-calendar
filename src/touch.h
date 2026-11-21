#pragma once
#include <Arduino.h>

class TFT_eSPI;

// Bit-banged XPT2046 resistive touch with a 3-point calibration stored in NVS.
namespace Touch {
void begin();
// True while the panel is pressed; x/y are screen coordinates (clamped).
bool read(int16_t &x, int16_t &y);
// Returns true once per tap (on press), with debounce. Use this for UI.
bool tapped(int16_t &x, int16_t &y);
// Blocks until the screen is released.
void waitRelease();
bool hasCalibration();
void calibrate(TFT_eSPI &tft);
}  // namespace Touch
