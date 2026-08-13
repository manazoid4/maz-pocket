#include <vector>

#include "../audio/sfx.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
#include "../storage/store.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

// Home is the only screen that must be readable in a half-second glance, so
// it shows exactly one piece of context (live Focus > next task > identity)
// above a fixed six-tile grid. Everything else is one keystroke away in the
// palette rather than crowded in here.
class HomeApp : public App {
public:
    const char* id() const override { return "home"; }
    const char* title() const override { return "MAZ Pocket"; }
    const char* hints() const override {
        return "hold SPACE capture   ^K command   ENTER open";
    }

    void onEnter() override {
        buildTiles();
        refreshNextTask();
        _spaceArmed = false;
        invalidate();
    }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) {
            // SPACE released before the hold threshold: treat as a plain
            // "open Capture" rather than eating the keypress.
            if (e.code == KEY_SPACE && _spaceArmed) {
                _spaceArmed = false;
                shell::pushById("capture");
                return true;
            }
            return false;
        }

        if (e.code == KEY_SPACE) {
            _spaceArmed = true;
            return true;
        }
        if (e.code == KEY_ENTER) {
            open(_sel);
            return true;
        }
        if (e.code == KEY_RIGHT) { move(+1); return true; }
        if (e.code == KEY_LEFT)  { move(-1); return true; }
        if (e.code == KEY_DOWN)  { move(+COLS); return true; }
        if (e.code == KEY_UP)    { move(-COLS); return true; }

        // Single-letter shortcuts: the whole point of a keyboard handheld.
        size_t                  n = 0;
        const apps::Descriptor* t = apps::table(n);
        for (size_t i = 0; i < n; ++i) {
            if (t[i].shortcut && t[i].shortcut == e.code) {
                sfx::confirm();
                shell::pushById(t[i].id);
                return true;
            }
        }
        if (e.code == KEY_SLASH) {
            shell::openPalette();
            return true;
        }
        return false;
    }

    void update() override {
        // Hold-to-capture: crossing the threshold jumps straight into Capture
        // already recording, so a thought costs one gesture, not a menu.
        if (_spaceArmed && KB.heldFor(KEY_SPACE) > 250) {
            _spaceArmed = false;
            shell::pushById("capture");
            return;
        }
        if (Sys.focusRunning) invalidate();
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        drawContext(g);
        drawGrid(g);
    }

private:
    static constexpr int COLS = 3;
    static constexpr int ROWS = 2;

    void buildTiles() {
        _tiles.clear();
        size_t                  n = 0;
        const apps::Descriptor* t = apps::table(n);
        for (size_t i = 0; i < n && _tiles.size() < COLS * ROWS; ++i)
            if (t[i].onHome) _tiles.push_back(&t[i]);
    }

    void refreshNextTask() {
        _next.clear();
        if (!store::ready()) return;
        for (const auto& t : store::loadTasks()) {
            if (!t.done && t.bucket == 0) {
                _next = t.text;
                break;
            }
        }
    }

    void move(int delta) {
        const int count = static_cast<int>(_tiles.size());
        if (!count) return;
        _sel = (_sel + delta + count) % count;
        sfx::select();
        invalidate();
    }

    void open(int idx) {
        if (idx < 0 || idx >= static_cast<int>(_tiles.size())) return;
        sfx::confirm();
        shell::pushById(_tiles[idx]->id);
    }

    void drawContext(M5Canvas& g) {
        const int y = BODY_Y + 2;
        if (Sys.focusRunning) {
            ui::panel(g, PAD, y, SCREEN_W - PAD * 2, 30, PANEL);
            g.setFont(&fonts::Font0);
            g.setTextDatum(top_left);
            g.setTextColor(ACCENT, PANEL);
            g.drawString("FOCUS", PAD + 6, y + 4);
            g.setTextColor(DIM, PANEL);
            g.drawString(ui::ellipsis(shell::focus::label(), 22).c_str(),
                         PAD + 44, y + 4);
            g.setFont(&fonts::Font4);
            g.setTextColor(TEXT, PANEL);
            g.drawString(ui::hhmmss(shell::focus::remaining()).c_str(), PAD + 6,
                         y + 12);
            return;
        }
        if (!_next.empty()) {
            ui::panel(g, PAD, y, SCREEN_W - PAD * 2, 30, PANEL);
            g.setFont(&fonts::Font0);
            g.setTextDatum(top_left);
            g.setTextColor(ACCENT, PANEL);
            g.drawString("NEXT", PAD + 6, y + 4);
            g.setFont(&fonts::Font2);
            g.setTextColor(TEXT, PANEL);
            g.drawString(ui::ellipsis(_next, 27).c_str(), PAD + 6, y + 13);
            return;
        }
        // Nothing pending: the device shows what it is.
        ui::mark(g, 26, y + 15, 11, ACCENT);
        g.setTextDatum(top_left);
        g.setFont(&fonts::Font4);
        g.setTextColor(TEXT, BG);
        g.drawString("MAZ", 48, y + 2);
        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, BG);
        g.drawString("POCKET", 50, y + 22);
    }

    void drawGrid(M5Canvas& g) {
        const int top = BODY_Y + 36;
        const int tw  = (SCREEN_W - PAD * 2 - 8) / COLS;
        const int th  = 30;
        for (size_t i = 0; i < _tiles.size(); ++i) {
            const int  col = i % COLS, row = i / COLS;
            const int  x  = PAD + col * (tw + 4);
            const int  y  = top + row * (th + 4);
            const bool on = static_cast<int>(i) == _sel;

            g.fillRoundRect(x, y, tw, th, 4, on ? ACCENT : PANEL);
            if (on) g.drawRoundRect(x - 1, y - 1, tw + 2, th + 2, 5, ACCENT);

            g.setFont(&fonts::Font2);
            g.setTextDatum(middle_center);
            g.setTextColor(on ? BG : TEXT, on ? ACCENT : PANEL);
            g.drawString(_tiles[i]->title, x + tw / 2, y + th / 2);
            g.setTextDatum(top_left);
        }
    }

    std::vector<const apps::Descriptor*> _tiles;
    std::string                          _next;
    int                                  _sel        = 0;
    bool                                 _spaceArmed = false;
};

}  // namespace

App* makeHome() { return new HomeApp(); }

}  // namespace apps
}  // namespace maz
