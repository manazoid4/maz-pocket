# OTA and the real flash layout

`partitions.csv` is NOT what runs on the device. M5Launcher owns the partition table; nod only sees it.
Layout read from Launcher's Web UI (8 MB flash):

| Name | Type | Offset | Size |
|---|---|---|---|
| app0 | app / test (Launcher itself) | 0x10000 | 0x150000 |
| coredump | data | 0x160000 | 0x10000 |
| mazdata | data / littlefs | 0x170000 | 0x200000 |
| nodfw0 | app / ota_0 | 0x370000 | 0x170000 (1,507,328 B) |
| nodfw1 | app / ota_1 | 0x4e0000 | 0x170000 |

nvs (0x9000) and otadata (0xe000) are required by Launcher's own table builder (`kRequiredBootPartitions`) so they exist, the Web UI just hides system partitions.

How Launcher boots an app: it writes the image into the slot, verifies it, then calls the standard `esp_ota_set_boot_partition()` (otadata). So nod's standard `esp_ota_*` self-update is the same mechanism. Update image must stay under 1,507,328 bytes.

Self-update (`fwUpdate`): refuses below 30% battery unless charging, erases the spare slot, downloads `/fw/latest.bin`, SHA-256 + `esp_ota_end` verify, `esp_ota_set_boot_partition`, 3-boot rollback guard (`fwBootGuard`).

## Remote diagnosis (no cable)

- `GET http://<device>/api/status`: `fw_build`, `fw_running_slot`, `fw_next_slot`, `fw_slot_size`, `fw_ota_state`, `fw_otadata_present`, `fw_last_stage`, `fw_last_error`, `fw_last_error_code` (ESP error code; name is inside `fw_last_error`; 0 = none).
- Core: every failed (and successful) update POSTs `/fw/report`; log line `fw report: ...`; last one at `GET /core/update` under `fw_report`.
- Stages: precheck, manifest, begin, download, write, verify, boot, ok.
