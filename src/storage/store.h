// MAZ Pocket — storage abstraction.
//
// Two facts drive this design:
//   1. MAZ Pocket must stay useful with no SD card in the slot.
//   2. When M5Launcher flashes us it uses ITS partition table, so our LittleFS
//      partition may simply not exist. Neither case is allowed to be fatal.
// So: pick the best available backend at boot, tell the user which one won,
// and degrade to "settings only" rather than crashing.
//
// Layout (identical on SD and internal flash):
//   /maz/notes/       *.txt      note bodies, first line is the title
//   /maz/recordings/  *.wav      16-bit mono PCM
//   /maz/captures/    *.txt|wav  quick captures
//   /maz/tasks/       tasks.tsv
//   /maz/snippets/    snippets.tsv
//   /maz/logs/
//   /maz/cache/       reserved: conversation cache, offline voice queue
#pragma once
#include <FS.h>

#include <string>
#include <utility>
#include <vector>

#include "../core/sys.h"

namespace maz {
namespace store {

struct Entry {
    std::string path;
    std::string name;   // filename without directory
    std::string title;  // first line for notes, filename otherwise
    uint32_t    created = 0;
    size_t      size    = 0;
};

struct Task {
    bool        done    = false;
    uint32_t    created = 0;
    uint8_t     bucket  = 0;  // 0 = Today, 1 = Later
    std::string text;
};

// Mounts SD (preferred when Cfg.preferSd) then internal LittleFS, creates the
// /maz tree, and sets Sys.storage / Sys.sdPresent.
bool begin();
void remount();  // after an SD is inserted or removed

bool        ready();
fs::FS*     fs();
const char* backendName();
uint64_t    freeBytes();
uint64_t    totalBytes();

std::string dir(const char* sub);  // "/maz/notes"

// A unique, sortable path: /maz/notes/20260813-140512.txt
std::string newPath(const char* sub, const char* ext);

std::vector<Entry> list(const char* sub, const char* ext, size_t limit = 64);
bool               writeText(const std::string& path, const std::string& text);
bool               appendText(const std::string& path, const std::string& text);
std::string        readText(const std::string& path, size_t maxBytes = 8192);
bool               remove(const std::string& path);
bool               rename(const std::string& from, const std::string& to);

// Tasks live in one small TSV rather than a file each: they are edited far
// more often than notes, and rewriting one line-oriented file is cheaper and
// far less likely to leave orphans on a yanked SD card.
//   done<TAB>created_epoch<TAB>bucket<TAB>text
std::vector<Task> loadTasks();
bool              saveTasks(const std::vector<Task>& tasks);

// Snippets: name<TAB>value
std::vector<std::pair<std::string, std::string>> loadSnippets();
bool saveSnippets(const std::vector<std::pair<std::string, std::string>>& s);

}  // namespace store
}  // namespace maz
