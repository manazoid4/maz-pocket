#include <algorithm>
#include <array>
#include <string>

#include "../audio/sfx.h"
#include "../core/shell.h"
#include "../input/keyboard.h"
#include "apps.h"
#include "common.h"

namespace maz {
namespace apps {

using namespace theme;

namespace {

struct HubItem {
    const char* label;
    const char* sub;
    const char* target;
};

class SurfaceHub : public App {
public:
    SurfaceHub(const char* appId, const char* heading, const char* status,
               const HubItem* items, int count)
        : _id(appId), _heading(heading), _status(status), _items(items), _count(count) {}

    const char* id() const override { return _id; }
    const char* title() const override { return _heading; }
    const char* hints() const override { return "UP/DOWN choose   ENTER open   ESC home"; }

    bool onKey(const KeyEvent& e) override {
        if (!e.down) return false;
        if (_cursor.onKey(e, _count)) {
            sfx::select();
            invalidate();
            return true;
        }
        if (e.code == KEY_ENTER && _count > 0) {
            sfx::confirm();
            shell::pushById(_items[_cursor.sel].target);
            return true;
        }
        return false;
    }

    std::string contextSnapshot() const override {
        if (_count <= 0) return _heading;
        return std::string(_heading) + " selected " + _items[_cursor.sel].label +
               " (" + _items[_cursor.sel].sub + ")";
    }

    void render(M5Canvas& g) override {
        g.fillScreen(BG);
        ui::header(g, _heading, _status);

        constexpr int visible = 5;
        constexpr int pitch = 18;
        if (_cursor.sel < _cursor.first) _cursor.first = _cursor.sel;
        if (_cursor.sel >= _cursor.first + visible)
            _cursor.first = _cursor.sel - visible + 1;
        const int maxFirst = std::max(0, _count - visible);
        if (_cursor.first > maxFirst) _cursor.first = maxFirst;

        for (int row = 0; row < visible; ++row) {
            const int idx = _cursor.first + row;
            if (idx >= _count) break;
            const int y = BODY_Y + 15 + row * pitch;
            const bool selected = idx == _cursor.sel;
            const uint16_t fill = selected ? ACCENT : PANEL;
            const uint16_t fg = selected ? BG : TEXT;

            g.fillRoundRect(PAD, y, SCREEN_W - PAD * 2, 15, 4, fill);
            g.drawRoundRect(PAD, y, SCREEN_W - PAD * 2, 15, 4, selected ? TEXT : LINE);
            g.setTextDatum(top_left);
            g.setFont(&fonts::Font0);
            g.setTextColor(fg, fill);
            g.drawString(_items[idx].label, PAD + 6, y + 3);
            g.setTextDatum(top_right);
            g.setTextColor(selected ? BG : DIM, fill);
            g.drawString(_items[idx].sub, SCREEN_W - PAD - 6, y + 3);
        }
        ui::scrollBar(g, _count, _cursor.first, visible);
        g.setTextDatum(top_left);
    }

private:
    const char* _id;
    const char* _heading;
    const char* _status;
    const HubItem* _items;
    int _count;
    ListCursor _cursor;
};

constexpr HubItem AGENT_ITEMS[] = {
    {"AGENT STATUS", "who is doing what", "nudge"},
    {"PLAN", "think before code", "plan"},
    {"CREW", "split agent work", "crew"},
    {"RETRO", "learn from work", "retro"},
    {"PROJECTS", "build / test / git", "core"},
};

constexpr HubItem CONTROL_ITEMS[] = {
    {"LAPTOP STATUS", "CPU / GPU / AI", "laptop"},
    {"PROJECTS + BUILDS", "git / tests", "core"},
    {"SEND TO PC", "text / links", "beam"},
    {"WI-FI", "scan / connect", "network"},
    {"DEVICE TESTS", "hardware checks", "tools"},
    {"SETTINGS", "device + AI", "settings"},
    {"CONTROL CENTER", "advanced controls", "control"},
};

constexpr HubItem RECALL_ITEMS[] = {
    {"RESULTS INBOX", "AI + agent outputs", "inbox"},
    {"PROMPT DECK", "build prompts", "prompts"},
    {"NOTES", "saved text", "notes"},
    {"SEND TO PC", "received / sent", "beam"},
    {"SNIPPETS", "quick reuse", "snippets"},
    {"TEXT VIEWER", "txt / md", "viewer"},
};

constexpr HubItem FLOW_ITEMS[] = {
    {"FOCUS TIMER", "stay on task", "focus"},
    {"WORK SPRINT", "25m outcome", "sprint"},
    {"TASKS", "next actions", "tasks"},
    {"REMINDERS", "don't forget", "reminders"},
    {"SHIFT CLOCK", "field work", "shift"},
};

}  // namespace

App* makeAgentsHub() {
    return new SurfaceHub("agents", "AGENTS", "PLAN + RUN + LEARN",
                          AGENT_ITEMS, sizeof(AGENT_ITEMS) / sizeof(AGENT_ITEMS[0]));
}

App* makeDesk() {
    return new SurfaceHub("desk", "CONTROL", "PC + DEVICE",
                          CONTROL_ITEMS, sizeof(CONTROL_ITEMS) / sizeof(CONTROL_ITEMS[0]));
}

App* makeRecall() {
    return new SurfaceHub("recall", "MEMORY", "RESULTS + PROMPTS",
                          RECALL_ITEMS, sizeof(RECALL_ITEMS) / sizeof(RECALL_ITEMS[0]));
}

App* makeFlow() {
    return new SurfaceHub("flow", "FOCUS", "TIME + ACTION",
                          FLOW_ITEMS, sizeof(FLOW_ITEMS) / sizeof(FLOW_ITEMS[0]));
}

}  // namespace apps
}  // namespace maz
