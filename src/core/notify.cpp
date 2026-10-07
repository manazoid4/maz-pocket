#include "notify.h"

#include <Arduino.h>

#include "../audio/sfx.h"
#include "../ui/theme.h"
#include "../ui/ui.h"
#include "shell.h"

namespace maz {
namespace notify {

using namespace theme;

namespace {
struct Toast {
    Note        kind = Note::Info;
    std::string title;
    std::string detail;
    uint32_t    shownAt = 0;
    bool        live    = false;
};
Toast t;

uint16_t colourFor(Note k) {
    switch (k) {
        case Note::Success: return OK;
        case Note::Warn:    return WARN;
        case Note::Error:   return ERR;
        default:            return ACCENT2;
    }
}
}  // namespace

void post(Note kind, const std::string& title, const std::string& detail) {
    t.kind    = kind;
    t.title   = title;
    t.detail  = detail;
    t.shownAt = millis();
    t.live    = true;
    sfx::forNote(kind);
    shell::wake();
    ESP_LOGI("note", "%s - %s", title.c_str(), detail.c_str());
}

bool active() { return t.live; }
void dismiss() { t.live = false; }

void update() {
    // Warn/Error carry something worth reading twice; give them longer on
    // screen than a routine Info/Success toast.
    const uint32_t life =
        (t.kind == Note::Warn || t.kind == Note::Error) ? T_TOAST * 2 : T_TOAST;
    if (t.live && millis() - t.shownAt > life) t.live = false;
}

void render(M5Canvas& g) {
    if (!t.live) return;

    // Slides up from the hint bar, so it never covers the thing you are
    // looking at for longer than it has to.
    const uint32_t age = millis() - t.shownAt;
    const int      h   = t.detail.empty() ? 20 : 32;
    int            y   = SCREEN_H - HINT_H - h;
    if (age < T_FAST) y += (h * (T_FAST - age)) / T_FAST;

    const uint16_t c = colourFor(t.kind);
    g.fillRoundRect(4, y, SCREEN_W - 8, h, 4, PANEL);
    g.drawRoundRect(4, y, SCREEN_W - 8, h, 4, c);
    g.fillRect(6, y + 3, 3, h - 6, c);

    g.setTextDatum(top_left);
    g.setFont(&fonts::Font2);
    g.setTextColor(c, PANEL);
    g.drawString(ui::ellipsis(t.title, TOAST_TITLE_CHARS).c_str(), 14, y + 2);
    if (!t.detail.empty()) {
        g.setFont(&fonts::Font0);
        g.setTextColor(DIM, PANEL);
        g.drawString(ui::ellipsis(t.detail, TOAST_TEXT_CHARS).c_str(), 14, y + 20);
    }
}

}  // namespace notify
}  // namespace maz
