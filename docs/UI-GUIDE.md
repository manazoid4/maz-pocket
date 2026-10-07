# nod UI guide (calm pass)

Screen 240x135. Tokens live in one place: `src/ui/theme.h`. Use them, do not hard-code numbers or colours in screens.

## Tokens
- Colours: BG, PANEL, LINE, TEXT, DIM, HINT, ACCENT (orange), ACCENT2 (cyan), OK, WARN, ERR. OK = good, WARN = needs attention, ERR = broken.
- Fonts: state word Font4 scaled by `STATE_SCALE`, sub lines Font2, body and chrome Font0 (6x8 px per character).
- Margins: `PAD` 6 px; bars `STATUS_H` 15, `HINT_H` 14; body is what is left.
- Limits: `HINT_CHARS` 33, `REPLY_CHARS` 38, `TOAST_TITLE_CHARS` 31, `TOAST_TEXT_CHARS` 36. Toast life: `T_TOAST` (warn/error get double). Splash: `T_BOOT` 250 ms.
- Status bar columns `SB_*` (update badge, timer, Claude light, Core dot, WiFi, battery %) are fixed x positions so nothing overlaps.

## Screen rules
- Home: one big status line (READY, or what needs you), at most 4 actions (`TABLE_PAGE`), small version text. If Wi-Fi or Core is down, the status line says so and one plain sentence says what to press. Other apps live in Ctrl+K.
- Call: one huge state word (LISTENING / THINKING / SPEAKING / ERROR, READY when idle), one short reason under it, then the reply as word-wrapped text (UP/DOWN scrolls).
- One idea per screen. No more than one accent colour doing the talking.

## Status bar
Left to right: time, UPD badge, recording/focus timer, Claude light, Core dot (green answering, red Wi-Fi up but Core down, hollow no Wi-Fi), WiFi, battery %, battery.

## Hint bar
Format `KEY action`, keys in capitals, two spaces between hints, at most 3 per screen, at most 33 characters. ESC is the fixed chip on the left, never repeat it.
Good: `SPACE talk  P replay  N new`. Bad: `hold SPACE to call | 1 Foo 2 Bar 3 Baz 4 Qux`.

## Toasts
Title says what happened (max 31 chars), detail says why or what next (max 36). They auto-dismiss; do not post one for something the screen already shows.
