#pragma once
#include <Arduino.h>

namespace Net {
// Connects using settings.ssid / settings.pass (from /calendar/config.txt).
// Returns true when online.
bool connect(uint32_t timeoutMs = 20000);
// Drops the current connection and connects with the current settings.
bool reconnect(uint32_t timeoutMs = 20000);
// Starts a connection attempt in the background (non-blocking).
void kick();
// Starts NTP without waiting.
void startTimeSync();
bool online();
// Starts NTP using settings.tz and waits up to timeoutMs for a valid time.
bool syncTime(uint32_t timeoutMs = 10000);
bool timeValid();
// HTTPS GET of a small text response (JSON from a web API) into `out`.
bool getString(const String &url, String &out, uint32_t timeoutMs = 8000);
// HTTPS GET of `url` into `path` on the SD card (via a temp file). Returns bytes written or -1.
long download(const String &url, const String &path);
}  // namespace Net
