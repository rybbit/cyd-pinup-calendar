#include "net.h"

#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <time.h>

#include "config.h"
#include "settings.h"

namespace Net {

bool hasCredentials() {
  WiFiManager wm;
  return wm.getWiFiIsSaved();
}

bool connect(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin();  // uses credentials saved by WiFiManager
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(200);
  return WiFi.status() == WL_CONNECTED;
}

bool runPortal() {
  WiFiManager wm;
  WiFiManagerParameter pUrl("catalog", "Calendar catalog URL", settings.catalogUrl.c_str(), 200);
  WiFiManagerParameter pTz("tz", "Time zone (POSIX, e.g. CST6CDT,M3.2.0,M11.1.0)", settings.tz.c_str(), 64);
  wm.addParameter(&pUrl);
  wm.addParameter(&pTz);
  wm.setTitle("Pin-Up Calendar");
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  wm.setBreakAfterConfig(true);  // return even if the new credentials fail, so params still save
  bool ok = wm.startConfigPortal(WIFI_AP_NAME);

  String url = pUrl.getValue();
  String tz = pTz.getValue();
  url.trim();
  tz.trim();
  bool changed = false;
  if (url.length() && url != settings.catalogUrl) { settings.catalogUrl = url; changed = true; }
  if (tz.length() && tz != settings.tz) { settings.tz = tz; changed = true; }
  if (changed) settings.save();

  if (!ok) ok = connect(10000);
  return ok;
}

bool online() { return WiFi.status() == WL_CONNECTED; }

bool timeValid() { return time(nullptr) > 1700000000; }

bool syncTime(uint32_t timeoutMs) {
  configTzTime(settings.tz.c_str(), "pool.ntp.org", "time.nist.gov");
  uint32_t start = millis();
  while (!timeValid() && millis() - start < timeoutMs) delay(200);
  return timeValid();
}

long download(const String &url, const String &path) {
  if (!online()) return -1;
  WiFiClientSecure client;
  // Public images from GitHub; skip CA pinning so certificate rotations never brick downloads.
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  if (!http.begin(client, url)) return -1;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("GET %s -> %d\n", url.c_str(), code);
    http.end();
    return -1;
  }
  int expected = http.getSize();  // -1 when chunked
  String tmp = path + ".part";
  SD.remove(tmp);
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) {
    http.end();
    return -1;
  }
  int written = http.writeToStream(&f);
  f.close();
  http.end();
  if (written <= 0 || (expected > 0 && written != expected)) {
    Serial.printf("Short download %s (%d of %d)\n", url.c_str(), written, expected);
    SD.remove(tmp);
    return -1;
  }
  SD.remove(path);
  if (!SD.rename(tmp, path)) return -1;
  return written;
}

}  // namespace Net
