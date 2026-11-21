#include "store.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>

#include <algorithm>

#include "config.h"
#include "net.h"
#include "settings.h"

namespace {
SPIClass sdSpi(VSPI);
bool sdMounted = false;

const int16_t DEFAULT_CROP_Y = 0;

String calDir(const String &id) { return String(CAL_ROOT) + "/" + id; }

// Directory URL that catalog.json lives in, with trailing slash.
String baseUrl() {
  const String &u = settings.catalogUrl;
  int slash = u.lastIndexOf('/');
  return slash >= 0 ? u.substring(0, slash + 1) : u + "/";
}

bool validId(const String &id) {
  if (id.isEmpty() || id.length() > 40) return false;
  for (char c : id)
    if (!(isalnum((unsigned char)c) || c == '-' || c == '_')) return false;
  return true;
}

bool fileNonEmpty(const String &path) {
  File f = SD.open(path);
  if (!f) return false;
  bool ok = !f.isDirectory() && f.size() > 0;
  f.close();
  return ok;
}
}  // namespace

namespace Store {

bool mountSD() {
  sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdMounted = SD.begin(SD_CS, sdSpi, 20000000);
  if (sdMounted && !SD.exists(CAL_ROOT)) SD.mkdir(CAL_ROOT);
  return sdMounted;
}

bool sdOk() { return sdMounted; }

String imagePath(const String &id, int month) {
  char name[8];
  snprintf(name, sizeof(name), "/%02d.jpg", month);
  return calDir(id) + name;
}

bool isDownloaded(const String &id) {
  if (!fileNonEmpty(calDir(id) + "/calendar.json")) return false;
  for (int m = 1; m <= 12; m++)
    if (!fileNonEmpty(imagePath(id, m))) return false;
  return true;
}

std::vector<CatalogEntry> loadCatalog() {
  std::vector<CatalogEntry> list;

  File f = SD.open(CATALOG_PATH);
  if (f) {
    JsonDocument filter;
    JsonObject fe = filter["calendars"].add<JsonObject>();
    fe["id"] = true;
    fe["title"] = true;
    fe["category"] = true;
    fe["year"] = true;
    fe["path"] = true;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f, DeserializationOption::Filter(filter));
    f.close();
    if (err) Serial.printf("catalog.json: %s\n", err.c_str());
    for (JsonObject c : doc["calendars"].as<JsonArray>()) {
      CatalogEntry e;
      e.id = c["id"] | "";
      if (!validId(e.id)) continue;
      e.title = c["title"] | e.id.c_str();
      e.category = c["category"] | "Misc";
      e.year = c["year"] | 0;
      e.path = c["path"] | ("calendars/" + e.id + "/").c_str();
      if (!e.path.endsWith("/")) e.path += "/";
      list.push_back(e);
    }
  }

  // Add calendars that are on the card but not (or no longer) in the catalog.
  File root = SD.open(CAL_ROOT);
  if (root) {
    for (File d = root.openNextFile(); d; d = root.openNextFile()) {
      if (d.isDirectory()) {
        String id = d.name();
        int slash = id.lastIndexOf('/');
        if (slash >= 0) id = id.substring(slash + 1);
        bool known = std::any_of(list.begin(), list.end(), [&](const CatalogEntry &e) { return e.id == id; });
        CalendarMeta meta;
        if (!known && validId(id) && loadMeta(id, meta)) {
          CatalogEntry e;
          e.id = id;
          e.title = meta.title;
          e.category = meta.category;
          e.year = meta.year;
          e.path = "calendars/" + id + "/";
          list.push_back(e);
        }
      }
      d.close();
    }
    root.close();
  }

  for (auto &e : list) e.downloaded = isDownloaded(e.id);

  std::sort(list.begin(), list.end(), [](const CatalogEntry &a, const CatalogEntry &b) {
    if (a.category != b.category) return a.category < b.category;
    if (a.year != b.year) return a.year > b.year;  // newest first
    return a.title < b.title;
  });
  return list;
}

bool refreshCatalog() {
  if (!Net::online()) return false;
  return Net::download(settings.catalogUrl, CATALOG_PATH) > 0;
}

bool downloadCalendar(const CatalogEntry &e, Progress progress) {
  if (!validId(e.id)) return false;
  String dir = calDir(e.id);
  if (!SD.exists(dir)) SD.mkdir(dir);
  String remote = e.path.startsWith("http") ? e.path : baseUrl() + e.path;

  const int total = 13;
  int done = 0;
  if (progress) progress(done, total);

  if (Net::download(remote + "calendar.json", dir + "/calendar.json") <= 0) return false;
  if (progress) progress(++done, total);

  bool allOk = true;
  for (int m = 1; m <= 12; m++) {
    String path = imagePath(e.id, m);
    if (!fileNonEmpty(path)) {
      char name[8];
      snprintf(name, sizeof(name), "%02d.jpg", m);
      // one retry for flaky Wi-Fi
      if (Net::download(remote + name, path) <= 0 && Net::download(remote + name, path) <= 0) allOk = false;
    }
    if (progress) progress(++done, total);
  }
  return allOk;
}

bool loadMeta(const String &id, CalendarMeta &meta) {
  meta = CalendarMeta();
  meta.id = id;
  for (int i = 0; i < 12; i++) meta.cropY[i] = DEFAULT_CROP_Y;

  File f = SD.open(calDir(id) + "/calendar.json");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  meta.title = doc["title"] | id.c_str();
  meta.category = doc["category"] | "Misc";
  meta.year = doc["year"] | 0;

  JsonArray months = doc["months"].as<JsonArray>();
  int i = 0;
  for (JsonVariant mv : months) {
    if (i >= 12) break;
    if (mv.is<const char *>()) {
      meta.captions[i] = mv.as<const char *>();
    } else {
      meta.captions[i] = mv["caption"] | "";
      meta.cropY[i] = constrain((int)(mv["crop_y"] | DEFAULT_CROP_Y), 0, 320 - 170);
    }
    i++;
  }
  return true;
}

bool removeCalendar(const String &id) {
  if (!validId(id)) return false;
  String dir = calDir(id);
  for (int m = 1; m <= 12; m++) SD.remove(imagePath(id, m));
  SD.remove(dir + "/calendar.json");
  return SD.rmdir(dir);
}

}  // namespace Store
