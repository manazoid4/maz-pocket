# MAZ Core v0.6

MAZ Core is the Windows companion for MAZ Pocket. The Cardputer stays a fast physical interface; Core owns speech-to-text, Ollama, factual project/PC context, safe project jobs, deterministic PC controls and Agent Nudge access.

## Recommended install

The normal v0.6 release path is **`START-HERE.cmd`** from `MAZ-Pocket-v0.6-Install.zip`. It installs/updates Core under:

```text
%LOCALAPPDATA%\MAZ Core
```

and preserves the existing `.env` across upgrades.

Inside the Core package, `INSTALL-MAZ-CORE.cmd` / `install-core.ps1` remain available as focused fallbacks.

The installer creates the Python environment/token, selects installed local Ollama models, enables local routing, discovers normal Desktop/Projects roots, adds start-at-login, starts Core immediately, enables the private GitHub bridge when `gh` is already authenticated and attempts private Tailscale Serve when Tailscale is installed.

It prints the LAN address + pairing token for MAZ Pocket. Keep port 8787 on a trusted private network; do not raw-port-forward it to the internet.

## v0.6 Core-only USB pairing

With v0.6 already installed on a USB-connected Cardputer, `pair.ps1` sends only Core address/port/token using `MAZCOREPAIR`. It **does not ask for or overwrite Wi-Fi credentials**.

The older full `MAZPAIR` protocol remains in firmware for explicit first-time Wi-Fi provisioning/backward compatibility.

## Factual tools

Core discovers configured projects and exposes only bounded operations:

- project/repo/branch/dirty/recent-commit status;
- bounded project + optional Obsidian text search;
- non-secret file reads inside configured project roots;
- `git status`, `git fetch`, `git pull --ff-only`;
- detected tests/builds for PlatformIO, Node, Python, Rust or Go projects;
- open project folder on the PC;
- Cardputer status + live LCD proxy.

Build/test operations are available as background jobs. There is **no generic shell endpoint**.

## ChatGPT / AI bridge

When enabled, Core polls the configured private GitHub repo for issues titled `[MAZ CORE] ...`. The issue body is JSON and can request only the same Core allow-list. Core writes evidence back as a private issue comment and closes the request. Unknown commands are rejected.

Authentication comes from `MAZ_GITHUB_TOKEN` when deliberately configured, otherwise from the locally authenticated GitHub CLI (`gh auth token`).

## Hidden Maz Works console

Maz Works contains unlinked/noindex browser clients at `/maz-core` and `/maz-pocket-ai`; it does not contain your Core URL or token. Supply private endpoint/token values in your own browser.

## Local AI failover

v0.6 supports:

```text
MAZ_OLLAMA_MODEL=<primary installed model>
MAZ_OLLAMA_BACKUP_MODEL=<second installed model>
MAZ_LOCAL_MODEL_POLICY=auto
MAZ_DEFAULT_ROUTE=local
```

Routing rules:

- **LOCAL:** primary -> backup -> clear local failure; never cloud.
- **AUTO:** primary -> backup -> configured cloud only after both local attempts fail.
- **CLOUD:** configured cloud directly.

`install-core.ps1` preserves working configured models and selects real installed alternatives when needed.

The model is deliberately not the source of truth for local state. Every normal conversation gets compact MAZ Core evidence; Agent Nudge evidence is added for agent questions. If evidence is missing, the model should state that it cannot verify the fact rather than inventing it.

## Serial acceptance

The bounded USB monitor remains opt-in for firmware acceptance:

- `POST /device/monitor/start`
- `GET /device` / `GET /device/logs`
- `POST /device/monitor/stop`

Stop serial monitoring before M5Launcher installs so it cannot occupy the COM port.
