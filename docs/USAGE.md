# Using MAZ Pocket

## Boot

Power on. The MAZ mark draws itself in, the wordmark comes up, and you are on
Home in under a second. There is no splash to sit through.

## Home

The top panel shows one thing, in priority order: a running Focus session, else
your next open task, else the MAZ mark. Below it, six tiles.

- Arrows move, **ENTER** opens.
- **C** Call, **X** Capture, **F** Focus, **N** Notes, **T** Tasks, **R** Recorder.
- **/** or **Ctrl+K** opens the command palette.
- **Hold SPACE** anywhere on Home drops straight into a voice capture, already
  recording. This is the gesture the device exists for.

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

Reachable from the command palette (`Ctrl+K`).

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

## Getting back to Bruce

Tools then Reboot, or power cycle and hold **ENTER** to land in M5Launcher.
MAZ Pocket never takes ownership of the launcher.
