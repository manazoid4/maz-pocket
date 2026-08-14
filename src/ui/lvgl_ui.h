// LVGL-backed MAZ surfaces.
//
// LVGL owns widget layout and styling. The existing shell remains the single
// owner of the physical framebuffer and keyboard/navigation contracts.
#pragma once

#include <M5Unified.h>

#include <stddef.h>

namespace maz {
namespace lvui {

bool begin(M5Canvas& canvas);
void setActive(bool active);
void tick();

void renderHome(M5Canvas& canvas, const char* const* titles, size_t count,
                int selected, const char* contextKind,
                const char* contextText, const char* connectionState,
                bool online);

}  // namespace lvui
}  // namespace maz
