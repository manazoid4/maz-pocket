# Cardputer ADV hardware notes

Everything below was verified against source or official documentation before
being used. Where the datasheet page and the vendor's own code disagree, the
code wins and the disagreement is recorded.

## Core

| Item | Value | Source |
|---|---|---|
| Module | M5Stamp S3A, ESP32-S3FN8 | M5 docs, Cardputer-Adv |
| Flash | 8MB | M5 docs |
| **PSRAM** | **none** | ESP32-S3FN8 part; confirmed in community threads |
| Display | ST7789V2, 240x135 | M5 docs |
| Keyboard | 56 keys (4x14) behind a **TCA8418** I2C expander | M5 docs |
| Audio codec | **ES8311** + NS4150B amp, 1W speaker, MEMS mic (SNR 65dB) | M5 docs |
| IMU | BMI270 | M5 docs |
| Battery | 1750mAh single cell | M5 docs |

No PSRAM is the constraint that shapes the whole firmware: 512KB SRAM total, so
audio is streamed to storage block-by-block and never buffered whole, and the
framebuffer sprite (240x135x2 = 64.8KB) is a deliberate, measured spend.

## Pin map used by MAZ Pocket

| Function | Pin | Source |
|---|---|---|
| I2C SDA / SCL | G8 / G9 | M5 docs; M5Unified ADV table |
| Keyboard INT | G11 | M5Cardputer-UserDemo `hal_config.h` (ADV branch) |
| TCA8418 address | 0x34 | UserDemo `Adafruit_TCA8418.h` |
| I2S SCLK / ASDOUT / LRCK / DSDIN | G41 / G46 / G43 / G42 | M5 docs |
| ES8311 I2C address | 0x18 | M5Unified `M5Unified.cpp` |
| IR emitter | G44 | M5 docs, UserDemo |
| Battery ADC | G10 | M5 docs |
| RGB LED | G21 | M5Unified ADV table |
| SD SCLK / MOSI / MISO | G40 / G14 / G39 | M5 docs and M5Unified agree |
| **SD CS** | **G12** | **Conflict:** the M5 docs page says G5. M5Unified's ADV pin table and M5Stack's own ADV UserDemo `hal_config.h` both say G12. MAZ Pocket uses G12. |

## The keyboard is the ADV trap

The original Cardputer scans a shift-register matrix on dedicated GPIOs, and
that is what `M5Cardputer`'s `Keyboard` class drives. The ADV moved the keyboard
behind a TCA8418. **Pre-ADV firmware therefore boots on an ADV with a completely
dead keyboard** — the single most reported ADV problem in the community threads,
and the reason MAZ Pocket ships its own driver (`src/input/keyboard.cpp`)
instead of using `M5Cardputer`.

Specifics that matter:

- Configure as `matrix(7, 8)` — the expander's own space, not the 4x14 legend.
- Key events arrive as a FIFO byte: bit 7 = press/release, bits 0-6 = code.
- `row = (code-1)/10`, `col = (code-1)%10`, then remap into the 4x14 legend:
  `col' = row*2 + (col > 3)`, `row' = (col + 4) % 4`.
- INT (G11) is active-low and stays asserted while the FIFO is non-empty, so
  polling the line each frame is sufficient and avoids ISR/app-switch races.

## Library support

`M5Unified` 0.2.19 (2026-07-22) is the first registry release carrying
`board_t::board_M5CardputerADV`, including the ES8311 speaker and microphone
enable callbacks and the ADV SD/I2C pin tables. `M5GFX` 0.2.26 is its matching
display release. Both are pinned exactly in `platformio.ini` — floating these
would silently break audio on a version bump.

`M5Unified` does **not** provide the ADV keyboard. That gap is ours to fill.

## Audio path

One ES8311 codec serves both directions, so the microphone and speaker cannot be
live simultaneously. `voice.cpp` explicitly ends one before beginning the other.
Recording format is 16kHz mono 16-bit PCM: the format speech models want, and
small enough to stream to SD (about 1.9MB per minute) without dropping frames.
