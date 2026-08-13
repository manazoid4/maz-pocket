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
constexpr int ROWS_VISIBLE = BODY_H / ROW_H;  // 6

// RGB565. MAZ identity: near-black slate, one hot amber accent, one cool cyan.
constexpr uint16_t BG      = 0x0861;  // #0B0D10 deep slate
constexpr uint16_t PANEL   = 0x18E3;  // #14181D raised surface
constexpr uint16_t LINE    = 0x2965;  // hairline
constexpr uint16_t TEXT    = 0xE71C;  // #E6E9EC
constexpr uint16_t DIM     = 0x7BCF;  // #7A848E secondary text
constexpr uint16_t ACCENT  = 0xFBC0;  // #FF7A18 MAZ amber
constexpr uint16_t ACCENT2 = 0x2679;  // #22D3EE cyan — live/streaming state
constexpr uint16_t OK      = 0x3E90;  // #3DDC84
constexpr uint16_t WARN    = 0xFD00;  // #FFA200
constexpr uint16_t ERR     = 0xF9A6;  // #FF4D4D

// Animation budget, milliseconds. Kept short: this is a device you pull out of
// a pocket to catch a thought, not a device you watch.
constexpr uint32_t T_FAST  = 120;
constexpr uint32_t T_BOOT  = 900;
constexpr uint32_t T_TOAST = 1800;

}  // namespace theme
}  // namespace maz
