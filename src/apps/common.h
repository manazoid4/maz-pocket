// MAZ Pocket — small shared pieces every app screen needs.
#pragma once
#include <string>

#include "../audio/dictate.h"
#include "../core/keys.h"
#include "../ui/ui.h"

namespace maz {
namespace apps {

// A one-line text field driven by the physical keyboard. Deliberately not a
// text editor: on a 240px panel, a field that scrolls horizontally and takes
// ENTER as "done" is the interaction that actually works.
//
// It also takes dictation. Every field on the device is this struct, so voice
// input belongs here rather than in each screen: Notes, Tasks, Decision,
// Sprint, Reminders, Snippets and Connections all gain it from one place, and
// none of them can implement it slightly differently.
struct TextField {
    std::string text;
    size_t      limit = 120;

    // Hold Ctrl+SPACE and talk. SPACE alone cannot be the trigger here — in a
    // text field that is a space — and Ctrl+SPACE is otherwise unbound.
    static bool isDictateKey(const KeyEvent& e) {
        return e.code == KEY_SPACE && (e.mods & MOD_CTRL);
    }

    // Returns true if the key was consumed.
    bool onKey(const KeyEvent& e) {
        if (isDictateKey(e)) {
            if (e.down) {
                if (!dictate::active(this)) dictate::start(this);
            } else if (dictate::state() == dictate::State::Listening) {
                dictate::stop();
            }
            return true;
        }
        if (!e.down) return false;
        // ESC while the mic is open abandons the dictation instead of leaving
        // the screen out from under a recording.
        if (dictate::active(this) && e.code == KEY_ESC) {
            dictate::cancel();
            return true;
        }
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

    // Called from draw(), so a screen receives its transcript without needing
    // an update() hook of its own.
    void collect() {
        std::string spoken;
        if (!dictate::take(this, spoken)) return;
        if (!text.empty() && text.back() != ' ') text.push_back(' ');
        for (char c : spoken) {
            if (text.size() >= limit) break;
            text.push_back(c);
        }
    }

    void draw(M5Canvas& g, int x, int y, int w, const char* placeholder = "") {
        collect();
        ui::panel(g, x, y, w, 18);
        g.setFont(&fonts::Font2);
        g.setTextDatum(top_left);

        if (dictate::active(this)) {
            const bool listening =
                dictate::state() == dictate::State::Listening;
            // A live meter rather than a spinner: you need to know the mic is
            // hearing you, not merely that something is happening.
            if (listening) {
                const int bar = static_cast<int>((w - 8) * dictate::level());
                g.fillRect(x + 4, y + 14, bar < 2 ? 2 : bar, 2, theme::ACCENT2);
            }
            g.setTextColor(listening ? theme::ACCENT2 : theme::WARN,
                           theme::PANEL);
            g.drawString(listening ? "listening..." : "transcribing...", x + 4,
                         y + 1);
            return;
        }

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
        // The header takes the first body row, so lists show LIST_ROWS items, not ROWS_VISIBLE.
        if (sel >= first + theme::LIST_ROWS)
            first = sel - theme::LIST_ROWS + 1;
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
