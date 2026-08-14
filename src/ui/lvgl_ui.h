// LVGL-backed MAZ surfaces.
//
// LVGL owns widget layout and styling. The existing shell remains the single
// owner of the physical framebuffer and keyboard/navigation contracts.
#pragma once

#include <M5Unified.h>

#include <stddef.h>

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

// `page`/`pageCount` drive the counter and dots. `selected` indexes `cells`,
// not the whole app list. Connection state is deliberately absent: the status
// bar already shows it, one row above.
void renderHome(M5Canvas& canvas, const Cell* cells, size_t count, int selected,
                const char* contextKind, const char* contextText, int page,
                int pageCount);

}  // namespace lvui
}  // namespace maz
