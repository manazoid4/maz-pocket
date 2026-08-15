# MAZ Pocket v0.5

The **control + real local intelligence** milestone for Cardputer ADV.

## Why v0.4 missed

v0.4 added capability but hid too much behind menus and still gave the model too little factual PC/project context. Wi-Fi existed but did not feel first-class, and the web UI duplicated controls rather than acting as one coherent system.

## v0.5

- Home remains six focused surfaces but **DESK is replaced by CONTROL**: COMM / CAPTURE / OPS / CONTROL / RECALL / FLOW.
- **W opens Wi-Fi directly from Home.** CONTROL exposes Wi-Fi, MAZ Core, PC/COMM, diagnostics, settings and M5Launcher.
- New full on-device Wi-Fi manager: status, reconnect, scan/connect primary, scan/save backup, disconnect, forget networks and setup hotspot.
- If no saved network works, the device exposes **MAZ-Pocket-Setup** so Wi-Fi is always repairable from the Cardputer/phone.
- `mazpocket.local` is now a real device control plane with Wi-Fi provisioning, surface launchers, Core configuration, PC controls, diagnostics, recovery and a **~2 FPS live LCD stream**.
- Cardputer ADV has no built-in camera; v0.5 streams the device screen. External camera video remains a hardware extension, not a fake software claim.

## MAZ Core

`MAZ-Core-v0.5.zip` turns the Windows PC into the persistent brain behind the Cardputer without requiring Codex:

- local AI: `lfm2.5-8b-a1b-gpu:latest` via Ollama;
- real project discovery across configured local roots;
- branch / dirty / recent-commit evidence;
- safe project search and non-secret file reads;
- allow-listed project actions: git status, fetch, fast-forward pull, detected tests, detected build, open folder;
- tests/builds are background jobs so the handheld does not freeze;
- Cardputer status + live screen proxy;
- Agent Nudge remains factual evidence, not an LLM guess;
- optional private GitHub issue bridge enables ChatGPT → private GitHub → MAZ Core → local PC without a generic remote shell.

## Hidden Maz Works console

A `/maz-core` client is added to Maz Works but is **not linked publicly and is `noindex`**. It contains no private URL/token. Your browser supplies its own private HTTPS Core endpoint and pairing token at runtime.

## Install

1. Copy `Maz-Pocket-v0.5-M5Launcher.bin` to SD.
2. M5Launcher → select `.bin` → Install → Launch.
3. Extract `MAZ-Core-v0.5.zip` on the PC and run `install-core.ps1` once.

## Safety

- Firmware is an app-only M5Launcher image; never flash it at address `0x0`.
- CI enforces the known `0x180000` Launcher slot ceiling.
- Generic ArduinoOTA stays disabled so MAZ cannot overwrite another Launcher app.
- MAZ Core has no arbitrary remote shell and blocks direct reads of common secret-file types.
- The GitHub bridge is private-repo + explicit-prefix + allow-list only.
