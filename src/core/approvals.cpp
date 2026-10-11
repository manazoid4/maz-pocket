#include "approvals.h"

#include <algorithm>
#include <string>
#include <vector>

#include "../audio/sfx.h"
#include "../net/host_worker.h"
#include "../net/mazhost.h"
#include "../ui/theme.h"
#include "sys.h"

namespace maz {
namespace approvals {

using namespace theme;

namespace {

struct Req {
    std::string id, tool, summary, project, session, risk, hash;
    uint32_t    deadline = 0;  // millis() when Core's 60 s window closes
};

enum class Phase : uint8_t { Idle, Showing, Sending, Result };

std::vector<Req>         gQueue;
Phase                    gPhase = Phase::Idle;
std::string              gSendId, gSendDecision, gBeeped, gMsg;
std::vector<std::string> gDone;       // ids already answered/vanished; ignored if Core still lists them briefly
uint16_t                 gMsgColor = 0;
uint32_t                 gMsgUntil = 0, gNextPoll = 0;
uint8_t                  gRetries = 0, gFails = 0;
bool                     gSubmitted = false;
std::string              gShownId;     // card page/arm state belongs to this request
uint8_t                  gPage = 0;
uint32_t                 gArmUntil = 0;  // first Y/ENTER on the last page arms approve until this millis()
constexpr size_t         COLS = 37, LINES = 5;

bool done(const std::string& id) { return std::find(gDone.begin(), gDone.end(), id) != gDone.end(); }
void markDone(const std::string& id) {
    if (done(id)) return;
    gDone.push_back(id);
    if (gDone.size() > 12) gDone.erase(gDone.begin());
}
void drop(const std::string& id) {
    gQueue.erase(std::remove_if(gQueue.begin(), gQueue.end(), [&](const Req& r) { return r.id == id; }), gQueue.end());
}
void toast(const char* msg, uint16_t color, uint32_t ms = 1000) {
    gMsg = msg; gMsgColor = color; gMsgUntil = millis() + ms; gPhase = Phase::Result;
}

bool absorb(const host::BuddyPoll& p) {
    const uint32_t now = millis();
    Sys.buddy = p.ok ? (p.agent == "needs_you" ? 2 : p.agent == "working" ? 1 : 0) : 0;
    if (!p.ok) { if (++gFails >= 3) gQueue.clear(); return false; }
    gFails = 0;
    std::vector<Req> next;
    for (const host::BuddyItem& it : p.items) {
        if (done(it.id)) continue;
        Req r{it.id, it.tool, it.summary, it.project, it.session, it.risk, it.hash, now + static_cast<uint32_t>(it.left) * 1000u};
        for (const Req& old : gQueue) if (old.id == it.id) { r.deadline = old.deadline; break; }
        next.push_back(std::move(r));
    }
    // The request we are showing vanished from Core while we were deciding on it:
    // it was answered or cancelled at the terminal.
    if (gPhase == Phase::Showing && !gQueue.empty()) {
        const std::string shown = gQueue.front().id;
        const bool still = std::any_of(next.begin(), next.end(), [&](const Req& r) { return r.id == shown; });
        if (!still) { markDone(shown); toast("Handled at terminal", DIM); }
    }
    gQueue = std::move(next);
    return !gQueue.empty();
}

void send(const char* decision) {
    if (gQueue.empty() || gPhase != Phase::Showing) return;
    gSendId = gQueue.front().id;
    gSendDecision = decision;
    gRetries = 0;
    gSubmitted = false;
    gPhase = Phase::Sending;
}

std::string clip(const std::string& s, size_t from, size_t n) {
    return from >= s.size() ? std::string() : s.substr(from, n);
}
// The card pages the FULL request text, COLS chars per line, LINES lines per page.
int pageCount(const Req& r) { return std::max<int>(1, (r.summary.size() + COLS * LINES - 1) / (COLS * LINES)); }
std::string toolName(const Req& r) { return r.tool.compare(0, 5, "mcp__") == 0 ? r.tool.substr(5) : r.tool; }

}  // namespace

bool active() { return gPhase != Phase::Idle; }

bool update(bool screenOff) {
    bool arrived = false;
    const uint32_t now = millis();

    // 1. Collect worker results that belong to us.
    if (host_worker::state() == host_worker::State::Done) {
        if (host_worker::jobKind() == host_worker::JobKind::BuddyPoll) {
            host::BuddyPoll p;
            if (host_worker::takeBuddyPollResult(p)) absorb(p);
        } else if (host_worker::jobKind() == host_worker::JobKind::BuddyDecide) {
            host_worker::BuddyDecideResult r;
            if (host_worker::takeBuddyDecideResult(r)) {
                if (r.ok) {
                    markDone(r.id); drop(r.id);
                    if (r.applied == "allow") { toast("Approved", OK); sfx::confirm(); }
                    else if (r.applied == "deny") toast("Denied", ERR);
                    else if (r.applied == "gone") toast("Expired", DIM);
                    else toast("Handled at terminal", DIM);
                    gNextPoll = now + 400;  // pick up the next queued request quickly
                } else if (gRetries++ < 2) {
                    gSubmitted = false;  // retry on the next idle worker slot
                } else {
                    markDone(r.id); drop(r.id);
                    toast("hub unreachable - use terminal", WARN, 1800);
                }
            }
        }
    }

    // 2. Phase housekeeping.
    if (gPhase == Phase::Result && static_cast<int32_t>(now - gMsgUntil) >= 0) gPhase = Phase::Idle;
    if (gPhase == Phase::Showing || gPhase == Phase::Idle) {
        // Timed-out requests vanish on their own (the hook already fell back to the terminal).
        while (!gQueue.empty() && static_cast<int32_t>(now - gQueue.front().deadline) >= 0) {
            markDone(gQueue.front().id);
            gQueue.erase(gQueue.begin());
        }
        if (!gQueue.empty() && gPhase == Phase::Idle) {
            gPhase = Phase::Showing;
            if (gQueue.front().id != gBeeped) {
                gBeeped = gQueue.front().id;
                sfx::approval();
                arrived = true;
            }
        } else if (gQueue.empty() && gPhase == Phase::Showing) {
            gPhase = Phase::Idle;
        }
    }

    // 3. One worker job per tick at most: pending decision first, else the slow poll.
    if (host_worker::state() != host_worker::State::Idle) return arrived;
    if (!host::configured() || !Sys.wifiConnected) return arrived;
    if (gPhase == Phase::Sending && !gSubmitted) {
        gSubmitted = host_worker::submitBuddyDecide(gSendId, gSendDecision);
        return arrived;
    }
    if (static_cast<int32_t>(now - gNextPoll) >= 0) {
        const bool hot = gPhase == Phase::Showing || gPhase == Phase::Result;
        gNextPoll = now + (hot ? 2500u : screenOff ? 12000u : 5000u);
        host_worker::submitBuddyPoll();
    }
    return arrived;
}

bool handleKey(const KeyEvent& e) {
    if (gPhase == Phase::Idle) return false;
    if (!e.down) return true;
    if (gPhase != Phase::Showing) return true;  // swallow while sending / confirming
    const int last = pageCount(gQueue.front()) - 1;
    if (e.code == KEY_N || e.code == KEY_ESC) send("deny");
    else if (e.code == KEY_UP || e.code == KEY_SEMICOLON) { if (gPage > 0) --gPage; gArmUntil = 0; }
    else if (e.code == KEY_DOWN || e.code == KEY_DOT) { if (gPage < last) ++gPage; gArmUntil = 0; }
    else if (e.code == KEY_Y || e.code == KEY_ENTER) {
        // Approve needs the whole text read (last page) and a second press within 4 s.
        if (gPage < last) ++gPage;
        else if (gArmUntil && static_cast<int32_t>(millis() - gArmUntil) < 0) { gArmUntil = 0; send("allow"); }
        else gArmUntil = millis() + 4000;
    }
    // No session-wide approval shortcut: each action needs a deliberate decision.
    return true;
}

void render(M5Canvas& g) {
    if (gPhase == Phase::Idle) return;
    const int x = 4, y = 16, w = SCREEN_W - 8, h = 116;
    g.fillRoundRect(x, y, w, h, 4, PANEL);
    g.drawRoundRect(x, y, w, h, 4, ACCENT);
    g.setTextDatum(top_left);

    if (gPhase == Phase::Result) {
        g.setFont(&fonts::Font4);
        g.setTextDatum(middle_center);
        g.setTextColor(gMsgColor, PANEL);
        g.drawString(gMsg.c_str(), SCREEN_W / 2, y + h / 2);
        g.setTextDatum(top_left);
        return;
    }
    if (gQueue.empty()) return;
    const Req& r = gQueue.front();
    if (r.id != gShownId) { gShownId = r.id; gPage = 0; gArmUntil = 0; }
    const int32_t leftMs = static_cast<int32_t>(r.deadline - millis());
    const int left = leftMs > 0 ? (leftMs + 999) / 1000 : 0;
    const int pages = pageCount(r);
    const bool armed = gArmUntil && static_cast<int32_t>(millis() - gArmUntil) < 0;

    // Row 1: risk word + request hash (same hash Core computes over the full text).
    g.setFont(&fonts::Font0);
    g.setTextColor(r.risk == "READ" ? OK : WARN, PANEL);
    g.drawString((r.risk + " #" + r.hash).c_str(), x + 6, y + 4);
    char c[24] = "";
    if (gQueue.size() > 1) snprintf(c, sizeof(c), "+%u more", static_cast<unsigned>(gQueue.size() - 1));
    g.setTextDatum(top_right);
    g.setTextColor(ACCENT2, PANEL);
    g.drawString(c[0] ? c : clip(r.project, 0, 18).c_str(), x + w - 6, y + 4);
    g.setTextDatum(top_left);

    // Rows 2-3: full tool name (MCP names wrap, never cut).
    g.setTextColor(TEXT, PANEL);
    const std::string tool = toolName(r);
    g.drawString(clip(tool, 0, COLS).c_str(), x + 6, y + 15);
    g.drawString(clip(tool, COLS, COLS).c_str(), x + 6, y + 24);

    // Body: this page of the full text.
    g.setTextColor(DIM, PANEL);
    const size_t base = static_cast<size_t>(gPage) * COLS * LINES;
    for (size_t i = 0; i < LINES; ++i) g.drawString(clip(r.summary, base + i * COLS, COLS).c_str(), x + 6, y + 36 + 9 * i);

    // Countdown bar (60 s window) + page counter.
    const int bw = w - 70;
    g.drawRect(x + 6, y + 84, bw, 7, LINE);
    const int fill = std::min(bw - 2, (bw - 2) * left / 60);
    g.fillRect(x + 7, y + 85, fill, 5, left > 15 ? OK : left > 7 ? WARN : ERR);
    char t[24];
    snprintf(t, sizeof(t), "%ds  %d/%d", left, gPage + 1, pages);
    g.setTextColor(TEXT, PANEL);
    g.drawString(t, x + bw + 12, y + 84);

    const char* hint = gPhase == Phase::Sending ? "Sending..."
                     : armed                  ? "Y/ENT again = APPROVE"
                     : gPage < pages - 1      ? "Y/ENT read on  ;/. page  N deny"
                                              : "Y/ENT x2 approve  N/ESC deny";
    g.setTextColor(gPhase == Phase::Sending ? WARN : armed ? ERR : ACCENT, PANEL);
    g.drawString(hint, x + 6, y + 100);
}

}  // namespace approvals
}  // namespace maz
