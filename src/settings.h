#pragma once
#include <Arduino.h>

// Settings come from two places:
//   * /calendar/config.txt on the SD card (key=value lines):
//         ssid=MyNetwork
//         pass=MyPassword
//         tz=CST6CDT,M3.2.0,M11.1.0            (optional, default US Eastern)
//         catalog=https://.../catalog.json     (optional)
//         organization=MODERN ENGINEERING CO.  (optional advertising block; each
//         address1=3555 SCOTT AVENUE            line takes space only when set.
//         address2=ST. LOUIS, MO 63103          "company" and ad_-prefixed names
//         contact=PHONE: JEFFERSON 3-3100       like ad_company also work; contact
//                                               may be a phone, email, website...)
//         motto=Everything For Welding
//     Blank lines and lines starting with # are ignored.
//   * NVS (flash): the currently selected calendar.
struct AppSettings {
  String calId;       // currently selected calendar id (NVS)
  String ssid, pass;  // from config.txt
  String catalogUrl;  // from config.txt or CATALOG_URL_DEFAULT
  String tz;          // POSIX TZ string, from config.txt or TZ_DEFAULT
  bool confFound = false;
  String adCompany, adAddress1, adAddress2, adContact, adMotto;
  bool hasAd() const {
    return adCompany.length() || adAddress1.length() || adAddress2.length() || adContact.length() || adMotto.length();
  }

  void load();          // NVS values + defaults
  bool loadConfig();  // reads CONFIG_PATH; SD must be mounted
  void save();          // writes calId to NVS
};

extern AppSettings settings;
