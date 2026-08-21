# MAZ Pocket v0.9.0 — MAZ Work full build prompt

## Mission

Implement, verify and prepare **MAZ Pocket v0.9.0 — WORK**. This is an
implementation assignment, not another planning pass. Build the smallest
complete release that gives the owner a glanceable, truthful view of their
work and AI-agent activity, plus a frictionless **MCP Ready** experience that
finds, checks, repairs and activates configured MCP servers.

Do not publish the release merely because CI passes. Produce the release
candidate, complete the automated gates, exercise it on the physical M5Stack
Cardputer ADV, validate MCP mutations against disposable client homes, then
publish through the repository's existing release workflow.

## Read before editing

Read these sources completely and treat the current repository as truth when
it has moved beyond this prompt:

- `README.md`, `RELEASE_NOTES.md`, `CHANGELOG.md`, `QUICKSTART.txt`, `VERSION`
- `docs/RELEASE_RULES.md`, `docs/VERIFICATION.md`
- `docs/ideas/maz-work.md`
- `docs/research/work-telemetry-landscape.md`
- `.github/workflows/firmware.yml`, `.github/workflows/release.yml`
- `host/README.md`, `host/.env.example`, `host/requirements.txt`
- the current implementations under `host/mazhost/`, `src/apps/`, `src/net/`
  and `src/core/`
- official client MCP documentation:
  - Codex: <https://developers.openai.com/codex/mcp/>
  - Claude Code: <https://code.claude.com/docs/en/mcp>
  - OpenCode: <https://opencode.ai/v2/docs/mcp-servers>
  - Hermes: <https://hermes-agent.nousresearch.com/docs/user-guide/features/mcp>

At task start, inspect Git status, fetch `origin`, inspect open PRs/issues and
confirm the latest release. Work on `agents/maz-work-v0-9` or reuse an existing
matching `agents/` branch. Preserve unrelated and untracked user files. Never
push directly to `main`.

## Product result

The six Home surfaces become:

`CALL / CAPTURE / AGENTS / CONTROL / MEMORY / WORK`

There is no seventh Home tile. Rename the existing `FOCUS` surface to `WORK`
while keeping Focus Timer, Work Sprint, Tasks, Reminders and Shift Clock
available inside it. Preserve stable internal IDs where changing them would
break saved shortcuts or portal actions.

Opening **WORK** must show useful information immediately. It must not open on
a menu that makes the owner dig before seeing their day.

The initial WORK dashboard has six glanceable metrics:

1. **FOCUS** — explicit focused/shift time today and comparison with the
   previous seven matching days.
2. **AGENTS** — running, waiting and failed/attention counts from Agent Nudge
   plus normalized local agent sessions.
3. **MODEL** — current model when work is active, otherwise today's most-used
   model. Show provider when space permits.
4. **TOKENS** — known input + output + cache token usage today. Unknown fields
   remain unknown; they never silently become zero.
5. **VALUE** — estimated public API-price equivalent using a versioned local
   pricing table. Label this `API VALUE`, never `SPEND`, unless actual billed
   cost was explicitly imported.
6. **DONE** — explicitly completed tasks/sprints today. The detail view may
   also show commits, successful checks and files touched, but those signals do
   not inflate the headline DONE count.

Selecting a metric opens one compact page. Required views are **Now**,
**Today**, **Mix**, **Efficiency** and **History**. Keep full transcripts,
session replay, large tables, Gantt charts and 30/90-day heatmaps off the
Cardputer. The laptop/phone surface may expose richer details.

## Truthful metric rules

- Human focus time, AI consumption, useful output and efficiency/health are
  separate dimensions. Do not synthesize a universal productivity score.
- Agent uptime and token volume are not productive hours.
- v0.9 focus time comes from explicit Focus/Shift/Sprint sessions. Automatic
  app/window surveillance is outside this release.
