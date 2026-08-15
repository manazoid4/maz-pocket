#pragma once

#include <M5Unified.h>
#include <stddef.h>

namespace maz {
namespace home_grid {

struct Cell {
    const char* title = "";
    char badge = ' ';
};

void render(M5Canvas& canvas, const Cell* cells, size_t count, int selected,
            const char* contextKind, const char* contextText);

}  // namespace home_grid
}  // namespace maz
