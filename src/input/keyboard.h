// MAZ Pocket — Cardputer ADV keyboard (TI TCA8418 I2C key-matrix expander).
//
// Why this exists: the original Cardputer scans a shift-register matrix on
// dedicated GPIOs and M5Cardputer's Keyboard class drives that. The ADV moved
// the keyboard behind a TCA8418 at I2C 0x34 with an INT line on GPIO11, which
// is why pre-ADV firmware boots on an ADV with a completely dead keyboard.
//
// Matrix/remap constants verified against M5Stack's own ADV HAL
// (M5Cardputer-UserDemo @ CardputerADV, MIT licensed).
#pragma once
#include <stdint.h>
#include "../core/keys.h"

namespace maz {

class Keyboard {
public:
    bool begin();
    void update();  // drain the TCA8418 FIFO into our event queue

    // Pop one event. Returns false when the queue is empty.
    bool pop(KeyEvent& out);

    uint8_t mods() const { return _mods; }
    bool    held(uint8_t hidCode) const;
    bool    anyHeld() const { return _heldCount > 0; }

    // Milliseconds the key has been continuously held, 0 if not held.
    uint32_t heldFor(uint8_t hidCode) const;

    bool ok() const { return _ok; }

private:
    static constexpr uint8_t I2C_ADDR  = 0x34;
    static constexpr int     INT_PIN   = 11;
    static constexpr uint8_t ROWS      = 7;  // TCA8418 config used by the ADV
    static constexpr uint8_t COLS      = 8;
    static constexpr uint8_t QUEUE_LEN = 16;
    static constexpr uint8_t HELD_MAX  = 8;

    struct HeldKey {
        uint8_t  code  = KEY_NONE;
        uint32_t since = 0;
    };

    uint8_t reg(uint8_t r) const;
    void    reg(uint8_t r, uint8_t v) const;
    void    push(const KeyEvent& e);
    void    decode(uint8_t rawEvent);

    KeyEvent _queue[QUEUE_LEN];
    uint8_t  _qHead = 0, _qTail = 0;

    HeldKey _held[HELD_MAX];
    uint8_t _heldCount = 0;

    uint8_t _mods = MOD_NONE;
    bool    _fn   = false;
    bool    _ok   = false;
};

extern Keyboard KB;

}  // namespace maz
