# Maz Pocket v0.3 product contract

Maz Pocket is not a tiny phone and not an ESP32 demo collection. It is a pocket
field terminal for one person: a communicator to the PC, a field log, and an
operations console for agents.

## Home has three surfaces

1. **COMM** — push-to-talk conversation with the PC plus a six-command Windows
   deck. LAN first, authenticated HTTPS fallback when away from home.
2. **LOG** — immediate voice capture with marks/highlights, raw-audio-first
   storage, then useful structuring when MAZ Host is available.
3. **OPS** — factual Agent Nudge state, evidence and explicit nudge action.

Every older utility can remain installed behind Ctrl+K while it is cheap and
working, but it does not earn a Home tile merely because it exists.

## Retro future means behaviour, not decoration

The design takes broad interaction inspiration from the future imagined by old
science fiction without copying any protected art or branding:

- a **Star Trek-style communicator** idea: take the device out, hold one control,
  speak, hear the remote computer answer;
- **KITT / talking-computer** interaction: common machine controls can be spoken
  and happen immediately rather than becoming a chatbot conversation;
- **Alien/Nostromo and Blade Runner field-terminal** thinking: terse status,
  explicit state words, amber/cyan instrument colours, no glossy app chrome;
- the old **captain's log** idea: speak a thought into a portable terminal and
  preserve the original recording before any AI touches it;
- a **mission-control panel** idea: Home tells you whether the PC is reachable
  and whether the agent fleet is clear, working, waiting, stale or needs you.

The useful fantasy is "my pocket terminal talks to and controls my machines",
not "my pocket terminal has animations that look futuristic".

## External project lessons retained

### slvDev/esp32-ai
Retain the edge-first lesson: do cheap deterministic decisions locally and
escalate generative reasoning. Do **not** ship its showcased 28.9M model on
Cardputer ADV; the demonstrated target expects substantially more memory/flash.

### Jasapetek/ESP32-S2-Wireless-PC-Remote-Controller
Retain the idea of immediate computer controls. Maz Pocket implements a much
smaller allow-list through MAZ Host: desktop, play/pause, mute, volume down,
volume up and lock. It deliberately does not expose arbitrary shell commands or
power actions. The same actions can be spoken and skip the LLM.

### FabrikappAgency/esp32-realtime-voice-assistant
Retain the ESP-as-I/O / PC-as-intelligence architecture, bidirectional audio and
interruptible playback. Do not add WebSocket streaming to v0.3: the existing
raw-WAV request path is simpler, already proven on this hardware and keeps the
failure/offline queue straightforward. Revisit streaming only if measured
latency on real hardware is the bottleneck.

### gelotek-com/esp32-AI-personal-assistant
Retain the simple PC STT -> model -> PC TTS -> ESP audio-return shape. No source
code is copied from this project; its README restricts commercial use. Cardputer
already has its own speaker, so Bluetooth-speaker support adds no value here.

### tnm/zclaw
Retain the discipline: deterministic tools, persistent state, recovery/admin
paths and an explicit firmware-size budget. v0.3 now fails CI above 2,100,000
bytes even though the OTA slot is larger. Spare flash is headroom, not a feature
quota.

### Espressif ESP-SparkBot
Retain the "secondary terminal" idea: the Home screen is ambient machine/agent
status, not merely an app launcher. Do not copy camera, face recognition, games,
robot motion or screen mirroring onto hardware that does not need them.

### agucova/awesome-esp
The useful references are FreeTouchDeck/Tasmotizer-style ideas: a tiny command
surface and a friendly configuration/flashing tool. Those become CALL's command
deck and the Windows Maz Pocket Updater. The giant project catalogue is not a
backlog.

### Reddit retro-assistant reference
The supplied Reddit short link was not reliably resolvable during this pass.
The surrounding public references to retro ESP32 voice terminals reinforce a
useful constraint already used here: short character-terminal-style replies and
physical push-to-talk, not animation-heavy UI.

## Non-goals for v0.3

Do not add these unless a measured real-user problem requires them:

- games, novelty generators or demos on Home;
- camera/vision features;
- a general-purpose remote shell;
- dozens of configurable macro buttons;
- wake-word / always-listening audio;
- a generative model squeezed onto the ADV for marketing value;
- WebSocket/realtime audio merely because another project uses it;
- duplicate Notes/Tasks/Recorder features under new names;
- cloud credentials on the ESP32;
- animations, icon packs or large assets that consume flash without reducing a
  real interaction step.

## v0.3 acceptance

A feature is only considered shipped when the CI host tests, updater checks and
Cardputer ADV compile pass, then a real Cardputer passes: Home navigation,
COMM voice round-trip, spoken TTS reply, command deck, LOG capture, OPS refresh,
LAN and remote paths, USB install, Wi-Fi OTA and M5Launcher hand-back.
