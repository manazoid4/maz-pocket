# Element14 Project14 — Make a Connection

## Goal

Prepare MAZ Pocket for a short, repeatable physical demonstration and one Element14 project blog before 13 September 2026, 23:59 UK time.

The competition story is simple: MAZ Pocket is an ESP32-S3 handheld that connects a human to a PC, local/cloud AI and coding-agent workflows.

## Competition-facing surfaces

### CALL — required
Human voice -> Cardputer -> Wi-Fi -> MAZ Core -> selected AI route -> response -> Cardputer screen/speaker.

### CAPTURE — required, small scope
Prioritise Brain Dump. Keep Teach Demo only if it is physically reliable. Other capture modes are secondary.

### AGENTS — required, small scope
Prioritise Agent Status and PLAN. CREW execution and RETRO are not required for the competition demo.

### CONTROL — required
Show truthful connection state: Wi-Fi, MAZ Core, route/provider state, pairing and useful failures. Add latency/RSSI only where the existing implementation can report them truthfully.

### MEMORY / FOCUS / WORK
Do not delete working code for the competition. Hide or demote non-essential surfaces if they make the primary demo harder to understand or less reliable.

## Reliability rule

A feature belongs in the recorded demo only after it succeeds three consecutive times on the physical Cardputer ADV.

Automated tests are evidence, not proof of physical behaviour.

## Priority

1. Boot / keyboard / Wi-Fi / MAZ Core connection
2. CALL round-trip
3. Route selection/failover and useful error handling
4. CAPTURE
5. AGENTS -> PLAN
6. CONTROL diagnostics
7. Presentation polish

## Explicitly out of scope

- v1 product expansion
- new AI frameworks or runtimes solely for the competition
- LoRa / ESP-NOW / sensors added only to fit the theme
- custom PCB work
- broad architecture rewrites
- additional top-level apps

## Video target

Approximately two minutes:

1. Device + premise
2. CALL voice round-trip
3. Connection/route view
4. CAPTURE
5. AGENTS -> PLAN
6. Architecture + GitHub source

## Public repository gate

The repository must remain private until a public-release audit confirms no secrets, tokens, credentials, private debug bundles or sensitive release artefacts are exposed in the current tree or relevant Git history.
