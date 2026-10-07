// MAZ Pocket — the single source of truth for how the device looks.
#pragma once
#include <stdint.h>

namespace maz {
namespace theme {

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;

constexpr int STATUS_H = 15;
constexpr int HINT_H   = 14;
constexpr int BODY_Y   = STATUS_H;
constexpr int BODY_H   = SCREEN_H - STATUS_H - HINT_H;
constexpr int PAD      = 6;
constexpr int ROW_H    = 17;
constexpr int ROWS_VISIBLE = BODY_H / ROW_H;
constexpr int LIST_ROWS = ROWS_VISIBLE - 1;

// Home shows at most four actions (TABLE_PAGE): CALL, CAPTURE, AGENTS,
// CONTROL. Everything else lives in Ctrl+K instead of competing on Home.
constexpr int HOME_STATUS_H = 36;  // big status line + one small line
constexpr int TABLE_Y      = BODY_Y + HOME_STATUS_H + 2;
constexpr int TABLE_COLS   = 2;
constexpr int TABLE_ROWS   = 2;
constexpr int TABLE_CELL_W = SCREEN_W / TABLE_COLS;
constexpr int TABLE_CELL_H = 30;
constexpr int TABLE_PAGE   = TABLE_COLS * TABLE_ROWS;

// RGB565 — dark terminal body with phosphor-like amber/cyan state accents.
constexpr uint16_t BG      = 0x0862;  // #0B0D10
constexpr uint16_t PANEL   = 0x10A3;  // #14181D
constexpr uint16_t LINE    = 0x2124;  // #202428
constexpr uint16_t TEXT    = 0xE73D;  // #E6E9EC
constexpr uint16_t DIM     = 0x9D35;  // #9AA4AE
constexpr uint16_t HINT    = 0xC659;  // #C3CAD1
constexpr uint16_t ACCENT  = 0xFBC0;  // #FF7A18
constexpr uint16_t ACCENT2 = 0x269D;  // #22D3EE
constexpr uint16_t OK      = 0x3E90;  // #3DDC84
constexpr uint16_t WARN    = 0xFD00;  // #FFA200
constexpr uint16_t ERR     = 0xFA4A;  // #FF4D50

namespace rgb {
constexpr uint32_t BG     = 0x0B0D10;
constexpr uint32_t PANEL  = 0x14181D;
constexpr uint32_t LINE   = 0x202428;
constexpr uint32_t TEXT   = 0xE6E9EC;
constexpr uint32_t DIM    = 0x9AA4AE;
constexpr uint32_t ACCENT = 0xFF7A18;
constexpr uint32_t ACCENT2 = 0x22D3EE;
constexpr uint32_t OK     = 0x3DDC84;
constexpr uint32_t WARN   = 0xFFA200;
constexpr uint32_t ERR    = 0xFF4D50;
}  // namespace rgb

// Calm UI tokens (see docs/UI-GUIDE.md). Fonts: state word Font4 scaled,
// sub lines Font2, body and chrome Font0 (6x8 px per character).
constexpr int   HINT_CHARS        = 33;    // fits beside the <ESC chip
constexpr int   TOAST_TITLE_CHARS = 31;    // Font2 inside the toast box
constexpr int   TOAST_TEXT_CHARS  = 36;    // Font0 detail line
constexpr float STATE_SCALE       = 1.5f;  // Font4 multiplier for state words
constexpr int   STATE_Y           = BODY_Y + 2;
constexpr int   REASON_Y          = BODY_Y + 42;
constexpr int   REPLY_Y           = BODY_Y + 60;
constexpr int   REPLY_LINE_H      = 11;
constexpr int   REPLY_LINES       = 4;
constexpr int   REPLY_CHARS       = 38;
// Status bar columns, left to right, so nothing overlaps at 240 px.
constexpr int   SB_UPD_X   = 50;   // "UPD" badge, 18 px
constexpr int   SB_ACT_X   = 74;   // recording / focus timer, up to 57 px
constexpr int   SB_BUDDY_X = 142;  // Claude Code light
constexpr int   SB_CORE_X  = 152;  // Core dot
constexpr int   SB_WIFI_X  = 160;  // "WiFi", 24 px
constexpr int   SB_PCT_R   = 213;  // battery % right edge, up to 24 px

constexpr uint32_t T_FAST  = 120;
// A boot logo should brand the device, not hold it hostage. Hardware and LVGL
// init still happen normally; this only removes 650 ms of intentional waiting.
constexpr uint32_t T_BOOT  = 250;
constexpr uint32_t T_TOAST = 1800;

}  // namespace theme
}  // namespace maz
