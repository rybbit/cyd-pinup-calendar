#pragma once
#include <Arduino.h>

// Settings come from two places:
//   * /config/wifi.conf on the SD card (key=value lines):
//         ssid=MyNetwork
//         pass=MyPassword
//         tz=CST6CDT,M3.2.0,M11.1.0            (optional, default US Eastern)
//         catalog=https://.../catalog.json     (optional)
//         ad_company=MODERN ENGINEERING CO.    (optional advertising block; any
//         ad_address1=3555 SCOTT AVENUE         ad_ field switches the calendar page
//         ad_address2=ST. LOUIS, MO 63103       to the advertising layout)
//         ad_phone=PHONE: JEFFERSON 3-3100
//         ad_motto=Everything For Welding
//     Blank lines and lines starting with # are ignored.
//   * NVS (flash): the currently selected calendar.
struct AppSettings {
  String calId;       // currently selected calendar id (NVS)
  String ssid, pass;  // from wifi.conf
  String catalogUrl;  // from wifi.conf or CATALOG_URL_DEFAULT
  String tz;          // POSIX TZ string, from wifi.conf or TZ_DEFAULT
  bool confFound = false;
  String adCompany, adAddress1, adAddress2, adPhone, adMotto;
  bool hasAd() const {
    return adCompany.length() || adAddress1.length() || adAddress2.length() || adPhone.length() || adMotto.length();
  }

  void load();          // NVS values + defaults
  bool loadWifiConf();  // reads WIFI_CONF_PATH; SD must be mounted
  void save();          // writes calId to NVS
};

extern AppSettings settings;
