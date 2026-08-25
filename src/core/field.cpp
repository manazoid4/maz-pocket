#include "field.h"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>
#include <ctime>

#include "../apps/apps.h"
#include "../net/host_worker.h"
#include "../net/mazhost.h"
#include "../storage/store.h"
#include "launcher.h"
#include "notify.h"
#include "settings.h"
#include "shell.h"
#include "sys.h"

namespace maz {
namespace field {
namespace {

struct QuickOption { const char* id; const char* label; };
constexpr QuickOption QUICK[] = {
    {"talk", "TALK"}, {"braindump", "NOTE"}, {"laptop", "LAP"}, {"beam", "BEAM"},
    {"shift", "SHIFT"}, {"focus25", "FOCUS"}, {"lock", "LOCK"},
    {"play_pause", "PLAY"}, {"mute", "MUTE"}, {"recall", "RECALL"},
    {"flow", "FLOW"}, {"launcher", "LAUNCH"},
};
constexpr int QUICK_COUNT = sizeof(QUICK) / sizeof(QUICK[0]);
constexpr time_t VALID_WALL_CLOCK = 1704067200;

std::string gContext;
uint32_t gShiftStartedMs = 0;
uint32_t gLastCountsAt = 0;
uint32_t gNextOutboxTry = 0;
uint32_t gOutboxBackoffMs = 15000;
uint32_t gNextBeamPoll = 0;
bool gSystemRequested = false;
bool gWorkRequested = false;
bool gReminderDue = false;

std::string& quickSetting(int slot) {
    if (slot == 1) return Cfg.quick1;
    if (slot == 2) return Cfg.quick2;
    if (slot == 3) return Cfg.quick3;
    return Cfg.quick4;
}

int quickIndex(const std::string& id) {
    for (int i = 0; i < QUICK_COUNT; ++i)
        if (id == QUICK[i].id) return i;
    return -1;
}

bool dueNow(const store::Record& r) {
    if (r.status != "open" || !r.due) return false;
    if (r.dueIsUptime) return Sys.uptimeSeconds() >= r.due;
    const time_t now = time(nullptr);
    return now >= VALID_WALL_CLOCK && static_cast<uint32_t>(now) >= r.due;
}

void refreshCounts() {
    if (!store::ready()) {
        Sys.outboxQueued = 0;
        Sys.beamUnread = 0;
        gReminderDue = false;
        return;
    }
    uint8_t outbox = 0;
    for (const auto& r : store::loadRecords("outbox", 64))
        if (r.status == "queued" && outbox < 255) ++outbox;
    Sys.outboxQueued = outbox;

    uint8_t beams = 0;
    for (const auto& r : store::loadRecords("beam", 64))
        if (r.status == "open" && beams < 255) ++beams;
    Sys.beamUnread = beams;

    gReminderDue = false;
    for (const auto& r : store::loadRecords("reminder", 32)) {
        if (dueNow(r)) { gReminderDue = true; break; }
    }
}

store::Record* findRecord(std::vector<store::Record>& rows, const std::string& id) {
    for (auto& row : rows) if (row.id == id) return &row;
    return nullptr;
}

void saveReminderFrom(const host::Reply& reply) {
    if (reply.reminderTitle.empty() || !reply.reminderDelay) return;
    store::Record reminder;
    reminder.kind = "reminder";
    reminder.status = "open";
    reminder.title = reply.reminderTitle;
    reminder.source = "voice-outbox";
    apps::scheduleReminder(reminder, reply.reminderDelay);
    store::addRecord(reminder);
}

void finishOutboxAudio(const host_worker::OutboxAudioResult& result) {
    auto rows = store::loadRecords(nullptr, 512);
    store::Record* queued = findRecord(rows, result.recordId);
    if (!queued) return;
    if (!result.reply.ok) {
        gNextOutboxTry = millis() + gOutboxBackoffMs;
        gOutboxBackoffMs = std::min<uint32_t>(gOutboxBackoffMs * 2u, 300000u);
        return;
    }

    queued->status = "done";
    store::updateRecord(*queued);
    if (!result.wavPath.empty()) store::remove(result.wavPath);

    store::Record answer;
    answer.kind = "inbox";
    answer.status = "open";
    answer.title = result.context.empty() ? "COMM / delayed" : "CONTEXT ASK / delayed";
    answer.body = result.reply.text;
    answer.source = result.reply.provider.empty() ? "MAZ Core" : result.reply.provider;
    answer.ref = result.recordId;
    store::addRecord(answer);
    saveReminderFrom(result.reply);

    gOutboxBackoffMs = 15000;
    gNextOutboxTry = millis() + 500;
    notify::post(Note::Success, "Outbox sent", "answer saved in Recall");
    refreshCounts();
}

void finishOutboxBeam(const host_worker::OutboxBeamResult& result) {
    auto rows = store::loadRecords(nullptr, 512);
    store::Record* queued = findRecord(rows, result.recordId);
    if (!queued) return;
    if (!result.reply.ok) {
        gNextOutboxTry = millis() + gOutboxBackoffMs;
        gOutboxBackoffMs = std::min<uint32_t>(gOutboxBackoffMs * 2u, 300000u);
        return;
    }
    queued->status = "done";
    store::updateRecord(*queued);
    gOutboxBackoffMs = 15000;
    gNextOutboxTry = millis() + 500;
    notify::post(Note::Success, "Beam delivered", "copied/saved on laptop");
    refreshCounts();
}

void finishBeamPull(const host::BeamMessage& msg) {
    const uint32_t cadence = Cfg.fieldMode ? 45000u : 15000u;
    gNextBeamPoll = millis() + cadence;
    if (!msg.ok || !msg.hasMessage || msg.text.empty()) return;

    store::Record r;
    r.kind = "beam";
    r.status = "open";
    r.title = msg.kind == "link" ? "BEAM / LINK" : "BEAM / TEXT";
    r.body = msg.text.substr(0, 2000);
    r.source = "laptop";
    r.ref = msg.id;
    if (store::addRecord(r)) {
        notify::post(Note::Success, "Beam received", r.title);
        refreshCounts();
    }
}

void finishSystem(const host::SystemStatus& s) {
    gSystemRequested = false;
    Sys.laptopStatusOk = s.ok;
    Sys.laptopStatusAt = millis();
    if (!s.ok) return;
    Sys.laptopCpuPct = s.cpuPct;
    Sys.laptopRamPct = s.ramPct;
    Sys.laptopBatteryPct = s.batteryPct;
    Sys.laptopCharging = s.charging;
    Sys.laptopGpuAvailable = s.gpuAvailable;
    Sys.laptopGpuPct = s.gpuPct;
    Sys.laptopVramUsedMb = s.vramUsedMb;
    Sys.laptopVramTotalMb = s.vramTotalMb;
    Sys.laptopGpuTempC = s.gpuTempC;
    Sys.ollamaOnline = s.ollamaOnline;
    Sys.ollamaLoaded = s.ollamaLoaded;
    Sys.ollamaModel = s.ollamaModel;
    Sys.ollamaVramMb = s.ollamaVramMb;
    Sys.ollamaContext = s.ollamaContext;
    shell::invalidate();
}

void finishWork(const host::WorkSummary& s) {
    gWorkRequested = false;
    if (!s.ok) return;  // keep last-known values on a failed poll — never blank/zero
    Sys.workLoaded = true;
    Sys.workReceivedAt = millis();
    Sys.workTrackCount = static_cast<uint8_t>(std::min(s.trackCount, SysState::WORK_MAX_TRACKS));
    for (int i = 0; i < Sys.workTrackCount; ++i) {
        Sys.workTrackLabel[i] = s.tracks[i].shortLabel;
        Sys.workTrackToday[i] = s.tracks[i].todayTotal;
        Sys.workTrackHasTarget[i] = s.tracks[i].hasTarget;
        Sys.workTrackTarget[i] = s.tracks[i].target;
    }
    for (int d = 0; d < SysState::WORK_HISTORY_DAYS; ++d) Sys.workSeven[d] = s.sevenDay[d];
    shell::invalidate();
}

bool submitNextOutbox() {
    if (!store::ready()) return false;
    const auto rows = store::loadRecords("outbox", 64);
    // Oldest first: failed work preserves the order it was captured.
    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
        const auto& r = *it;
        if (r.status != "queued") continue;
        if (r.source == "beam")
            return host_worker::submitOutboxBeam(r.id, r.body);
        if (r.source == "talk" || r.source == "talk-context") {
            if (r.ref.empty() || !store::fs() || !store::fs()->exists(r.ref.c_str())) {
                store::Record broken = r;
                broken.status = "failed";
                broken.body = "recording missing";
                store::updateRecord(broken);
                refreshCounts();
                continue;
            }
            const std::string context = r.source == "talk-context" ? r.body : "";
            return host_worker::submitOutboxAudio(r.id, r.ref, context);
        }
    }
    return false;
}

std::string elapsed(uint32_t seconds) {
    char out[16];
    const uint32_t h = seconds / 3600;
    const uint32_t m = (seconds / 60) % 60;
    const uint32_t s = seconds % 60;
    snprintf(out, sizeof(out), "%02lu:%02lu:%02lu",
             static_cast<unsigned long>(h), static_cast<unsigned long>(m),
             static_cast<unsigned long>(s));
    return out;
}

}  // namespace

void begin() {
    Sys.fieldMode = Cfg.fieldMode;
    refreshCounts();
    gNextBeamPoll = millis() + 5000;
}

void update() {
    if (Sys.shiftRunning) Sys.shiftSeconds = (millis() - gShiftStartedMs) / 1000;

    if (host_worker::state() == host_worker::State::Done) {
        switch (host_worker::jobKind()) {
            case host_worker::JobKind::OutboxAudio: {
                host_worker::OutboxAudioResult r;
                if (host_worker::takeOutboxAudioResult(r)) finishOutboxAudio(r);
                break;
            }
            case host_worker::JobKind::OutboxBeam: {
                host_worker::OutboxBeamResult r;
                if (host_worker::takeOutboxBeamResult(r)) finishOutboxBeam(r);
                break;
            }
            case host_worker::JobKind::BeamPull: {
                host::BeamMessage r;
                if (host_worker::takeBeamPullResult(r)) finishBeamPull(r);
                break;
            }
            case host_worker::JobKind::SystemStatus: {
                host::SystemStatus r;
                if (host_worker::takeSystemStatusResult(r)) finishSystem(r);
                break;
            }
            case host_worker::JobKind::WorkSummary: {
                host::WorkSummary r;
                if (host_worker::takeWorkSummaryResult(r)) finishWork(r);
                break;
            }
            default: break;  // COMM/PC results belong to their caller.
        }
    }

    const uint32_t countCadence = Cfg.fieldMode ? 12000u : 5000u;
    if (millis() - gLastCountsAt >= countCadence) {
        gLastCountsAt = millis();
        refreshCounts();
    }

    if (host_worker::state() != host_worker::State::Idle ||
        !host::configured() || WiFi.status() != WL_CONNECTED)
        return;

    if (gSystemRequested) {
        if (host_worker::submitSystemStatus()) return;
    }

    if (gWorkRequested) {
        if (host_worker::submitWorkSummary()) return;
    }

    if (Sys.outboxQueued && static_cast<int32_t>(millis() - gNextOutboxTry) >= 0) {
        if (submitNextOutbox()) return;
    }

    if (static_cast<int32_t>(millis() - gNextBeamPoll) >= 0) {
        if (host_worker::submitBeamPull()) return;
        gNextBeamPoll = millis() + (Cfg.fieldMode ? 45000u : 15000u);
    }
}

void armContext(const std::string& appId, const std::string& snapshot) {
    std::string clean = snapshot;
    for (char& c : clean) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    if (clean.size() > 220) clean.resize(220);
    gContext = appId;
    if (!clean.empty()) gContext += ": " + clean;
}

const std::string& context() { return gContext; }
bool contextArmed() { return !gContext.empty(); }
void clearContext() { gContext.clear(); }

std::string nowText() {
    if (Sys.agentQuestion) return "AGENT NEEDS MAZ";
    if (Sys.outboxQueued) return "OUTBOX " + std::to_string(Sys.outboxQueued) + " WAITING";
    if (Sys.shiftRunning) return "SHIFT " + elapsed(Sys.shiftSeconds).substr(0, 5);
    if (gReminderDue) return "REMINDER DUE";
    if (Sys.beamUnread) return "BEAM " + std::to_string(Sys.beamUnread) + " NEW";
    if (Sys.agentsStale) return std::to_string(Sys.agentsStale) + " AGENTS STALE";
    if (Sys.agentsWaiting) return std::to_string(Sys.agentsWaiting) + " AGENTS WAIT";
    if (host::configured() && !Sys.hostOnline) return "CORE OFFLINE / QUEUE SAFE";
    if (Cfg.fieldMode) return "FIELD READY";
    return "READY";
}

const char* quickId(int slot) {
    std::string& id = quickSetting(std::max(1, std::min(slot, 4)));
    const int idx = quickIndex(id);
    return idx >= 0 ? QUICK[idx].id : QUICK[slot - 1].id;
}

const char* quickLabel(int slot) {
    const int idx = quickIndex(quickId(slot));
    return idx >= 0 ? QUICK[idx].label : "?";
}

bool runQuick(int slot) {
    const std::string id = quickId(slot);
    if (id == "talk" || id == "braindump" || id == "laptop" || id == "beam" ||
        id == "shift" || id == "recall" || id == "flow")
        return shell::pushById(id.c_str());
    if (id == "focus25") {
        shell::focus::start(25 * 60, "Quick focus");
        notify::post(Note::Success, "Focus started", "25 minutes");
        return true;
    }
    if (id == "launcher") { launcher::reboot(); return true; }
    if (id == "lock" || id == "play_pause" || id == "mute") {
        if (host_worker::submitPcAction(id, false)) {
            notify::post(Note::Info, "PC command queued", quickLabel(slot));
            return true;
        }
        notify::post(Note::Warn, "PC busy", "try again in a moment");
        return false;
    }
    return false;
}

void cycleQuick(int slot) {
    slot = std::max(1, std::min(slot, 4));
    std::string& current = quickSetting(slot);
    int idx = quickIndex(current);
    if (idx < 0) idx = slot - 1;
    current = QUICK[(idx + 1) % QUICK_COUNT].id;
    Cfg.save();
    notify::post(Note::Success, "Quick key changed", quickLabel(slot));
}

void toggleFieldMode() {
    Cfg.fieldMode = !Cfg.fieldMode;
    Sys.fieldMode = Cfg.fieldMode;
    Cfg.save();
    gNextBeamPoll = millis() + (Cfg.fieldMode ? 45000u : 5000u);
    notify::post(Note::Success, Cfg.fieldMode ? "FIELD mode ON" : "FIELD mode OFF",
                 Cfg.fieldMode ? "lower background + faster dim" : "normal cadence restored");
    shell::invalidate();
}

void toggleShift() {
    if (!Sys.shiftRunning) {
        gShiftStartedMs = millis();
        Sys.shiftSeconds = 0;
        Sys.shiftRunning = true;
        notify::post(Note::Success, "Shift started", "NOW will keep elapsed time");
        shell::invalidate();
        return;
    }
    Sys.shiftSeconds = (millis() - gShiftStartedMs) / 1000;
    Sys.shiftRunning = false;
    store::Record r;
    r.kind = "shift";
    r.status = "done";
    r.title = "Shift";
    r.body = "Elapsed " + elapsed(Sys.shiftSeconds);
    r.source = "field";
    store::addRecord(r);
    notify::post(Note::Success, "Shift saved", elapsed(Sys.shiftSeconds).c_str());
    shell::invalidate();
}

std::string shiftElapsedText() { return elapsed(Sys.shiftSeconds); }

void requestSystemStatus() {
    gSystemRequested = true;
    if (!host::configured()) {
        Sys.laptopStatusOk = false;
        Sys.laptopStatusAt = millis();
    }
}

void requestWorkSummary() { gWorkRequested = true; }

void queueBeam(const std::string& text) {
    if (text.empty()) return;
    store::Record r;
    r.kind = "outbox";
    r.status = "queued";
    r.title = "BEAM to laptop";
    r.body = text.substr(0, 2000);
    r.source = "beam";
    if (store::addRecord(r)) {
        gNextOutboxTry = millis();
        refreshCounts();
        notify::post(Note::Success, "Beam queued", "sends when Core is reachable");
    } else {
        notify::post(Note::Error, "Beam not saved", store::backendName());
    }
}

}  // namespace field
}  // namespace maz
