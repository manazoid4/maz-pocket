# MAZ Pocket v0.6 — synthesis / selected build

This pass reconciles Audit A (daily usefulness/friction) and Audit B (technical reliability). Selection rule: maximize actions saved and failures avoided per unit of firmware/Core risk.

## Ranked candidates

| Candidate | Daily utility | Risk | Effort | v0.6 decision |
|---|---:|---:|---:|---|
| One-click unified Windows setup | 10/10 | Low | Medium | **SHIP** |
| Phone-first firmware staging as normal update path | 10/10 | Low — already implemented | Low | **SHIP / PROMOTE** |
| Canonical version + CI consistency | 9/10 | Very low | Low | **SHIP** |
| Trusted-browser pair persistence + forget button | 9/10 | Low | Low | **SHIP** |
| Core-only USB pairing without Wi-Fi password | 9/10 | Low | Low | **SHIP** |
| Local Ollama primary→backup failover | 9/10 | Low-medium | Low | **SHIP** |
| Full realtime COMM streaming | 10/10 potential | High | High | **DEFER TO v0.6.x after hardware proof** |
| Generated action-ID protocol schema | 5/10 user-visible | Medium | Medium | Defer |
| More apps/games | 2/10 | Low | Medium | Reject |
| Self-flashing OTA | 6/10 | High / rollback risk | Medium | Reject; keep M5Launcher |

## v0.6 product promise

**MAZ Pocket v0.6 is the friction release.** It should make the existing useful system easier to install, pair, update and keep online rather than expanding the menu.

### Fresh install
1. Download one `MAZ-Pocket-v0.6-Install.zip`.
2. Extract it.
3. Double-click `START-HERE.cmd`.
4. The setup installs/updates MAZ Core in a durable per-user location, starts it, preserves existing configuration, detects optional USB Cardputer pairing, detects a single removable microSD when present, and opens the local device portal.
5. M5Launcher remains the only firmware installer.

### Existing v0.5.2+ device
1. Download `Maz-Pocket-v0.6-M5Launcher.bin` on phone.
2. Open `http://mazpocket.local` on the same LAN.
3. Browser stays paired if previously trusted.
4. Select the `.bin` → verify/stage to SD → M5Launcher → Install → Launch.

### AI behavior
- LOCAL: primary Ollama model → backup installed Ollama model → fail clearly; **never cloud**.
- AUTO: primary → backup → configured cloud.
- CLOUD: configured cloud directly.

## Guardrails

- No arbitrary remote shell.
- No firmware self-flash or partition writer.
- No secret committed into Maz Works.
- No claim of physical ADV validation until physical acceptance is run.
- No app-count expansion in v0.6.

## Acceptance criteria

- `VERSION` is `0.6` and firmware/CI/install artifacts derive from it.
- One release ZIP contains `START-HERE.cmd`, unified PowerShell setup, firmware, Core bundle, legacy safe SD helper, Quickstart, release notes and checksums.
- Existing `.env` survives Core upgrade.
- Core runs from `%LOCALAPPDATA%\MAZ Core`, not a disposable Downloads extraction.
- USB-connected Cardputer can receive Core address/token without re-entering Wi-Fi credentials.
- `mazpocket.local` remembers pairing on a trusted browser and exposes a forget action.
- Host tests prove local failover order and LOCAL no-cloud rule.
- Cardputer build remains below the known M5Launcher slot ceiling.
- CI is green before merge/release.

## Explicitly not blocking v0.6

Realtime COMM streaming remains the next high-leverage experiment, but only after the v0.6 friction/reliability baseline is physically exercised on the Cardputer ADV. The durable WAV→Core path remains the production voice transport in this cut.