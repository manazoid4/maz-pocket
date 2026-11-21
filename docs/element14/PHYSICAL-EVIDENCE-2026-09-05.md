# Physical evidence — 5 September 2026

Testing began on 5 September and concluded after midnight on 6 September, UK time.

## Proven

- Windows enumerated a physical USB device on `COM5` with `VID_303A&PID_1001`.
- `MAZPING` identified the running firmware as MAZ Pocket `1.0.0` at `192.168.1.47`.
- The device reported internal storage ready, `sdbad=0`, `wiped=0`, no heap pressure, and Wi-Fi online.
- After MAZ Core started normally on the PC, the device reported `host=online` and `link=LAN`.
- USB control opened Home, CALL, Brain Dump, Agent Status, PLAN and CONTROL, then returned to Home.
- The device portal reported measured RSSI of `-72 dBm` during this session.
- Core diagnostics reported MAZLATEST and local Ollama available; AUTO excluded CLOUD because it shared MAZLATEST's endpoint and retained local fallbacks.

## Failed or incomplete

- The first USB-driven CALL attempt accepted Space down/up but never increased Inbox from 13. Recording was never observed active. No audio, transcription, route response, display response or spoken reply is claimed.
- Opening PLAN coincided with an observed firmware loop maximum of 7,833 ms and set the stall flag. Cause is not yet established.
- Core's live `/nudge` request returned HTTP 503 during the session, so Agent Status is not proven as a live-agent demonstration.
- Opening Brain Dump starts recording immediately. The surface check created one new preserved raw capture, increasing the count from 4 to 5; it was not processed or deleted.
- The three-consecutive-run gates for CALL, Brain Dump and PLAN have not passed.
- Firmware flashing, reboot persistence and M5Launcher return were not attempted.

## Tool finding

The USB acceptance client used `readline()` with a 250 ms timeout. Windows USB serial sometimes returned a partial line at that timeout, splitting `MAZSCREEN` into fragments. The competition branch now buffers bytes until a newline before parsing a response.

The first CALL test also showed that the CLI key events can be acknowledged while the device never exposes an active recording or result. This is a product/acceptance-path failure, not proof that the physical keyboard or microphone itself is broken.

## Next physical action

After Astra selects the competition code baseline, build and flash that exact commit through the guarded app-only path. Then run the checklist three consecutive times while recording the firmware commit, route, result and failure state. Preserve all existing Cardputer, SD, Wi-Fi and Launcher data.
