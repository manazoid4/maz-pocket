# MAZ Pocket v0.4

Daily-driver release for Cardputer ADV + M5Launcher.

## What changed

- Home is now the visible six-surface product: **COMM / CAPTURE / OPS / DESK / RECALL / FLOW**.
- DESK, RECALL and FLOW are real hubs into useful tools rather than hidden ideas.
- Saved Wi-Fi connects automatically at boot, supports two saved networks and retries after drops with bounded backoff.
- `mazpocket.local` is rebuilt as a phone-friendly control dashboard with six-surface launchers, live Wi-Fi/PC/agent/battery state, PC media controls, reconnect, diagnostics, MAZ Host settings, backup Wi-Fi, reboot and M5Launcher hand-back.
- COMM keeps persistent voice/text sessions, local/auto/cloud routing, spoken replies and the allow-listed PC command deck.
- The preferred local AI is now **`lfm2.5-8b-a1b-gpu:latest`** through Ollama instead of the old tiny default model.
- Local generation is tuned for low hallucination (`temperature 0.15`) and MAZ Host now gives the model a verified v0.4 capability map so it must distinguish current features from future ideas.
- OPS keeps Agent Nudge state, detail/evidence and manual nudge actions. Agent facts are supplied to the model only as evidence, not guessed.
- SD remains the data/firmware library; installed apps still consume the Cardputer ADV's internal 8 MB flash.
- Canonical version naming is simplified to **v0.1 / v0.2 / v0.3 / v0.4**.
- Official install/update path is the simple **M5Launcher + SD-card `.bin`** path. The experimental browser ZIP flasher is not part of the v0.4 release path.

## Install

Copy `Maz-Pocket-v0.4-M5Launcher.bin` to the SD card, open M5Launcher, select the file, install it into a Launcher app slot, then launch MAZ Pocket.

## AI setup

MAZ Host defaults to Ollama model `lfm2.5-8b-a1b-gpu:latest` and local routing. Existing `host/.env` files override defaults, so update `MAZ_OLLAMA_MODEL` and `MAZ_DEFAULT_ROUTE=local` if an older host configuration is still present.

## Safety / limits

- The release `.bin` is an app-only ESP32 image, not a whole-device image. Do not flash it at address `0x0` as a full firmware image.
- CI rejects the build if it exceeds the known `0x180000` Launcher slot ceiling.
- Generic ArduinoOTA remains disabled because it cannot safely infer ownership when several Launcher apps share internal flash.
- Keep Bruce and other firmware as separate M5Launcher apps; keep extra `.bin` files on SD and swap installed apps when internal flash is tight.
