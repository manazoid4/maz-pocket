#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "../audio/sfx.h"
#include "../core/field.h"
#include "../core/shell.h"
#include "../core/sys.h"
#include "../input/keyboard.h"
#include "../net/mazhost.h"
#include "../net/net.h"
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
    const char* hints() const override { return _hints.c_str(); }

    void onEnter() override {
        buildPrimary();
        _spaceArmed = false;
        _easter = false;
        lvui::setActive(true);
        rebuildHints();
        invalidate();
    }

    void onExit() override { lvui::setActive(false); }

    bool onKey(const KeyEvent& e) override {
        if (_easter && e.down) {
            _easter = false;
            rebuildHints();
            sfx::select();
            invalidate();
            return true;
        }

        if (!e.down) {
            if (e.code == KEY_SPACE && _spaceArmed) {
                _spaceArmed = false;
                shell::pushById("talk");
                return true;
            }
            return false;
        }

        if ((e.mods & MOD_FN) && e.code == KEY_M) {
            _easter = true;
            _hints = "any key returns to 2026";
            sfx::confirm();
            invalidate();
            return true;
        }

        if (e.code >= KEY_1 && e.code <= KEY_4) {
            const int slot = static_cast<int>(e.code - KEY_1) + 1;
            if (e.mods & MOD_FN) field::cycleQuick(slot);
            else field::runQuick(slot);
            rebuildHints();
            invalidate();
            return true;
        }

        if (e.code == KEY_SPACE) {
            _spaceArmed = true;
            return true;
        }
        if (e.code == KEY_ENTER) { open(_sel); return true; }
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
            return;
        }
        if (millis() - _lastRefresh > 500) {
            _lastRefresh = millis();
            rebuildHints();
            invalidate();
        }
    }

    std::string contextSnapshot() const override {
        if (_primary.empty() || _sel < 0 || _sel >= static_cast<int>(_primary.size()))
            return field::nowText();
        return std::string("Home selected ") + _primary[_sel]->cellTitle() + "; NOW: " + field::nowText();
    }

    void render(M5Canvas& g) override {
        if (_easter) {
            renderEaster(g);
            return;
        }
        std::array<lvui::Cell, TABLE_PAGE> cells{};
        for (size_t i = 0; i < _primary.size() && i < cells.size(); ++i) {
            cells[i].title = _primary[i]->cellTitle();
            cells[i].badge = iconFor(*_primary[i]);
        }
        const std::string now = field::nowText();
        lvui::renderHome(g, cells.data(), _primary.size(), _sel,
                         "NOW", now.c_str(), 0, 1);
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

    void rebuildHints() {
        _hints = "1 "; _hints += field::quickLabel(1);
        _hints += " 2 "; _hints += field::quickLabel(2);
        _hints += " 3 "; _hints += field::quickLabel(3);
        _hints += " 4 "; _hints += field::quickLabel(4);
    }

    static char iconFor(const Descriptor& d) {
        if (!strcmp(d.id, "talk")) return 'C';
        if (!strcmp(d.id, "braindump")) return '+';
        if (!strcmp(d.id, "nudge")) return 'O';
        if (!strcmp(d.id, "desk")) return '#';
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

    void renderEaster(M5Canvas& g) {
        g.fillScreen(BG);
        g.setFont(&fonts::Font0);
        g.setTextDatum(top_left);
        g.setTextColor(ACCENT, BG);
        g.drawString("MAZ TERMINAL / 2026", PAD, BODY_Y + 5);
        g.setTextColor(TEXT, BG);
        g.drawString("UPLINK....OK", PAD, BODY_Y + 21);
        g.drawString("MEMORY....OK", PAD, BODY_Y + 34);
        g.drawString("AGENTS....?", PAD, BODY_Y + 47);
        g.drawString("HUMAN.....MAZ", PAD, BODY_Y + 60);
        g.setTextColor(ACCENT2, BG);
        g.drawString("> THE FUTURE WAS SUPPOSED", PAD, BODY_Y + 80);
        g.drawString("  TO LOOK LIKE THIS.", PAD, BODY_Y + 94);
    }

    std::vector<const Descriptor*> _primary;
    int  _sel = 0;
    bool _spaceArmed = false;
    bool _easter = false;
    uint32_t _lastRefresh = 0;
    std::string _hints;
};

}  // namespace

App* makeHome() { return new HomeApp(); }

}  // namespace apps
}  // namespace maz