- DONE counts explicit task completions and completed sprint outcomes once.
  Commits, tests and line counts are supporting evidence only.
- Missing data is `unknown`, not `0`.
- Deduplicate sessions/events using stable source + source ID + event ID keys.
- Do not store prompts, completions, file contents or raw transcript bodies for
  telemetry. Store metadata required for the dashboard.
- All telemetry stays local by default.

## Host architecture

Heavy parsing, aggregation, pricing and MCP work belongs in MAZ Core. The
Cardputer receives compact summaries and renders them. Extend the existing
bounded host-worker pattern; do not perform network, filesystem or subprocess
work on the UI loop and do not add an unbounded FreeRTOS worker.

Add cohesive host modules rather than growing `app.py` or `control_routes.py`
further. A reasonable shape is:

- `host/mazhost/work_store.py` — SQLite storage and schema migration
- `host/mazhost/work_collectors.py` — Codex/Claude event adapters
- `host/mazhost/work_service.py` — aggregation, pricing and focus/outcome rules
- `host/mazhost/work_routes.py` — authenticated API surface
- `host/mazhost/mcp_manager.py` — client adapters, scan, repair transaction
- `host/mazhost/mcp_routes.py` — authenticated status/actions

Use names that better fit existing conventions if the code shows a clearer
boundary. Keep one owner for each contract.

Store normalized telemetry under `~/.maz-pocket/work/`. SQLite from the Python
standard library is preferred. Use WAL, bounded queries, schema versioning and
incremental source cursors so a refresh does not rescan all historical logs.
Collection is lazy/cached: one caller may trigger an incremental refresh, but
repeated Cardputer/portal polls reuse a recent snapshot. A missing/corrupt
source must degrade that source only, not the whole dashboard.

### Initial telemetry sources

Support these first:

1. **Codex** — local and archived session JSONL under the active Codex home.
   Tolerate schema/version differences and discover fields rather than assuming
   every event has the same payload.
2. **Claude Code** — local project/session JSONL plus available hours/metrics
   files. Treat estimated cost fields as API-equivalent unless proven billed.
3. **Agent Nudge** — existing live agent state and attention evidence.
4. **MAZ Pocket records** — Focus, Shift, Sprint and Tasks.
5. **Git/project evidence** — existing MAZ Core project status for commits and
   checks, displayed as evidence rather than a score.

OpenCode telemetry ingestion is a follow-up unless it can be added without
delaying a complete Codex + Claude implementation. OpenCode **MCP management**
is required in v0.9.

### Normalized session fields

At minimum retain:

- source, session ID, parent/subagent relationship when known
- agent/client, provider and model
- project/worktree and bounded task summary when already provided as metadata
- started, last activity, ended, active duration and state
- input, output, cache-read and cache-write tokens as nullable integers
- API-equivalent value as nullable decimal plus pricing-table version
- tool/MCP/skill counts, error/retry/compaction counts when available
- explicit output evidence counts

Never persist credential values or raw MCP command output containing secrets.

### Work API

Expose authenticated, versioned-enough JSON contracts through the existing
MAZ Core app composition. Required behavior:

- `GET /work/summary?window=today` — six headline metrics, freshness and source
  health
- `GET /work/now` — active work/agent/model/project summary
- `GET /work/history?days=7` — bounded daily totals and deltas
- `POST /work/refresh` — starts or reuses a bounded refresh job
- explicit focus/outcome endpoints only if existing device record sync cannot
  carry these facts reliably

Return compact payloads and explicit source errors. Cap history to a safe
range. Authenticate through the existing bearer-token dependency.

## MCP Ready

Add **MCP READY** inside CONTROL and to the authenticated phone control centre.
The experience answers:

> Are the MCP servers I intend to use configured, enabled, authenticated and
> actually reachable in each installed agent client?

Support installed **Codex, Claude Code, OpenCode and Hermes** clients through
separate adapters behind one normalized status model. Detect absence cleanly.

