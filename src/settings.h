#pragma once
#include <Arduino.h>

// Persistent settings in NVS (survive reflashing unless flash is erased).
struct AppSettings {
  String calId;       // currently selected calendar id
  String catalogUrl;  // raw URL of catalog.json
  String tz;          // POSIX TZ string, e.g. "CST6CDT,M3.2.0,M11.1.0"
  void load();
  void save();
};

extern AppSettings settings;
