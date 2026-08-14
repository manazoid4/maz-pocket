#include "shell.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "../apps/apps.h"
#include "../audio/dictate.h"
#include "../audio/sfx.h"
#include "../audio/voice.h"
#include "../input/keyboard.h"
#include "../net/net.h"
#include "../storage/store.h"
#include "../ui/ui.h"
#include "../ui/lvgl_ui.h"
#include "notify.h"
#include "settings.h"
#include "launcher.h"
#include "sys.h"

namespace maz {
namespace shell {

using namespace theme;

namespace {

M5Canvas          gCanvas(&M5.Display);
std::vector<App*> gStack;
uint32_t          gLastInput   = 0;
bool              gDimmed      = false;
bool              gScreenOff   = false;
uint32_t          gLastPowerMs = 0;
bool              gEscHandled  = false;
// Set when the focused app claims the ESC *press*. Without it the matching
// release falls through to the shell and pops the app as well, so one ESC in
// a sub-mode both cancelled the edit and left the screen.
bool              gEscClaimed  = false;

// ------------------------------------------------------------- Focus timer
struct FocusTimer {
    bool        running  = false;
    bool        paused   = false;
    uint32_t    total    = 0;  // seconds
    uint32_t    left     = 0;
    uint32_t    lastTick = 0;
    std::string label;
} gFocus;

void tickFocus() {
    if (!gFocus.running || gFocus.paused) return;
    if (millis() - gFocus.lastTick < 1000) return;
    gFocus.lastTick = millis();
    if (gFocus.left > 0) gFocus.left--;

    Sys.focusRemain = gFocus.left;
    if (gFocus.left == 0) {
        gFocus.running   = false;
        Sys.focusRunning = false;
        sfx::timerDone();
        notify::post(Note::Success, "Focus complete",
                     gFocus.label.empty() ? "session finished" : gFocus.label);
    }
}

// -------------------------------------------------------- power / display
void pollPower() {
    if (millis() - gLastPowerMs < 5000) return;
    gLastPowerMs = millis();

    Sys.batteryPct = M5.Power.getBatteryLevel();
    Sys.charging   = M5.Power.isCharging() == m5::Power_Class::is_charging;

    if (Sys.batteryPct >= 0 && Sys.batteryPct <= 10 && !Sys.charging &&
        !Sys.lowBatteryWarned) {
        Sys.lowBatteryWarned = true;
        notify::post(Note::Warn, "Battery low", "about to run out");
    }
    if (Sys.batteryPct > 20) Sys.lowBatteryWarned = false;
}

void applyScreenTimeout() {
    if (Cfg.screenTimeout == 0) return;
    // Recording keeps the screen honest: you should be able to glance down
    // and see that MAZ is still listening.
    if (Sys.recording) return;

    const uint32_t idle = (millis() - gLastInput) / 1000;
    if (!gDimmed && idle >= Cfg.screenTimeout) {
        M5.Display.setBrightness(12);
        gDimmed = true;
    }
    if (!gScreenOff && idle >= Cfg.screenTimeout * 3u) {
        M5.Display.setBrightness(0);
        gScreenOff = true;
    }
}

// -------------------------------------------------------- command palette
// Subsequence matching, not Levenshtein: "rec" should find Recorder, "cal"
// should find both Calculator and Call, and it must never be slow enough to
// notice between keystrokes.
int score(const char* haystack, const std::string& needle) {
    if (needle.empty()) return 1;
    size_t hi = 0, ni = 0;
    int    hits = 0, streak = 0, best = 0;
    while (haystack[hi] && ni < needle.size()) {
        if (tolower(haystack[hi]) == tolower(needle[ni])) {
            ni++;
            hits++;
            streak++;
            best = std::max(best, streak);
        } else {
            streak = 0;
        }
        hi++;
    }
    if (ni < needle.size()) return 0;
    return hits * 4 + best * 6 - static_cast<int>(hi);
}

class Palette : public App {
public:
    const char* id() const override { return "palette"; }
    const char* title() const override { return "Command"; }
    const char* hints() const override { return "^SPACE say it  ENTER run"; }

    void onEnter() override {
        _query.clear();
        rebuild();
    }

