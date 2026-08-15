# MAZ Pocket Web Flasher

Browser-first transition/update path for Cardputer ADV.

## Product contract

- One primary action: **CONNECT & UPDATE**.
- Read the live ESP32 partition table before any write.
- Update only the existing Launcher app partition whose label starts with `MAZ-Pocket`.
- Refuse if no unique MAZ partition exists or if the firmware does not fit.
- Never erase the full flash.
- Never write the partition table, NVS, M5Launcher, SD data, or sibling firmware.
- Download a full raw backup of the MAZ partition before writing.
- Read the flashed image back and require an exact SHA-256 match.
- If verification fails, restore the previous bootable backup; if an older broken updater already invalidated its header, use the published known-good v0.02 recovery image.
- Desktop Chromium uses Web Serial. Android Chrome can fall back to Google's WebUSB Serial polyfill.
- Advanced local `.bin` selection is allowed, but it still passes the same live-slot and ESP image checks.

## Why this replaced device-side OTA

M5Launcher can manage several unrelated ESP32 OTA app partitions. Generic ArduinoOTA/`esp_ota_get_next_update_partition()` uses round-robin OTA semantics, not application ownership semantics, so it is not an acceptable update boundary for MAZ Pocket. The browser flasher instead reads the live partition map and targets the existing MAZ-labelled partition explicitly.

Moving the large installer UI and OTA writer out of the Cardputer also keeps v0.03 within the physical v0.02 Launcher slot (`0x180000` bytes) rather than repartitioning a user's device.

## UX references

The interaction deliberately follows mature ESP browser installers rather than inventing another desktop updater:

- Bruce Web Flasher — Cardputer-specific G0 recovery instructions and ESP Web Tools-style one-click install.
- FZEE Flasher — minimal connect/program flow.
- Tasmota Web Installer — browser-first install, preserve settings for updates, clear separation between installer and device WebUI.
- Meshtastic Web Flasher — device → firmware → flash progression, custom firmware fallback and visible destructive-action boundaries.
- Espressif `esptool-js` — official browser-side ESP flash/read primitive used by MAZ.
- `Mraanderson/CardputerSDtool` (MIT) — inspiration for non-destructive SD visibility, read/write sanity checking, remount/recovery thinking. MAZ does not copy its experimental formatting path into normal updates.

The MAZ implementation is its own code and keeps the existing repository licence boundaries.
