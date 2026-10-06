#include "buddy.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include <cstring>

#include "../audio/sfx.h"
#include "../core/notify.h"
#include "../core/sys.h"
#include "ble_link.h"

namespace maz {
namespace buddy {

namespace {

constexpr size_t   LINE_CAP     = 1536;
constexpr uint32_t STALE_MS     = 30000;
constexpr size_t   MAX_ENTRIES  = 6;
constexpr size_t   MAX_FIELD    = 96;   // per-string cap; desktop sends short lines

bool     on = false;
State    st;
char     line[LINE_CAP];
size_t   lineLen  = 0;
bool     overflow = false;
uint32_t lastLive = 0;

std::string clip(const char* s) {
    if (!s) return {};
    std::string out(s);
    if (out.size() > MAX_FIELD) out.resize(MAX_FIELD);
    return out;
}

void send(JsonDocument& doc) {
    std::string out;
    serializeJson(doc, out);
    out.push_back('\n');
    ble_link::write(reinterpret_cast<const uint8_t*>(out.data()), out.size());
}

void ack(const char* cmd, bool ok, const char* error = nullptr) {
    JsonDocument d;
    d["ack"] = cmd;
    d["ok"]  = ok;
    d["n"]   = 0;
    if (error) d["error"] = error;
    send(d);
}

void sendStatus() {
    JsonDocument d;
    d["ack"] = "status";
    d["ok"]  = true;
    JsonObject data = d["data"].to<JsonObject>();
    data["name"] = ble_link::name();
    data["sec"]  = ble_link::secure();
    if (Sys.batteryPct >= 0) data["bat"]["pct"] = Sys.batteryPct;
    data["sys"]["up"]   = millis() / 1000;
    data["sys"]["heap"] = ESP.getFreeHeap();
    data["stats"]["appr"] = st.approved;
    data["stats"]["deny"] = st.denied;
    send(d);
}

void handleCommand(const char* cmd, JsonDocument& doc) {
    if (!strcmp(cmd, "status")) { sendStatus(); return; }
    if (!strcmp(cmd, "owner"))  { st.owner = clip(doc["name"] | ""); ack(cmd, true); return; }
    if (!strcmp(cmd, "name"))   { ack(cmd, true); return; }  // name is MAC-derived; accept quietly
    if (!strcmp(cmd, "unpair")) { ble_link::clearBonds(); ack(cmd, true); return; }
    // Folder push (char_begin/file/chunk/...) is deliberately unsupported:
    // not acking char_begin makes the desktop time out cleanly, and Pocket
    // never writes desktop-supplied paths to storage.
    if (!strcmp(cmd, "char_begin")) return;
    ack(cmd, false, "unsupported");
}

void applySnapshot(JsonDocument& doc) {
    st.total       = doc["total"]        | st.total;
    st.running     = doc["running"]      | st.running;
    st.waiting     = doc["waiting"]      | st.waiting;
    st.tokensToday = doc["tokens_today"] | st.tokensToday;
    if (const char* m = doc["msg"]) st.msg = clip(m);

    JsonArray es = doc["entries"];
    if (!es.isNull()) {
        st.entries.clear();
        for (JsonVariant v : es) {
            if (st.entries.size() >= MAX_ENTRIES) break;
            st.entries.push_back(clip(v.as<const char*>()));
        }
    }

    JsonObject pr = doc["prompt"];
    if (pr.isNull()) {
        st.promptId.clear(); st.promptTool.clear(); st.promptHint.clear();
        return;
    }
    std::string id = clip(pr["id"] | "");
    const bool fresh = !id.empty() && id != st.promptId;
    st.promptId   = id;
    st.promptTool = clip(pr["tool"] | "");
    st.promptHint = clip(pr["hint"] | "");
    if (fresh) {
        sfx::forNote(Note::Warn);
        notify::post(Note::Warn, "Claude: approve " + st.promptTool + "?",
                     "AGENTS > CLAUDE BUDDY  Y/N");
    }
}

void applyLine(const char* text) {
    JsonDocument doc;
    if (deserializeJson(doc, text)) return;
    lastLive = millis();

    if (const char* cmd = doc["cmd"]) { handleCommand(cmd, doc); return; }
    if (!doc["time"].isNull()) return;     // shell clock is owned by Wi-Fi/NTP
    if (!doc["evt"].isNull()) return;      // turn events: not surfaced yet
    applySnapshot(doc);
}

bool decide(const char* decision) {
    if (st.promptId.empty() || !ble_link::connected()) return false;
    JsonDocument d;
    d["cmd"]      = "permission";
    d["id"]       = st.promptId;   // echoed exactly; serialised, never string-spliced
    d["decision"] = decision;
    send(d);
    st.promptId.clear(); st.promptTool.clear(); st.promptHint.clear();
    return true;
}

}  // namespace

void boot() {
    Preferences p;
    if (p.begin("buddy", true)) {
        on = p.getBool("on", false);
        p.end();
    }
    if (on) ble_link::begin();
}

void enable() {
    if (on) return;
    on = true;
    Preferences p;
    if (p.begin("buddy", false)) { p.putBool("on", true); p.end(); }
    ble_link::begin();
}

bool enabled() { return on; }

void update() {
    if (!on) return;
    // Bound work per loop so a burst of BLE data cannot stall the UI.
    for (int budget = 512; budget > 0 && ble_link::available(); --budget) {
        const int c = ble_link::read();
        if (c < 0) break;
        if (c == '\n' || c == '\r') {
            if (lineLen && !overflow && line[0] == '{') { line[lineLen] = 0; applyLine(line); }
            lineLen = 0; overflow = false;
        } else if (lineLen < LINE_CAP - 1) {
            line[lineLen++] = static_cast<char>(c);
        } else {
            overflow = true;  // drop the oversized line, resync on next newline
        }
    }
    if (!linked() && !st.promptId.empty()) {
        st.promptId.clear(); st.promptTool.clear(); st.promptHint.clear();
    }
}

bool linked() {
    return on && ble_link::connected() && lastLive && millis() - lastLive <= STALE_MS;
}
bool        secure() { return ble_link::secure(); }
bool        hasPrompt() { return linked() && !st.promptId.empty(); }
uint32_t    passkey() { return ble_link::passkey(); }
const char* deviceName() { return ble_link::name(); }
const State& state() { return st; }

bool approve() { if (!decide("once")) return false; st.approved++; return true; }
bool deny()    { if (!decide("deny")) return false; st.denied++;   return true; }

}  // namespace buddy
}  // namespace maz