    bool onKey(const KeyEvent& e) override {
        // Say the name of the app instead of spelling it. The palette is the
        // fastest route to anything on the device, so it is the last place
        // that should require the keyboard.
        if (e.code == KEY_SPACE && (e.mods & MOD_CTRL)) {
            if (e.down) {
                if (!dictate::active(this)) dictate::start(this);
            } else if (dictate::state() == dictate::State::Listening) {
                dictate::stop();
            }
            return true;
        }
        if (!e.down) return false;
        if (dictate::active(this) && e.code == KEY_ESC) {
            dictate::cancel();
            return true;
        }
        if (e.code == KEY_BACKSPACE) {
            if (!_query.empty()) _query.pop_back();
            rebuild();
            return true;
        }
        if (e.code == KEY_UP) {
            if (_sel > 0) _sel--;
            invalidate();
            return true;
        }
        if (e.code == KEY_DOWN) {
            if (_sel + 1 < static_cast<int>(_hits.size())) _sel++;
            invalidate();
            return true;
        }
        if (e.code == KEY_ENTER) {
            if (_hits.empty()) return true;
            const char* target = _hits[_sel]->id;
            pop();             // close the palette first...
            pushById(target);  // ...so ESC from the app lands where it was
            sfx::confirm();
            return true;
        }
        if (e.ch >= 32 && e.ch < 127) {
            _query.push_back(e.ch);
            rebuild();
            return true;
        }
        return false;
    }

    void render(M5Canvas& g) override {
        // Pick up anything that was dictated into the query.
        std::string spoken;
        if (dictate::take(this, spoken)) {
            _query += spoken;
            rebuild();
        }

        g.fillScreen(BG);
        ui::header(g, "Command", store::backendName());

        ui::panel(g, PAD, BODY_Y + 22, SCREEN_W - PAD * 2, 18);
        g.setFont(&fonts::Font2);
        g.setTextDatum(top_left);

        if (dictate::active(this)) {
            const bool listening = dictate::state() == dictate::State::Listening;
            g.setTextColor(listening ? ACCENT2 : WARN, PANEL);
            g.drawString(listening ? "listening..." : "transcribing...",
                         PAD + 4, BODY_Y + 23);
            return;
        }

        g.setTextColor(TEXT, PANEL);
        std::string shown = "> " + _query;
        if ((millis() / 500) % 2) shown += "_";
        g.drawString(shown.c_str(), PAD + 4, BODY_Y + 23);

        if (_hits.empty()) {
            g.setTextColor(DIM, BG);
            g.setFont(&fonts::Font0);
            g.drawString("no match", PAD + 4, BODY_Y + 48);
            return;
        }
        const int rows = std::min<int>(3, static_cast<int>(_hits.size()));
        const int from = std::max(0, _sel - 2);
        for (int i = 0; i < rows; ++i) {
            const int idx = from + i;
            if (idx >= static_cast<int>(_hits.size())) break;
            const int y = BODY_Y + 44 + i * 17;
            if (idx == _sel)
                g.fillRoundRect(PAD, y - 1, SCREEN_W - PAD * 2, 16, 3, ACCENT);
            g.setFont(&fonts::Font2);
            g.setTextColor(idx == _sel ? BG : TEXT, idx == _sel ? ACCENT : BG);
            g.drawString(_hits[idx]->title, PAD + 6, y);
        }
    }

private:
    void rebuild() {
        _hits.clear();
        size_t                  n = 0;
        const apps::Descriptor* t = apps::table(n);
        std::vector<std::pair<int, const apps::Descriptor*>> ranked;
        for (size_t i = 0; i < n; ++i) {
            if (!strcmp(t[i].id, "home")) continue;
            const int s =
                std::max(score(t[i].title, _query), score(t[i].keywords, _query));
            if (s > 0) ranked.emplace_back(s, &t[i]);
        }
        std::sort(ranked.begin(), ranked.end(),
                  [](const std::pair<int, const apps::Descriptor*>& a,
                     const std::pair<int, const apps::Descriptor*>& b) {
                      return a.first > b.first;
                  });
        for (auto& r : ranked) _hits.push_back(r.second);
        _sel = 0;
        invalidate();
    }

