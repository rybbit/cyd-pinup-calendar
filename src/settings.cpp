#include "settings.h"

#include <Preferences.h>
#include <SD.h>

#include <vector>

#include "config.h"

AppSettings settings;

namespace {
// Canonical config.txt keys, in the order new keys are appended when saving.
const char *const KEYS[] = {"ssid",     "pass",     "tz",      "catalog", "organization",
                            "address1", "address2", "contact", "motto",   "env", "theme"};
const int NKEYS = sizeof(KEYS) / sizeof(KEYS[0]);

String *member(AppSettings &s, int i) {
  String *m[] = {&s.ssid,       &s.pass,       &s.tz,        &s.catalogUrl, &s.adCompany,
                 &s.adAddress1, &s.adAddress2, &s.adContact, &s.adMotto,    &s.env, &s.theme};
  return m[i];
}
}  // namespace

// Accepted spellings -> canonical key ("" if unknown). ad_ prefixes are optional.
String canonicalKey(String key) {
  key.trim();
  key.toLowerCase();
  if (key.startsWith("ad_")) key = key.substring(3);
  if (key == "pass" || key == "password" || key == "psk") return "pass";
  if (key == "tz" || key == "timezone") return "tz";
  if (key == "catalog" || key == "catalog_url") return "catalog";
  if (key == "organization" || key == "company") return "organization";
  if (key == "contact" || key == "phone" || key == "email") return "contact";
  for (int i = 0; i < NKEYS; i++)
    if (key == KEYS[i]) return key;
  return "";
}

void AppSettings::load() {
  Preferences p;
  p.begin("pinup", true);
  calId = p.getString("cal", "");
  p.end();
  catalogUrl = CATALOG_URL_DEFAULT;
  tz = TZ_DEFAULT;
  computeDisplay();
}

void AppSettings::computeDisplay() {
  showCompany = adCompany;
  showAddress1 = adAddress1;
  showAddress2 = adAddress2;
  showContact = adContact;
  // In dev, an empty marketing block shows the project's own details instead.
  if (isDev() && adCompany.isEmpty() && adAddress1.isEmpty() && adAddress2.isEmpty() && adContact.isEmpty()) {
    showCompany = DEV_ORGANIZATION;
    showContact = DEV_CONTACT;
  }
}

bool AppSettings::loadConfig() {
  ssid = pass = env = theme = "";
  catalogUrl = CATALOG_URL_DEFAULT;
  tz = TZ_DEFAULT;
  adCompany = adAddress1 = adAddress2 = adContact = adMotto = "";
  confFound = false;

  if (!SD.exists(CONFIG_PATH)) {
    computeDisplay();
    return false;
  }
  File f = SD.open(CONFIG_PATH);
  if (!f) {
    computeDisplay();
    return false;
  }
  confFound = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();  // also strips \r from Windows line endings
    if (line.isEmpty() || line[0] == '#') continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String key = canonicalKey(line.substring(0, eq));
    String val = line.substring(eq + 1);
    val.trim();
    if (key.isEmpty()) {
      Serial.printf("config.txt: unknown key \"%s\" (ignored)\n", line.substring(0, eq).c_str());
      continue;
    }
    if ((key == "tz" || key == "catalog") && val.isEmpty()) continue;  // keep the default
    for (int i = 0; i < NKEYS; i++)
      if (key == KEYS[i]) *member(*this, i) = val;
  }
  f.close();
  env.toLowerCase();
  computeDisplay();
  Serial.printf("config.txt: organization=\"%s\" address1=\"%s\" address2=\"%s\" contact=\"%s\" motto=\"%s\" env=%s\n",
                adCompany.c_str(), adAddress1.c_str(), adAddress2.c_str(), adContact.c_str(), adMotto.c_str(),
                env.length() ? env.c_str() : "(prod)");
  Serial.printf("config.txt: ssid=\"%s\" pass=%s tz=%s\n", ssid.c_str(), pass.length() ? "(set)" : "(none)",
                tz.c_str());
  return ssid.length() > 0;
}

// Rewrites config.txt in place: lines for known keys get the new value (keeping the
// user's spelling of the key), comments and unknown lines are kept, and keys that are
// set but not in the file yet are appended.
bool AppSettings::saveConfig() const {
  AppSettings &self = const_cast<AppSettings &>(*this);  // member() needs a non-const ref; nothing is modified
  bool written[NKEYS] = {false};
  std::vector<String> out;

  if (SD.exists(CONFIG_PATH)) {
    File f = SD.open(CONFIG_PATH);
    while (f && f.available()) {
      String raw = f.readStringUntil('\n');
      raw.replace("\r", "");
      String line = raw;
      line.trim();
      int eq = line.indexOf('=');
      bool replaced = false;
      if (line.length() && line[0] != '#' && eq > 0) {
        String spelled = line.substring(0, eq);
        spelled.trim();
        String key = canonicalKey(spelled);
        for (int i = 0; i < NKEYS && !replaced; i++) {
          if (key == KEYS[i]) {
            out.push_back(spelled + "=" + *member(self, i));
            written[i] = replaced = true;
          }
        }
      }
      if (!replaced) out.push_back(raw);
    }
    if (f) f.close();
  }
  for (int i = 0; i < NKEYS; i++) {
    const String &v = *member(self, i);
    if (written[i] || v.isEmpty()) continue;
    if (String(KEYS[i]) == "tz" && v == TZ_DEFAULT) continue;
    if (String(KEYS[i]) == "catalog" && v == CATALOG_URL_DEFAULT) continue;
    out.push_back(String(KEYS[i]) + "=" + v);
  }

  String tmp = String(CONFIG_PATH) + ".new";
  if (SD.exists(tmp)) SD.remove(tmp);
  File w = SD.open(tmp, FILE_WRITE);
  if (!w) return false;
  for (auto &l : out) {
    w.print(l);
    w.print("\n");
  }
  w.close();
  if (SD.exists(CONFIG_PATH)) SD.remove(CONFIG_PATH);
  bool ok = SD.rename(tmp, CONFIG_PATH);
  Serial.printf("config.txt: saved (%d lines) %s\n", (int)out.size(), ok ? "OK" : "FAILED");
  return ok;
}

void AppSettings::save() {
  Preferences p;
  p.begin("pinup", false);
  p.putString("cal", calId);
  p.end();
}
