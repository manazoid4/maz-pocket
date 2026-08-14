#include "store.h"

#include <LittleFS.h>
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <time.h>

#include <algorithm>
#include <limits>

#include "../core/settings.h"

namespace maz {
namespace store {

namespace {
// Cardputer ADV SD wiring. These differ from the published datasheet page
// (which lists CS on G5); both M5Unified's ADV pin table and M5Stack's own ADV
// UserDemo HAL use G12, so we follow the code, not the web page.
constexpr int PIN_SCLK = 40;
constexpr int PIN_MISO = 39;
constexpr int PIN_MOSI = 14;
constexpr int PIN_CS   = 12;

fs::FS*     active    = nullptr;
bool        sdMounted = false;
bool        fsMounted = false;
const char* backend   = "none";

const char* SUBDIRS[] = {"notes", "recordings", "captures", "tasks",
                         "snippets", "logs", "cache", "settings", "records",
                         "braindumps", "outbox"};

void ensureTree(fs::FS& f) {
    f.mkdir("/maz");
    for (const char* s : SUBDIRS) {
        const std::string p = std::string("/maz/") + s;
        if (!f.exists(p.c_str())) f.mkdir(p.c_str());
    }
    for (const char* path : {"/maz/tasks/tasks.tsv", "/maz/records/records.tsv",
                             "/maz/snippets/snippets.tsv"}) {
        File data = f.open(path, FILE_APPEND);
        if (data) data.close();
    }
}

bool mountSd() {
    // Cardputer ADV leaves several legacy keyboard pins floating. Launcher
    // proves the slot is reliable when they are driven high before SD starts.
    for (const int pin : {3, 4, 5, 6, 13, 15}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }
    // M5GFX owns SPI3/HSPI. Arduino's global SPI object is the separate FSPI
    // host, avoiding two SPIClass drivers competing for the same peripheral.
    SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, PIN_CS);
    delay(10);
    // Match M5Launcher's conservative default. Capture reliability matters
    // more here than peak card throughput.
    if (!SD.begin(PIN_CS, SPI, 4000000)) {
        SPI.end();
        return false;
    }
    if (SD.cardType() == CARD_NONE) {
        SD.end();
        SPI.end();
        return false;
    }
    ensureTree(SD);
    return true;
}
}  // namespace

bool begin() {
    sdMounted     = mountSd();
    Sys.sdPresent = sdMounted;

    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* dedicated = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "mazdata");
    const bool ownsPartitionTable =
        running && running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY;
    if (dedicated) {
        // M5Launcher provisions this partition specifically for MAZ Pocket.
        // Formatting it cannot affect Launcher or another installed app.
        fsMounted = LittleFS.begin(/*formatOnFail=*/true, "/littlefs", 10,
                                   "mazdata");
    } else {
        // Launcher-installed OTA apps must never format shared internal storage.
        fsMounted = LittleFS.begin(/*formatOnFail=*/ownsPartitionTable);
    }
    if (fsMounted) ensureTree(LittleFS);
    Sys.internalFs = fsMounted;

    if (sdMounted && (Cfg.preferSd || !fsMounted)) {
        active      = &SD;
        backend     = "SD";
        Sys.storage = Storage::SD;
    } else if (fsMounted) {
        active      = &LittleFS;
        backend     = "internal";
        Sys.storage = Storage::Internal;
    } else {
        // Launched from a launcher whose partition table has no LittleFS, and
        // no card inserted. Settings still work (NVS); data apps say so.
        active      = nullptr;
        backend     = "none";
        Sys.storage = Storage::None;
        ESP_LOGW("store", "no writable storage - running settings-only");
    }
    ESP_LOGI("store", "storage backend: %s", backend);
    return active != nullptr;
}

void remount() {
    if (sdMounted) {
        SD.end();
        SPI.end();
        sdMounted = false;
    }
    begin();
}

bool        ready() { return active != nullptr; }
fs::FS*     fs() { return active; }
const char* backendName() { return backend; }

uint64_t totalBytes() {
    if (active == &SD) return SD.totalBytes();
    if (active == &LittleFS) return LittleFS.totalBytes();
    return 0;
}
uint64_t freeBytes() {
    if (active == &SD) return SD.totalBytes() - SD.usedBytes();
    if (active == &LittleFS) return LittleFS.totalBytes() - LittleFS.usedBytes();
    return 0;
}

