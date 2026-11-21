#pragma once
#include <Arduino.h>

#include <functional>
#include <vector>

// Calendar catalog + SD card storage.
//
// Remote layout (GitHub content repo, relative to catalog.json):
//   catalog.json                       list of calendars (small: id/title/category/year)
//   calendars/<id>/calendar.json       per-calendar details (captions, crop)
//   calendars/<id>/01.jpg .. 12.jpg    240x320 baseline JPEGs
//
// SD card layout:
//   /calendar/catalog.json             cached copy of the catalog
//   /calendar/<id>/calendar.json
//   /calendar/<id>/01.jpg .. 12.jpg

struct CatalogEntry {
  String id, title, category, path;
  int year = 0;
  bool downloaded = false;
};

struct CalendarMeta {
  String id, title, category;
  int year = 0;
  String captions[12];
  int16_t cropY[12];  // first image row shown in the 240x170 calendar view
};

namespace Store {
using Progress = std::function<void(int done, int total)>;

bool mountSD();
bool sdOk();

// Reads the cached catalog from SD and merges in any calendars that exist on SD
// but are missing from the catalog (so downloaded calendars always work offline).
std::vector<CatalogEntry> loadCatalog();
// Fetches catalog.json from settings.catalogUrl into the SD cache.
bool refreshCatalog();

bool isDownloaded(const String &id);
// Downloads calendar.json and all 12 images, skipping images already on SD.
bool downloadCalendar(const CatalogEntry &e, Progress progress);
bool loadMeta(const String &id, CalendarMeta &meta);
String imagePath(const String &id, int month);  // month 1..12
bool removeCalendar(const String &id);
}  // namespace Store
