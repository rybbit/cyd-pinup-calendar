#pragma once
#include <Arduino.h>

#include <functional>
#include <vector>

// Calendars are organized as SERIES (e.g. "bikinis") with one EDITION per year.
//
// Remote layout (GitHub, relative to catalog.json):
//   catalog.json                                  series list with per-year versions
//   calendars/<series>/series.json                title, category, placeholder fingerprint
//   calendars/<series>/placeholder.jpg            optional, shown for years with no edition
//   calendars/<series>/<year>/calendar.json       edition title, captions, crops, fingerprints
//   calendars/<series>/<year>/01.jpg .. 12.jpg    240x320 baseline JPEGs
//
// SD card layout mirrors it under /calendar/<series>/. Custom calendars the user
// copies onto the card live in /calendar/custom/<name>/ with the same structure
// (series.json and calendar.json optional). They are listed first and never touched
// by downloads or updates.

struct YearEntry {
  int year = 0;
  String version;       // from catalog.json
  String localVersion;  // from the SD copy of calendar.json
  bool downloaded = false;
};

struct SeriesEntry {
  String id;  // "bikinis", or "custom/<name>" for SD-only calendars
  String title, category, path;
  String version, localVersion;  // series.json version (catalog vs SD)
  bool custom = false;
  std::vector<YearEntry> years;  // sorted ascending

  bool downloaded() const;       // series.json and every listed year are on the SD card
  bool updateAvailable() const;  // on the SD card, but something changed on GitHub
  String yearsLabel() const;     // "2027-2028", "2027", ...
};

// What the calendar page needs about the selected series (from the SD card).
struct SeriesInfo {
  String id, title, category;
  bool custom = false;
  bool hasPlaceholder = false;
  std::vector<int> years;  // editions on the SD card
};

// One year's edition.
struct CalendarMeta {
  int year = 0;
  String title;
  String version;
  String sha[12];     // per-image fingerprint; a changed sha means re-download that month
  String topSha[12];  // same for the optional calendar-page header MM_top.jpg
  String captions[12];
  int16_t cropY[12];   // first image row shown in the 240x170 calendar view
  bool framed[12];  // month built with --fit frame (photo centered on the canvas)
};

namespace Store {
// Progress callback: label (e.g. "2028 edition"), files done, files total for that label.
using Progress = std::function<void(const String &label, int done, int total)>;

bool mountSD();
bool sdOk();
// Removes calendars saved in the old one-folder-per-year layout (/calendar/<id>/calendar.json).
void cleanupLegacy();

// Custom SD calendars first, then the cached catalog, then series that are on the
// card but no longer in the catalog (so downloaded calendars always work offline).
std::vector<SeriesEntry> loadCatalog();
bool refreshCatalog();
bool findEntry(const String &id, SeriesEntry &out);

// Downloads series.json, the placeholder and every edition, fetching only what is
// missing or whose fingerprint changed. An edition's calendar.json is replaced only
// after all its images succeed, so an interrupted update is retried next time.
bool downloadSeries(const SeriesEntry &e, Progress progress);

bool loadSeries(const String &id, SeriesInfo &out);
bool loadYear(const String &id, int year, CalendarMeta &out);
String imagePath(const String &id, int year, int month);
String topImagePath(const String &id, int year, int month);  // optional MM_top.jpg
String placeholderPath(const String &id);
}  // namespace Store
