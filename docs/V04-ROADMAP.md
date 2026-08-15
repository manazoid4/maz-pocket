# MAZ Pocket v0.4 capability roadmap

This is the approved product direction after v0.3. It is deliberately broader
than three apps without becoming an app dump.

## Product rule

**Few primary surfaces, many capabilities underneath them.** A feature earns a
new Home surface only when it introduces a genuinely different interaction.
Everything else is a tool exposed through an existing surface or through the
host-side tool protocol.

## Six primary surfaces

1. **COMM** — realtime voice/text communicator: persistent conversations,
   lower-latency streaming, interruption, local/auto/cloud routing, spoken
   replies, agent tool-progress and remote access.
2. **CAPTURE** — instant voice/text capture: Raw / Clean / Build modes,
   highlights, offline queueing, project-aware storage and direct send to
   Codex/Claude/local agents.
3. **OPS** — agent operations: fleet state, NEEDS MAZ approvals, start/cancel
   work, nudge, inspect results/evidence, CI/build state, session health and the
   three-day cross-agent notes/sync check.
4. **DESK** — computer control: USB/BLE HID where appropriate, voice typing,
   mouse/scroll, app switching, media/volume, desktop/lock, bidirectional
   clipboard, bounded user macros, current PC context and optional Wake-on-LAN.
5. **RECALL** — personal/project retrieval: captures, notes, agent outputs,
   question answering over stored context, recent/pinned material, SD reference
   cache and reusable prompt/snippet access.
6. **FLOW** — reminders, schedules, one-shot/recurring automations, custom host
   tools, workflows, smart-home/IR actions, computer routines and
   project-specific actions.

## Background layer: PULSE

PULSE is not another app. It surfaces agent completion, CI failure, approvals,
reminders, host disconnect/reconnect, background-job results and priority
notifications at the moment they matter.

## PC/context intelligence

MAZ Host should progressively expose the active application, current project,
VS Code/repo context, git branch, CI state, machine/network state, common work
routines and deliberate context handoff to an agent.

## mazpocket.local

The local browser control plane is now a first-class product surface. Target
capabilities are:

- live device, network, host and agent status;
- authenticated configuration of Wi-Fi and MAZ Host routing;
- browser firmware OTA;
- speaker and microphone self-tests;
- diagnostics and recovery controls;
- safe hand-back to M5Launcher;
- later: backup/restore, action/tool management and richer logs.

The site is device-hosted at `http://mazpocket.local` over mDNS. It is not a
public Internet admin panel. Mutating actions reuse the MAZ pairing token.

## Offline / edge direction

Keep useful deterministic work on the ESP32: command routing, cached actions,
local reminders, notes/reference access and graceful LAN/Internet/host fallback.
Do not force a general LLM onto Cardputer ADV. A tiny local model is acceptable
only when a narrow classifier/router measurably reduces latency or dependence.

## Existing utilities retained

Calculator, Stopwatch, Focus, Tasks, Notes, Beam/QR, Text Viewer, Generator,
Recorder, Snippets and diagnostics remain behind Ctrl+K unless one becomes
important enough to merge into a primary surface.

## Hardware capabilities to exploit

- BMI270 gestures/tilt shortcuts;
- native USB HID, with M5Launcher/update compatibility kept intact;
- Bluetooth HID where it earns a real workflow;
- IR transmit/control;
- microSD storage;
- Wi-Fi + USB recovery/control;
- ESP-NOW peer/device links;
- optional LoRa/mesh hardware later;
- Grove/EXT sensors and modules through explicit tools.

## Extras policy

Snake and Hyperdrive may remain hidden extras because they cost little and do
not influence the product architecture. Future novelty features must stay small,
hidden and removable. They never get priority over reliability or core tools.

## Delivery order

### Now

- keep COMM / LOG / OPS proven;
- make `mazpocket.local` the normal configuration/diagnostic/update surface;
- preserve USB updater, ArduinoOTA and M5Launcher recovery paths;
- keep the full v0.4 capability contract in-repo.

### Next

- promote DESK into a dedicated surface;
- add voice typing + clipboard + bounded macros;
- add agent job dispatch/approval in OPS;
- introduce host-side generic tool discovery so FLOW does not require firmware
  changes for every integration;
- add RECALL search over the user's existing project/capture systems;
- move COMM toward interruptible streaming only after real-device latency is
  measured.

### Later

- six-surface Home once DESK/RECALL/FLOW each have enough real functionality to
  deserve their icon;
- PULSE event delivery;
- optional BLE/IR/ESP-NOW/LoRa tool packs;
- offline specialist classifier if its resource cost is justified.

## Non-negotiables

- no arbitrary remote shell;
- no hidden auto-approval of agent/tool actions;
- credentials stay PC-side where possible and are never rendered by the web UI;
- authenticated network mutation and OTA;
- raw capture survives before AI processing;
- physical acceptance remains separate from compile/CI success;
- every major feature must reduce friction, remove another device/step, or
  surface actionable information.
