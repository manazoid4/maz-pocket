# MAZ Core v0.6.4 Windows recovery audit

> **Historical record.** The v0.6.4 recovery build described here was never merged; it
> targeted the llama.cpp / llama-swap runtime that shipping MAZ Core no longer uses.
> Kept for the Windows PowerShell 5.1 findings, which stayed true and drove the v0.7.1
> installer hotfix.

## Confirmed v0.6.3 stage-7 failure

The target Windows 11 / Windows PowerShell 5.1 transcript showed that the pinned llama.cpp binary was correct:

```text
version: 0.1.0-dev (build 10442, commit 9b0a2ce85)
```

The installer still aborted because `$ErrorActionPreference = "Stop"` promoted native STDERR emitted by `llama-server.exe --version` to a terminating `NativeCommandError` before the intended exit-code/build validation ran.

v0.6.4 changes runtime probes to `Start-Process` with separate stdout/stderr capture, bounded waits, real exit-code checking, and exact build/commit matching. The same rule applies to device discovery and optional GitHub CLI probing.

A second latent v0.6.3 bug was found during review: if Qwen3.5 failed at 8K context but succeeded at 4K, the later selection block configured 8K again. v0.6.4 records and persists the context that actually passed.

## Fifteen downstream failure gates

1. Native CLI writes valid information to STDERR: do not treat STDERR itself as process failure.
2. Verified archive but stale extracted runtime: re-expand the SHA-verified archive each install.
3. Vulkan device probe emits STDERR: capture safely and retain CPU fallback.
4. Ollama model directory differs: support default storage and `OLLAMA_MODELS`.
5. Existing hard link is stale: rebuild zero-copy model links every install.
6. GGUF header is valid but architecture is unsupported: require direct CPU inference.
7. Thinking model exhausts tiny preflight token budget: use 64 tokens and `enable_thinking=false` for preflight.
8. Qwen3.5 succeeds at 4K but not 8K: persist the actual successful context.
9. Vulkan/GPU route fails: separately prove Qwen3 GPU and CPU fallbacks.
10. Test/model upstream port collision: choose free direct-test ports and a free llama-swap upstream port block.
11. Another OpenAI-compatible server occupies 8790: verify process ownership before trusting `/v1/models`.
12. Windows path-with-spaces command parsing fails: quote runtime/model paths and prove generated config through real llama-swap inference.
13. Preserved `.env` contains stale bind/route settings: enforce MAZ Core 0.0.0.0:8787 and LOCAL route while preserving unrelated settings.
14. Optional `gh auth status` emits STDERR or fails: optional bridge detection cannot abort Core installation.
15. Raw inference works but Pocket-facing integration does not: require authenticated Core health, exact version, session creation, LOCAL `/turn/text`, startup recreation, and installed-location diagnostics.

## Build-C pre-rerun findings

A further review before asking the user to rerun found and fixed five more latent issues:

- direct GPU tests now query llama.cpp `/slots` and persist the **actual fitted context** rather than assuming an 8K request remained 8K;
- superseded `start-llamacpp.ps1`, `INSTALL-MAZ-CORE.cmd`, and old `VERSION` files are removed from the stable Core folder;
- generated `llama-swap.yaml` carries the exact MAZ build marker and `start-local-ai.ps1` refuses a stale config;
- llama-swap no longer overwrites the caller's `max_tokens`/temperature; it only forces `enable_thinking=false` for the Qwen pocket-assistant path;
- the independent diagnostic now uses safe native-process capture as well and reports private-LAN firewall readiness separately.

## Additional hardening

- Pinned MAZ-owned llama.cpp and llama-swap runtimes; no PATH/winget inference dependency.
- Qwen3.5 4B remains preferred; installed Qwen3 4B is separately tested fallback.
- One concurrent local inference job and five-minute idle model unload.
- llama.cpp/llama-swap remain loopback-only; authenticated MAZ Core is the LAN-facing gateway.
- Old MAZ-owned model processes are killed only when their command line points into `%LOCALAPPDATA%\\MAZ Core`; independent Ollama/other local-AI processes are untouched.
- Failure report remains sanitized and excludes `.env`/token contents.

## Validation already completed off-target

- 33/33 migrated MAZ Core Python tests passed.
- Python source AST parsing passed for all 19 `mazhost` modules.
- 25 recovery-package contract checks passed.
- Representative llama-swap YAML with Windows paths containing spaces parsed correctly.
- Final ZIP integrity test passed.

These checks are not substitutes for the target Windows run. Native EXE/model inference remains gated on the real machine.

## Release gate

Do not merge the runtime migration or publish a release until the exact v0.6.4 recovery build passes the real target Windows machine end-to-end. Physical Cardputer validation remains a separate acceptance gate after Core is green.
