#include "lvgl_ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>

#include "theme.h"

namespace maz {
namespace lvui {
namespace {

using namespace theme;

constexpr size_t CELL_COUNT = TABLE_PAGE;
constexpr int DRAW_ROWS = 14;
constexpr int STRIP_Y = BODY_Y;
constexpr int STRIP_H = 22;

M5Canvas* target = nullptr;
lv_display_t* display = nullptr;
lv_obj_t* screen = nullptr;
lv_obj_t* contextKindLabel = nullptr;
lv_obj_t* contextTextLabel = nullptr;
lv_obj_t* cells[CELL_COUNT] = {};
lv_obj_t* iconBoxes[CELL_COUNT] = {};
lv_obj_t* iconLabels[CELL_COUNT] = {};
lv_obj_t* titleLabels[CELL_COUNT] = {};
alignas(LV_DRAW_BUF_ALIGN) uint16_t drawBuffer[SCREEN_W * DRAW_ROWS];
bool active = false;
uint32_t lastHandler = 0;

lv_color_t colour(uint32_t rgbHex) { return lv_color_hex(rgbHex); }
uint32_t tickMillis() { return static_cast<uint32_t>(millis()); }

void flush(lv_display_t* disp, const lv_area_t* area, uint8_t* pixels) {
    if (target) {
        const int width = area->x2 - area->x1 + 1;
        const int height = area->y2 - area->y1 + 1;
        const bool oldSwap = target->getSwapBytes();
        target->setSwapBytes(true);
        target->pushImage(area->x1, area->y1, width, height,
                          reinterpret_cast<uint16_t*>(pixels));
        target->setSwapBytes(oldSwap);
    }
    lv_display_flush_ready(disp);
}

void base(lv_obj_t* obj, uint32_t background, int radius = 0) {
    lv_obj_set_style_bg_color(obj, colour(background), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void buildContext() {
    lv_obj_t* strip = lv_obj_create(screen);
    base(strip, rgb::BG);
    lv_obj_set_pos(strip, 0, STRIP_Y);
    lv_obj_set_size(strip, SCREEN_W, STRIP_H);

    contextKindLabel = lv_label_create(strip);
    lv_obj_set_pos(contextKindLabel, 7, 4);
    lv_obj_set_style_text_color(contextKindLabel, colour(rgb::ACCENT), 0);
    lv_obj_set_style_text_font(contextKindLabel, &lv_font_montserrat_10, 0);

    contextTextLabel = lv_label_create(strip);
    lv_obj_set_pos(contextTextLabel, 58, 2);
    lv_obj_set_size(contextTextLabel, 174, 18);
    lv_label_set_long_mode(contextTextLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(contextTextLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(contextTextLabel, colour(rgb::DIM), 0);
    lv_obj_set_style_text_font(contextTextLabel, &lv_font_montserrat_12, 0);

    lv_obj_t* line = lv_obj_create(strip);
    base(line, rgb::LINE);
    lv_obj_set_pos(line, 5, STRIP_H - 1);
    lv_obj_set_size(line, SCREEN_W - 10, 1);
}

void buildTiles() {
    for (size_t i = 0; i < CELL_COUNT; ++i) {
        const int x = static_cast<int>(i) * TABLE_CELL_W + 4;
        cells[i] = lv_obj_create(screen);
        base(cells[i], rgb::PANEL, 5);
        lv_obj_set_pos(cells[i], x, TABLE_Y);
        lv_obj_set_size(cells[i], TABLE_CELL_W - 8, TABLE_CELL_H);
        lv_obj_set_style_border_width(cells[i], 1, 0);
        lv_obj_set_style_border_color(cells[i], colour(rgb::LINE), 0);

        iconBoxes[i] = lv_obj_create(cells[i]);
        base(iconBoxes[i], rgb::BG, 5);
        lv_obj_set_pos(iconBoxes[i], (TABLE_CELL_W - 8 - 36) / 2, 8);
        lv_obj_set_size(iconBoxes[i], 36, 34);
        lv_obj_set_style_border_width(iconBoxes[i], 1, 0);
        lv_obj_set_style_border_color(iconBoxes[i], colour(rgb::DIM), 0);

        iconLabels[i] = lv_label_create(iconBoxes[i]);
        lv_obj_center(iconLabels[i]);
        lv_obj_set_style_text_font(iconLabels[i], &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(iconLabels[i], colour(rgb::ACCENT), 0);

        titleLabels[i] = lv_label_create(cells[i]);
        lv_obj_set_pos(titleLabels[i], 2, 48);
        lv_obj_set_size(titleLabels[i], TABLE_CELL_W - 12, 17);
        lv_obj_set_style_text_align(titleLabels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(titleLabels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(titleLabels[i], colour(rgb::TEXT), 0);
        lv_label_set_long_mode(titleLabels[i], LV_LABEL_LONG_DOT);
    }
}

void buildHome() {
    screen = lv_screen_active();
    base(screen, rgb::BG);
    buildContext();
    buildTiles();
}

void styleTile(size_t index, bool selected) {
    lv_obj_set_style_bg_color(cells[index],
                              colour(selected ? rgb::ACCENT : rgb::PANEL), 0);
    lv_obj_set_style_border_color(cells[index],
                                  colour(selected ? rgb::TEXT : rgb::LINE), 0);
    lv_obj_set_style_bg_color(iconBoxes[index],
                              colour(selected ? rgb::BG : rgb::BG), 0);
    lv_obj_set_style_border_color(iconBoxes[index],
                                  colour(selected ? rgb::BG : rgb::DIM), 0);
    lv_obj_set_style_text_color(iconLabels[index],
                                colour(selected ? rgb::ACCENT : rgb::ACCENT), 0);
    lv_obj_set_style_text_color(titleLabels[index],
                                colour(selected ? rgb::BG : rgb::TEXT), 0);
}

}  // namespace

bool begin(M5Canvas& canvas) {
    target = &canvas;
    lv_init();
    lv_tick_set_cb(tickMillis);
    display = lv_display_create(SCREEN_W, SCREEN_H);
    if (!display) return false;
    lv_display_set_buffers(display, drawBuffer, nullptr, sizeof(drawBuffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    buildHome();
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
                int, int) {
    if (!display || !screen) return;
    target = &canvas;
    active = true;

    lv_label_set_text(contextKindLabel, contextKind ? contextKind : "MAZ");
    lv_label_set_text(contextTextLabel, contextText ? contextText : "POCKET 0.3");

    const size_t visible = std::min(count, CELL_COUNT);
    for (size_t i = 0; i < CELL_COUNT; ++i) {
        if (i >= visible) {
            lv_obj_add_flag(cells[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(cells[i], LV_OBJ_FLAG_HIDDEN);
        const char icon[2] = {cellData[i].badge, '\0'};
        lv_label_set_text(iconLabels[i], icon);
        lv_label_set_text(titleLabels[i], cellData[i].title ? cellData[i].title : "");
        styleTile(i, static_cast<int>(i) == selected);
    }
    lv_obj_invalidate(screen);
    lv_refr_now(display);
}

}  // namespace lvui
}  // namespace maz
