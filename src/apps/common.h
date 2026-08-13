// MAZ Pocket — small shared pieces every app screen needs.
#pragma once
#include <string>

#include "../core/keys.h"
#include "../ui/ui.h"

namespace maz {
namespace apps {

// A one-line text field driven by the physical keyboard. Deliberately not a
// text editor: on a 240px panel, a field that scrolls horizontally and takes
// ENTER as "done" is the interaction that actually works.
struct TextField {
    std::string text;
    size_t      limit = 120;

    // Returns true if the key was consumed.
    bool onKey(const KeyEvent& e) {
        if (!e.down) return false;
        if (e.code == KEY_BACKSPACE) {
            if (!text.empty()) text.pop_back();
            return true;
        }
        if (e.ch >= 32 && e.ch < 127 && text.size() < limit) {
            text.push_back(e.ch);
            return true;
        }
        return false;
    }

    void draw(M5Canvas& g, int x, int y, int w, const char* placeholder = "") {
        ui::panel(g, x, y, w, 18);
        g.setFont(&fonts::Font2);
        g.setTextDatum(top_left);
        const bool empty = text.empty();
        g.setTextColor(empty ? theme::DIM : theme::TEXT, theme::PANEL);

        // Show the tail, so you can always see what you are typing.
        const size_t maxChars = (w - 10) / 8;
        std::string  shown =
            empty ? std::string(placeholder)
                  : (text.size() > maxChars ? text.substr(text.size() - maxChars)
                                            : text);
        if (!empty && (millis() / 500) % 2) shown += "_";
        g.drawString(shown.c_str(), x + 4, y + 1);
    }
};

// Shared list cursor: keeps selection and scroll window in step so every list
// screen in MAZ Pocket behaves identically.
struct ListCursor {
    int sel   = 0;
    int first = 0;

    void clamp(int count) {
        if (count <= 0) {
            sel = first = 0;
            return;
        }
        if (sel >= count) sel = count - 1;
        if (sel < 0) sel = 0;
        if (sel < first) first = sel;
        if (sel >= first + theme::ROWS_VISIBLE)
            first = sel - theme::ROWS_VISIBLE + 1;
        if (first < 0) first = 0;
    }
    bool onKey(const KeyEvent& e, int count) {
        if (!e.down) return false;
        if (e.code == KEY_UP) {
            sel--;
            clamp(count);
            return true;
        }
        if (e.code == KEY_DOWN) {
            sel++;
            clamp(count);
            return true;
        }
        return false;
    }
};

}  // namespace apps
}  // namespace maz