### Normalized MCP states

Every client/server pair resolves to one of:

- `ready`
- `disabled`
- `auth_required`
- `approval_required`
- `restart_required`
- `missing_binary`
- `missing_environment`
- `invalid_config`
- `unreachable`
- `timeout`
- `conflict`
- `unknown`

Also return client version, config scope, transport, last check time, bounded
tool count and a redacted explanation. Never return command environment values,
headers, tokens, expanded secret URLs or complete subprocess output.

### Client-specific rules

- **Codex:** respect user `~/.codex/config.toml` and trusted project
  `.codex/config.toml`; support STDIO and Streamable HTTP, `enabled`, OAuth and
  tool policy. Use `codex mcp list/get/login` where they provide authoritative
  state. Do not treat `Auth: Unsupported` as a failure for a server that does
  not require auth.
- **Claude Code:** respect local, project and user scopes. Use
  `claude mcp list/get/login`. Preserve workspace trust and project approval;
  a repository cannot approve its own `.mcp.json`. Report
  `approval_required` and launch the official interactive approval path only
  after a human click. Do not write flags intended to bypass trust.
- **OpenCode v2:** read `mcp.servers`; a server is active unless `disabled` is
  `true`. Do not write the legacy v1 `enabled` shape into a v2 config. Respect
  precedence and replacement between global/project configs. Use
  `opencode mcp list/auth/debug` where appropriate.
- **Hermes:** respect `~/.hermes/config.yaml` under `mcp_servers`, including
  `enabled`, environment references, filters and auth. Prefer
  `hermes mcp list/test/login/install` over reimplementing official catalog
  behavior.

Client commands can hang while probing unhealthy servers. Run every external
probe with a hard per-client timeout and kill its process tree on expiry. One
hung Claude/OpenCode scan must not block Codex, Hermes, the API or the UI.
Start with an 8-second per-client budget and a 20-second total scan budget,
then adjust only from measured evidence. Execute independent read-only probes
concurrently with bounded concurrency.

Configuration presence alone is not health. Where client-native status is
insufficient and credentials are available without exposing them, perform a
bounded MCP initialize + tools/list probe using a pinned compatible official
MCP client library. Start local commands exactly as configured, with their
declared cwd/environment, and disconnect immediately after discovery. Never
invoke a server tool as part of health checking.

### One-click interaction

The authenticated `/control/` page gets an **MCP READY** section:

- **CHECK MCP** performs a read-only bounded scan.
- Show one card per installed client and compact server rows with state.
- Default-select existing configured servers. Never silently install a newly
  discovered arbitrary server.
- The primary action is **FIX & ACTIVATE SELECTED**.
- One click sends one structured batch request. The server enables selected
  existing entries, repairs only deterministic schema/path defects, applies
  safe cross-client definitions the user explicitly selected, verifies the
  result and returns a per-client proof report.
- If OAuth or Claude project approval is unavoidable, complete everything else
  and present one direct `SIGN IN` or `APPROVE IN CLAUDE` action. Never label
  that server ready before the official flow completes and a rescan passes.

The Cardputer CONTROL → MCP READY screen shows the aggregate result such as
`8 READY / 1 SIGN IN / 1 FAILED`, a short client list and `R` to rescan. ENTER
on a problem shows the redacted reason. Configuration mutation is initiated
from the authenticated phone page; the Cardputer may show the phone URL/QR but
must not become a secret-entry interface.

### Reliable mutation contract

MCP repair is a privileged, structured configuration mutation, not arbitrary
shell execution. The authenticated phone session is the human authorization
boundary. Record the action in the existing audit feed.

For one batch request:

1. Discover exact target files and scopes.
2. Parse every target and prepare all proposed contents in memory.
3. Reject ambiguous formats, unknown legacy shapes and duplicate normalized
   server names before writing.
