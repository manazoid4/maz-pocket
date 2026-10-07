#include "lvgl_ui.h"

#include <algorithm>
#include <string>

#include "theme.h"

namespace maz {
namespace lvui {

using namespace theme;

namespace {
bool active = false;

// One big status line, one small line under it, small version at the right.
void drawStatus(M5Canvas& g, const Status& st) {
    g.fillRect(0, BODY_Y, SCREEN_W, HOME_STATUS_H, BG);
    g.setTextDatum(top_left);
    g.setFont(&fonts::Font4);
    // Long agent/outbox phrases drop to the smaller font instead of clipping.
    if (g.textWidth(st.line) > SCREEN_W - PAD * 2) g.setFont(&fonts::Font2);
    g.setTextColor(st.colour, BG);
    g.drawString(st.line, PAD, BODY_Y + 3);

    g.setFont(&fonts::Font0);
    g.setTextColor(DIM, BG);
    if (st.sentence) g.drawString(st.sentence, PAD, BODY_Y + 28);
    g.setTextDatum(top_right);
    g.drawString(st.version, SCREEN_W - PAD, BODY_Y + 28);
    g.setTextDatum(top_left);
}

void drawTile(M5Canvas& g, const Cell& cell, int index, bool selected) {
    const int col = index % TABLE_COLS;
    const int row = index / TABLE_COLS;
    const int x = col * TABLE_CELL_W + 4;
    const int y = TABLE_Y + row * (TABLE_CELL_H + 4);
    const int w = TABLE_CELL_W - 8;
    const int h = TABLE_CELL_H;
    const uint16_t tileFill = selected ? ACCENT : PANEL;
    const uint16_t tileBorder = selected ? TEXT : LINE;
    const uint16_t titleColour = selected ? BG : TEXT;

    g.fillRoundRect(x, y, w, h, 4, tileFill);
    g.drawRoundRect(x, y, w, h, 4, tileBorder);

    const int boxW = 22;
    const int boxH = 18;
    const int boxX = x + 6;
    const int boxY = y + 5;
    g.fillRoundRect(boxX, boxY, boxW, boxH, 3, BG);
    g.drawRoundRect(boxX, boxY, boxW, boxH, 3, selected ? BG : DIM);

    char icon[2] = {cell.badge, '\0'};
    g.setFont(&fonts::Font2);
    g.setTextDatum(middle_center);
    g.setTextColor(ACCENT, BG);
    g.drawString(icon, boxX + boxW / 2, boxY + boxH / 2);

    g.setFont(&fonts::Font0);
    g.setTextDatum(middle_left);
    g.setTextColor(titleColour, tileFill);
    std::string title = cell.title ? cell.title : "";
    constexpr size_t maxTitle = 10;
    if (title.size() > maxTitle) {
        title.resize(maxTitle - 1);
        title += '~';
    }
    g.drawString(title.c_str(), boxX + boxW + 5, y + h / 2);
    g.setTextDatum(top_left);
}
}  // namespace

// Kept behind the existing lvui API so Home and shell contracts do not change.
// Despite the legacy namespace name, this path is direct M5Canvas and carries
// no LVGL dependency.
bool begin(M5Canvas&) { return true; }
void setActive(bool value) { active = value; }
void tick() {}

void renderHome(M5Canvas& canvas, const Cell* cellData, size_t count,
                int selected, const Status& status) {
    if (!active) return;

    canvas.fillRect(0, BODY_Y, SCREEN_W, BODY_H, BG);
    drawStatus(canvas, status);

    const size_t visible = std::min(count, static_cast<size_t>(TABLE_PAGE));
    for (size_t i = 0; i < visible; ++i) {
        drawTile(canvas, cellData[i], static_cast<int>(i),
                 static_cast<int>(i) == selected);
    }
}

}  // namespace lvui
}  // namespace maz
