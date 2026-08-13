# Verification status — MAZ Pocket v0.1

Written 2026-08-13. **No Cardputer ADV was attached during this build**, so the
honest summary is: the firmware compiles clean and the hardware facts were taken
from vendor source rather than guessed, but nothing has been observed running.
Everything below is labelled accordingly.

## Verified

| Check | Result |
|---|---|
| Full build, `pio run` | **SUCCESS**, first attempt |
| Toolchain | PlatformIO 6.1.19, platform espressif32 6.9.0, Arduino framework |
| Target | `m5stack-stamps3` (ESP32-S3FN8), matches the ADV's Stamp S3A |
| RAM (static) | 52,784 B of 327,680 B — **16.1%** |
| Flash | 1,082,721 B of 3,145,728 B — **34.4%** |
| Binary produced | `.pio/build/cardputer-adv/firmware.bin`, 1,083,088 B |
| Runtime canvas headroom | 240x135x2 = 64.8KB from heap at boot; allocation failure is detected and reported rather than drawn corrupt |
| Library ADV support | `board_M5CardputerADV` confirmed present in M5Unified 0.2.19 source, with ES8311 mic/speaker callbacks |
| Pin map | Cross-checked against M5 docs, M5Unified's ADV table and M5Stack's ADV UserDemo; the one conflict (SD CS) is documented |

## Not verified — needs the device in hand

These are the things to check first, in this order. Each has a built-in test.

1. **Keyboard.** Tools > Keyboard test. Every key should echo a code and a
   character. *Highest risk item in the build:* the TCA8418 driver is written
   from the datasheet plus M5's ADV remap arithmetic, and has never run. If keys
   are dead, check that the TCA8418 answers at 0x34 (Tools > Device info reports
   `TCA8418 ok` or `NOT DETECTED`). If keys are alive but wrong, `remapToGrid()`
   in `src/input/keyboard.cpp` is the place to look.
2. **Microphone.** Tools > Microphone test. Speak; expect a peak above ~10%.
   Then tune Settings > Mic gain. The default of 12 is deliberately conservative.
3. **Speaker.** Tools > Speaker test.
4. **SD card.** Tools > Storage. The `CS = G12 vs G5` conflict resolves here: if
   the card never mounts, try G5 in `src/storage/store.cpp`.
5. **Record to playback round trip.** Call: hold SPACE, speak, release. It should
   auto-play. This is the acceptance test that matters most.
6. **M5Launcher install.** Copy the bin to SD, install from the launcher, confirm
   Bruce still launches afterwards.
7. **Battery reporting.** `M5.Power.getBatteryLevel()` on the ADV's 1750mAh
   single cell — plausible but unconfirmed.
8. **Screen timeout and wake.**

## Known limitations, by design

- **Notes are capped at 400 characters** and edited in a single-line field with a
  scrolling tail. A deliberate v0.1 limit, not a bug; a real multi-line editor is
  a v0.2 item.
- **No IR remote.** The ADV has the emitter on G44, but NEC timing cannot be
  verified without hardware, and shipping unverified IR is worse than shipping
  none. Reasoning in the community research doc.
- **No game.** Same reasoning as IR: it earns space only once the device proves
  it is carried daily.
- **Focus survives screen changes but not a reboot.** Persisting a running timer
  across power loss adds failure modes for little gain.
- **Wi-Fi is off until asked.** The radio is the largest battery cost on a device
  meant to live in a pocket.
- **MAZ Host is a placeholder** and says so. `probeHost()` opens a real TCP
  connection; the status is never inferred from a saved setting.

## Known risks

- **SPI bus sharing.** SD uses `SPIClass(HSPI)` on G40/G14/G39. If M5GFX drives
  the ADV panel on the same SPI host, the display or the card will misbehave. Not
  reproducible without hardware; the first symptom would be a corrupt screen while
  writing a recording.
- **Mic/speaker handover.** The single ES8311 codec is ended and restarted on
  every record/play transition. Repeated fast cycling is the most likely source of
  an audio-subsystem hang, and is worth hammering deliberately during test 5.