std::string dir(const char* sub) { return std::string("/maz/") + sub; }

std::string newPath(const char* sub, const char* ext) {
    char      buf[64];
    time_t    t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    // Timestamped names sort chronologically in any file browser, which
    // matters when these files are later pulled onto a laptop for OpenFlowKit.
    snprintf(buf, sizeof(buf), "/maz/%s/%04d%02d%02d-%02d%02d%02d.%s", sub,
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour,
             tmv.tm_min, tmv.tm_sec, ext);
    return buf;
}

std::vector<Entry> list(const char* sub, const char* ext, size_t limit) {
    std::vector<Entry> out;
    if (!active) return out;

    File d = active->open(dir(sub).c_str());
    if (!d || !d.isDirectory()) return out;

    const std::string want = ext ? ext : "";
    for (File f = d.openNextFile(); f && out.size() < limit;
         f      = d.openNextFile()) {
        if (f.isDirectory()) continue;
        std::string  name  = f.name();
        const size_t slash = name.find_last_of('/');
        if (slash != std::string::npos) name = name.substr(slash + 1);
        if (!want.empty() &&
            (name.size() < want.size() ||
             name.compare(name.size() - want.size(), want.size(), want) != 0))
            continue;

        Entry e;
        e.path    = dir(sub) + "/" + name;
        e.name    = name;
        e.size    = f.size();
        e.created = f.getLastWrite();
        e.title   = name;
        out.push_back(e);
    }
    d.close();

    // Newest first: the thing you just captured is the thing you want.
    std::sort(out.begin(), out.end(),
              [](const Entry& a, const Entry& b) { return a.name > b.name; });
    return out;
}

bool writeText(const std::string& path, const std::string& text) {
    if (!active) return false;
    File f = active->open(path.c_str(), FILE_WRITE);
    if (!f) {
        ESP_LOGE("store", "open for write failed: %s", path.c_str());
        return false;
    }
    const size_t n = f.print(text.c_str());
    f.close();
    return n == text.size();
}

bool appendText(const std::string& path, const std::string& text) {
    if (!active) return false;
    File f = active->open(path.c_str(), FILE_APPEND);
    if (!f) return false;
    f.print(text.c_str());
    f.close();
    return true;
}

std::string readText(const std::string& path, size_t maxBytes) {
    std::string out;
    if (!active || !active->exists(path.c_str())) return out;
    File f = active->open(path.c_str(), FILE_READ);
    if (!f) return out;
    const size_t n = f.size() > maxBytes ? maxBytes : f.size();
    out.resize(n);
    f.read(reinterpret_cast<uint8_t*>(&out[0]), n);
    f.close();
    return out;
}

bool remove(const std::string& path) {
    return active && active->remove(path.c_str());
}
bool rename(const std::string& from, const std::string& to) {
    return active && active->rename(from.c_str(), to.c_str());
}

// ------------------------------------------------------------------- tasks
std::vector<Task> loadTasks() {
    std::vector<Task> out;
    const std::string raw = readText("/maz/tasks/tasks.tsv", 8192);
    size_t            i   = 0;
    while (i < raw.size()) {
        size_t eol = raw.find('\n', i);
        if (eol == std::string::npos) eol = raw.size();
        const std::string line = raw.substr(i, eol - i);
        i                      = eol + 1;
        if (line.size() < 7) continue;

        const size_t a = line.find('\t');
        const size_t b = a == std::string::npos ? a : line.find('\t', a + 1);
        const size_t c = b == std::string::npos ? b : line.find('\t', b + 1);
        if (c == std::string::npos) continue;  // malformed line, skip quietly

        Task t;
        t.done    = line[0] == '1';
        t.created = strtoul(line.substr(a + 1, b - a - 1).c_str(), nullptr, 10);
        t.bucket  = static_cast<uint8_t>(line[b + 1] - '0');
        t.text    = line.substr(c + 1);
        if (!t.text.empty() && t.text.back() == '\r') t.text.pop_back();
        out.push_back(t);
    }
    return out;
}

