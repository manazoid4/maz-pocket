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

// v0.03.1 Home is the six-surface MAZ Pocket dashboard: COMM, CAPTURE, OPS,
// DESK, RECALL and FLOW. Utilities remain in Ctrl+K rather than bloating Home.
constexpr int TABLE_Y      = BODY_Y + 27;
constexpr int TABLE_COLS   = 3;
constexpr int TABLE_ROWS   = 2;
constexpr int TABLE_CELL_W = SCREEN_W / TABLE_COLS;
constexpr int TABLE_CELL_H = 34;
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

constexpr uint32_t T_FAST  = 120;
constexpr uint32_t T_BOOT  = 900;
constexpr uint32_t T_TOAST = 1800;

}  // namespace theme
}  // namespace maz
