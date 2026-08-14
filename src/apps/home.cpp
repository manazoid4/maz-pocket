#include <cstring>
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

// Home is the only screen that must be readable in a half-second glance, so it
// shows exactly one piece of context (live Focus > next task > identity) above
// a table of apps. The table pages: every app in the registry sits on one of
// the pages with its label and the key that opens it, so there is no longer
// such a thing as a feature you can only find by already knowing its name.
class HomeApp : public App {
public:
    const char* id() const override { return "home"; }
    const char* title() const override { return "MAZ Pocket"; }
    const char* hints() const override {
        return "ENTER open  ,. page  SPACE talk";
    }

    void onEnter() override {
        buildPages();
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

        // Paging. LEFT/RIGHT walk the row and then step to the neighbouring
        // page, landing on the column you entered from — that is what makes it
        // feel like panning across one wide table rather than teleporting.
        if (e.code == KEY_RIGHT) { stepHorizontal(+1); return true; }
        if (e.code == KEY_LEFT)  { stepHorizontal(-1); return true; }
        // UP/DOWN stay inside the page on purpose: vertical movement that also
        // changed page would wreck the mental model of a table.
        if (e.code == KEY_DOWN)  { stepVertical(+1); return true; }
        if (e.code == KEY_UP)    { stepVertical(-1); return true; }
        // The fast "flip through everything" key, which previously did nothing.
        if (e.code == KEY_TAB) {
            turnPage(e.shift() ? -1 : +1, 0);
            return true;
        }

        // Digits open the cell showing that digit as its badge.
        if (e.code >= KEY_1 && e.code <= KEY_8) {
            open(e.code - KEY_1);
            return true;
        }

        // Single-letter shortcuts: the whole point of a keyboard handheld.
        // They work from any page, not only the one the app is drawn on.
        size_t                  n = 0;
        const apps::Descriptor* t = apps::table(n);
        for (size_t i = 0; i < n; ++i) {
            if (t[i].shortcut && t[i].shortcut == e.code) {
                sfx::confirm();
                shell::pushById(t[i].id);
                return true;
            }
        }
        // `/` is deliberately not bound here: it is the RIGHT arrow printed on
        // the key, and paging Home without holding Fn matters more than a
        // second way into the palette. Ctrl+K still opens it from anywhere.
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
        std::array<lvui::Cell, TABLE_PAGE> cells{};
        const int count = pageSize();
        for (int i = 0; i < count; ++i) {
            const Descriptor* d = _all[_page * TABLE_PAGE + i];
            cells[i].title = d->cellTitle();
            cells[i].badge = badgeFor(*d, i);
        }

        std::string kind = "MAZ";
        std::string text = "POCKET";
        if (_page > 0) {
            // Each page past the first gets an identity of its own, which is
            // what turns "batches" into sections you can remember.
            kind = _page == 1 ? "TOOLS" : "SYSTEM";
            text = "page " + std::to_string(_page + 1) + " of " +
                   std::to_string(_pageCount);
        } else if (Sys.focusRunning) {
            kind = "FOCUS";
            text = ui::ellipsis(shell::focus::label(), 14) + "  " +
                   ui::hhmmss(shell::focus::remaining());
        } else if (!_next.empty()) {
            kind = "NEXT";
            text = ui::ellipsis(_next, 20);
        }

        lvui::renderHome(g, cells.data(), static_cast<size_t>(count), _sel,
                         kind.c_str(), text.c_str(), _page, _pageCount);
    }

private:
    // The badge is the letter that opens the app, or the digit of its slot on
    // this page when it has no letter of its own. Either way the cell carries
    // its own instructions.
    static char badgeFor(const Descriptor& d, int slot) {
        if (d.shortcut >= KEY_A && d.shortcut <= KEY_Z)
            return static_cast<char>('A' + (d.shortcut - KEY_A));
        return static_cast<char>('1' + slot);
    }

    // Registry order, `onHome` entries first. That keeps the eight apps the
    // device is actually for on page one, and puts everything else behind them
    // rather than nowhere at all.
    void buildPages() {
        _all.clear();
        size_t                  n = 0;
        const apps::Descriptor* t = apps::table(n);
        for (size_t i = 0; i < n; ++i)
            if (t[i].onHome && strcmp(t[i].id, "home")) _all.push_back(&t[i]);
        for (size_t i = 0; i < n; ++i)
            if (!t[i].onHome && strcmp(t[i].id, "home")) _all.push_back(&t[i]);

        _pageCount =
            static_cast<int>((_all.size() + TABLE_PAGE - 1) / TABLE_PAGE);
        if (_pageCount < 1) _pageCount = 1;
        if (_page >= _pageCount) _page = 0;
        clampSelection();
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

    int pageSize() const {
        const int remaining =
            static_cast<int>(_all.size()) - _page * TABLE_PAGE;
        if (remaining < 0) return 0;
        return remaining > TABLE_PAGE ? TABLE_PAGE : remaining;
    }

    void clampSelection() {
        const int count = pageSize();
        if (count <= 0) { _sel = 0; return; }
        if (_sel >= count) _sel = count - 1;
        if (_sel < 0) _sel = 0;
    }

    void turnPage(int delta, int landingColumn) {
        if (_pageCount <= 1) {
            sfx::select();
            return;
        }
        const int row = _sel / TABLE_COLS;
        _page = (_page + delta + _pageCount) % _pageCount;
        _sel  = row * TABLE_COLS + landingColumn;
        clampSelection();
        sfx::select();
        invalidate();
    }

    void stepHorizontal(int delta) {
        const int column = _sel % TABLE_COLS;
        const int target = column + delta;
        // Off an edge: step to the neighbouring page, entering from the column
        // you would have arrived at.
        if (target < 0) { turnPage(-1, TABLE_COLS - 1); return; }
        if (target >= TABLE_COLS) { turnPage(+1, 0); return; }

        const int next = _sel + delta;
        if (next < 0 || next >= pageSize()) return;
        _sel = next;
        sfx::select();
        invalidate();
    }

    void stepVertical(int delta) {
        const int count = pageSize();
        if (count <= 0) return;
        const int next = _sel + delta * TABLE_COLS;
        if (next < 0 || next >= count) return;
        _sel = next;
        sfx::select();
        invalidate();
    }

    void open(int slot) {
        if (slot < 0 || slot >= pageSize()) return;
        sfx::confirm();
        shell::pushById(_all[_page * TABLE_PAGE + slot]->id);
    }

    std::vector<const apps::Descriptor*> _all;
    std::string                          _next;
    int                                  _page       = 0;
    int                                  _pageCount  = 1;
    int                                  _sel        = 0;
    bool                                 _spaceArmed = false;
};

}  // namespace

App* makeHome() { return new HomeApp(); }

}  // namespace apps
}  // namespace maz