4. Create a timestamped backup set plus manifest under
   `~/.maz-pocket/mcp/backups/` with restrictive permissions.
5. Write sibling temporary files, fsync, reparse and validate them.
6. Commit with atomic replacement. Maintain a transaction journal so a failure
   during a multi-file batch rolls every committed target back.
7. Re-read every file and rescan every selected client/server.
8. Report exact targets, backups, states and counts. Success requires the
   persisted end state to match the requested state.

The action must be idempotent. Repeating it against a ready system produces no
duplicate servers and no semantic config change. Preserve unrelated settings,
comments and ordering where the format allows it. Prefer official client CLI
mutations where they preserve client semantics; otherwise use format-aware
editors. Never rewrite TOML/YAML with regex.

Do not copy literal secrets from one client config into another. Cross-client
sync is allowed only for secret-free entries or environment-variable
references. Missing secrets become `missing_environment` or `auth_required`.
Redact credential-shaped material in UI, logs, tests, backups metadata and
debug capsules.

## Firmware implementation

- Change the sixth Home label from FOCUS to WORK without adding a Home entry.
- Replace the current WORK/FOCUS landing menu with a fast dashboard showing the
  six metrics. Preserve access to Focus Timer, Sprint, Tasks, Reminders and
  Shift Clock through a clear secondary action/menu.
- Prototype at the real 240x135 resolution. Each cell needs a large readable
  value, short label, selection state and unknown/error state. Prefer six
  simple cells over miniature desktop charts.
- Add compact host response structs/parsers under the existing network client
  boundary.
- Add new bounded job kinds to the single host worker for Work and MCP status.
- Cache the last successful summary so temporary Core loss leaves a visibly
  stale dashboard instead of a blank screen.
- Use exception-led color: normal metrics remain calm; waiting approval,
  failed agent, near quota/context and broken MCP become high contrast.
- Extend context snapshots with bounded factual screen state. Screen text
  remains evidence, never authorization.

## Portal and phone UI

Keep `mazpocket.local` lightweight and within the firmware image ceiling.
Update it to say MAZ Work, open WORK on the Cardputer and link directly to the
authenticated Core phone page for detailed Work and MCP management. Do not
duplicate the full analytics or config editor into firmware HTML.

Extend the MAZ Core `/control/` page with:

- Today cards matching the six Work metrics
- a compact recent/active agent list
- the MCP READY scan and one-click repair interaction
- clear freshness, unknown and partial-failure states
- mobile-first layout, large primary buttons and no secret display

Keep the emergency revoke and authority controls prominent.

## Testing

Write tests before or alongside each boundary. At minimum add:

### Work telemetry

- Codex and Claude fixture parsers across current and missing-field variants
- incremental cursor/dedup behavior
- corrupt/truncated JSONL and CSV recovery
- nullable token/value semantics
- pricing version and unknown-model behavior
- explicit focus/DONE rules and midnight/day-boundary handling
- seven-day aggregation and source-level degradation
- API authentication, bounds and compact response shape

Use synthetic fixtures. Never commit real user session logs or prompts.

### MCP Ready

- Codex TOML, Claude JSON, OpenCode v2 JSON/JSONC and Hermes YAML fixtures
- enabled/disabled, missing binary/env, auth/approval, invalid config,
  duplicate/conflict and remote/local transport states
- hanging CLI/server timeout with process-tree cleanup
- secret redaction in results, errors and audit events
- one structured batch request across multiple clients
- idempotent second run
- injected mid-commit failure proves complete rollback
- backup manifest and restore verification
- client-native status output changes do not crash parsers
- no tool invocation during health check

### Firmware and integration

- compact JSON parsing for Work/MCP summaries and missing fields
- host-worker queue/busy/offline behavior
- no seventh Home tile
- WORK secondary menu still reaches all former Focus functions
- firmware portal contains MAZ Work/MCP Ready links without direct Core config
  mutation
