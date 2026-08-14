# Using MAZ Pocket

## Boot

Power on. The MAZ mark draws itself in, the wordmark comes up, and you are on
Home in under a second. There is no splash to sit through.

## Home

A strip across the top shows one thing, in priority order: a running Focus
session, else your next open task, else the MAZ mark. Under it is the app
table — two columns, four rows, eight apps to a page.

Every app in the firmware sits on one of the pages. Nothing is hidden behind
already knowing its name.

- **Arrows** move. Pushing left or right off the edge of a row turns the page
  and lands you on the column you came in from, so it reads as one wide table
  rather than a jump.
- **TAB** goes straight to the next page, **Shift+TAB** the previous one.
- **ENTER** opens the selected app.
- The page shows as `1/3` and as dots at the right of the strip.
- Each cell carries a badge: the **letter** that opens that app from anywhere on
  Home, or the **digit** of its slot on this page.
- **Hold SPACE** anywhere on Home drops straight into a voice capture, already
  recording. This is the gesture the device exists for.
- **/** or **Ctrl+K** opens the command palette, which searches full names.

### Page 1 — the assistant

**T** Talk, **B** BrainDump, **I** Inbox, **D** Decision,
**F** Focus, **S** Sprint, **N** Nudge, **R** Reminders.

### Page 2 — tools

Notes, Tasks, Recorder (digits **1**-**3**), **C** Calculator, **W** Stopwatch,
**Q** QR Code, **V** Viewer, **G** Generator.

### Page 3 — system

**P** Snippets, Connections, Tools, Settings (digits **2**-**4**), **H** Help.

## Going back

The bottom left of every screen shows the **`<ESC`** chip. It is filled amber
whenever there is somewhere to go back to, and grey on Home where there is not.

- **ESC** goes back one step. Inside an editor or a detail view it cancels that
  first, and only leaves the app on a second press.
- **Hold ESC** returns to Home from anywhere.
- A small amber tick beside the chip means you are more than one screen deep.

## Call

Hold **SPACE**, talk, release. The take plays back automatically.

- **P** play again, **S** save it into Recorder, **D** bin it.
- **SPACE** again records a fresh take.
- Capped at 60 seconds — it is a turn, not a lecture.
- The header shows `MAZ HOST OFFLINE` because in v0.1 there is no host. It will
  never claim otherwise.

## Capture

Two ways in, both fast:

- **Hold SPACE** — voice memo, saved on release.
- **Just type** — **ENTER** saves it as a note.

## Notes

- **N** new, **ENTER** read, **E** edit, **D** twice to delete.
- The first line becomes the title in the list.
- 400 characters per note in v0.1.

## Focus

- Up/down picks 10 / 25 / 45 / 60 / 5 minutes.
- **ENTER** asks for a label ("FlowLens") then starts. **ESC** skips the label.
- **ENTER** pauses and resumes, **C** cancels.
- Leave the screen — it keeps counting, shows in the status bar, and appears on
  Home. Completion sounds even if UI sounds are off.

## Tasks

- **A** add, **ENTER** toggles done, **L** moves between Today and Later,
  **D** twice deletes, **TAB** switches view.
- Open tasks sort above completed ones. The first open Today task shows on Home.

## Recorder

- **ENTER** starts and stops. **P** plays the selected file, **D** twice deletes.
- 30-minute cap. Files are 16kHz mono WAV, about 1.9MB per minute.

## Utilities

On page 2 of Home, or from the command palette (`Ctrl+K`).

- **Calculator** — type `1250*0.2`, **ENTER**. `A` appends the last answer.
- **Stopwatch** — **ENTER** start/stop, **L** lap, **R** reset.
- **QR Code** — type, **ENTER**, hold it up to a phone.
- **Text Viewer** — reads `.txt`/`.md` from `/maz/notes`, `/maz/captures`, `/maz/logs`.
- **Generator** — password, passphrase or number. **S** saves it to Snippets.
- **Snippets** — **A** add, **ENTER** shows the full value.

## Settings

Left/right change a value, **ENTER** toggles, **ESC** saves.
Brightness, volume, UI sounds, screen timeout, mic gain, storage preference,
time zone, plus shortcuts into Wi-Fi and Help.

Settings live in NVS, so they survive being re-flashed by a launcher.

## Connections

**W** toggles the radio, **S** scans and connects, **H** tests MAZ Host,
**T** syncs the clock from NTP. Until a host is configured the line reads
`NOT CONFIGURED`.

## Where your data lives

MAZ Pocket writes to an SD card when one is present and readable, and to
internal flash otherwise. The status bar shows `SD` when the card is the
backend; Tools reports the backend by name.

**Internal flash is the volatile option.** It lives in a partition, and a
partition table is set by whoever flashed the device — M5Launcher uses its own,
`pio run -t upload` writes the one in `partitions.csv`. Switching between those
two moves the data region, the old filesystem no longer mounts, and it has to
be reformatted before it can be used again.

That used to happen in silence. It now announces itself:

- **"Internal storage reset"** — the internal filesystem had to be formatted,
  so notes and recordings stored there are gone. Expected after changing how
  the device is flashed; not expected on an ordinary power cycle.
- **"SD card unreadable"** — a card is in the slot but carries no FAT volume.
  Format it as FAT32 and it will be picked up on the next boot.

If you want data that survives reflashing, keep a FAT32 card in the slot. That
is the only backend a firmware install cannot disturb.

## Wireless install and control

Once the device is paired, the cable is optional.

**Updating over the air.** MAZ Pocket carries two app slots and writes an
update into the one it is not running from, so a failed transfer cannot leave
you with half a firmware.

```
$env:MAZ_POCKET_IP    = "192.168.1.47"     # MAZPING reports this
$env:MAZ_POCKET_TOKEN = "<your MAZ_TOKEN>" # the same token host/.env holds
pio run -e cardputer-adv-ota -t upload
```

The screen shows the progress bar and says not to power off. The password is
the MAZ Host pairing token the device already holds, so there is no second
secret to manage, and a device that has never been paired cannot be updated
remotely at all.

**Driving it over the network.** The same MAZ* commands the USB surface accepts
are served on TCP port 8022:

```
MAZAUTH <token>      then MAZSCREEN, MAZOPEN <id>, MAZKEY <key> DOWN|UP,
                     MAZTYPE <text>, MAZPING, MAZSTATUS, MAZBYE
```

Before authenticating, a connection may read `MAZPING`, `MAZSTATUS` and
`MAZSCREEN` and nothing else. Anything that moves the device needs the token,
and a wrong one closes the connection rather than allowing another guess. This
is a LAN control channel for your own laptop — it is not exposed to the
internet, and it should not be forwarded there.

**When the cable is still required.** Only for changing the partition table
itself, and for recovering a device that will not join Wi-Fi. Note that USB
flashing on this board needs `--no-stub` (already set in `platformio.ini`): the
ESP32-S3's native USB drops its CDC link when esptool's stub takes over.

## Getting back to Bruce

Tools then Reboot, or power cycle and hold **ENTER** to land in M5Launcher.
MAZ Pocket never takes ownership of the launcher.
