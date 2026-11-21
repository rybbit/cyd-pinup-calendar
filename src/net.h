#pragma once
#include <Arduino.h>

namespace Net {
// Connects using settings.ssid / settings.pass (from /config/wifi.conf).
// Returns true when online.
bool connect(uint32_t timeoutMs = 20000);
// Drops the current connection and connects again (after wifi.conf was re-read).
bool reconnect(uint32_t timeoutMs = 20000);
bool online();
// Starts NTP using settings.tz and waits up to timeoutMs for a valid time.
bool syncTime(uint32_t timeoutMs = 10000);
bool timeValid();
// HTTPS GET of `url` into `path` on the SD card (via a temp file). Returns bytes written or -1.
long download(const String &url, const String &path);
}  // namespace Net