    std::string                          _query;
    std::vector<const apps::Descriptor*> _hits;
    int                                  _sel = 0;
};

// ------------------------------------------------------------- boot screen
void bootScreen() {
    M5.Display.fillScreen(BG);
    const uint32_t t0 = millis();
    while (millis() - t0 < T_BOOT) {
        const float p = (millis() - t0) / static_cast<float>(T_BOOT);
        gCanvas.fillScreen(BG);
        // The mark draws itself in, then the wordmark fades up. Under a
        // second, because a splash you wait for is a splash you resent.
        ui::mark(gCanvas, SCREEN_W / 2, 44, 12, ACCENT, p < 0.7f ? p : 0.f);
        if (p > 0.35f) ui::wordmark(gCanvas, SCREEN_W / 2, 62, TEXT);
        if (p > 0.8f) {
            gCanvas.setFont(&fonts::Font0);
            gCanvas.setTextDatum(top_center);
            gCanvas.setTextColor(DIM, BG);
            gCanvas.drawString("v" MAZ_POCKET_VERSION, SCREEN_W / 2, 118);
            gCanvas.setTextDatum(top_left);
        }
        gCanvas.pushSprite(0, 0);
        delay(16);
    }
}

// ------------------------------------------------------------- nav layer
// The ADV prints the arrows and ESC on the Fn layer, so every menu was a
// two-handed operation: Fn+; Fn+, Fn+. Fn+/ to move, Fn+` to go back.
//
// Rather than steal those characters outright, the shell offers the key to the
// focused app as itself first. Only if the app does not want the character do
// we re-offer it as the navigation code printed beside it. A text field
// consumes the comma and keeps typing; a menu ignores it and gets LEFT. No app
// needs a flag, and no screen can get it wrong.
uint8_t navFallback(uint8_t code) {
    switch (code) {
        case KEY_SEMICOLON:  return KEY_UP;
        case KEY_COMMA:      return KEY_LEFT;
        case KEY_DOT:        return KEY_DOWN;
        case KEY_SLASH:      return KEY_RIGHT;
        case KEY_GRAVE:      return KEY_ESC;
        default:             return KEY_NONE;
    }
}

// -------------------------------------------------------- global shortcuts
bool handleGlobalKey(const KeyEvent& e) {
    if (!e.down) return false;
    // Ctrl+K anywhere: the palette is the one thing that must always answer.
    if ((e.mods & MOD_CTRL) && e.code == KEY_K) {
        openPalette();
        return true;
    }
    // Ctrl+L always hands control back to M5Launcher.
    if ((e.mods & MOD_CTRL) && e.code == KEY_L) {
        launcher::reboot();
        return true;
    }
    return false;
}

}  // namespace

// -------------------------------------------------------------------- API
M5Canvas& canvas() { return gCanvas; }
int       depth() { return static_cast<int>(gStack.size()); }
const char* currentId() {
    return gStack.empty() ? "none" : gStack.back()->id();
}

void invalidate() {
    if (!gStack.empty()) gStack.back()->invalidate();
}

void wake() {
    gLastInput = millis();
    if (gDimmed || gScreenOff) {
        Cfg.applyToHardware();
        gDimmed    = false;
        gScreenOff = false;
        invalidate();
    }
}

void push(App* app) {
    if (!app) return;
    if (!gStack.empty()) gStack.back()->onExit();
    gStack.push_back(app);
    Sys.navDepth = static_cast<uint8_t>(gStack.size());
    app->onEnter();
    app->invalidate();
}

bool pushById(const char* id) {
    App* a = apps::create(id);
    if (!a) {
        notify::post(Note::Error, "Unknown command", id);
        return false;
    }
    push(a);
    return true;
}

void pop() {
    if (gStack.size() <= 1) return;  // Home is the floor
    gStack.back()->onExit();
    delete gStack.back();
    gStack.pop_back();
    Sys.navDepth = static_cast<uint8_t>(gStack.size());
    gStack.back()->onEnter();
    gStack.back()->invalidate();
}

void goHome() {
    while (gStack.size() > 1) pop();
}

void openPalette() {
    if (!gStack.empty() && !strcmp(gStack.back()->id(), "palette")) return;
    push(new Palette());
}

void dispatchKey(const KeyEvent& e) {
    if (e.down) wake();
    if (notify::active() && e.down) notify::dismiss();

    if (handleGlobalKey(e)) return;
    if (gStack.empty()) return;

    KeyEvent ev = e;
    if (ev.down && ev.code == KEY_ESC) {
        gEscHandled = false;
        gEscClaimed = false;
    }
    if (gStack.back()->onKey(ev)) {
        // The app used ESC for its own back step (closing a detail view,
        // cancelling an edit). Remember it so the release does not pop again.
        if (ev.down && ev.code == KEY_ESC) gEscClaimed = true;
        return;
    }

    // The app did not want the character, so offer the same key as the
    // navigation code printed beside it. This is what makes the arrows and ESC
    // work without holding Fn.
    const uint8_t nav = navFallback(ev.code);
    if (nav != KEY_NONE) {
        ev.code = nav;
        ev.ch   = 0;
        if (ev.down && nav == KEY_ESC) {
            gEscHandled = false;
            gEscClaimed = false;
        }
        if (gStack.back()->onKey(ev)) {
            if (ev.down && nav == KEY_ESC) gEscClaimed = true;
            return;
        }
    }

    // Unclaimed ESC is navigation. Long-press-to-Home stays on the real ESC
    // (Fn+`) because it depends on the keyboard's held-key clock, and a held
    // backtick inside a text field must stay a backtick.
    if (!ev.down && ev.code == KEY_ESC && !gEscHandled && !gEscClaimed) {
        pop();
        sfx::select();
    }
}

namespace focus {
void start(uint32_t seconds, const std::string& lbl) {
    gFocus.running   = true;
    gFocus.paused    = false;
    gFocus.total     = seconds;
    gFocus.left      = seconds;
    gFocus.label     = lbl;
    gFocus.lastTick  = millis();
    Sys.focusRunning = true;
    Sys.focusRemain  = seconds;
    Sys.focusLabel   = lbl;
}
void pause() { gFocus.paused = true; }
void resume() {
    gFocus.paused   = false;
    gFocus.lastTick = millis();
}
void cancel() {
    gFocus.running   = false;
    gFocus.paused    = false;
    gFocus.left      = 0;
    Sys.focusRunning = false;
    Sys.focusRemain  = 0;
}
bool               running() { return gFocus.running; }
bool               paused() { return gFocus.paused; }
uint32_t           remaining() { return gFocus.left; }
uint32_t           total() { return gFocus.total; }
const std::string& label() { return gFocus.label; }
}  // namespace focus

// ------------------------------------------------------------------- loop
bool begin() {
    Serial.printf("[boot] shell canvas heap=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()));
    gCanvas.setColorDepth(16);
    if (!gCanvas.createSprite(SCREEN_W, SCREEN_H)) {
        // 64KB of a 512KB part. If this fails something else has eaten the
        // heap and we would rather say so than draw a corrupted frame.
        ESP_LOGE("shell", "canvas allocation failed");
        return false;
    }
    Serial.printf("[boot] canvas ready heap=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()));
    bootScreen();
    Serial.println("[boot] lvgl begin");
    if (!lvui::begin(gCanvas)) {
        ESP_LOGE("shell", "LVGL display allocation failed");
        return false;
    }
    Serial.printf("[boot] lvgl ready heap=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()));
    push(apps::makeHome());
    gLastInput = millis();
    sfx::boot();
    return true;
}

void loop() {
    M5.update();
    KB.update();

    KeyEvent e;
    bool     sawInput = false;
    while (KB.pop(e)) {
        sawInput = true;
        dispatchKey(e);
    }

    if (KB.held(KEY_ESC) && KB.heldFor(KEY_ESC) > 600 && !gEscHandled) {
        gEscHandled = true;
        goHome();
        sfx::select();
    }
    if (sawInput) wake();

    tickFocus();
    pollPower();
    voice::update();
    dictate::update();
    net::update();
    notify::update();
    apps::updateProductServices();
    lvui::tick();
    applyScreenTimeout();

    if (gStack.empty()) return;
    App* top = gStack.back();
    top->update();

    // Chrome animates (clock, level meter, timers) so we repaint on a fixed
    // cadence, but only when something asked for it or 100ms has passed.
    static uint32_t lastPaint = 0;
    const bool      due       = millis() - lastPaint > 100;
    if (!top->dirty() && !due) return;
    if (gScreenOff) return;

    top->render(gCanvas);
    ui::statusBar(gCanvas);
    ui::hintBar(gCanvas, top->hints());
    notify::render(gCanvas);
    gCanvas.pushSprite(0, 0);
    top->clean();
    lastPaint = millis();
}

}  // namespace shell
}  // namespace maz
