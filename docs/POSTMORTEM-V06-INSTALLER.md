# MAZ Pocket / MAZ Core v0.6 installer incident postmortem

## Incident summary

The firmware release itself built and packaged correctly, but the Windows onboarding path was not production-grade. Multiple nested launchers, stale extracted copies, globally managed inference runtimes, and insufficient end-to-end validation created failures that looked like MAZ Core/model failures even when the wrong installer was being executed.

## Confirmed v0.6.3 stage-7 incident

The target Windows PowerShell 5.1 transcript showed the pinned llama.cpp binary was correct:

```text
version: 0.1.0-dev (build 10442, commit 9b0a2ce85)
```

The installer aborted because `$ErrorActionPreference = "Stop"` caused normal native STDERR from `llama-server.exe --version` to become a terminating `NativeCommandError`. The correct binary was rejected before the intended exit-code/build check completed.

v0.6.4 replaces direct native probes with `Start-Process`, separate stdout/stderr capture, bounded waits, real process exit-code checks, and exact build/commit matching.

The same audit found a second latent v0.6.3 bug: an 8K GPU attempt could fail, a 4K retry could pass, and the selection code would still configure 8K again. v0.6.4 persists the exact context that actually passed.

## Root causes

1. Stale package execution was insufficiently obvious.
2. The first llama.cpp migration depended on a global package-manager/PATH runtime.
3. GGUF magic was treated as stronger compatibility evidence than it is.
4. One model/runtime pairing could block the whole local-AI path.
5. Health checks proved process liveness rather than real inference and Pocket-facing behavior.
6. Core and firmware component version identities were muddled.
7. Failure diagnostics were too hidden.
8. Windows PowerShell 5.1 native STDERR semantics were not represented in CI/testing.
9. Context fallback state was not carried through to final config.
10. Fixed local test/upstream ports created avoidable collision risk.
11. Superseded direct-llama launcher files could survive in the stable Core directory.
12. llama.cpp auto-fit could lower context without the installer recording the actual value.
13. llama-swap filters could accidentally override diagnostic generation limits.
14. A stale llama-swap config could be reused after a Core build change.
15. The independent diagnostic originally repeated the PowerShell native-STDERR pattern.

## Recovery design

- Firmware remains v0.6-compatible; Core can patch independently.
- MAZ owns pinned/hash-verified llama.cpp + llama-swap binaries under LocalAppData.
- Qwen3.5 4B is preferred; existing Qwen3 4B is separately proven as fallback.
- Direct CPU inference proves parser/model compatibility before GPU tests.
- GPU 8K is attempted safely, then 4K; `/slots` is used to retain the actual fitted context.
- llama-swap gets dynamically selected upstream ports, one-model concurrency, and a five-minute TTL.
- llama-swap/llama.cpp are loopback-only; authenticated MAZ Core remains the LAN gateway.
- A real llama-swap completion and a real MAZ Core session/LOCAL turn are required before READY.
- Stale MAZ-owned processes are cleaned only when their command lines point into `%LOCALAPPDATA%\\MAZ Core`.
- Superseded `start-llamacpp.ps1` / old Core launcher files are removed from the stable install.
- llama-swap config is tagged with the exact MAZ build ID and startup refuses stale config.
- Optional GitHub integration cannot fail the base installer.
- Installed-location diagnostics and a sanitized Desktop failure report remain mandatory.

## Off-target validation before the next Windows rerun

- 33/33 MAZ Core Python tests passed with the migrated local-backend expectations.
- Python AST parsing passed for all 19 `mazhost` modules.
- 25 recovery contract checks passed.
- Representative llama-swap YAML with Windows paths containing spaces parsed correctly.
- Final recovery ZIP integrity passed.

These checks are not a substitute for the real Windows/NVIDIA run. The exact recovery build remains gated on the target machine before merge/release.

## Release gate

Do not merge or publish the runtime migration until the exact recovery package passes on the real Windows target. After that, physical Cardputer acceptance remains separate: voice, Wi-Fi loss/recovery, Core loss/recovery, phone portal, SD staging, M5Launcher rollback, USB Core-only pairing, firewall/LAN reachability, and soak testing.
