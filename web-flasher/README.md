# MAZ Pocket Web Flasher

Browser-first transition/update path for Cardputer ADV, following the simple browser-installer pattern while preserving M5Launcher ownership boundaries.

## User path

1. Download the single CI release ZIP; do not extract it.
2. Open `/maz-pocket/flasher/` in Chrome/Edge.
3. Choose the ZIP. The page verifies pinned v0.03 + v0.02 recovery SHA-256 values before USB access is enabled.
4. Connect Cardputer ADV and press **CONNECT & UPDATE**.
5. If automatic ROM entry fails: unplug, hold G0, reconnect USB, release G0 and press the button again.

The package-selection step exists because `manazoid4/maz-pocket` is private. The public page contains no GitHub credential and does not need the repository to become public. A manifest carried inside the ZIP is never trusted by itself: the release and recovery SHA-256 values are pinned in the flasher code.

## Product contract

- Use the ESP32-S3 ROM loader only; do not upload the RAM flasher stub that was unreliable on the earlier Cardputer ADV update path.
- SHA-256 verify the exact v0.03 image and known-good v0.02 recovery before any flash write.
- Read only the live 4KB ESP32 partition table before writing.
- Update only the unique Launcher OTA app partition whose label starts with `maz-pocket`.
- Refuse if no unique Maz partition exists, the partition map is unsafe, or firmware does not fit.
- Never erase the full flash.
- Never write the partition table, NVS, M5Launcher, SD data, or sibling firmware.
- Verify the flashed application using upstream `esptool-js` ROM-compatible `flashMd5sum()`.
- If write/verification fails after a write begins, restore the published physical-hardware-accepted v0.02 image and verify recovery by ROM MD5.
- Desktop Chromium uses Web Serial. Android Chrome can fall back to Google's WebUSB Serial polyfill.
- No arbitrary `.bin`, erase, or repartition button exists in the normal transition UI.

A full live-partition backup is intentionally not part of this ROM-only transition. Stub-free ROM reads are slow; the firmware image is replaceable while user settings/NVS, M5Launcher, SD data and sibling applications are outside the target partition and are never written. The known-good v0.02 image is the deterministic rollback target.

## Why this replaced the Windows updater and device-side OTA

M5Launcher can manage several unrelated ESP32 OTA app partitions. Generic ArduinoOTA/`esp_ota_get_next_update_partition()` uses round-robin OTA semantics rather than application ownership semantics, so the browser flasher reads the live partition map and targets the existing Maz-labelled partition explicitly.

The desktop updater also accumulated fragile assumptions about COM-port persistence, Launcher UI banners and PyInstaller console streams. Those are removed from the normal path.

Moving installer/OTA logic out of the Cardputer helped v0.03 fit within the physical v0.02 Launcher slot (`0x180000`) without repartitioning. The three-tile Home renderer was moved from LVGL to the firmware's existing M5Canvas/M5GFX stack, preserving the interaction while removing unnecessary flash/RAM weight.

## Safety tests

CI covers:

- Maz Pocket + Bruce + Nemo mixed partition tables;
- missing/duplicate Maz labels;
- wrong partition type/subtype;
- overlap/out-of-flash bounds and truncated tables;
- ROM `0x0E` slow-read packet layout/chunking;
- browser-flasher syntax against upstream `esptool-js` ROM APIs;
- ESP32 application-image validation;
- hard `0x180000` physical-v0.02 slot ceiling;
- known-good v0.02 recovery SHA-256 + size;
- final release ZIP assembly.

Passing CI is not physical hardware proof. The exact package still needs acceptance on the actual Cardputer ADV before preview status is removed.

## UX / architecture references

- Bruce Web Flasher — Cardputer G0 recovery and browser flashing.
- FZEE Flasher — minimal connect/program surface.
- Tasmota Web Installer — browser-first install separated from device WebUI.
- Meshtastic Web Flasher — clear device/firmware/flash flow and destructive-action boundaries.
- Espressif `esptool-js` (Apache-2.0) — browser-side ESP transport/flash implementation; MAZ uses ROM-capable paths without invoking the normal stub-uploading `main()` flow.
- Google `web-serial-polyfill` — Android WebUSB compatibility fallback.
- `Mraanderson/CardputerSDtool` (MIT) — inspiration for non-destructive SD visibility, read/write sanity checking and recovery thinking. MAZ does not copy experimental formatting into normal updates.

The MAZ implementation is its own code and preserves existing repository licence boundaries.
