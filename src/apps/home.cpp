#include <array>
#include <cstring>
#include <vector>

#include "../audio/sfx.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
#include "../ui/lvgl_ui.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

class HomeApp : public App {
public:
    const char* id() const override { return "home"; }
    const char* title() const override { return "MAZ Pocket"; }
    const char* hints() const override { return "< > choose   ENTER open   SPACE call"; }

    void onEnter() override {
        buildPrimary();
        _spaceArmed = false;
        lvui::setActive(true);
        invalidate();
    }

    void onExit() override { lvui::setActive(false); }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) {
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

        // Direct letter shortcuts remain available for every registered app,
        // including utilities intentionally removed from Home. Ctrl+K remains
        // the discoverable route to the full command palette.
        size_t n = 0;
        const Descriptor* t = apps::table(n);
        for (size_t i = 0; i < n; ++i) {
            if (t[i].shortcut && t[i].shortcut == e.code) {
                sfx::confirm();
                shell::pushById(t[i].id);
                return true;
            }
        }
        return false;
    }

    void update() override {
        if (_spaceArmed && KB.heldFor(KEY_SPACE) > 250) {
            _spaceArmed = false;
            shell::pushById("talk");
        }
    }

    void render(M5Canvas& g) override {
        std::array<lvui::Cell, TABLE_PAGE> cells{};
        for (size_t i = 0; i < _primary.size() && i < cells.size(); ++i) {
            cells[i].title = _primary[i]->cellTitle();
            cells[i].badge = iconFor(*_primary[i]);
        }

        const char* state = Sys.hostOnline ? "PC ONLINE" : "PC STANDBY";
        lvui::renderHome(g, cells.data(), _primary.size(), _sel,
                         "MAZ 0.3", state, 0, 1);
    }

private:
    void buildPrimary() {
        _primary.clear();
        size_t n = 0;
        const Descriptor* t = apps::table(n);
        for (size_t i = 0; i < n; ++i) {
            if (t[i].onHome && strcmp(t[i].id, "home")) _primary.push_back(&t[i]);
        }
        if (_sel >= static_cast<int>(_primary.size())) _sel = 0;
    }

    static char iconFor(const Descriptor& d) {
        if (!strcmp(d.id, "talk")) return 'P';     // PC / phone
        if (!strcmp(d.id, "braindump")) return 'R'; // record
        if (!strcmp(d.id, "nudge")) return 'A';    // agents
        return '*';
    }

    void move(int delta) {
        if (_primary.empty()) return;
        _sel = (_sel + delta + static_cast<int>(_primary.size())) %
               static_cast<int>(_primary.size());
        sfx::select();
        invalidate();
    }

    void open(int slot) {
        if (slot < 0 || slot >= static_cast<int>(_primary.size())) return;
        sfx::confirm();
        shell::pushById(_primary[slot]->id);
    }

    std::vector<const Descriptor*> _primary;
    int  _sel = 0;
    bool _spaceArmed = false;
};

}  // namespace

App* makeHome() { return new HomeApp(); }

}  // namespace apps
}  // namespace maz
