// Docked: a calm dark screen for a device on a desk. Large clock, battery with
// its voltage TREND (never "charging": this board cannot report it), link state,
// and one line when something needs you. The shell lowers CPU speed and screen
// light while this is the top screen (see shell.cpp); this file only draws.
#include <time.h>

#include <string>

#include "../core/approvals.h"
#include "../core/power.h"
#include "../core/settings.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
#include "../net/mazhost.h"
#include "../ui/ui.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

class DockApp : public App {
public:
    const char* id() const override { return "dock"; }
    const char* title() const override { return "Docked"; }
    const char* hints() const override {
        return host::updateReady() ? "ESC leave  U update  SPACE call" : "ESC leave   hold SPACE: call";
    }

    void onEnter() override {
        _spaceArmed = false;
        M5.Display.setBrightness(power::DOCK_LIGHT);
    }

    void onExit() override {
        Cfg.applyToHardware();
        power::cpuFull();  // a call, update or any other screen gets full speed
    }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) {
            if (e.code == KEY_SPACE && _spaceArmed) {  // quick tap also opens Call, like Home
                _spaceArmed = false;
                shell::pushById("talk");
                return true;
            }
            return false;
        }
        if (e.code == KEY_ESC) return false;  // shell pops on release
        if (e.code == KEY_SPACE) _spaceArmed = true;
        else if (e.code == KEY_U && host::updateReady()) shell::confirmFwUpdate();
        else if (e.code == KEY_N) shell::pushById("agents");
        return true;  // every other key only resets the dark timer
    }

    void update() override {
        if (_spaceArmed && KB.heldFor(KEY_SPACE) > 250) {
            _spaceArmed = false;
            shell::pushById("talk");
            return;
        }
        // The shell restores full light for an approval; once it is decided, go back to dock light.
        if (!approvals::active() && M5.Display.getBrightness() > power::DOCK_LIGHT)
            M5.Display.setBrightness(power::DOCK_LIGHT);
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);

        char clock[8] = "--:--";
        if (Sys.timeValid) {
            time_t    t = time(nullptr);
            struct tm tmv;
            localtime_r(&t, &tmv);
            snprintf(clock, sizeof(clock), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        }
        g.setTextDatum(top_center);
        g.setFont(&fonts::Font4);
        g.setTextSize(2.0f);
        g.setTextColor(Sys.timeValid ? TEXT : DIM, BG);
        g.drawString(clock, SCREEN_W / 2, 6);
        g.setTextSize(1.0f);

        // Battery, percent, trend word. The colour follows the trend, not a charge flag.
        const int      pct = Sys.batteryPct < 0 ? 0 : Sys.batteryPct;
        const uint16_t tc  = ui::trendColour(Sys.powerTrend);
        const uint16_t bc  = power::lowBattery() ? ERR : tc;
        g.drawRoundRect(14, 66, 40, 20, 3, bc);
        g.fillRect(54, 72, 3, 8, bc);
        if (pct > 0) g.fillRect(16, 68, (36 * pct) / 100, 16, bc);

        char b[8];
        snprintf(b, sizeof(b), "%d%%", Sys.batteryPct < 0 ? 0 : Sys.batteryPct);
        g.setTextDatum(top_left);
        g.setTextColor(TEXT, BG);
        g.drawString(Sys.batteryPct < 0 ? "?" : b, 66, 66);
        ui::trendArrow(g, 118, 71, 12, Sys.powerTrend, tc);
        g.setFont(&fonts::Font2);
        g.setTextColor(tc, BG);
        g.drawString(Sys.powerTrend ? power::trendName() : "reading...", 136, 68);

        // Small row: volts, Wi-Fi, Core.
        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, BG);
        char v[16];
        snprintf(v, sizeof(v), "%d.%02d V", Sys.batteryMv / 1000, (Sys.batteryMv % 1000) / 10);
        g.drawString(v, 14, 92);
        g.fillCircle(104, 95, 3, Sys.wifiConnected ? OK : (Sys.wifiOn ? WARN : DIM));
        g.drawString("Wi-Fi", 111, 92);
        g.fillCircle(160, 95, 3, Sys.hostOnline ? OK : DIM);
        g.drawString("Core", 167, 92);

        // One line: the most urgent thing, else calm.
        const char* line = "All quiet";
        uint16_t    lc   = DIM;
        if (power::lowBattery()) { line = "Charge me: plug in, switch ON"; lc = ERR; }
        else if (host::updateReady()) { line = "Update ready: press U"; lc = ACCENT; }
        else if (Sys.agentQuestion || Sys.agentsWaiting) { line = "Agent needs you: press N"; lc = WARN; }
        g.setFont(&fonts::Font2);
        if (g.textWidth(line) > SCREEN_W - PAD * 2) g.setFont(&fonts::Font0);
        g.setTextDatum(top_center);
        g.setTextColor(lc, BG);
        g.drawString(line, SCREEN_W / 2, 104);
        g.setTextDatum(top_left);
    }

    std::string contextSnapshot() const override { return "Docked: clock and battery screen"; }

private:
    bool _spaceArmed = false;
};

}  // namespace

App* makeDock() { return new DockApp(); }

}  // namespace apps
}  // namespace maz
