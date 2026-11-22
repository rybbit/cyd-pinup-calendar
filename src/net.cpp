#include "net.h"

#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "config.h"
#include "settings.h"

namespace Net {

bool connect(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (settings.ssid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(200);
  return WiFi.status() == WL_CONNECTED;
}

bool reconnect(uint32_t timeoutMs) {
  WiFi.disconnect(true);
  delay(300);
  return connect(timeoutMs);
}

void kick() {
  if (settings.ssid.isEmpty() || WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());
}

void startTimeSync() { configTzTime(settings.tz.c_str(), "pool.ntp.org", "time.nist.gov"); }

bool online() { return WiFi.status() == WL_CONNECTED; }

bool timeValid() { return time(nullptr) > 1700000000; }

bool syncTime(uint32_t timeoutMs) {
  startTimeSync();
  uint32_t start = millis();
  while (!timeValid() && millis() - start < timeoutMs) delay(200);
  return timeValid();
}

bool getString(const String &url, String &out, uint32_t timeoutMs) {
  if (!online()) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(timeoutMs);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  bool ok = code == HTTP_CODE_OK;
  if (ok) out = http.getString();
  else Serial.printf("GET %s -> %d\n", url.c_str(), code);
  http.end();
  return ok;
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
  if (SD.exists(tmp)) SD.remove(tmp);
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
  if (SD.exists(path)) SD.remove(path);
  if (!SD.rename(tmp, path)) return -1;
  Serial.printf("  got %s (%d bytes)\n", path.c_str(), written);
  return written;
}

}  // namespace Net
