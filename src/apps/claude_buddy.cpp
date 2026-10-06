// CLAUDE BUDDY — Claude desktop Hardware Buddy on the Cardputer.
// Lives inside AGENTS; no new Home tile. Keyboard approvals: Y approve, N deny.
#include <cstdio>
#include <string>

#include "../audio/sfx.h"
#include "../buddy/buddy.h"
#include "../core/notify.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

class ClaudeBuddyApp final : public App {
public:
    const char* id() const override { return "buddy"; }
    const char* title() const override { return "Claude Buddy"; }

    const char* hints() const override {
        if (!buddy::enabled()) return "ENTER turn on   ESC back";
        if (buddy::hasPrompt()) return "Y approve   N deny   ESC back";
        return "ESC back";
    }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (!buddy::enabled() && e.code == KEY_ENTER) {
            buddy::enable();
            sfx::confirm();
            notify::post(Note::Info, "Hardware Buddy on", buddy::deviceName());
            invalidate();
            return true;
        }
        if (buddy::hasPrompt() && e.code == KEY_Y) {
            if (buddy::approve()) { sfx::confirm(); notify::post(Note::Success, "Approved"); }
            invalidate();
            return true;
        }
        if (buddy::hasPrompt() && e.code == KEY_N) {
            if (buddy::deny()) { sfx::select(); notify::post(Note::Info, "Denied"); }
            invalidate();
            return true;
        }
        return false;
    }

    void update() override {
        if (millis() - _lastPaint > 400) { _lastPaint = millis(); invalidate(); }
    }

    std::string contextSnapshot() const override {
        if (!buddy::linked()) return "Claude Buddy: not linked";
        const auto& s = buddy::state();
        return "Claude sessions: " + std::to_string(s.running) + " running, " +
               std::to_string(s.waiting) + " waiting";
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        g.setFont(&fonts::Font0);
        g.setTextDatum(top_left);

        if (!buddy::enabled()) {
            ui::header(g, "CLAUDE BUDDY", "OFF");
            ui::emptyState(g, "Approve Claude from your pocket", "ENTER enables Bluetooth");
            return;
        }

        if (const uint32_t pk = buddy::passkey()) {
            ui::header(g, "CLAUDE BUDDY", "PAIRING");
            char buf[8];
            snprintf(buf, sizeof(buf), "%06lu", static_cast<unsigned long>(pk));
            ui::bigValue(g, buf, "type this in Claude desktop", ACCENT2);
            return;
        }

        if (!buddy::linked()) {
            ui::header(g, "CLAUDE BUDDY", "WAITING");
            ui::emptyState(g, buddy::deviceName(), "Claude desktop > Developer > Hardware Buddy");
            return;
        }

        const auto& s = buddy::state();
        ui::header(g, "CLAUDE BUDDY", buddy::secure() ? "LINKED" : "LINKED!");

        if (buddy::hasPrompt()) {
            renderPrompt(g, s);
            return;
        }

        char top[48];
        snprintf(top, sizeof(top), "RUN %u  WAIT %u  ALL %u", s.running, s.waiting, s.total);
        g.setTextColor(s.running ? OK : DIM, BG);
        g.drawString(top, PAD, BODY_Y + 16);

        g.setTextColor(TEXT, BG);
        g.drawString(ui::ellipsis(s.msg.empty() ? "idle" : s.msg, 38).c_str(), PAD, BODY_Y + 30);

        int y = BODY_Y + 46;
        for (size_t i = 0; i < s.entries.size() && y < SCREEN_H - HINT_H - 10; ++i, y += 12) {
            g.setTextColor(i == 0 ? HINT : DIM, BG);
            g.drawString(ui::ellipsis(s.entries[i], 38).c_str(), PAD, y);
        }

        char tok[32];
        snprintf(tok, sizeof(tok), "%lu tok today", static_cast<unsigned long>(s.tokensToday));
        g.setTextDatum(top_right);
        g.setTextColor(DIM, BG);
        g.drawString(tok, SCREEN_W - PAD, BODY_Y + 16);
        g.setTextDatum(top_left);
    }

private:
    void renderPrompt(M5Canvas& g, const buddy::State& s) {
        ui::panel(g, PAD - 2, BODY_Y + 14, SCREEN_W - 2 * PAD + 4, BODY_H - 18);
        g.setTextColor(WARN, PANEL);
        g.drawString(("APPROVE  " + s.promptTool).c_str(), PAD + 2, BODY_Y + 19);

        // Hint wrapped across up to three lines — this is what Claude will run,
        // so show as much of it as fits rather than an ellipsis on line one.
        g.setTextColor(TEXT, PANEL);
        constexpr size_t COLS = 37;
        const std::string& h = s.promptHint;
        int y = BODY_Y + 34;
        for (size_t off = 0, n = 0; off < h.size() && n < 3; off += COLS, ++n, y += 12) {
            std::string chunk = h.substr(off, COLS);
            if (n == 2 && off + COLS < h.size()) chunk = ui::ellipsis(h.substr(off), COLS);
            g.drawString(chunk.c_str(), PAD + 2, y);
        }

        g.setTextColor(OK, PANEL);
        g.drawString("[Y] once", PAD + 2, BODY_Y + BODY_H - 16);
        g.setTextDatum(top_right);
        g.setTextColor(ERR, PANEL);
        g.drawString("[N] deny", SCREEN_W - PAD - 2, BODY_Y + BODY_H - 16);
        g.setTextDatum(top_left);
    }

    uint32_t _lastPaint = 0;
};

}  // namespace

App* makeClaudeBuddy() { return new ClaudeBuddyApp(); }

}  // namespace apps
}  // namespace maz
