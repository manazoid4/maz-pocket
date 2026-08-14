#include <vector>
#include <array>

#include "../audio/sfx.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
#include "../storage/store.h"
#include "../ui/lvgl_ui.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

// Home is the only screen that must be readable in a half-second glance, so
// it shows exactly one piece of context (live Focus > next task > identity)
// above a fixed eight-tile grid. Everything else is one keystroke away in the
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
        lvui::setActive(true);
        invalidate();
    }

    void onExit() override { lvui::setActive(false); }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) {
            // SPACE released before the hold threshold: treat as a plain
            // "open Capture" rather than eating the keypress.
            if (e.code == KEY_SPACE && _spaceArmed) {
                _spaceArmed = false;
                shell::pushById("talk");
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
            shell::pushById("talk");
            return;
        }
        if (Sys.focusRunning) invalidate();
    }

    void render(M5Canvas& g) override {
        std::array<const char*, 8> titles{};
        for (size_t i = 0; i < _tiles.size() && i < titles.size(); ++i)
            titles[i] = _tiles[i]->title;

        std::string kind = "MAZ";
        std::string text = "POCKET";
        if (Sys.focusRunning) {
            kind = "FOCUS";
            text = ui::ellipsis(shell::focus::label(), 16) + "  " +
                   ui::hhmmss(shell::focus::remaining());
        } else if (!_next.empty()) {
            kind = "NEXT";
            text = ui::ellipsis(_next, 22);
        }

        const char* state = Sys.hostOnline ? "ONLINE" :
                            (Sys.wifiConnected ? "NO HOST" : "OFFLINE");
        lvui::renderHome(g, titles.data(), _tiles.size(), _sel, kind.c_str(),
                         text.c_str(), state, Sys.hostOnline);
    }

private:
    static constexpr int COLS = 2;
    static constexpr int ROWS = 4;

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

    std::vector<const apps::Descriptor*> _tiles;
    std::string                          _next;
    int                                  _sel        = 0;
    bool                                 _spaceArmed = false;
};

}  // namespace

App* makeHome() { return new HomeApp(); }

}  // namespace apps
}  // namespace maz
