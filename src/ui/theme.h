// MAZ Pocket — the single source of truth for how the device looks.
// Every screen pulls spacing, colour and type from here so the firmware reads
// as one designed object rather than a pile of LCD prints.
#pragma once
#include <stdint.h>

namespace maz {
namespace theme {

// 240x135 landscape. Everything below is in real pixels — the panel is small
// enough that a scaling system would cost more than it earns.
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;

constexpr int STATUS_H = 15;                  // top chrome
constexpr int HINT_H   = 14;                  // bottom key hints
constexpr int BODY_Y   = STATUS_H;
constexpr int BODY_H   = SCREEN_H - STATUS_H - HINT_H;
constexpr int PAD      = 6;
constexpr int ROW_H    = 17;                  // list row
constexpr int ROWS_VISIBLE = BODY_H / ROW_H;  // 6 rows of body space...
// ...but every list screen spends row 0 on its header, so this is the number
// of rows a list may actually draw. Was rediscovered as `ROWS_VISIBLE - 1` in
// each app; named once here instead.
constexpr int LIST_ROWS = ROWS_VISIBLE - 1;   // 5

// Home's paged app table. Four rows of two, sized so a montserrat_14 label
// clears the 85px body with room to breathe.
constexpr int TABLE_Y      = BODY_Y + 19;              // below the context strip
constexpr int TABLE_COLS   = 2;
constexpr int TABLE_ROWS   = 4;
constexpr int TABLE_CELL_W = SCREEN_W / TABLE_COLS;    // 120
constexpr int TABLE_CELL_H = 21;
constexpr int TABLE_PAGE   = TABLE_COLS * TABLE_ROWS;  // 8 apps per page

// RGB565. MAZ identity: near-black slate, one hot amber accent, one cool cyan.
// These encodings are exact. The previous values drifted up to a whole hue
// away from the hex in their own comments, which is why LVGL (fed real
// 0xRRGGBB) and M5Canvas (fed these) rendered two different "panel" greys side
// by side on Home.
constexpr uint16_t BG      = 0x0862;  // #0B0D10 deep slate
constexpr uint16_t PANEL   = 0x10A3;  // #14181D raised surface
constexpr uint16_t LINE    = 0x2124;  // #202428 hairline
constexpr uint16_t TEXT    = 0xE73D;  // #E6E9EC
constexpr uint16_t DIM     = 0x9D35;  // #9AA4AE secondary text — 6.8:1 on PANEL
constexpr uint16_t HINT    = 0xC659;  // #C3CAD1 hint-bar text — 10.4:1 on PANEL
constexpr uint16_t ACCENT  = 0xFBC0;  // #FF7A18 MAZ amber
constexpr uint16_t ACCENT2 = 0x269D;  // #22D3EE cyan — live/streaming state
constexpr uint16_t OK      = 0x3E90;  // #3DDC84
constexpr uint16_t WARN    = 0xFD00;  // #FFA200
constexpr uint16_t ERR     = 0xFA4A;  // #FF4D50 — 5.6:1 on PANEL

// The same palette as 0xRRGGBB for the LVGL side, which takes real hex rather
// than RGB565. One source of truth: these must stay in step with the above.
namespace rgb {
constexpr uint32_t BG     = 0x0B0D10;
constexpr uint32_t PANEL  = 0x14181D;
constexpr uint32_t LINE   = 0x202428;
constexpr uint32_t TEXT   = 0xE6E9EC;
constexpr uint32_t DIM    = 0x9AA4AE;
constexpr uint32_t ACCENT = 0xFF7A18;
constexpr uint32_t OK     = 0x3DDC84;
constexpr uint32_t WARN   = 0xFFA200;
}  // namespace rgb

// Animation budget, milliseconds. Kept short: this is a device you pull out of
// a pocket to catch a thought, not a device you watch.
constexpr uint32_t T_FAST  = 120;
constexpr uint32_t T_BOOT  = 900;
constexpr uint32_t T_TOAST = 1800;

}  // namespace theme
}  // namespace maz
