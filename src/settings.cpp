#include "settings.h"

#include <Preferences.h>
#include <SD.h>

#include "config.h"

AppSettings settings;

void AppSettings::load() {
  Preferences p;
  p.begin("pinup", true);
  calId = p.getString("cal", "");
  p.end();
  catalogUrl = CATALOG_URL_DEFAULT;
  tz = TZ_DEFAULT;
}

bool AppSettings::loadWifiConf() {
  ssid = "";
  pass = "";
  catalogUrl = CATALOG_URL_DEFAULT;
  tz = TZ_DEFAULT;
  confFound = false;

  File f = SD.open(WIFI_CONF_PATH);
  if (!f) return false;
  confFound = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();  // also strips \r from Windows line endings
    if (line.isEmpty() || line[0] == '#') continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String key = line.substring(0, eq);
    String val = line.substring(eq + 1);
    key.trim();
    key.toLowerCase();
    val.trim();
    if (key == "ssid") ssid = val;
    else if (key == "pass" || key == "password" || key == "psk") pass = val;
    else if ((key == "tz" || key == "timezone") && val.length()) tz = val;
    else if ((key == "catalog" || key == "catalog_url") && val.length()) catalogUrl = val;
  }
  f.close();
  Serial.printf("wifi.conf: ssid=\"%s\" pass=%s tz=%s\n", ssid.c_str(), pass.length() ? "(set)" : "(none)",
                tz.c_str());
  return ssid.length() > 0;
}

void AppSettings::save() {
  Preferences p;
  p.begin("pinup", false);
  p.putString("cal", calId);
  p.end();
}
