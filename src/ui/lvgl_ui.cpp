#include "lvgl_ui.h"

#include <algorithm>
#include <string>

#include "theme.h"

namespace maz {
namespace lvui {

using namespace theme;

namespace {
bool active = false;

void drawContext(M5Canvas& g, const char* kind, const char* text) {
    constexpr int stripY = BODY_Y;
    constexpr int stripH = 22;

    g.fillRect(0, stripY, SCREEN_W, stripH, BG);
    g.setTextDatum(top_left);
    g.setFont(&fonts::Font0);
    g.setTextColor(ACCENT, BG);
    g.drawString(kind ? kind : "MAZ", 7, stripY + 5);

    g.setTextDatum(top_right);
    g.setTextColor(DIM, BG);
    std::string state = text ? text : "POCKET 0.3";
    // Font0 is six pixels wide. Leave enough room for the left MAZ label.
    constexpr size_t maxChars = 27;
    if (state.size() > maxChars) {
        state.resize(maxChars - 1);
        state += '~';
    }
    g.drawString(state.c_str(), SCREEN_W - 7, stripY + 5);
    g.setTextDatum(top_left);
    g.drawFastHLine(5, stripY + stripH - 1, SCREEN_W - 10, LINE);
}

void drawTile(M5Canvas& g, const Cell& cell, int index, bool selected) {
    const int x = index * TABLE_CELL_W + 4;
    const int y = TABLE_Y;
    const int w = TABLE_CELL_W - 8;
    const int h = TABLE_CELL_H;
    const uint16_t tileFill = selected ? ACCENT : PANEL;
    const uint16_t tileBorder = selected ? TEXT : LINE;
    const uint16_t titleColour = selected ? BG : TEXT;

    g.fillRoundRect(x, y, w, h, 5, tileFill);
    g.drawRoundRect(x, y, w, h, 5, tileBorder);

    const int boxW = 36;
    const int boxH = 34;
    const int boxX = x + (w - boxW) / 2;
    const int boxY = y + 8;
    g.fillRoundRect(boxX, boxY, boxW, boxH, 5, BG);
    g.drawRoundRect(boxX, boxY, boxW, boxH, 5, selected ? BG : DIM);

    char icon[2] = {cell.badge, '\0'};
    g.setFont(&fonts::Font4);
    g.setTextDatum(middle_center);
    g.setTextColor(ACCENT, BG);
    g.drawString(icon, boxX + boxW / 2, boxY + boxH / 2);

    g.setFont(&fonts::Font0);
    g.setTextDatum(top_center);
    g.setTextColor(titleColour, tileFill);
    std::string title = cell.title ? cell.title : "";
    constexpr size_t maxTitle = 10;
    if (title.size() > maxTitle) {
        title.resize(maxTitle - 1);
        title += '~';
    }
    g.drawString(title.c_str(), x + w / 2, y + 52);
    g.setTextDatum(top_left);
}
}  // namespace

// Kept behind the existing lvui API so Home and shell contracts do not change.
// v0.03 no longer needs the LVGL library: three launcher tiles are faster and
// substantially smaller when drawn directly into the shell's M5Canvas.
bool begin(M5Canvas&) { return true; }
void setActive(bool value) { active = value; }
void tick() {}

void renderHome(M5Canvas& canvas, const Cell* cellData, size_t count,
                int selected, const char* contextKind, const char* contextText,
                int, int) {
    if (!active) return;

    canvas.fillRect(0, BODY_Y, SCREEN_W, BODY_H, BG);
    drawContext(canvas, contextKind, contextText);

    const size_t visible = std::min(count, static_cast<size_t>(TABLE_PAGE));
    for (size_t i = 0; i < visible; ++i) {
        drawTile(canvas, cellData[i], static_cast<int>(i),
                 static_cast<int>(i) == selected);
    }
}

}  // namespace lvui
}  // namespace maz
