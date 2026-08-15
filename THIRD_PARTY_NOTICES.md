# MAZ Pocket third-party notices

This file records the dependency and research boundary used for MAZ Pocket v0.5.1.
It is not a claim that every project below is redistributed by MAZ Pocket.

## Shipped / linked firmware dependencies

| Component | Version / role | License | Source |
|---|---|---|---|
| M5Unified | 0.2.19, Cardputer/audio/power hardware abstraction | MIT | https://github.com/m5stack/M5Unified |
| M5GFX | 0.2.26, display/canvas | MIT | https://github.com/m5stack/M5GFX |
| QRCode by Richard Moore / Project Nayuki | 0.0.1, QR generation | MIT | https://github.com/ricmoo/QRCode |
| ArduinoJson | 7.2.1, bounded protocol/status JSON | MIT | https://github.com/bblanchon/ArduinoJson |
| Arduino-ESP32 / ESP-IDF framework components | PlatformIO espressif32 6.9.0 framework | Apache-2.0 and component-specific upstream licenses | https://github.com/espressif/arduino-esp32 ; https://github.com/espressif/esp-idf |
| esp_websocket_client | framework WebSocket client used by COMM | Apache-2.0 | https://github.com/espressif/esp-protocols/tree/master/components/esp_websocket_client |

The applicable upstream license texts and copyright notices remain authoritative.
The QRCode upstream license includes copyright notices for Richard Moore and Project Nayuki.

## Host-side model option

`host/models/Modelfile.maz-pocket-lite` creates the optional local model
`maz-pocket-lite:latest` from `qwen3.5:4b-q4_K_M` using Ollama. The model weights
are downloaded by the user from Ollama and are not stored in this repository or
inside MAZ Pocket firmware. Qwen3.5-4B is published under Apache-2.0 by Qwen.

Source: https://huggingface.co/Qwen/Qwen3.5-4B
Ollama package: https://ollama.com/library/qwen3.5

## Research / design references — no source copied into MAZ Pocket

The following projects were inspected for architecture, interaction patterns,
protocol choices, or validation ideas. Their source code is **not vendored or
copied into MAZ Pocket v0.5.1**.

| Project | What was studied | License observed |
|---|---|---|
| 78/xiaozhi-esp32 | persistent WebSocket voice/control session pattern | MIT |
| dakshaymehta/cardputer-claude-os | Cardputer + agent/assistant workflow patterns | Apache-2.0 |
| tnm/zclaw | small-device metrics/tooling patterns | MIT |
| bmorcelli/Launcher | Launcher-owned install/update and rollback model | MIT |
| m5stack/M5Cardputer | keyboard/input implementation reference | repository metadata does not declare a license; reference-only |
| BruceDevices/firmware | compact handheld UI inspiration only | AGPL-3.0 |

Bruce is intentionally treated as visual/product research only. No Bruce source
is included, adapted, linked, or used as a firmware dependency, so MAZ Pocket
does not import the AGPL codebase through this work.

`esp-sr` VAD/AFE material was considered during research but is not included in
v0.5.1. VAD is therefore not represented as a shipped dependency or feature.

## MAZ Pocket implementation boundary

The v0.5.1 streaming protocol, bounded queues, session/turn IDs, cancellation,
SD-first WAV fallback, action-ID schema, metrics screens, and Host routing were
implemented specifically for MAZ Pocket. Where a standard library/framework API
is used, MAZ Pocket calls that API rather than copying an upstream implementation.
