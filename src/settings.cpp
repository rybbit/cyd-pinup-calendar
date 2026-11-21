#include "settings.h"

#include <Preferences.h>

#include "config.h"

AppSettings settings;

void AppSettings::load() {
  Preferences p;
  p.begin("pinup", true);
  calId = p.getString("cal", "");
  catalogUrl = p.getString("url", CATALOG_URL_DEFAULT);
  tz = p.getString("tz", TZ_DEFAULT);
  p.end();
  if (catalogUrl.isEmpty()) catalogUrl = CATALOG_URL_DEFAULT;
  if (tz.isEmpty()) tz = TZ_DEFAULT;
}

void AppSettings::save() {
  Preferences p;
  p.begin("pinup", false);
  p.putString("cal", calId);
  p.putString("url", catalogUrl);
  p.putString("tz", tz);
  p.end();
}
