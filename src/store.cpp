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
const char *CUSTOM = "custom";

String seriesDir(const String &id) { return String(CAL_ROOT) + "/" + id; }
String yearDir(const String &id, int year) { return seriesDir(id) + "/" + String(year); }

// Directory URL that catalog.json lives in, with trailing slash.
String baseUrl() {
  const String &u = settings.catalogUrl;
  int slash = u.lastIndexOf('/');
  return slash >= 0 ? u.substring(0, slash + 1) : u + "/";
}

bool validName(const String &id) {
  if (id.isEmpty() || id.length() > 40) return false;
  for (char c : id)
    if (!(isalnum((unsigned char)c) || c == '-' || c == '_')) return false;
  return true;
}

bool validSeriesId(const String &id) { return validName(id) && id != CUSTOM; }

// "bikinis" or "custom/<name>"
bool validKey(const String &key) {
  if (key.startsWith("custom/")) return validName(key.substring(7));
  return validSeriesId(key);
}

String baseName(const String &path) {
  int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

bool fileNonEmpty(const String &path) {
  if (!SD.exists(path)) return false;  // SD.open on a missing file logs an error
  File f = SD.open(path);
  if (!f) return false;
  bool ok = !f.isDirectory() && f.size() > 0;
  f.close();
  return ok;
}

// "beach-babes_2" -> "Beach Babes 2"
String prettify(const String &name) {
  String out;
  bool up = true;
  for (char c : name) {
    if (c == '-' || c == '_') {
      out += ' ';
      up = true;
    } else {
      out += up ? (char)toupper((unsigned char)c) : c;
      up = false;
    }
  }
  return out;
}

std::vector<String> listDirs(const String &path) {
  std::vector<String> names;
  if (!SD.exists(path)) return names;
  File root = SD.open(path);
  if (!root) return names;
  for (File d = root.openNextFile(); d; d = root.openNextFile()) {
    if (d.isDirectory()) names.push_back(baseName(d.name()));
    d.close();
  }
  root.close();
  return names;
}

std::vector<int> scanYears(const String &id) {
  std::vector<int> years;
  for (auto &n : listDirs(seriesDir(id))) {
    if (n.length() != 4) continue;
    int y = n.toInt();
    if (y >= 1900 && y <= 2999) years.push_back(y);
  }
  std::sort(years.begin(), years.end());
  return years;
}

bool readJson(const String &path, JsonDocument &doc) {
  if (!SD.exists(path)) return false;
  File f = SD.open(path);
  if (!f) return false;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) Serial.printf("%s: %s\n", path.c_str(), err.c_str());
  return !err;
}

String readVersion(const String &path) {
  JsonDocument doc;
  return readJson(path, doc) ? String(doc["version"] | "") : String();
}

bool loadMetaFile(const String &path, int year, CalendarMeta &meta) {
  meta = CalendarMeta();
  meta.year = year;
  for (int i = 0; i < 12; i++) {
    meta.cropY[i] = DEFAULT_CROP_Y;
    meta.framed[i] = false;
  }
  JsonDocument doc;
  if (!readJson(path, doc)) return false;
  meta.title = doc["title"] | "";
  meta.version = doc["version"] | "";
  bool allFramed = String(doc["fit"] | "") == "frame";  // older edition-wide setting
  int i = 0;
  for (JsonVariant mv : doc["months"].as<JsonArray>()) {
    if (i >= 12) break;
    if (mv.is<const char *>()) {
      meta.captions[i] = mv.as<const char *>();
    } else {
      meta.captions[i] = mv["caption"] | "";
      meta.sha[i] = mv["sha"] | "";
      meta.framed[i] = allFramed || String(mv["fit"] | "") == "frame";
      meta.cropY[i] = constrain((int)(mv["crop_y"] | DEFAULT_CROP_Y), 0, 320 - 170);
    }
    i++;
  }
  return true;
}

