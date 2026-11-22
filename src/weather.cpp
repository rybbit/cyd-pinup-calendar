#include "weather.h"

#include <ArduinoJson.h>

#include "net.h"
#include "settings.h"

namespace {
Weather::Now now;
String resolvedFor;  // settings.location the lat/lon below belong to
float lat = 0, lon = 0;
bool haveLoc = false;
bool tried = false;
uint32_t lastTry = 0;

const uint32_t REFRESH_MS = 15UL * 60 * 1000, RETRY_MS = 2UL * 60 * 1000, STALE_MS = 2UL * 3600 * 1000;

String urlEncode(const String &s) {
  String out;
  for (char c : s) {
    if (isalnum((unsigned char)c) || c == '-' || c == '.' || c == '_') out += c;
    else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}

bool resolveLocation() {
  String loc = settings.location;
  loc.trim();
  if (loc.isEmpty()) return false;
  if (haveLoc && loc == resolvedFor) return true;
  haveLoc = false;
  resolvedFor = loc;
  int comma = loc.indexOf(',');
  if (comma > 0 && isdigit((unsigned char)loc[loc.length() - 1])) {  // "44.98,-93.27"
    String a = loc.substring(0, comma), b = loc.substring(comma + 1);
    a.trim();
    b.trim();
    lat = a.toFloat();
    lon = b.toFloat();
    haveLoc = lat != 0 || lon != 0;
    return haveLoc;
  }
  String body;
  if (!Net::getString("https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(loc) +
                          "&count=1&countryCode=US",
                      body))
    return false;
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  JsonObject r = doc["results"][0];
  if (r.isNull()) {
    Serial.printf("Weather: location \"%s\" not found\n", loc.c_str());
    return false;
  }
  lat = r["latitude"] | 0.0f;
  lon = r["longitude"] | 0.0f;
  haveLoc = true;
  Serial.printf("Weather: \"%s\" -> %s, %s (%.3f, %.3f)\n", loc.c_str(), (const char *)(r["name"] | "?"),
                (const char *)(r["admin1"] | "?"), lat, lon);
  return true;
}
}  // namespace

namespace Weather {

void update() {
  uint32_t ms = millis();
  if (tried && ms - lastTry < (now.valid ? REFRESH_MS : RETRY_MS)) return;
  if (!Net::online() || settings.location.isEmpty()) return;
  tried = true;
  lastTry = ms;
  if (!resolveLocation()) return;

  String body;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) + "&longitude=" + String(lon, 4) +
               "&current=temperature_2m,relative_humidity_2m,pressure_msl";  // Celsius, hPa
  if (!Net::getString(url, body)) return;
  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  JsonObject c = doc["current"];
  if (c.isNull()) return;
  now.tempC = c["temperature_2m"] | 0.0f;
  now.tempF = now.tempC * 9 / 5 + 32;
  now.humidity = c["relative_humidity_2m"] | 0;
  now.pressureHpa = c["pressure_msl"] | 0.0f;
  now.pressureInHg = now.pressureHpa * 0.0295300f;
  now.at = millis();
  now.valid = true;
  Serial.printf("Weather: %.1fC/%.1fF %d%% %.0f hPa/%.2f inHg\n", now.tempC, now.tempF, now.humidity, now.pressureHpa,
                now.pressureInHg);
}

const Now &current() {
  if (now.valid && millis() - now.at > STALE_MS) now.valid = false;
  return now;
}

void invalidate() {
  now = Now();
  haveLoc = false;
  resolvedFor = "";
  tried = false;
}

}  // namespace Weather
