# nod setup

## Audit findings (before this change)
1. Seven scripts had to be run by hand: setup.ps1, install-core.ps1, install-core-task.ps1, BOOTSTRAP-AUTOUPDATE.cmd, setup-remote.ps1, install-nodflow.ps1, pair.ps1. Two of them (install-core-task, point-core-task) registered the same "nod Core" task by two different recipes (copied code vs. running from the git checkout).
2. The device stored `Cfg.hostAddr` and `Cfg.hostToken`; the only on-device way to set them was Connections > C, which asks for an IP and then the full MAZ_TOKEN, typed by hand. USB `pair.ps1` pushed the token over serial; the web portal also had a token field.
3. Core already had `host/mazhost/pairing.py`: 8-char code, `POST /pair/start` (bearer-authed), `POST /pair/claim` (no auth, returns `{token}`), 5-minute single use, 8 tries per code, 10 claims/min/IP. `/pair/*` except `start` is refused for remote (Funnel) peers by `security.py`, so the claim is LAN-only already.
4. The device only used that flow in the other direction (CONTROL > PAIRING + PHONE shows a code for a phone). Nothing let the device claim a code from the PC.
5. `mazpocket.local` (and `MAZ_CARDPUTER_URL` in Core's config/health) is the DEVICE's own mDNS name, so it cannot be used to find Core. The device had no way to discover Core at all.
6. `/health` is already unauthenticated and minimal (`{ok,name:"nod Core",version}`), which makes a LAN probe safe.
7. Core self-update (`selfupdate.py`) read GitHub Releases and showed a raw `HTTPStatusError` for a private repo (GitHub answers 404).
8. Firewall rule creation needed an admin PowerShell; without it the device could not reach Core.
9. Tests and the release workflows reference `setup.ps1`, `install-core.ps1` and `pair.ps1`, so those files are unchanged and only documented as legacy.
10. nod Flow (dictation hotkey) was a separate manual script; its libraries are part of `requirements.txt` and can fail on their own.

## What it is now
1. Double-click `host\nod-setup.cmd` (or the standalone `nod-setup.cmd` release asset, which fetches the installer and clones the repo). No questions. Safe to re-run.
2. It installs Python/Git via winget only if missing, creates the venv under `%LOCALAPPDATA%\MAZ Core`, installs libraries, creates `MAZ_TOKEN` if missing (never printed), registers the "nod Core" scheduled task running from the git checkout, and allows ports in the Windows Firewall (one UAC prompt, only if a rule is missing).
3. It asks Core to self-update (`POST /core/update/check`) which stages the latest firmware. The repo is public, so no token is needed.
4. It enables the nod Flow hotkey (hold Right Ctrl) only if its libraries imported.
5. It prints "nod Core is running", an 8-character code and the one next step.
6. On the Cardputer press **Ctrl+U** (while unpaired) or Settings > Connect to PC. The device looks for Core: last known address, then a UDP broadcast Core answers (`NOD-CORE?` on port 8787), then a /24 scan of port 8787. Type the code; the device receives the key over your home network and stores it. Nobody types the token.

Errors on the device: "Core not found", "Wrong code - or expired. Rerun nod-setup" (Core deliberately gives one answer for wrong, expired and used codes), "Too many tries", "Wi-Fi is not connected".

Advanced (hidden): Connections (Settings > Wi-Fi & host) key `C` still accepts an address and token by hand.

## Troubleshooting
- Device says Core not found: same Wi-Fi as the PC? Windows network set to Private? Re-run `nod-setup.cmd` and click Yes on the Windows prompt.
- Code expired: re-run `nod-setup.cmd`; it prints a new code.
- Private repo: `GET /core/update` shows `repo is private - set MAZ_GITHUB_TOKEN or make it public`.
- Away from home: `host\setup-remote.ps1` (Tailscale Funnel). Pairing itself stays on the home network.
- Logs: `%LOCALAPPDATA%\MAZ Core\logs\nod-setup.log`.

## Obsolete scripts
`INSTALL-MAZ-CORE.cmd`, `BOOTSTRAP-AUTOUPDATE.cmd`, `install-core-task.ps1`, `point-core-task-at-checkout.ps1`, `install-nodflow.ps1`, `update-core.ps1` now forward to `nod-setup`. `setup.ps1`, `install-core.ps1` and `pair.ps1` remain only for the legacy release package.
