# Verification status — MAZ Pocket v0.2

Updated 2026-08-13. Compiling is not counted as a product demonstration.

## Verified in automation

| Check | Result |
|---|---|
| Cardputer ADV release build | PASS — PlatformIO 6.1.19, espressif32 6.9.0 |
| Static memory | 53,832 B / 327,680 B (16.4%) |
| App flash | 1,258,433 B / 3,145,728 B (40.0%) |
| MAZ Host | 11 Python tests pass, including authenticated streamed WAV and deterministic voice reminders |
| Agent Nudge | Typecheck, 65 unit tests, 39 integration tests, 2 end-to-end tests and production build pass |
| Release packaging | Produces a Launcher-safe app binary and an explicitly destructive merged web image |

## Observed on the physical Cardputer ADV

- USB identity `303A:1001` was detected on COM5 and the chip identified as an
  ESP32-S3 revision 0.2 with 8 MB flash.
- An earlier v0.2 build was written and booted far enough to emit ESP32 and
  storage logs. This exposed a wrong internal filesystem partition label, which
  is fixed in source.
- The full-chip test replaced M5Launcher; the owner restored Launcher. All
  subsequent device delivery is app-only through M5Launcher.

The corrected build has **not** yet been installed through M5Launcher, so none
of the flows below are marked complete.

## Physical acceptance still required

1. Install `maz-pocket-app.bin` through M5Launcher WUI or FAT32 SD manager.
2. Confirm screen, ADV keyboard and Home shortcuts.
3. Confirm storage. The card observed during the first boot was not a valid FAT
   volume to Arduino's SD driver; Launcher itself recommends SDHC, max 32 GB,
   FAT32 and MBR.
4. Run microphone, speaker and record/playback diagnostics.
5. Measure `speak → MAZ Host → STT → model → visible answer` latency.
6. Prove BrainDump raw preservation, highlight processing and useful Inbox output.
7. Prove Sprint outcome/timer/debrief and reminder fire/done/snooze.
8. Prove real Agent Nudge stale detection, explicit nudge, sync and acknowledgement.

MAZ Works must not present these as demonstrated until this checklist is
captured with real photos/screenshots and timings.
