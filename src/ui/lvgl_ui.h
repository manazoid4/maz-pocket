// LVGL-backed MAZ surfaces.
//
// LVGL owns widget layout and styling. The existing shell remains the single
// owner of the physical framebuffer and keyboard/navigation contracts.
#pragma once

#include <M5Unified.h>

#include <stddef.h>

#include "ui.h"

namespace maz {
namespace lvui {

// One cell of Home's app table. `badge` is the letter that opens the app from
// Home, or a '1'..'8' digit when the app has no letter of its own — either
// way the cell says how to reach it without needing a second screen.
struct Cell {
    const char* title = "";
    char        badge = ' ';
};

bool begin(M5Canvas& canvas);
void setActive(bool active);
void tick();

// Home's status area: `line` is the one big status line, `sentence` (optional)
// is a plain sentence saying what is wrong and what to press, `version` is the
// small firmware text. `selected` indexes `cells`.
struct Status {
    ui::Phase   phase    = ui::Phase::Ready;
    const char* line     = "READY";
    uint16_t    colour   = 0xFFFF;
    const char* sentence = nullptr;
    const char* version  = "";
};

void renderHome(M5Canvas& canvas, const Cell* cells, size_t count, int selected,
                const Status& status);

}  // namespace lvui
}  // namespace maz
