# MAZ Pocket v0.5.1

A hardening release for the six-surface Cardputer ADV product: **COMM / CAPTURE / OPS / CONTROL / RECALL / FLOW**.

v0.5 remains the known-good rollback image. v0.5.1 does not repartition the device and does not add a firmware writer.

## COMM: SD-first, stream-second

- Voice is always written to the local WAV first.
- On the LAN, COMM simultaneously streams PCM16LE mono 16 kHz in bounded 20 ms frames over an authenticated WebSocket.
- Every turn carries explicit turn/session IDs.
- Cancel and disconnect are explicit protocol events.
- A connection can retry before audio starts; after audio has left the device, MAZ falls back to the complete saved WAV instead of pretending a partial stream is complete.
- Ollama reply deltas are streamed back as they are generated.
- REST remains the compatibility/failure path.
- If the PC is unavailable, the raw WAV stays in the device Outbox.

## No blocking Host work in app screens

Cardputer-to-PC work now goes through one bounded FreeRTOS Host worker rather than waiting inside the UI task. This covers:

- COMM REST fallback and PC controls;
- BrainDump processing;
- Agent Nudge assurance/nudge actions;
- TTS;
- MAZ Core status/projects/job start/job polling.

Keyboard, timers, display and audio therefore keep running while the PC, network, model, build or agent is slow.

## Local AI: primary + low-VRAM backup

The existing primary stays unchanged:

`lfm2.5-8b-a1b-gpu:latest`

v0.5.1 adds an optional lighter local assistant:

`maz-pocket-lite:latest` → Qwen3.5 4B Q4_K_M with an 8K runtime context and a MAZ-specific personal-assistant profile.

Default local policy is:

`LFM2.5 primary → MAZ Pocket Lite`

The Cardputer **LOCAL** route stays entirely local. **AUTO** may continue to the configured cloud provider only if both local models fail. The host can also force `primary` or `backup` without reflashing the Cardputer.

`install-core.ps1` performs the setup and installs Pocket Lite through Ollama unless `-SkipModels` is used.

## Runtime proof

CONTROL now includes **RUNTIME**. The same measurements are exposed by `mazpocket.local/api/status`:

- current/free/minimum heap;
- largest free block;
- main, Host-worker and WebSocket-task stack watermarks;
- bounded Host/WS queue depth + high-water marks;
- Host rejections/result drops;
- WS frame drops/reconnects/protocol errors;
- Host latency, first-token latency and total streamed-turn latency.

`scripts/adv_soak.py` is the 30-minute physical ADV soak harness. It fails on dropped COMM frames, dropped Host completions or excessive end-state heap loss.

## Cleanup and safety gates

- The old `v03.cpp` identity is gone; active COMM/OPS code is `comm_ops.cpp`.
- The old `lvgl_ui` name is gone. Home is direct M5Canvas in `home_grid.*`; there is no LVGL dependency.
- PC/Core action IDs come from one protocol schema and generated device/host constants.
- CI rejects direct blocking Host I/O from app/portal code.
- CI rejects firmware updater APIs, LVGL regression, common arbitrary-shell patterns and obvious committed secrets.
- CI verifies the app image remains within the known `0x180000` Launcher slot.
- CI downloads the released v0.5 M5Launcher binary and verifies its exact size/SHA before packaging it as the rollback image.

## Installation target

The final v0.5.1 artifact contains:

- `Maz-Pocket-v0.5.1-M5Launcher.bin` — app-only M5Launcher image;
- `ROLLBACK-v0.5-M5Launcher.bin` — exact verified known-good rollback;
- `MAZ-Core-v0.5.1.zip` — PC host;
- `SHA256SUMS.txt`;
- quickstart, notes and third-party notices.

A stable Launcher-friendly `latest.bin` URL is the intended normal install/update path so users do not need to browse GitHub release ZIPs.

## License boundary

`THIRD_PARTY_NOTICES.md` records shipped dependencies and research references. Bruce (AGPL-3.0) was treated as product/UI inspiration only; no Bruce source is copied, linked or vendored into v0.5.1.
