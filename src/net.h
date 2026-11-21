#pragma once
#include <Arduino.h>

namespace Net {
// True if Wi-Fi credentials have been stored by a previous setup.
bool hasCredentials();
// Connects with stored credentials (no portal). Returns true when online.
bool connect(uint32_t timeoutMs = 15000);
// Runs the captive setup portal (blocking, 3 min timeout). Also lets the user
// edit the catalog URL and time zone; changes are written to `settings`.
bool runPortal();
bool online();
// Starts NTP using settings.tz and waits up to timeoutMs for a valid time.
bool syncTime(uint32_t timeoutMs = 10000);
bool timeValid();
// HTTPS GET of `url` into `path` on the SD card (via a temp file). Returns bytes written or -1.
long download(const String &url, const String &path);
}  // namespace Net
