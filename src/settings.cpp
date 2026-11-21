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

bool AppSettings::loadConfig() {
  ssid = "";
  pass = "";
  catalogUrl = CATALOG_URL_DEFAULT;
  tz = TZ_DEFAULT;
  adCompany = adAddress1 = adAddress2 = adContact = adMotto = "";
  confFound = false;

  if (!SD.exists(CONFIG_PATH)) return false;
  File f = SD.open(CONFIG_PATH);
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
    else {
      if (key.startsWith("ad_")) key = key.substring(3);  // ad_company and company both work
      if (key == "organization" || key == "company") adCompany = val;
      else if (key == "address1") adAddress1 = val;
      else if (key == "address2") adAddress2 = val;
      else if (key == "contact" || key == "phone" || key == "email") adContact = val;
      else if (key == "motto") adMotto = val;
      else Serial.printf("config.txt: unknown key \"%s\" (ignored)\n", key.c_str());
    }
  }
  f.close();
  Serial.printf("config.txt: organization=\"%s\" address1=\"%s\" address2=\"%s\" contact=\"%s\" motto=\"%s\"\n",
                adCompany.c_str(), adAddress1.c_str(), adAddress2.c_str(), adContact.c_str(), adMotto.c_str());
  Serial.printf("config.txt: ssid=\"%s\" pass=%s tz=%s\n", ssid.c_str(), pass.length() ? "(set)" : "(none)",
                tz.c_str());
  return ssid.length() > 0;
}

void AppSettings::save() {
  Preferences p;
  p.begin("pinup", false);
  p.putString("cal", calId);
  p.end();
}
