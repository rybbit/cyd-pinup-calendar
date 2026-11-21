#pragma once
#include <Arduino.h>

// Settings come from two places:
//   * /calendar/config.txt on the SD card (key=value lines), editable on the device
//     from the picker's SETUP screen:
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
//         env=dev                               (optional: dev or prod, default prod)
//     Blank lines and lines starting with # are ignored.
//   * NVS (flash): the currently selected calendar.
//
// env=dev: tapping the advertising block opens the calendar picker, and an empty block
// (no organization, addresses or contact) shows the project's details instead.
struct AppSettings {
  String calId;       // currently selected calendar id (NVS)
  String ssid, pass;  // from config.txt
  String catalogUrl;  // from config.txt or CATALOG_URL_DEFAULT
  String tz;          // POSIX TZ string, from config.txt or TZ_DEFAULT
  String env;         // "dev" or "prod" ("" = prod)
  bool confFound = false;
  // Raw values from config.txt (what the SETUP screen edits)
  String adCompany, adAddress1, adAddress2, adContact, adMotto;
  // What the calendar page shows (env=dev fills an empty block with defaults)
  String showCompany, showAddress1, showAddress2, showContact;

  bool isDev() const { return env.equalsIgnoreCase("dev"); }
  bool hasAd() const {
    return showCompany.length() || showAddress1.length() || showAddress2.length() || showContact.length() ||
           adMotto.length();
  }

  void load();               // NVS values + defaults
  bool loadConfig();         // reads CONFIG_PATH; SD must be mounted
  bool saveConfig() const;   // writes CONFIG_PATH, keeping comments and key spellings
  void computeDisplay();     // fills the show* fields
  void save();               // writes calId to NVS
};

// "Password" -> "pass", "ad_company" -> "organization", unknown -> ""
String canonicalKey(String key);

extern AppSettings settings;