// Months an edition has a picture for. Editions with fingerprints list exactly the months
// that have one (partial years); older editions without fingerprints have all 12.
bool monthExpected(const CalendarMeta &meta, int m) {
  for (int i = 0; i < 12; i++)
    if (meta.sha[i].length()) return meta.sha[m - 1].length() > 0;
  return true;
}

bool yearDownloaded(const String &id, int year) {
  CalendarMeta meta;
  if (!loadMetaFile(yearDir(id, year) + "/calendar.json", year, meta)) return false;
  for (int m = 1; m <= 12; m++)
    if (monthExpected(meta, m) && !fileNonEmpty(Store::imagePath(id, year, m))) return false;
  return true;
}

bool downloadTwice(const String &url, const String &path) {  // one retry for flaky Wi-Fi
  return Net::download(url, path) > 0 || Net::download(url, path) > 0;
}

void removeYearFiles(const String &dir) {
  for (int m = 1; m <= 12; m++) {
    char name[8];
    snprintf(name, sizeof(name), "/%02d.jpg", m);
    if (SD.exists(dir + name)) SD.remove(dir + name);
  }
  for (const char *f : {"/calendar.json", "/calendar.new"})
    if (SD.exists(dir + f)) SD.remove(dir + f);
}

bool downloadYear(const String &remoteSeries, const String &id, int year, Store::Progress progress) {
  String dir = yearDir(id, year);
  if (!SD.exists(dir)) SD.mkdir(dir);
  String remote = remoteSeries + String(year) + "/";
  String label = String(year) + " edition";
  String metaPath = dir + "/calendar.json";
  String newMetaPath = dir + "/calendar.new";

  // Work out which images need fetching before downloading any.
  CalendarMeta oldMeta, newMeta;
  bool haveOld = loadMetaFile(metaPath, year, oldMeta);
  if (Net::download(remote + "calendar.json", newMetaPath) <= 0) return false;
  if (!loadMetaFile(newMetaPath, year, newMeta)) return false;

  bool need[12];
  int total = 1;
  for (int m = 1; m <= 12; m++) {
    const String &now = newMeta.sha[m - 1];
    bool changed = haveOld && (now.isEmpty() ? oldMeta.version != newMeta.version : oldMeta.sha[m - 1] != now);
    need[m - 1] = monthExpected(newMeta, m) && (!fileNonEmpty(Store::imagePath(id, year, m)) || changed);
    if (!monthExpected(newMeta, m) && SD.exists(Store::imagePath(id, year, m)))
      SD.remove(Store::imagePath(id, year, m));  // month dropped from the edition
    total += need[m - 1];
  }
  int done = 1;
  if (progress) progress(label, done, total);

  bool allOk = true;
  for (int m = 1; m <= 12; m++) {
    if (!need[m - 1]) continue;
    char name[8];
    snprintf(name, sizeof(name), "%02d.jpg", m);
    if (!downloadTwice(remote + name, Store::imagePath(id, year, m))) allOk = false;
    if (progress) progress(label, ++done, total);
  }
  if (!allOk) return false;  // keep the old calendar.json so changed images are retried
  if (SD.exists(metaPath)) SD.remove(metaPath);
  return SD.rename(newMetaPath, metaPath);
}
}  // namespace

// ---------------------------------------------------------------- SeriesEntry

bool SeriesEntry::downloaded() const {
  if (custom) return true;
  if (localVersion.isEmpty()) return false;
  for (auto &y : years)
    if (!y.downloaded) return false;
  return true;
}

bool SeriesEntry::updateAvailable() const {
  if (custom || localVersion.isEmpty()) return false;
  if (version.length() && version != localVersion) return true;
  for (auto &y : years)
    if (!y.downloaded || (y.version.length() && y.version != y.localVersion)) return true;
  return false;
}

String SeriesEntry::yearsLabel() const {
  if (years.empty()) return "";
  int lo = years.front().year, hi = years.back().year;
  return lo == hi ? String(lo) : String(lo) + "-" + String(hi);
}

// ---------------------------------------------------------------- Store

