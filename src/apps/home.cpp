#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "../audio/sfx.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
#include "../net/mazhost.h"
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
    const char* hints() const override { return "< > move   ENTER open   SPACE call"; }

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
        const int pageStart = (_sel / TABLE_PAGE) * TABLE_PAGE;
        size_t visible = 0;
        for (int i = 0; i < TABLE_PAGE; ++i) {
            const int idx = pageStart + i;
            if (idx >= static_cast<int>(_primary.size())) break;
            cells[i].title = _primary[idx]->cellTitle();
            cells[i].badge = iconFor(*_primary[idx]);
            ++visible;
        }

        const int page = pageStart / TABLE_PAGE + 1;
        const int pages = (_primary.size() + TABLE_PAGE - 1) / TABLE_PAGE;
        std::string state = std::to_string(page) + "/" + std::to_string(pages) + "  PC " + host::linkName();
        if (Sys.agentQuestion) state += " / NEEDS MAZ";
        else if (Sys.agentsStale) state += " / " + std::to_string(Sys.agentsStale) + " STALE";
        else if (Sys.agentsWaiting) state += " / " + std::to_string(Sys.agentsWaiting) + " WAIT";
        else if (Sys.agentsWorking) state += " / " + std::to_string(Sys.agentsWorking) + " WORK";
        else if (Sys.hostOnline) state += " / CLEAR";

        lvui::renderHome(g, cells.data(), visible, _sel - pageStart,
                         "MAZ 0.3.1", state.c_str(), pageStart, pages);
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
        if (!strcmp(d.id, "talk")) return 'C';
        if (!strcmp(d.id, "braindump")) return '+';
        if (!strcmp(d.id, "nudge")) return 'O';
        if (!strcmp(d.id, "desk")) return 'D';
        if (!strcmp(d.id, "recall")) return 'R';
        if (!strcmp(d.id, "flow")) return 'F';
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
