#include "lvgl_ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>

#include "theme.h"

namespace maz {
namespace lvui {
namespace {

using namespace theme;

constexpr size_t CELL_COUNT = TABLE_PAGE;  // 8
constexpr size_t MAX_DOTS   = 5;           // room to grow past today's 3 pages
constexpr int DRAW_ROWS = 14;  // slightly more than LVGL's recommended 1/10

// Context strip: a band, not a card. A card implies something you could tap,
// and there is nothing here to tap it with.
constexpr int STRIP_Y = BODY_Y;             // 15
constexpr int STRIP_H = 18;
constexpr int RULE_Y  = STRIP_Y + STRIP_H;  // 33, hairline under the strip

M5Canvas*     target  = nullptr;
lv_display_t* display = nullptr;
lv_obj_t*     screen  = nullptr;
lv_obj_t*     contextKindLabel = nullptr;
lv_obj_t*     contextTextLabel = nullptr;
lv_obj_t*     pageLabel = nullptr;
lv_obj_t*     dots[MAX_DOTS] = {};
lv_obj_t*     cells[CELL_COUNT] = {};
lv_obj_t*     cellBars[CELL_COUNT] = {};  // selection cue that is not a colour
lv_obj_t*     cellBadges[CELL_COUNT] = {};
lv_obj_t*     cellBadgeLabels[CELL_COUNT] = {};
lv_obj_t*     cellTitles[CELL_COUNT] = {};
alignas(LV_DRAW_BUF_ALIGN) uint16_t
    drawBuffer[theme::SCREEN_W * DRAW_ROWS];
bool     active = false;
uint32_t lastHandler = 0;

lv_color_t colour(uint32_t rgbHex) { return lv_color_hex(rgbHex); }
uint32_t tickMillis() { return static_cast<uint32_t>(millis()); }

void flush(lv_display_t* disp, const lv_area_t* area, uint8_t* pixels) {
    if (target) {
        const int width = area->x2 - area->x1 + 1;
        const int height = area->y2 - area->y1 + 1;
        const bool oldSwap = target->getSwapBytes();
        // LVGL supplies native RGB565; M5GFX's uint16_t image overload expects
        // that fact to be declared when copying into its swapped sprite.
        target->setSwapBytes(true);
        target->pushImage(area->x1, area->y1, width, height,
                          reinterpret_cast<uint16_t*>(pixels));
        target->setSwapBytes(oldSwap);
    }
    lv_display_flush_ready(disp);
}

void baseObject(lv_obj_t* obj, uint32_t background, int radius) {
    lv_obj_set_style_bg_color(obj, colour(background), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* rule(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* r = lv_obj_create(parent);
    baseObject(r, rgb::LINE, 0);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_size(r, w, h);
    return r;
}

// Cells are zebra-striped by row rather than boxed. Shared row rules and one
// aligned column rule are what make this read as a table; eight floating
// rounded boxes read as eight buttons.
bool zebra(size_t index) { return ((index / TABLE_COLS) % 2) == 0; }

void buildContextStrip() {
    lv_obj_t* strip = lv_obj_create(screen);
    baseObject(strip, rgb::PANEL, 0);
    lv_obj_set_pos(strip, 0, STRIP_Y);
    lv_obj_set_size(strip, SCREEN_W, STRIP_H);

    // The mark, at the radius the status bar already draws it, so the identity
    // is one shape repeated rather than two shapes that nearly match.
    lv_obj_t* ring = lv_obj_create(strip);
    lv_obj_set_pos(ring, 3, 4);
    lv_obj_set_size(ring, 11, 11);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ring, colour(rgb::ACCENT), 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(ring, 0, 0);

    lv_obj_t* core = lv_obj_create(strip);
    baseObject(core, rgb::ACCENT, LV_RADIUS_CIRCLE);
    lv_obj_set_pos(core, 6, 7);
    lv_obj_set_size(core, 5, 5);

    contextKindLabel = lv_label_create(strip);
    lv_obj_set_pos(contextKindLabel, 18, 5);
    lv_obj_set_style_text_color(contextKindLabel, colour(rgb::ACCENT), 0);
    lv_obj_set_style_text_font(contextKindLabel, &lv_font_montserrat_10, 0);

    // Bumped from montserrat_10: this is the one line of live state on the
    // screen, and 10 sits at the legibility floor for a 1.14" panel.
    contextTextLabel = lv_label_create(strip);
    lv_obj_set_pos(contextTextLabel, 54, 3);
    lv_obj_set_size(contextTextLabel, 130, 14);
    lv_label_set_long_mode(contextTextLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(contextTextLabel, colour(rgb::TEXT), 0);
    lv_obj_set_style_text_font(contextTextLabel, &lv_font_montserrat_12, 0);

    // Two redundant page channels: a number that survives a dimmed backlight,
    // and dots that read positionally without having to be parsed.
    pageLabel = lv_label_create(strip);
    lv_obj_set_pos(pageLabel, 186, 4);
    lv_obj_set_size(pageLabel, 22, 12);
    lv_obj_set_style_text_align(pageLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(pageLabel, colour(rgb::DIM), 0);
    lv_obj_set_style_text_font(pageLabel, &lv_font_montserrat_10, 0);

    for (size_t i = 0; i < MAX_DOTS; ++i) {
        dots[i] = lv_obj_create(strip);
        baseObject(dots[i], rgb::LINE, 1);
        lv_obj_set_pos(dots[i], 213 + static_cast<int>(i) * 6, 8);
        lv_obj_set_size(dots[i], 5, 3);
    }
}

void buildTable() {
    rule(screen, 0, RULE_Y, SCREEN_W, 1);

    for (size_t i = 0; i < CELL_COUNT; ++i) {
        const int column = static_cast<int>(i % TABLE_COLS);
        const int row    = static_cast<int>(i / TABLE_COLS);
        const int x      = column * TABLE_CELL_W;
        const int y      = TABLE_Y + row * TABLE_CELL_H;

        cells[i] = lv_obj_create(screen);
        baseObject(cells[i], zebra(i) ? rgb::PANEL : rgb::BG, 0);
        lv_obj_set_pos(cells[i], x, y);
        lv_obj_set_size(cells[i], TABLE_CELL_W, TABLE_CELL_H);

        // Shape, not hue. Survives the dimmed-backlight state, where the amber
        // fill and the panel fill converge.
        cellBars[i] = lv_obj_create(cells[i]);
        baseObject(cellBars[i], rgb::TEXT, 0);
        lv_obj_set_pos(cellBars[i], 0, 0);
        lv_obj_set_size(cellBars[i], 3, TABLE_CELL_H);
        lv_obj_add_flag(cellBars[i], LV_OBJ_FLAG_HIDDEN);

        cellBadges[i] = lv_obj_create(cells[i]);
        baseObject(cellBadges[i], rgb::LINE, 2);
        lv_obj_set_pos(cellBadges[i], 5, 4);
        lv_obj_set_size(cellBadges[i], 13, 13);

        cellBadgeLabels[i] = lv_label_create(cellBadges[i]);
        lv_obj_center(cellBadgeLabels[i]);
        lv_obj_set_style_text_font(cellBadgeLabels[i], &lv_font_montserrat_10,
                                   0);
        lv_obj_set_style_text_color(cellBadgeLabels[i], colour(rgb::DIM), 0);

        // montserrat_14 is the first size with a ~1mm cap height on this
        // panel. App names are the highest-value text on the device; they do
        // not belong in the chrome font.
        cellTitles[i] = lv_label_create(cells[i]);
        lv_obj_set_pos(cellTitles[i], 22, 3);
        lv_obj_set_size(cellTitles[i], TABLE_CELL_W - 28, 16);
        lv_label_set_long_mode(cellTitles[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(cellTitles[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(cellTitles[i], colour(rgb::TEXT), 0);

        // Row rule under every row but the last.
        if (column == 0 && row + 1 < TABLE_ROWS)
            rule(screen, 4, y + TABLE_CELL_H - 1, SCREEN_W - 8, 1);
    }

    // The single vertical rule that turns two lists into two columns.
    rule(screen, TABLE_CELL_W, TABLE_Y, 1, TABLE_ROWS * TABLE_CELL_H);
}

void buildHome() {
    screen = lv_screen_active();
    baseObject(screen, rgb::BG, 0);
    buildContextStrip();
    buildTable();
}

void styleCell(size_t index, bool selected) {
    lv_obj_set_style_bg_color(
        cells[index],
        colour(selected ? rgb::ACCENT : (zebra(index) ? rgb::PANEL : rgb::BG)),
        0);
    if (selected)
        lv_obj_remove_flag(cellBars[index], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(cellBars[index], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(cellBadges[index],
                              colour(selected ? rgb::BG : rgb::LINE), 0);
    lv_obj_set_style_text_color(cellBadgeLabels[index],
                                colour(selected ? rgb::ACCENT : rgb::DIM), 0);
    lv_obj_set_style_text_color(cellTitles[index],
                                colour(selected ? rgb::BG : rgb::TEXT), 0);
}

}  // namespace

bool begin(M5Canvas& canvas) {
    target = &canvas;
    lv_init();
    Serial.printf("[boot] lvgl core heap=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()));
    lv_tick_set_cb(tickMillis);

    display = lv_display_create(theme::SCREEN_W, theme::SCREEN_H);
    Serial.printf("[boot] lvgl display=%s heap=%u\n",
                  display ? "ok" : "failed",
                  static_cast<unsigned>(ESP.getFreeHeap()));
    if (!display) return false;
    // LV_COLOR_FORMAT_NATIVE is RGB565 at the configured 16-bit depth. Do not
    // re-apply the same format here: that API emits a display event before the
    // first draw buffer exists on this embedded target.
    lv_display_set_buffers(display, drawBuffer, nullptr, sizeof(drawBuffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.println("[boot] lvgl buffer ready");
    lv_display_set_flush_cb(display, flush);
    Serial.println("[boot] lvgl widgets");
    buildHome();
    Serial.printf("[boot] lvgl widgets ready heap=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()));
    return true;
}

void setActive(bool value) { active = value; }

void tick() {
    if (!active || !display) return;
    const uint32_t now = millis();
    if (now - lastHandler < 5) return;
    lastHandler = now;
    lv_timer_handler();
}

void renderHome(M5Canvas& canvas, const Cell* cellData, size_t count,
                int selected, const char* contextKind, const char* contextText,
                int page, int pageCount) {
    if (!display || !screen) return;
    target = &canvas;
    active = true;

    lv_label_set_text(contextKindLabel, contextKind ? contextKind : "MAZ");
    lv_label_set_text(contextTextLabel, contextText ? contextText : "POCKET");

    if (pageCount < 1) pageCount = 1;
    char counter[8];
    snprintf(counter, sizeof(counter), "%d/%d", page + 1, pageCount);
    lv_label_set_text(pageLabel, counter);

    const size_t shownDots = std::min<size_t>(pageCount, MAX_DOTS);
    for (size_t i = 0; i < MAX_DOTS; ++i) {
        if (i < shownDots) {
            lv_obj_remove_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(
                dots[i],
                colour(static_cast<int>(i) == page ? rgb::ACCENT : rgb::LINE),
                0);
        } else {
            lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    const size_t visible = std::min(count, CELL_COUNT);
    for (size_t i = 0; i < CELL_COUNT; ++i) {
        if (i >= visible) {
            lv_obj_add_flag(cells[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(cells[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(cellTitles[i],
                          cellData[i].title ? cellData[i].title : "");
        const char badge[2] = {cellData[i].badge, '\0'};
        lv_label_set_text(cellBadgeLabels[i], badge);
        styleCell(i, static_cast<int>(i) == selected);
    }

    lv_obj_invalidate(screen);
    lv_refr_now(display);
}

}  // namespace lvui
}  // namespace maz
