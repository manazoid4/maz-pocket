# MAZ Pocket v0.4

Daily-driver release for Cardputer ADV + M5Launcher.

## What changed

- Home is the six-surface product: **COMM / CAPTURE / OPS / DESK / RECALL / FLOW**.
- DESK, RECALL and FLOW are real hubs into the existing useful tools instead of hidden features.
- Saved Wi-Fi connects automatically at boot, supports two saved networks and retries after drops with bounded backoff.
- `mazpocket.local` remains the local status/control surface once Wi-Fi is connected.
- COMM keeps voice sessions, local/auto/cloud routing, spoken replies and the PC command deck.
- OPS keeps Agent Nudge state, inspection and nudge controls.
- SD remains data/firmware-library storage; installed apps still consume the Cardputer's internal 8 MB flash.
- Version naming is simplified to **v0.1, v0.2, v0.3, v0.4** going forward.
- Official install/update path is now the simple **M5Launcher + SD-card `.bin`** path. The experimental browser ZIP flasher is not part of this release path.

## Install

Copy `Maz-Pocket-v0.4-M5Launcher.bin` to the SD card, open M5Launcher, choose the file, install it into a Launcher app slot, then launch MAZ Pocket.

## Safety / limits

- This is an app-only ESP32 image, not a whole-device image. Do not flash it at address `0x0` as a full firmware image.
- CI rejects the build if it exceeds the known `0x180000` Launcher slot ceiling.
- Generic ArduinoOTA remains disabled because it cannot safely infer ownership when several Launcher apps share internal flash.
- Keep Bruce and other firmware as separate M5Launcher apps; keep extra `.bin` files on SD and swap installed apps when internal flash is tight.
