#pragma once
#include <Arduino.h>

// Current outdoor conditions from Open-Meteo (free, no API key) for settings.location:
// a US ZIP code or place name (looked up once with Open-Meteo's geocoder) or "lat,lon".
namespace Weather {
struct Now {
  bool valid = false;
  float tempC = 0, tempF = 0;
  int humidity = 0;                     // %
  float pressureHpa = 0, pressureInHg = 0;  // sea-level pressure
  uint32_t at = 0;       // millis() of the reading
};
// Call often (from loop): fetches when online and due (every 15 min; retries failures
// every 2 min). Blocks for a second or two while fetching.
void update();
// The latest reading; invalid if none yet, no location is set, or it is over 2 hours old.
const Now &current();
// Forget the cached location and reading (after settings change).
void invalidate();
}  // namespace Weather
