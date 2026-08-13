# Community feature research — what Cardputer owners actually use

Research pass run 2026-08-13, before the MAZ Pocket v0.1 feature set was frozen.

## Method and its limits

Reddit blocks direct fetching from this environment (`www.reddit.com` and
`old.reddit.com` both refused), so threads were reached through the Exa index
and mirrors, plus the M5Stack community forum, which is where the more detailed
usage discussion actually happens. Sources read:

- r/CardPuter — "What are you currently using your CardPuter for day to day?"
- r/CardPuter — "Should I get a CardPuter?"
- r/CardPuter — "Just got my CardPuter and I'm new to this. What's the…"
- M5Stack forum — "Cardputer ADV Development / Usage", "OS for the Cardputer",
  "M5Apps — multiple apps installer" (incl. its curated TOP-20 app list),
  "Fully featured audio player for Cardputer", "Cardputer ADV Noobie"
- Raspberry Pi Official Magazine, Cardputer review
- fluxcoil.net, "The Cardputer M5 for LoRa, Mesh and more" (2026-05)

**Honest caveat:** this is a read of about a dozen threads, not a survey. Where
a claim rests on a single post it is marked as such. Comment-level sentiment was
read where the mirror exposed comments; some threads exposed only the post body.

## What the research actually says

The loudest signal is uncomfortable and worth stating plainly: **most owners
struggle to name a daily use.** The Raspberry Pi magazine review lands on "it's
a fun gadget, but we're not sure what to use it for", and the r/CardPuter
day-to-day thread is largely people asking each other the same question. What
*does* get used repeatedly clusters into four groups:

1. **Firmware hopping itself** — Bruce, Nemo, Meshtastic, M5Launcher. The most
   cited actual activity. MAZ Pocket must not fight this; it should be one more
   good entry in that list.
2. **Running small personal programs on a pocket device with a keyboard.** The
   "Should I get a CardPuter?" poster is explicit: an automation specialist who
   wants "a bunch of calculators and small programs" in his pocket at work.
3. **Media and utility apps** — the M5Apps TOP-20 curated list shows what people
   install: an MP3 player, a guitar tuner, Snake, a two-panel file manager
   (FINDER), a partition tool.
4. **Meshtastic / LoRa**, which needs the LoRa cap and is a different product.

Nobody in the sources describes carrying a Cardputer for note-taking. That is
either a gap or a warning. MAZ Pocket bets it is a gap — and the bet is only
defensible because of the voice hardware the ADV added.

## Scored candidates

Score = usefulness × frequency × ease × fit. Anything scoring only on "possible"
was rejected.

| Feature | Community signal | Why people like it | Complexity | Fit | Decision |
|---|---|---|---|---|---|
| **Calculator** | Explicit ask in "Should I get a CardPuter?" | Physical number row beats a phone keypad for `1250*0.2` | Low — expression parser, ~120 lines | High: keyboard-first, offline | **Include** |
| **Text/file viewer** | FINDER in M5Apps TOP-20; AppTextEditor in M5's own UserDemo | Reading reference material without a laptop | Low | High: prompts, project notes | **Include** |
| **Stopwatch/timer** | Timer app ships in M5's UserDemo; generic but always present | Instant, no unlock, no notifications | Low | Medium-high, distinct from Focus | **Include** |
| **QR generator** | Not loudly requested, but the only cable-free path off the device | Moves a URL or Wi-Fi password to a phone in seconds | Low — MIT `ricmoo/QRCode` | High: solves a real transfer gap | **Include** |
| **Password/passphrase generator** | Weak direct signal; common in handheld firmware | Hardware RNG, no cloud, one keypress | Very low | Medium — cheap, genuinely used | **Include** |
| **Snippets / stored text** | Implied by the "small programs I actually use" cluster | Frequently typed strings on a device with no clipboard | Low | High — becomes prompt fragments in v0.2 | **Include** |
| **Snake / tiny game** | In the M5Apps TOP-20 curated list | Boredom-breaker | Low | Low for MAZ Pocket's stated purpose | **Later** — earns a place only once the device is already carried daily |
| **IR remote** | ADV has an IR emitter (G44); IR apps in UserDemo and Bruce | Convenient for own devices | **Medium-high** — NEC timing, per-device codes, needs hardware to verify | Medium | **Later** — cannot be verified without the device in hand; shipping unverified IR is worse than shipping none |
| **MP3 / audio player** | CMUS and the Winamp-style player are among the most discussed community apps | Genuinely popular | Medium-high — decoder, and the ADV has no PSRAM | Low — duplicates a phone with no advantage | **Reject** |
| **Meshtastic / LoRa** | Very high interest | Real off-grid use | High + extra hardware | None — needs a LoRa cap | **Reject** (dedicated firmware exists) |
| **Wi-Fi/BLE packet tools, spam, jammers** | High volume in Bruce discussion | Flipper-adjacent novelty | Medium | **Actively wrong for MAZ Pocket** | **Reject** — Bruce is one launcher entry away |
| **BadUSB / HID injection** | Popular in Bruce | — | Medium | Out of scope, offensive | **Reject** |
| **Emulators / Doom** | Present and popular | Fun | High | None | **Reject** |

## What shipped in v0.1

Six community-derived items, all offline, none needing extra hardware:
Calculator, Stopwatch, QR Code, Text Viewer, Generator, Snippets.

## What this research changed about the product

1. **Diagnostics got smaller, not bigger.** The temptation on this hardware is a
   scanner suite. The research says that space is fully served by Bruce and that
   duplicating it makes MAZ Pocket less distinct, so Tools was cut to "is my
   hardware working" only.
2. **The daily-use problem is real, and voice is the answer or nothing is.**
   Since no source describes carrying a Cardputer for text capture, MAZ Pocket
   leans on the one thing the ADV has that a phone makes awkward: a physical
   push-to-talk button on a device with no feed behind it. If hold-SPACE-to-capture
   does not become a habit, the honest conclusion is that this device has no
   daily job, and no number of extra utilities will fix that.
