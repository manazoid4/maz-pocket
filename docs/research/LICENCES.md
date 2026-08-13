# Licence review

MAZ Pocket is MIT. Every external input was checked before use, and nothing in
this repository is a copy of another firmware's source or assets.

## Dependencies actually linked

| Dependency | Version | Licence | Compatible with MIT distribution |
|---|---|---|---|
| M5Unified | 0.2.19 (pinned) | MIT | Yes |
| M5GFX | 0.2.26 (pinned) | MIT | Yes |
| ricmoo/QRCode | 0.0.1 | MIT | Yes |
| ArduinoJson | 7.2.1 | MIT | Yes |
| Arduino-ESP32 / ESP-IDF | platform espressif32 6.9.0 | Apache-2.0 / LGPL components | Yes, standard for ESP32 firmware |

## Projects read for reference, and what was taken

| Project | Licence | What we used | What we did **not** use |
|---|---|---|---|
| **M5Cardputer-UserDemo** (CardputerADV branch) | MIT | Hardware facts only: TCA8418 configuration (`matrix(7,8)`), INT on G11, the row/col remap arithmetic, and the physical 4x14 key legend. These describe the hardware, and the licence permits reuse regardless. | No source files, no assets, no UI code, no Mooncake framework |
| **M5Unified / M5GFX** | MIT | Linked as libraries, unmodified | — |
| **Bruce** | **AGPL-3.0** | **Nothing.** Studied only as a UX reference — information density, icon-led navigation, status bar conventions. | Any source, any branding. AGPL would force MAZ Pocket to be AGPL; not copying is what keeps MIT honest. |
| **Nemo (m5stick-nemo)** | GPL-3.0 | Nothing. Read for feature scope only. | Any source |
| **bmorcelli/Launcher (M5Launcher)** | GPL-3.0 | Nothing. Read its wiki for the binary/partition requirements. | Any source |
| **AuraFriday/terminal_mcp** | Repository `LICENSE` says MPL-2.0 while README says Apache-2.0 | Product lesson only: opt-in serial sessions should auto-reconnect by USB identity and retain bounded boot logs. MAZ Host's narrow monitor was written from scratch. | No source copied; no multi-protocol server vendored, especially while upstream licence metadata conflicts. |
| **NucleoOs** (indecenti) | The clone timed out and no code was read, so nothing could be or was taken. Listed for honesty rather than credit. | — | — |
| **TCA8418 datasheet (TI SCPS239)** | Vendor documentation | Register map and semantics | — |

## Assets

All visual identity in MAZ Pocket is drawn programmatically at runtime — the
MAZ mark, the wordmark, app tiles, status glyphs, the listening ring, the boot
animation. There are no imported bitmaps, icon sets, or fonts beyond those
bundled in M5GFX (MIT). This was deliberate: it removes the entire class of
asset-licence questions, and it keeps the mark crisp at any size.

## Attribution stated in the firmware

The Help screen names Bruce and Nemo as separately available through
M5Launcher, which is accurate and non-competitive; MAZ Pocket does not claim,
bundle, or replace them.
