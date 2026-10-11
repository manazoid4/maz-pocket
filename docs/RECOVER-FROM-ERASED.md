# Recover a Cardputer ADV with no nod on it

Status: **UNPROVEN on hardware.** Built and parse-checked only. Say so if you try it.

Two routes. Use the first if M5 Launcher is still on the device.

## Route A: M5 Launcher + SD card (keeps Launcher)

1. `pio run`, then copy `.pio/build/cardputer-adv/firmware.bin` to the root of a FAT32 SD card.
2. Put the card in the Cardputer, open M5 Launcher, pick the file from the SD card and install it.
3. Start nod from the Launcher list.

This is an app-only image. Never write it at address 0x0.

## Route B: full USB flash (wipes everything, including Launcher)

Use this when the device holds nothing useful, or Launcher is gone too.

1. `pio run` (PlatformIO supplies Python and esptool; nothing else to install).
2. Plug the Cardputer in with a data cable.
3. `powershell -ExecutionPolicy Bypass -File scripts/flash-usb.ps1`

The script builds `dist/nod-factory-8MB.bin` (bootloader 0x0, partition table 0x8000, boot_app0 0xE000, app 0x10000), finds the port (native USB, VID 303A), erases the chip, writes the image and checks its MD5. It ends with `PASS` or `FAIL`. `-DryRun` shows what it would do and writes nothing. `-Image <file>` flashes a ready-made image.

If no port appears: unplug, hold **G0**, plug in while holding it, let go, run it again.

Unconfirmed: that the S3 ROM loader accepts the whole-chip erase over USB. If it refuses, the script falls back to a normal checked write and says so.

After Route B, Launcher is gone, so Ctrl+L (return to Launcher) has nothing to return to until Launcher is installed again.
