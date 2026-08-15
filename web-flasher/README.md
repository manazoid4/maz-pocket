# MAZ Pocket Web Flasher

Browser-first transition/update path for Cardputer ADV.

## Product contract

- One primary action: **CONNECT & UPDATE**.
- Use the ESP32-S3 ROM loader only; do not upload the RAM flasher stub that has already been unreliable on Cardputer ADV native USB.
- SHA-256 verify the packaged v0.03 image and the known-good v0.02 recovery image before any flash write.
- Read only the live 4KB ESP32 partition table before writing.
- Update only the existing Launcher OTA app partition whose label starts with `MAZ-Pocket`.
- Refuse if no unique MAZ partition exists or if the firmware does not fit.
- Never erase the full flash.
- Never write the partition table, NVS, M5Launcher, SD data, or sibling firmware.
- Verify the flashed application using `esptool-js`'s upstream ROM-compatible `flashMd5sum()` implementation.
- If write/verification fails, restore the published physical-hardware-accepted v0.02 recovery image and verify that recovery using the same ROM MD5 path.
- Desktop Chromium uses Web Serial. Android Chrome can fall back to Google's WebUSB Serial polyfill.
- The normal transition path deliberately has no arbitrary local `.bin` option and no erase button.

A full live-partition backup is intentionally not part of this ROM-only transition. Stub-free ROM reads are slow; the firmware image itself is replaceable while user settings/NVS, M5Launcher, SD data and sibling applications are outside the target partition and are never written. The known-good v0.02 image is therefore the deterministic rollback target.

## Why this replaced the Windows updater and device-side OTA

M5Launcher can manage several unrelated ESP32 OTA app partitions. Generic ArduinoOTA/`esp_ota_get_next_update_partition()` uses round-robin OTA semantics, not application ownership semantics, so it is not an acceptable update boundary for MAZ Pocket. The browser flasher instead reads the live partition map and targets the existing MAZ-labelled partition explicitly.

The previous desktop updater also accumulated fragile assumptions about COM-port persistence, Launcher UI banners and PyInstaller console streams. Those are removed from the normal user path.

Moving the installer UI and OTA writer out of the Cardputer also keeps v0.03 within the physical v0.02 Launcher slot (`0x180000` bytes) rather than repartitioning a user's device.

## Safety tests

CI exercises the pure partition and ROM helpers before packaging:

- MAZ-Pocket + Bruce + Nemo mixed partition table;
- missing and duplicate MAZ labels;
- wrong partition type/subtype;
- overlap and out-of-flash bounds;
- truncated partition tables;
- ROM `0x0E` slow-read packet layout and chunking;
- browser-flasher syntax against the upstream `esptool-js` ROM write/MD5 APIs.

The final device flash still requires physical acceptance on the real Cardputer; passing CI is not represented as hardware proof.

## UX references

The interaction deliberately follows mature ESP browser installers rather than inventing another desktop updater:

- Bruce Web Flasher — Cardputer-specific G0 recovery instructions and one-action web flashing.
- FZEE Flasher — minimal connect/program surface.
- Tasmota Web Installer — browser-first install and a clean separation between installer and device WebUI.
- Meshtastic Web Flasher — clear device/firmware/flash flow and visible destructive-action boundaries.
- Espressif `esptool-js` (Apache-2.0) — official browser-side ESP transport and flash implementation; MAZ uses its ROM-capable internals without calling the normal stub-uploading `main()` path.
- Google `web-serial-polyfill` — Android WebUSB compatibility fallback.
- `Mraanderson/CardputerSDtool` (MIT) — inspiration for non-destructive SD visibility, read/write sanity checking, remount/recovery thinking. MAZ does not copy its experimental formatting path into normal updates.

The MAZ implementation is its own code and keeps the existing repository licence boundaries.