bool saveTasks(const std::vector<Task>& tasks) {
    std::string out;
    for (const auto& t : tasks) {
        char head[40];
        snprintf(head, sizeof(head), "%d\t%u\t%u\t", t.done ? 1 : 0,
                 (unsigned)t.created, (unsigned)t.bucket);
        out += head;
        out += t.text;
        out += "\n";
    }
    return writeText("/maz/tasks/tasks.tsv", out);
}

// --------------------------------------------------------------- records
namespace {
std::string cleanField(std::string value) {
    for (char& c : value)
        if (c == '\t' || c == '\r' || c == '\n') c = ' ';
    return value;
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t tab = line.find('\t', start);
        fields.push_back(line.substr(start, tab == std::string::npos ? tab : tab - start));
        if (tab == std::string::npos) break;
        start = tab + 1;
    }
    return fields;
}
}  // namespace

std::vector<Record> loadRecords(const char* kind, size_t limit) {
    std::vector<Record> out;
    const std::string raw = readText("/maz/records/records.tsv",
                                     std::numeric_limits<size_t>::max());
    size_t start = 0;
    while (start < raw.size()) {
        size_t end = raw.find('\n', start);
        if (end == std::string::npos) end = raw.size();
        const auto f = splitTabs(raw.substr(start, end - start));
        start = end + 1;
        if ((f.size() != 9 && f.size() != 10) || (kind && f[1] != kind)) continue;
        Record r;
        r.id = f[0]; r.kind = f[1]; r.status = f[2];
        r.created = strtoul(f[3].c_str(), nullptr, 10);
        r.due = strtoul(f[4].c_str(), nullptr, 10);
        r.title = f[5]; r.body = f[6]; r.source = f[7]; r.ref = f[8];
        r.dueIsUptime = f.size() == 10 && f[9] == "1";
        out.push_back(r);
    }
    std::sort(out.begin(), out.end(), [](const Record& a, const Record& b) {
        return a.created > b.created;
    });
    if (out.size() > limit) out.resize(limit);
    return out;
}

bool saveRecords(const std::vector<Record>& records) {
    std::string out;
    for (const auto& r : records) {
        out += cleanField(r.id) + "\t" + cleanField(r.kind) + "\t" + cleanField(r.status) + "\t";
        out += std::to_string(r.created) + "\t" + std::to_string(r.due) + "\t";
        out += cleanField(r.title) + "\t" + cleanField(r.body) + "\t";
        out += cleanField(r.source) + "\t" + cleanField(r.ref) + "\t";
        out += r.dueIsUptime ? "1\n" : "0\n";
    }
    return writeText("/maz/records/records.tsv", out);
}

bool addRecord(Record& record) {
    if (record.created == 0) record.created = static_cast<uint32_t>(time(nullptr));
    if (record.id.empty()) record.id = std::to_string(record.created) + "-" + std::to_string(millis());
    auto records = loadRecords(nullptr, std::numeric_limits<size_t>::max());
    records.push_back(record);
    return saveRecords(records);
}

bool updateRecord(const Record& record) {
    auto records = loadRecords(nullptr, std::numeric_limits<size_t>::max());
    bool found = false;
    for (auto& existing : records) {
        if (existing.id == record.id) { existing = record; found = true; break; }
    }
    return found && saveRecords(records);
}

// ---------------------------------------------------------------- snippets
std::vector<std::pair<std::string, std::string>> loadSnippets() {
    std::vector<std::pair<std::string, std::string>> out;
    const std::string raw = readText("/maz/snippets/snippets.tsv", 4096);
    size_t            i   = 0;
    while (i < raw.size()) {
        size_t eol = raw.find('\n', i);
        if (eol == std::string::npos) eol = raw.size();
        const std::string line = raw.substr(i, eol - i);
        i                      = eol + 1;
        const size_t tab       = line.find('\t');
        if (tab == std::string::npos) continue;
        out.emplace_back(line.substr(0, tab), line.substr(tab + 1));
    }
    return out;
}

bool saveSnippets(const std::vector<std::pair<std::string, std::string>>& s) {
    std::string out;
    for (const auto& kv : s) out += kv.first + "\t" + kv.second + "\n";
    return writeText("/maz/snippets/snippets.tsv", out);
}

}  // namespace store
}  // namespace maz
