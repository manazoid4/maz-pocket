# Verification status — MAZ Pocket v0.2

Updated 2026-08-14. Compiling is not counted as a product demonstration.

## Verified in automation

| Check | Result |
|---|---|
| Cardputer ADV release build | PASS — PlatformIO 6.1.19, espressif32 6.9.0 |
| Static memory | 126,756 B / 327,680 B (38.7%), including LVGL's 64 KiB pool and 6,720 B partial draw buffer |
| App flash | 1,527,969 B / 3,145,728 B (48.6%) |
| MAZ Host | 16 Python tests pass, including authenticated streamed WAV, Agent Nudge credential discovery, deterministic voice reminders and malformed BrainDump-output recovery |
| Agent Nudge | Typecheck, 65 unit tests, 39 integration tests, 2 end-to-end tests and production build pass |
| Live BrainDump host flow | PASS — generated speech was transcribed, highlighted, structured by the installed local model and returned in 6.64 s |
| Release packaging | Produces only a 1,528,336 B Launcher-safe app binary (`SHA-256 41EB8A09AB769C168B71EA4BA08EDE34C8E8E4CA71317B859C84EAF478E8F620`); the destructive merged web image was removed |

## Observed on the physical Cardputer ADV

- USB identity `303A:1001` was detected on COM5 and the chip identified as an
  ESP32-S3 revision 0.2 with 8 MB flash.
- **M5Launcher is no longer installed.** It lived in `app0` at 0x10000 under its
  own partition table. Flashing MAZ Pocket directly over USB writes our table at
  0x8000 and our image at 0x10000, over the top of it; the later OTA wrote into
  `ota_1` (0x310000-0x610000), which also covers the old `mazpoc` slot. Restoring
  it means flashing Launcher's bootloader, table and app over USB, which removes
  the two-slot layout and therefore OTA. It is one or the other.
  (Historically: the one-command installer prepared an isolated `mazdata`
  partition, installed one MAZ OTA slot and observed
  `MAZ Pocket 0.2.0 READY board=24 keyboard=ok storage=internal`.)
- The display SPI deadlock was reproduced and removed by moving SD to the
  separate FSPI host. Repeated boots now reach the shell.
- Device-to-laptop status is verified over the real LAN connection:
  `wifi=online host=online nudge=ALL_SYNCED agents=8`.
- LVGL 9.5.0 initialized its core, RGB565 display, aligned partial buffer and
  Home widget tree on the physical device before the READY banner. The first
  hardware attempt exposed a 2-byte-aligned draw buffer; applying LVGL's
  required 4-byte alignment removed the assertion halt.
- The current Launcher-installed build includes automatic offline Talk/BrainDump
  dispatch, lossless record updates, clock-safe relative reminders, reachable
  Sprint debrief and configured host binding. It boots and reports live status;
  the interaction details remain in the acceptance list below.
- MAZ Pocket's serial/keyboard Launcher hand-back was exercised. M5Launcher
  booted without an abort, and the installer successfully replaced the old app.
- A generated spoken WAV completed STT → local Ollama → answer. Warm timings:
  upload 16 ms, STT 906 ms, model 929 ms, total 1.85 s. First cold run was
  12.16 s, so the shipped laptop default uses the installed `gemma3:1b` model.

## Recovering a device that will not boot

A flat battery cannot be flashed in one pass. The ESP32-S3 browns out partway
through a large write, because USB alone does not carry sustained flash-write
current, and the symptom is misleading: the serial port disappears mid-transfer
at a consistent percentage, which reads exactly like a driver or cable fault and
is not one. Retrying, dropping the baud rate and changing `--before` mode all
fail the same way.

Two things make it work. Skip esptool's stub — the S3's native USB drops its CDC
link when the stub takes over — and write the image in 128 KB chunks so no
single write outlasts the available current.

```powershell
# split the image
python -c "import os;d=open(r'.pio/build/cardputer-adv/firmware.bin','rb').read();CH=0x20000;[open(os.path.join(os.environ['TEMP'],f'fwc{n}.bin'),'wb').write(d[i:i+CH]) for n,i in enumerate(range(0,len(d),CH))]"

# write each chunk at 0x10000 + n*0x20000, retrying on brownout
$esp="$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py"
for ($n=0; $n -lt 13; $n++) {
  $addr = '0x{0:x}' -f (0x10000 + $n*0x20000)
  python $esp --chip esp32s3 --port COM5 --baud 115200 --no-stub `
    --before default_reset --after no_reset write_flash -z `
    --flash_mode dio --flash_freq 80m --flash_size 8MB $addr "$env:TEMP\fwc$n.bin"
}
```

If the app is missing or corrupt the device boot-loops, printing only
`rst:0x3 (RTC_SW_SYS_RST)` and never reaching `[boot] serial`. Blanking
`otadata` (write 8 KB of 0xFF at 0xe000) makes the bootloader fall back to the
first app slot. Recovery does not touch `/maz/**`, so notes and recordings
survive.

## Physical acceptance still required for the new build

0. Confirm dictation end to end. Hold Ctrl+SPACE in Decision, speak, release,
   and check the words land in the field. This is the one path in the current
   build that no automated harness can reach, because it needs a human voice.
1. Confirm the LVGL-rendered home screen visually and press each Home shortcut.
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