- existing M5Launcher non-destructive handoff guard remains green

## Verification commands

Run at minimum:

```powershell
python -m pytest host/tests -q
python scripts/check-version.py
python scripts/check-launcher-handoff.py
python -m platformio run
./scripts/package-release.ps1
```

Also run `git diff --check` and inspect the generated install ZIPs rather than
assuming packaging source equals shipped output. Add CI guards for the Work
surface, MCP mutation tests and required v0.9 documentation where useful.

## Disposable MCP acceptance matrix

Before touching real client configuration, run the manager against temporary
home/config directories for every supported client. Prove:

1. empty configuration;
2. one healthy local STDIO server;
3. one healthy remote HTTP server;
4. disabled server;
5. missing executable;
6. missing environment variable;
7. authentication required;
8. Claude project approval required;
9. hanging server;
10. malformed config;
11. multi-client repair success;
12. injected write failure and full rollback.

Then run a real read-only scan on the owner's machine. Only run the real
**FIX & ACTIVATE SELECTED** action after reviewing its redacted proposed
targets on the authenticated phone page. Verify the backup and final state by
re-reading configs and client-native status.

## Physical Cardputer gate

On the actual Cardputer ADV verify:

- Home shows CALL / CAPTURE / AGENTS / CONTROL / MEMORY / WORK.
- WORK opens directly to the six metrics with no visible layout clipping.
- arrows, ENTER, ESC, refresh and the secondary Work tools menu are reliable.
- Focus/Shift/Sprint/Task changes appear in the dashboard.
- active Codex/Claude model and agent state reach the device.
- stale/offline/unknown states are honest when MAZ Core disappears.
- CONTROL → MCP READY shows the same aggregate state as the phone page.
- the phone MCP repair result refreshes on the Cardputer.
- normal use for at least 30 minutes causes no watchdog/reset or UI stalls.
- M5Launcher handoff keeps MAZ Pocket installed and rollback still works.

Continue the seven-day carry test. Record whether the owner checks MAZ Work
without prompting and whether FOCUS, DONE and VALUE match their own judgment.

## Version and release

When implementation and acceptance are complete:

1. Set `VERSION` to `0.9.0` and update every version contract required by
   `scripts/check-version.py`.
2. Update README, Quickstart, host README, changelog, release notes,
   verification and release documentation around **v0.9.0 — WORK**.
3. Produce the exact Core, Cardputer, combined installer, raw app image and
   SHA-256 artifacts required by `docs/RELEASE_RULES.md`.
4. Push the feature branch and open a PR. Keep it draft until automated and
   physical gates are recorded with evidence.
5. Merge through review. Never push the implementation directly to `main`.
6. Publish only through `.github/workflows/release.yml` using the one
   `.release/v0.9.0` marker after the merged `main` version contract passes.
7. Keep v0.8.0 available as rollback.

## Definition of done

The work is complete only when all are true:

- MAZ Work is immediately useful on the physical Cardputer.
- The six metrics obey the truthful metric rules.
- Codex and Claude telemetry work from local metadata without uploading data.
- MCP Ready reports all four installed client types independently and a hung
  client cannot stall the system.
- One authenticated phone action repairs and activates the selected
  automatable configurations, then verifies persisted state.
- OAuth, workspace approval and missing-secret cases remain explicit rather
  than being falsely marked fixed.
- Atomic backups, rollback, idempotency and redaction are proven by tests.
- Existing Call, Capture, Agents, Control, Memory, Focus tools, phone authority,
  installer, M5Launcher and packaging behavior have no regressions.
- CI is green, shipped ZIP contents are inspected, physical acceptance is
  recorded and v0.9.0 is published only after those gates.

Hand off with a concise report containing the PR, commit, test counts,
firmware RAM/flash use, generated artifacts and hashes, disposable MCP matrix,
real read-only MCP scan, physical evidence, known limitations and rollback
instructions.
