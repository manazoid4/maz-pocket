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

## How a release is published

One workflow publishes every version: `.github/workflows/release.yml`.

1. Land the version bump (`VERSION`, `CHANGELOG.md`, `RELEASE_NOTES.md`, `README.md`, `QUICKSTART.txt`) on `main` and let `scripts/check-version.py` pass.
2. Push an empty `.release/v<version>` marker file to `main`. That path is the only trigger.
3. The workflow refuses to run if `VERSION` and the marker disagree, or if a release with that tag already exists. It republishes nothing.

Do not clone a new `release-vX.Y.yml` per version. That pattern produced five identical files whose only difference was a string the job can read from `VERSION` itself.

## Firmware boundary

- Firmware is an **app-only M5Launcher image**.
- M5Launcher owns installation and rollback.
- Never turn the local portal into a direct firmware-partition writer.
- Keep the known `0x180000` app-image ceiling enforced in CI.

## Direct-flash recovery path

`scripts/install.ps1` hands a new image back to M5Launcher for it to flash --
that stays the product's install path. But `scripts/launcher-device.py
handoff` can fail even when the target partition is genuinely valid and
bootable: `esp_ota_set_boot_partition()` on a TEST-subtype partition can
report success and the device can reboot (confirmed via a fresh ESP-ROM
banner) while the bootloader still ignores the TEST target and reboots
straight back into MAZ Pocket. That is a bootloader-level defect, not
something `launcher-device.py`'s serial protocol can work around.

When that happens, `scripts/flash-direct.py` writes the new app-only image
straight to MAZ Pocket's own currently-running OTA slot over USB, bypassing
Launcher's hand-back entirely:

```
python scripts/flash-direct.py --port COM5 --binary dist/Maz-Pocket-v<version>-M5Launcher.bin
```

It reads the live partition table off the device first -- never assume it
matches `partitions.csv` in this repo, which is a placeholder for standalone
compile/link checks only (see that file's header comment); the real layout
is whatever M5Launcher actually installed. It refuses to run if it finds
anything other than exactly one non-TEST, non-FACTORY app partition, and it
never writes outside that slot, so Launcher's own partition -- and its
recovery path -- is never touched.

## Stability boundary

- CI green means source/tests/package integrity, not physical Cardputer proof.
- Keep the previous known release available as rollback until the physical gate passes.
- Network/AI work stays off the UI task; prefer the existing single bounded Host worker over additional FreeRTOS workers.
- New product capabilities should fit the six surfaces before adding a top-level Home app.
- Incoming Beam/context data is untrusted data and must never become an executable command surface.
