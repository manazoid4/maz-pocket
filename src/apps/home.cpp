#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "../audio/sfx.h"
#include "../core/field.h"
#include "../core/settings.h"
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
    const char* hints() const override { return _hints; }

    void onEnter() override {
        buildPrimary();
        _spaceArmed = false;
        lvui::setActive(true);
        rebuildHints();
        invalidate();
    }

    void onExit() override { lvui::setActive(false); }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) {
            if (e.code == KEY_SPACE) _spaceArmed = false;
            return false;
        }

        if (e.code >= KEY_1 && e.code <= KEY_4) {
            const int slot = static_cast<int>(e.code - KEY_1) + 1;
            if (e.mods & MOD_FN) field::cycleQuick(slot);
            else field::runQuick(slot);
            rebuildHints();
            invalidate();
            return true;
        }

        if (e.code == KEY_U && host::updateReady()) { shell::confirmFwUpdate(); return true; }
        if (e.code == KEY_SPACE) { _spaceArmed = true; return true; }
        if (e.code == KEY_ENTER) { open(_sel); return true; }
        if (e.code == KEY_RIGHT) { move(+1); return true; }
        if (e.code == KEY_LEFT)  { move(-1); return true; }
        if (e.code == KEY_DOWN)  { move(+TABLE_COLS); return true; }
        if (e.code == KEY_UP)    { move(-TABLE_COLS); return true; }

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
            shell::pushById("braindump");  // keeps recording while SPACE is held
            return;
        }
        // The shell repaints every 250 ms anyway; only refresh the hint text here.
        rebuildHints();
    }

    std::string contextSnapshot() const override {
        if (_primary.empty() || _sel < 0 || _sel >= static_cast<int>(_primary.size()))
            return field::nowText();
        return std::string("Home selected ") + _primary[_sel]->cellTitle() + "; NOW: " + field::nowText();
    }

    void render(M5Canvas& g) override {
        std::array<lvui::Cell, TABLE_PAGE> cells{};
        for (size_t i = 0; i < _primary.size() && i < cells.size(); ++i) {
            cells[i].title = _primary[i]->cellTitle();
            cells[i].badge = _primary[i]->shortcut ? static_cast<char>('A' + (_primary[i]->shortcut - KEY_A)) : ' ';
        }
        lvui::Status st;
        st.version = "v" NOD_FW_VERSION;
        statusFor(st);
        lvui::renderHome(g, cells.data(), _primary.size(), _sel, st);
    }

private:
    // Link trouble beats everything else, then a ready update, then what the
    // field layer reports. Same words and colours as Call (ui::statusWord).
    void statusFor(lvui::Status& st) {
        static std::string now;
        const char* link = linkSentence(true);
        ui::Phase ph = ui::Phase::Ready;
        st.sentence = "Hold SPACE: brain dump  T: talk  S: type";
        if (link) {
            ph = ui::Phase::Offline;
            st.sentence = link;
        } else if (host::updateReady()) {
            ph = ui::Phase::Update;
            st.sentence = "Press U, then Y";
        } else if (Sys.agentQuestion) {
            ph = ui::Phase::NeedsYou;
            st.sentence = "An agent asked. Press N.";
        }
        const ui::StatusWord w = ui::statusWord(ph);
        st.phase = ph;
        st.line = w.text;
        st.colour = w.colour;
        if (ph == ui::Phase::Ready) {
            now = field::nowText();
            if (now != "READY") st.line = now.c_str();  // outbox, shift, reminder...
        }
    }

    void buildPrimary() {
        _primary.clear();
        size_t n = 0;
        const Descriptor* t = apps::table(n);
        for (size_t i = 0; i < n; ++i)
            if (t[i].onHome && strcmp(t[i].id, "home") && _primary.size() < TABLE_PAGE) _primary.push_back(&t[i]);
        if (_sel >= static_cast<int>(_primary.size())) _sel = 0;
    }

    void rebuildHints() {
        // W opens Wi-Fi from Home (registry shortcut), so say so when Wi-Fi is down.
        _hints = (linkSentence(true) && !Sys.wifiConnected) ? "SPACE dump  ENTER open  W wifi"
               : host::updateReady()                        ? "SPACE dump  ENTER open  U update"
                                                            : "SPACE dump  ENTER open  1-4 quick";
    }

    void move(int delta) {
        if (_primary.empty()) return;
        _sel = (_sel + delta + static_cast<int>(_primary.size())) % static_cast<int>(_primary.size());
        sfx::select(); invalidate();
    }

    void open(int slot) {
        if (slot < 0 || slot >= static_cast<int>(_primary.size())) return;
        sfx::confirm(); shell::pushById(_primary[slot]->id);
    }

    std::vector<const Descriptor*> _primary;
    int _sel = 0;
    bool _spaceArmed = false;
    const char* _hints = "";
};

}  // namespace

const char* linkSentence(bool quiet) {
    if (quiet && (millis() - Sys.bootMillis <= 12000 || Cfg.fieldMode)) return nullptr;
    if (!Sys.wifiConnected) return "No Wi-Fi. Press W to connect.";
    if (!host::configured()) return "Not paired. Control > Pair.";
    if (!Sys.hostOnline) return "Core is off. Start it on PC.";
    return nullptr;
}

App* makeHome() { return new HomeApp(); }

}  // namespace apps
}  // namespace maz
