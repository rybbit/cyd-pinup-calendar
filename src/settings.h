#pragma once
#include <Arduino.h>

// Settings come from two places:
//   * /config/wifi.conf on the SD card (key=value lines):
//         ssid=MyNetwork
//         pass=MyPassword
//         tz=CST6CDT,M3.2.0,M11.1.0            (optional, default US Eastern)
//         catalog=https://.../catalog.json     (optional)
//     Blank lines and lines starting with # are ignored.
//   * NVS (flash): the currently selected calendar.
struct AppSettings {
  String calId;       // currently selected calendar id (NVS)
  String ssid, pass;  // from wifi.conf
  String catalogUrl;  // from wifi.conf or CATALOG_URL_DEFAULT
  String tz;          // POSIX TZ string, from wifi.conf or TZ_DEFAULT
  bool confFound = false;

  void load();          // NVS values + defaults
  bool loadWifiConf();  // reads WIFI_CONF_PATH; SD must be mounted
  void save();          // writes calId to NVS
};

extern AppSettings settings;