namespace Store {

bool mountSD() {
  sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdMounted = SD.begin(SD_CS, sdSpi, 20000000);
  if (sdMounted && !SD.exists(CAL_ROOT)) SD.mkdir(CAL_ROOT);
  return sdMounted;
}

bool sdOk() { return sdMounted; }

String imagePath(const String &id, int year, int month) {
  char name[8];
  snprintf(name, sizeof(name), "/%02d.jpg", month);
  return yearDir(id, year) + name;
}

String placeholderPath(const String &id) { return seriesDir(id) + "/placeholder.jpg"; }

void cleanupLegacy() {
  for (auto &name : listDirs(CAL_ROOT)) {
    if (name == CUSTOM || !validName(name)) continue;
    String dir = seriesDir(name);
    if (!SD.exists(dir + "/calendar.json")) continue;  // new-style series keep it in year folders
    Serial.printf("Removing old-layout calendar %s\n", dir.c_str());
    removeYearFiles(dir);
    SD.rmdir(dir);
  }
}

bool loadSeries(const String &id, SeriesInfo &out) {
  out = SeriesInfo();
  if (!validKey(id) || !SD.exists(seriesDir(id))) return false;
  out.id = id;
  out.custom = id.startsWith("custom/");
  JsonDocument doc;
  bool haveJson = readJson(seriesDir(id) + "/series.json", doc);
  if (!haveJson && !out.custom) return false;
  out.title = doc["title"] | "";
  out.category = doc["category"] | "";
  if (out.title.isEmpty()) out.title = prettify(baseName(id));
  if (out.category.isEmpty()) out.category = out.custom ? "Custom" : "Misc";
  out.hasPlaceholder = fileNonEmpty(placeholderPath(id));
  out.years = scanYears(id);
  return true;
}

bool loadYear(const String &id, int year, CalendarMeta &out) {
  String dir = yearDir(id, year);
  if (!SD.exists(dir)) return false;
  if (!loadMetaFile(dir + "/calendar.json", year, out)) {
    // Custom calendars may skip calendar.json; any images in the folder will do.
    if (!id.startsWith("custom/")) return false;
    for (int m = 1; m <= 12; m++)
      if (fileNonEmpty(imagePath(id, year, m))) return true;
    return false;
  }
  return true;
}

std::vector<SeriesEntry> loadCatalog() {
  std::vector<SeriesEntry> custom, list;

  // 1. Custom calendars on the SD card
  for (auto &name : listDirs(String(CAL_ROOT) + "/" + CUSTOM)) {
    SeriesInfo info;
    if (!validName(name) || !loadSeries(String("custom/") + name, info)) continue;
    SeriesEntry e;
    e.id = info.id;
    e.title = info.title;
    e.category = info.category;
    e.custom = true;
    for (int y : info.years) {
      YearEntry ye;
      ye.year = y;
      ye.downloaded = true;
      e.years.push_back(ye);
    }
    custom.push_back(e);
  }

  // 2. Cached catalog
  JsonDocument filter;
  JsonObject fe = filter["series"].add<JsonObject>();
  for (const char *k : {"id", "title", "category", "version", "path", "years"}) fe[k] = true;
  JsonDocument doc;
  if (SD.exists(CATALOG_PATH)) {
    File f = SD.open(CATALOG_PATH);
    if (f) {
      DeserializationError err = deserializeJson(doc, f, DeserializationOption::Filter(filter));
      f.close();
      if (err) Serial.printf("catalog.json: %s\n", err.c_str());
    }
  }
  for (JsonObject s : doc["series"].as<JsonArray>()) {
    SeriesEntry e;
    e.id = s["id"] | "";
    if (!validSeriesId(e.id)) continue;
    e.title = s["title"] | e.id.c_str();
    e.category = s["category"] | "Misc";
    e.version = s["version"] | "";
    e.path = s["path"] | "";
    if (e.path.isEmpty()) e.path = "calendars/" + e.id + "/";
    if (!e.path.endsWith("/")) e.path += "/";
    e.localVersion = readVersion(seriesDir(e.id) + "/series.json");
    for (JsonPair kv : s["years"].as<JsonObject>()) {
      YearEntry ye;
      ye.year = atoi(kv.key().c_str());
      if (ye.year < 1900 || ye.year > 2999) continue;
      ye.version = kv.value() | "";
      ye.localVersion = readVersion(yearDir(e.id, ye.year) + "/calendar.json");
      ye.downloaded = yearDownloaded(e.id, ye.year);
      e.years.push_back(ye);
    }
    std::sort(e.years.begin(), e.years.end(), [](const YearEntry &a, const YearEntry &b) { return a.year < b.year; });
    list.push_back(e);
  }

  // 3. Series on the card that the catalog no longer lists
  for (auto &name : listDirs(CAL_ROOT)) {
    if (!validSeriesId(name)) continue;
    if (std::any_of(list.begin(), list.end(), [&](const SeriesEntry &e) { return e.id == name; })) continue;
    SeriesInfo info;
    if (!loadSeries(name, info) || info.years.empty()) continue;
    SeriesEntry e;
    e.id = name;
    e.title = info.title;
    e.category = info.category;
    e.localVersion = e.version = readVersion(seriesDir(name) + "/series.json");
    if (e.localVersion.isEmpty()) e.localVersion = e.version = "local";
    for (int y : info.years) {
      YearEntry ye;
      ye.year = y;
      ye.downloaded = yearDownloaded(name, y);
      ye.version = ye.localVersion = readVersion(yearDir(name, y) + "/calendar.json");
      if (ye.downloaded) e.years.push_back(ye);
    }
    if (!e.years.empty()) list.push_back(e);
  }

  std::sort(custom.begin(), custom.end(), [](const SeriesEntry &a, const SeriesEntry &b) { return a.title < b.title; });
  std::sort(list.begin(), list.end(), [](const SeriesEntry &a, const SeriesEntry &b) {
    if (a.category != b.category) return a.category < b.category;
    return a.title < b.title;
  });
  custom.insert(custom.end(), list.begin(), list.end());  // custom calendars first
  return custom;
}

bool refreshCatalog() {
  if (!Net::online()) return false;
  return Net::download(settings.catalogUrl, CATALOG_PATH) > 0;
}

bool findEntry(const String &id, SeriesEntry &out) {
  for (auto &e : loadCatalog())
    if (e.id == id) {
      out = e;
      return true;
    }
  return false;
}

bool downloadSeries(const SeriesEntry &e, Progress progress) {
  if (e.custom || !validSeriesId(e.id)) return false;
  String dir = seriesDir(e.id);
  if (!SD.exists(dir)) SD.mkdir(dir);
  String remote = e.path.startsWith("http") ? e.path : baseUrl() + e.path;

  // series.json + placeholder
  String sPath = dir + "/series.json", sNew = dir + "/series.new";
  if (e.version.isEmpty() || e.version != e.localVersion || !fileNonEmpty(sPath)) {
    if (progress) progress("Series info", 0, 1);
    JsonDocument oldDoc, newDoc;
    readJson(sPath, oldDoc);
    if (Net::download(remote + "series.json", sNew) <= 0 || !readJson(sNew, newDoc)) return false;
    String oldSha = oldDoc["placeholder_sha"] | "";
    String newSha = newDoc["placeholder_sha"] | "";
    String ph = placeholderPath(e.id);
    if (newSha.length()) {
      if ((newSha != oldSha || !fileNonEmpty(ph)) && !downloadTwice(remote + "placeholder.jpg", ph)) return false;
    } else if (SD.exists(ph)) {
      SD.remove(ph);
    }
    if (SD.exists(sPath)) SD.remove(sPath);
    if (!SD.rename(sNew, sPath)) return false;
    if (progress) progress("Series info", 1, 1);
  }

  bool ok = true;
  for (auto &y : e.years)
    if (!y.downloaded || y.version != y.localVersion) ok &= downloadYear(remote, e.id, y.year, progress);
  return ok;
}

}  // namespace Store
