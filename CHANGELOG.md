# Changelog

## v0.7.1 — installer hotfix

- Fixed the Windows install path. `START-HERE.cmd` runs `powershell.exe`, and Windows PowerShell 5.1 decodes a BOM-less script with the ANSI code page. Two em dashes in `setup-all.ps1` therefore arrived as cp1252 text ending in `0x94`, a smart closing quote that PowerShell accepts as a string delimiter, which unbalanced the quoting and produced misleading `}` / `elseif` parse errors at lines 85-92.
- Executed installer scripts are now plain ASCII and are packaged with a UTF-8 BOM, so neither PowerShell edition can mis-decode them.
- `START-HERE.cmd` prefers `pwsh.exe` when present and still falls back to Windows PowerShell 5.1.
- CI now builds the release package, extracts the generated `MAZ-Pocket-v<version>-Install.zip` and validates the scripts users actually download by parsing and dry-running them under real `powershell.exe`, on both the 5.1 fallback and the preferred shell.
- No firmware behaviour, product surface or MAZ Core capability changed.

## v0.7 — FIELD (release candidate)

- Home NOW priority strip and programmable quick actions 1–4.
- Global Fn+Space Context Ask with bounded current-screen context.
- Durable auto-retrying Outbox for voice, Context Ask and Beam.
- Beam short text/URL transfer with durable Core queue and Windows clipboard handoff.
- On-demand Laptop CPU/RAM/GPU/VRAM/battery/Ollama status.
- FIELD mode and Shift Clock.
- Smarter local-AI resource profiles: SMART/SAVE/FAST with adaptive context and measured Ollama usage.
- Unified MAZ Core version reporting.
- Full split client + Cardputer ZIPs are now a CI-enforced release rule.
- Documentation and Knowledge Vault architecture updated.

## v0.6

- One-entry Windows install/update flow.
- Stable per-user MAZ Core installation and Core-only USB pairing.
- Phone-first verified firmware staging to SD for M5Launcher.
- Local primary -> backup model failover.
- Hardened nonblocking local portal and authenticated LCD mirror.
- Single version source and release packaging cleanup.

## v0.5.1

- Runtime health instrumentation and bounded Host worker.
- COMM durable WAV path and safer PC actions.
- Verified installer/release workflow hardening.
