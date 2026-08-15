# v0.5 daily-driver hardening

This branch intentionally keeps the released six-surface product model and M5Launcher ownership boundary.

## Runtime evidence

The firmware records free heap, minimum free heap, largest contiguous block, main-loop stack high-water mark and worst observed foreground loop duration every 30 seconds. This is observational only: no memory threshold triggers an automatic reboot.

ESP-IDF reports `uxTaskGetStackHighWaterMark()` in bytes, so both the main-task and COMM-worker readings are recorded directly in bytes.

Use the serial `[health]` line during real Cardputer ADV soak tests. A `loop_max` above 250 ms is evidence of foreground work that should move off the UI loop.

## COMM responsiveness boundary

The active COMM surface no longer performs session creation, WAV upload or optional TTS on the UI task. Those operations run on one low-priority, bounded 8 KB `maz-host` worker. The worker copies only session/path data and returns a copied result; it never retains an `App*`, so leaving COMM while the PC is slow cannot create an app-lifetime use-after-free.

The worker prints `[host-worker] stack_free_min=... bytes` after the expensive HTTP/TLS/TTS path. Use that measurement to tune the 8 KB stack on real hardware rather than shrinking it by assumption.

This is deliberately an incremental safety change, not a claim of realtime streaming. The transport remains durable WAV -> MAZ Core HTTP -> answer/TTS. True microphone-frame streaming should be a separate hardware-tested increment after this branch establishes stable heap, stack and UI-loop measurements.

The USB/LAN control status path is also snapshot-only: `MAZSTATUS` no longer performs Host health/agent HTTP calls, and USB input is consumed byte-by-byte instead of waiting on `readStringUntil()` timeouts.

## Hardware acceptance gate

Before replacing the current v0.5 release, validate on a real Cardputer ADV:

- 30 minute idle/normal-use soak with no reset or watchdog event;
- at least 20 COMM turns, including leaving/re-entering COMM while a Host turn is running;
- confirm navigation/input remain responsive while MAZ Core is stopped or unreachable;
- repeated Wi-Fi loss/reconnect and MAZ Core loss/reconnect;
- SD absent, unreadable and near-full behavior;
- microphone record/playback cycles;
- M5Launcher hand-back, reinstall and rollback;
- record the lowest `min` and `largest` heap values, main-task stack headroom, COMM-worker stack headroom and maximum `loop_max` seen.

Do not call streaming COMM complete until a later frame-streaming transport is implemented and physically validated. Do not reduce task/buffer sizes from desktop CI alone.
