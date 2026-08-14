#include "lvgl_ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>

#include "theme.h"

namespace maz {
namespace lvui {
namespace {

constexpr size_t TILE_COUNT = 8;
constexpr int DRAW_ROWS = 14;  // slightly more than LVGL's recommended 1/10

M5Canvas*    target = nullptr;
lv_display_t* display = nullptr;
lv_obj_t*    screen = nullptr;
lv_obj_t*    contextKindLabel = nullptr;
lv_obj_t*    contextTextLabel = nullptr;
lv_obj_t*    connectionLabel = nullptr;
lv_obj_t*    tiles[TILE_COUNT] = {};
lv_obj_t*    tileLabels[TILE_COUNT] = {};
alignas(LV_DRAW_BUF_ALIGN) uint16_t
    drawBuffer[theme::SCREEN_W * DRAW_ROWS];
bool         active = false;
uint32_t     lastHandler = 0;

lv_color_t colour(uint32_t rgb) { return lv_color_hex(rgb); }
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

void buildHome() {
    screen = lv_screen_active();
    baseObject(screen, 0x0B0D10, 0);

    lv_obj_t* context = lv_obj_create(screen);
    baseObject(context, 0x14181D, 5);
    lv_obj_set_pos(context, 6, 17);
    lv_obj_set_size(context, 228, 30);

    lv_obj_t* ring = lv_obj_create(context);
    lv_obj_set_pos(ring, 6, 6);
    lv_obj_set_size(ring, 18, 18);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ring, colour(0xFF7A18), 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(ring, 0, 0);

    lv_obj_t* core = lv_obj_create(context);
    baseObject(core, 0xFF7A18, LV_RADIUS_CIRCLE);
    lv_obj_set_pos(core, 12, 12);
    lv_obj_set_size(core, 6, 6);

    contextKindLabel = lv_label_create(context);
    lv_obj_set_pos(contextKindLabel, 30, 3);
    lv_obj_set_style_text_color(contextKindLabel, colour(0xFF7A18), 0);
    lv_obj_set_style_text_font(contextKindLabel, &lv_font_montserrat_10, 0);

    contextTextLabel = lv_label_create(context);
    lv_obj_set_pos(contextTextLabel, 30, 14);
    lv_obj_set_size(contextTextLabel, 142, 13);
    lv_label_set_long_mode(contextTextLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(contextTextLabel, colour(0xE6E9EC), 0);
    lv_obj_set_style_text_font(contextTextLabel, &lv_font_montserrat_10, 0);

    connectionLabel = lv_label_create(context);
    lv_obj_set_pos(connectionLabel, 174, 9);
    lv_obj_set_size(connectionLabel, 48, 12);
    lv_obj_set_style_text_align(connectionLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(connectionLabel, &lv_font_montserrat_10, 0);

    constexpr int tileWidth = 112;
    constexpr int tileHeight = 16;
    for (size_t i = 0; i < TILE_COUNT; ++i) {
        const int column = i % 2;
        const int row = i / 2;
        tiles[i] = lv_obj_create(screen);
        baseObject(tiles[i], 0x14181D, 4);
        lv_obj_set_pos(tiles[i], 6 + column * 116, 49 + row * 18);
        lv_obj_set_size(tiles[i], tileWidth, tileHeight);
        lv_obj_set_style_border_color(tiles[i], colour(0x293038), 0);
        lv_obj_set_style_border_width(tiles[i], 1, 0);

        tileLabels[i] = lv_label_create(tiles[i]);
        lv_obj_center(tileLabels[i]);
        lv_obj_set_style_text_font(tileLabels[i], &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(tileLabels[i], colour(0xE6E9EC), 0);
    }
}

void styleTile(size_t index, bool selected) {
    lv_obj_set_style_bg_color(tiles[index],
                              colour(selected ? 0xFF7A18 : 0x14181D), 0);
    lv_obj_set_style_border_color(tiles[index],
                                  colour(selected ? 0xFFB36D : 0x293038), 0);
    lv_obj_set_style_border_width(tiles[index], selected ? 2 : 1, 0);
    lv_obj_set_style_text_color(tileLabels[index],
                                colour(selected ? 0x0B0D10 : 0xE6E9EC), 0);
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
    Serial.println("[boot] lvgl widgets ready");
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

void renderHome(M5Canvas& canvas, const char* const* titles, size_t count,
                int selected, const char* contextKind,
                const char* contextText, const char* connectionState,
                bool online) {
    if (!display || !screen) return;
    target = &canvas;
    active = true;

    lv_label_set_text(contextKindLabel, contextKind ? contextKind : "MAZ");
    lv_label_set_text(contextTextLabel, contextText ? contextText : "POCKET");
    lv_label_set_text(connectionLabel,
                      connectionState ? connectionState : "OFFLINE");
    lv_obj_set_style_text_color(connectionLabel,
                                colour(online ? 0x3DDC84 : 0xFFA200), 0);

    const size_t visible = std::min(count, TILE_COUNT);
    for (size_t i = 0; i < TILE_COUNT; ++i) {
        const bool shown = i < visible;
        if (shown) {
            lv_obj_remove_flag(tiles[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(tileLabels[i], titles[i]);
            styleTile(i, static_cast<int>(i) == selected);
        } else {
            lv_obj_add_flag(tiles[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_invalidate(screen);
    lv_refr_now(display);
}

}  // namespace lvui
}  // namespace maz
