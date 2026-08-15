# MAZ Core v0.5

MAZ Core is the Windows companion for MAZ Pocket. The Cardputer stays a fast physical interface; Core owns speech-to-text, Ollama, factual project/PC context, safe project jobs, deterministic PC controls and Agent Nudge access.

## One-shot install

```powershell
.\install-core.ps1
```

It creates the Python environment/token, pins `lfm2.5-8b-a1b-gpu:latest`, enables local routing, discovers normal Desktop/Projects roots, adds start-at-login, starts Core immediately, enables the private GitHub bridge when `gh` is already authenticated and attempts private Tailscale Serve when Tailscale is installed.

It prints the LAN address + pairing token for MAZ Pocket. Keep port 8787 on a trusted private network; do not raw-port-forward it to the internet.

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

## ChatGPT bridge without Codex

When enabled, Core polls the configured private GitHub repo for issues titled `[MAZ CORE] ...`. The issue body is JSON and can request only the same Core allow-list. Core writes evidence back as a private issue comment and closes the request. Unknown commands are rejected.

Authentication comes from `MAZ_GITHUB_TOKEN` when deliberately configured, otherwise from the locally authenticated GitHub CLI (`gh auth token`).

## Hidden Maz Works console

The public Maz Works code contains only an unlinked/noindex browser client at `/maz-core`; it does not contain your Core URL or token. Enter a private HTTPS endpoint and token in your own browser. The endpoint is kept in localStorage and the token only in sessionStorage.

## Local AI

Default:

```text
MAZ_OLLAMA_MODEL=lfm2.5-8b-a1b-gpu:latest
MAZ_DEFAULT_ROUTE=local
```

The model is deliberately not the source of truth for local state. Every normal conversation gets compact MAZ Core evidence; Agent Nudge evidence is added for agent questions. If evidence is missing, the prompt requires the model to say it cannot verify the fact instead of inventing it.

## Serial acceptance

The older bounded USB monitor remains opt-in for firmware acceptance:

- `POST /device/monitor/start`
- `GET /device` / `GET /device/logs`
- `POST /device/monitor/stop`

Stop serial monitoring before M5Launcher installs so it cannot occupy the COM port.
