# MAZ Pocket / MAZ Core v0.6 installer incident postmortem

## Incident summary

The firmware release itself built and packaged correctly, but the Windows onboarding path was not production-grade. Multiple nested launchers, stale extracted copies, globally managed inference runtimes, and insufficient end-to-end validation created failures that looked like MAZ Core/model failures even when the wrong installer was being executed.

## Independent audit tracks

The review was deliberately split into six independent tracks before synthesis:

1. **Packaging / release identity** — can a user accidentally execute stale code?
2. **Windows installer lifecycle** — extraction, paths, ports, processes, startup and permissions.
3. **Local inference runtime** — llama.cpp build compatibility, CPU/GPU backend and GGUF parsing.
4. **Model strategy** — Qwen3.5 primary, known-stable Qwen3 fallback, bounded context and VRAM policy.
5. **MAZ Core API** — authentication, version identity, local/cloud routing and actual end-to-end chat.
6. **Pocket/Core system boundary** — firmware remains v0.6-compatible; Core updates must not require reflashing.

## Root causes

### RC1 — stale package execution was not detectable enough
A user could have several extracted `MAZ-Core-*` folders. The old v0.6.1 launcher and newer package had similar filenames. Nothing in the old installer refused to run once superseded.

**Fix:** unique build ID, unmistakable banner, stable installed build verification, stale-process check, and a diagnostic that flags old paths.

### RC2 — v0.6.1 depended on a global llama.cpp install
The first migration invoked the package manager and then trusted command aliases/PATH. That made actual runtime build selection dependent on machine state.

**Fix:** MAZ owns pinned, hash-verified llama.cpp binaries inside LocalAppData and calls them by absolute path.

### RC3 — GGUF magic was treated as sufficient compatibility proof
`GGUF` only proves container format. It does not prove that a particular llama.cpp build can load that architecture/tensor layout.

**Fix:** direct CPU inference test, then GPU inference test, before llama-swap is configured.

### RC4 — no stable model fallback outside the same failing architecture
Qwen3.5 is desirable, but new architectures can hit runtime regressions. A daily driver should not become unusable because one model/runtime pair regresses.

**Fix:** use existing `qwen3:4b-q4_K_M` as a separately tested backup. llama-swap ensures only the requested model is loaded.

### RC5 — health checks were too shallow
A process answering `/health` does not prove model generation, MAZ authentication, session creation or Pocket-facing routes.

**Fix:** install now requires actual chat completion through raw llama.cpp, through llama-swap, and finally through MAZ Core `/session/start` + `/turn/text`.

### RC6 — component versioning was muddled
Firmware can remain compatible while MAZ Core needs rapid patch releases. Treating one release number as the identity of every component creates pressure to reflash unnecessarily and encourages stale hard-coded versions.

**Fix:** Core has `CORE_VERSION` + `BUILD-ID`; firmware compatibility remains a separate protocol/release concern. Python API version values read the installed Core version instead of hard-coding `0.5.0`.

### RC7 — installer logs were hidden from the user
The previous failure told the user to find a log file manually.

**Fix:** failure creates a sanitized `MAZ-Core-FAILED.txt` on Desktop and identifies the exact failing stage.

### RC8 — generated packages contained development debris
`__pycache__` / `.pytest_cache` appeared in one generated package.

**Fix:** release package excludes caches and generated interpreter artifacts.

## v0.6.3 failure matrix

The recovery installer/self-test explicitly covers:

1. wrong/stale build identity;
2. running from Windows temporary ZIP extraction;
3. unsupported 32-bit OS / critically low disk space;
4. stale MAZ-owned processes;
5. non-MAZ port collision;
6. broken Python venv/dependencies;
7. corrupted runtime download/hash mismatch;
8. wrong llama.cpp build on PATH (eliminated by absolute pinned paths);
9. missing/corrupt Ollama manifest/blob;
10. valid GGUF container but incompatible model architecture;
11. GPU/Vulkan backend failure or insufficient VRAM;
12. primary Qwen3.5 failure;
13. backup Qwen3 failure;
14. llama-swap configuration/start failure;
15. local OpenAI-compatible inference failure;
16. authenticated MAZ Core health/version mismatch;
17. complete MAZ Core session/chat failure and stale startup path.

## Release gate going forward

A Windows Core package is not considered releasable merely because Python unit tests pass. It must also pass a Windows smoke job that downloads the exact pinned runtimes, validates hashes, parses the generated config, launches a tiny test GGUF or mocked server path, validates installer build identity, and confirms that no deprecated installer command is reachable.

Physical Cardputer acceptance remains separate: voice capture, Wi-Fi loss/recovery, Core loss/recovery, phone portal, SD staging, M5Launcher rollback, USB Core-only pairing, and a soak test.
