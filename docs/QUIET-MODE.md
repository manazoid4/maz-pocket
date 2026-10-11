# nod quiet mode

Mute the chirps without missing what matters. It lives on the device only.
Nothing changes on your hub, and agents keep sending events as before.

## Levels

| Level | Agent `done` | Agent `usage` | Agent `needs` | Agent `error` | Menu clicks and other sounds |
|---|---|---|---|---|---|
| **Normal** | chirp + toast | beep + toast | beep + toast | error tone + toast | on |
| **Quiet** | silent toast | silent toast | beep + toast | error tone + toast | on |
| **Silent** | silent toast | silent toast | silent toast | silent toast | off, including the Focus alarm |

What never gets hidden: every agent event still shows its toast at every
level. A `needs` or `error` toast always appears, even in Silent. The small
`QUIET` or `SILENT` tag in the status bar stays on screen until you go back to
Normal.

UI sounds (Settings > UI sounds) still apply on top. If UI sounds are off, a
chirp that Quiet would allow stays quiet too. Quiet mode never turns sound on.

## Switching

- **Fn+Q** cycles Normal, Quiet, Silent, Normal. It uses the duration you last chose.
- **Settings > Quiet mode** picks the level (left/right).
- **Settings > Quiet for** picks how long: 30 min, 2 h, 8 h, or until off.
  Changing it while Quiet or Silent is on restarts the timer.

## Expiry

Quiet and Silent end by themselves after the chosen time. The device goes back
to Normal and shows one toast: "Sound back to normal". Choose "until off" to
keep it until you switch it yourself.

The timer runs on the device clock and is not saved. A reboot always starts in
Normal, so the device can never wake up muted. Only your last level and
duration choice are remembered, as the starting point in Settings.

## For developers

The sound rule is one pure function, `quiet::shouldChirp(level, event)` in
`src/core/quiet.h`. The truth table above is its specification. There is no
native test environment in this repo yet, so the function is checked by the
firmware build only and has no automated test.
