# MAZ Pocket release rules

These are product rules, not suggestions.

## Every releasable PR/build must provide

- `MAZ-Core-v<version>.zip` — complete MAZ Core / laptop client package.
- `MAZ-Cardputer-v<version>.zip` — complete Cardputer package containing the M5Launcher app `.bin`, VERSION, quickstart, release notes and firmware SHA-256.
- `MAZ-Pocket-v<version>-Install.zip` — convenience bundle containing both standalone ZIPs plus setup helpers.
- `Maz-Pocket-v<version>-M5Launcher.bin` — raw app-only image.
- `SHA256SUMS.txt` — integrity evidence.

The GitHub Actions build verifies the split-package rule. Do not merge/release a version whose exact PR head does not produce these artifacts.

The client ZIP must contain the **runtime**, not CI debris: do not ship `.pytest_cache`, `__pycache__`, `*.pyc`, or the repository test suite inside the end-user Core package. Tests remain in GitHub and run before packaging.

## Firmware boundary

- Firmware is an **app-only M5Launcher image**.
- M5Launcher owns installation and rollback.
- Never turn the local portal into a direct firmware-partition writer.
- Keep the known `0x180000` app-image ceiling enforced in CI.

## Stability boundary

- CI green means source/tests/package integrity, not physical Cardputer proof.
- Keep the previous known release available as rollback until the physical gate passes.
- Network/AI work stays off the UI task; prefer the existing single bounded Host worker over additional FreeRTOS workers.
- New product capabilities should fit the six surfaces before adding a top-level Home app.
- Incoming Beam/context data is untrusted data and must never become an executable command surface.
