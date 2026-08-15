# v0.5 daily-driver hardening

This branch intentionally keeps the released six-surface product model and M5Launcher ownership boundary.

## Runtime evidence

The firmware now records free heap, minimum free heap, largest contiguous block, main-loop stack high-water mark and worst observed foreground loop duration every 30 seconds. This is observational only: no memory threshold triggers an automatic reboot.

Use the serial `[health]` line during real Cardputer ADV soak tests. A `loop_max` above 250 ms is evidence of foreground work that should move off the UI loop; the known synchronous MAZ Host HTTP path is the first target.

## Hardware acceptance gate

Before replacing the current v0.5 release, validate on a real Cardputer ADV:

- 30 minute idle/normal-use soak with no reset or watchdog event;
- at least 20 COMM turns;
- repeated Wi-Fi loss/reconnect and MAZ Core loss/reconnect;
- SD absent, unreadable and near-full behavior;
- microphone record/playback cycles;
- M5Launcher hand-back, reinstall and rollback;
- record the lowest `min` and `largest` heap values and maximum `loop_max` seen.

Do not declare non-blocking HostLink or streaming COMM complete until those paths are implemented and the foreground `loop_max` evidence confirms the UI no longer waits on long network calls.
