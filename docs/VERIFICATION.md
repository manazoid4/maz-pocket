# Verification status — MAZ Pocket v0.2

Updated 2026-08-14. Compiling is not counted as a product demonstration.

## Verified in automation

| Check | Result |
|---|---|
| Cardputer ADV release build | PASS — PlatformIO 6.1.19, espressif32 6.9.0 |
| Static memory | 53,804 B / 327,680 B (16.4%) |
| App flash | 1,262,613 B / 3,145,728 B (40.1%) |
| MAZ Host | 13 Python tests pass, including authenticated streamed WAV, Agent Nudge credential discovery and deterministic voice reminders |
| Agent Nudge | Typecheck, 65 unit tests, 39 integration tests, 2 end-to-end tests and production build pass |
| Release packaging | Produces only a Launcher-safe app binary; the destructive merged web image was removed |

## Observed on the physical Cardputer ADV

- USB identity `303A:1001` was detected on COM5 and the chip identified as an
  ESP32-S3 revision 0.2 with 8 MB flash.
- Official M5Launcher 2.8.0 remains in its TEST partition. The one-command
  installer prepared isolated `mazdata`, installed one MAZ OTA slot and observed
  `MAZ Pocket 0.2.0 READY board=24 keyboard=ok storage=internal`.
- The display SPI deadlock was reproduced and removed by moving SD to the
  separate FSPI host. Repeated boots now reach the shell.
- Device-to-laptop status is verified over the real LAN connection:
  `wifi=online host=online nudge=ALL_SYNCED agents=8`.
- The current Launcher-installed build includes automatic offline Talk/BrainDump
  dispatch, lossless record updates, clock-safe relative reminders, reachable
  Sprint debrief and configured host binding. It boots and reports live status;
  the interaction details remain in the acceptance list below.
- MAZ Pocket's serial/keyboard Launcher hand-back was exercised. M5Launcher
  booted without an abort, and the installer successfully replaced the old app.
- A generated spoken WAV completed STT → local Ollama → answer. Warm timings:
  upload 16 ms, STT 906 ms, model 929 ms, total 1.85 s. First cold run was
  12.16 s, so the shipped laptop default uses the installed `gemma3:1b` model.

## Physical acceptance still required

1. Confirm the rendered home screen visually and press each Home shortcut.
2. Confirm storage. The card observed during boot was not a valid FAT
   volume to Arduino's SD driver; Launcher itself recommends SDHC, max 32 GB,
   FAT32 and MBR.
3. Run microphone, speaker and record/playback diagnostics using the device mic.
4. Prove BrainDump raw preservation, highlight processing and useful Inbox output.
5. Prove Sprint outcome/timer/debrief and reminder fire/done/snooze.
6. Prove real stale detection, explicit nudge, sync and acknowledgement with a
   deliberately stale test agent. Current hardware proof covers authenticated
   real fleet status only.

MAZ Works must not present these as demonstrated until this checklist is
captured with real photos/screenshots and timings.
