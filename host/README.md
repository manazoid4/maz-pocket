# MAZ Core v0.6.4 — recovery build

Core-only compatibility update for MAZ Pocket firmware v0.6. **No Cardputer reflash is required.**

## What changed

- One obvious installer identity: `v0.6.4 / MAZCORE-064-RECOVERY-20260815C`.
- No `winget` dependency for local inference.
- Pinned, SHA-256 verified llama.cpp **Vulkan and CPU** runtimes live under `%LOCALAPPDATA%\MAZ Core\runtime`.
- Pinned llama-swap v250 supervises model processes on `127.0.0.1:8790`.
- Reuses existing Ollama GGUF files without deleting or modifying Ollama.
- Primary preference: Qwen3.5 4B. Stable fallback: Qwen3 4B.
- Installer proves CPU parsing/inference first, then GPU inference, then llama-swap, then MAZ Core, then a real MAZ Core chat turn.
- GPU failure is not fatal if a proven CPU route exists.
- Model failure is not fatal if the proven backup model works.
- Independent post-install self-test validates runtime identity, models, supervisor, Core, startup and LAN-firewall readiness.
- Sanitized failure report is written to the Desktop; secrets are not included.

## Runtime topology

`MAZ Pocket -> MAZ Core :8787 -> llama-swap :8790 -> tested llama.cpp backend -> tested local GGUF`

Only MAZ Core is LAN-facing. llama-swap and llama.cpp remain loopback-only.

## Resource policy

- one inference request per model at a time;
- 8K target context when the primary GPU path proves healthy;
- 4K safe fallback context;
- actual fitted context is read back from llama.cpp rather than assumed;
- 5-minute idle unload via llama-swap;
- no giant 262K context allocation;
- no arbitrary shell endpoint;
- LOCAL route never spills to cloud.

## If it fails

Run `DIAGNOSE.cmd` or share `Desktop\MAZ-Core-FAILED.txt`. The report contains versions, failing stage and log tails but deliberately omits `.env` and the MAZ token.
