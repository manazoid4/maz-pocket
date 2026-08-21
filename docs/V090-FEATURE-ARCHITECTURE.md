# MAZ Pocket v0.9 feature architecture

This document is the controlling scope and placement decision for v0.9.0.
It reconciles the current application audit, the UX/clutter audit and the
15-project telemetry research. If the older full build prompt suggests a
broader presentation refactor, this document wins.

## Release promise

Ship two complete daily loops:

1. Open **WORK** and understand today in one glance.
2. Open **MCP READY**, check health, then repair selected existing
   configurations from the authenticated phone UI.

Do not merge or delete whole apps in v0.9. Remove duplicated discovery paths,
preserve stable IDs and keep niche tools in the global palette.

## Final information architecture

### Cardputer

```text
CALL      CAPTURE      AGENTS
CONTROL   MEMORY       WORK
```

The stable Home IDs remain `talk`, `capturehub`, `agents`, `desk`, `recall`
and `flow`. The `flow` label changes from FOCUS to WORK; it is not renamed.

- **CALL** opens the conversation directly.
- **CAPTURE** contains Brain Dump, Teach Demo, Voice Recorder and Decision Note.
- **AGENTS** contains Agent Status, Plan, Crew and a contextual Prompt Deck
  shortcut. Projects & Builds has one canonical home under CONTROL.
- **CONTROL** opens a readiness overview for Core/laptop, Wi-Fi, MCP and device
  health. Its secondary menu contains Pairing, Projects & Builds, Send to PC,
  Wi-Fi, Diagnostics, Settings and guarded Update.
- **MEMORY** contains Results Inbox, Prompt Deck, Notes, Snippets and Text
  Viewer. Send to PC is reached from CONTROL.
- **WORK** opens directly on Focus, Agents, Model, Tokens, API Value and Done.
  Its tools menu contains Focus Timer, Work Sprint, Tasks, Reminders, Shift
  Clock and Retro.

All other registered apps remain available through the global palette or
their existing shortcuts.

### Web surfaces

`mazpocket.local` remains a lightweight device companion: live readiness, six
surface launchers, pairing/Core links and a collapsed setup/update section. It
links to authenticated Work and MCP pages; it does not contain analytics or
MCP mutation logic.

The authenticated MAZ Core phone shell has three tabs:

```text
WORK | MCP | AUTHORITY
```

- WORK shows the six today cards, active agents, seven-day history and source
  freshness.
- MCP performs Check MCP, selection and one batch Fix & Activate action, then
  shows any remaining OAuth or approval step.
- AUTHORITY retains approvals, grants, sessions and the action feed.
- Revoke All Control remains visible from every tab and pending approvals are
  badged.

## Complete feature placement

| Feature | v0.9 decision | Visible placement |
|---|---|---|
| Home/NOW/quick keys | Keep | Home |
| Call MAZ | Keep primary | CALL |
| Brain Dump | Keep first | CAPTURE |
| Teach Demo | Keep | CAPTURE |
| Voice Recorder | Keep | CAPTURE |
| Decision | Move discovery | CAPTURE as Decision Note |
| Agent Status/Nudge | Keep first | AGENTS |
| Plan and Crew | Keep separate in v0.9 | AGENTS; merge presentation later |
| Retro | Move discovery | WORK tools/history |
| Projects & Builds | Remove duplicates | CONTROL only |
| Pairing & Phone | Keep | CONTROL |
| Laptop/Core/overview | Merge presentation | CONTROL readiness landing |
| Wi-Fi | Keep one link | CONTROL |
| Send to PC/Beam | Keep one link | CONTROL |
| Device Tests/Diagnostics/Storage | Merge presentation | CONTROL Diagnostics |
| Settings | Keep one link | CONTROL |
| Update/M5Launcher | Keep guarded | CONTROL Device/Update |
| Live Screen | Move | Phone/portal only |
| PC Commands | Remove duplicate | CALL control mode/phone |
| Results Inbox | Keep first | MEMORY |
| Prompt Deck | Keep | MEMORY; contextual link from AGENTS |
| Notes/Snippets/Text Viewer | Keep separate in v0.9 | MEMORY; merge presentation later |
| Focus Timer/Work Sprint | Keep separate in v0.9 | WORK tools; merge presentation later |
| Tasks/Reminders/Shift Clock | Keep | WORK tools |
| WORK metrics and details | Add | WORK direct landing/progressive detail |
| MCP aggregate | Add read-only | CONTROL landing and row |
| MCP check/fix/auth | Add mutation | Authenticated phone MCP tab |
| Calculator/Stopwatch/QR/Generator | Keep hidden | Palette/shortcuts |
| Help | Keep hidden | `H`/palette |
| Snake/Hyperdrive | Keep hidden | Palette/easter egg |
| Legacy Connections | Hide | Compatibility ID only |
| Internal Home descriptor | Hide | Internal only |

## Release priority

### Now: v0.9

1. Direct WORK dashboard with truthful unknown/stale states.
2. Codex and Claude telemetry, Agent Nudge and explicit MAZ work records.
3. Read-only MCP scan for Codex, Claude, OpenCode and Hermes.
4. Transactional repair of selected existing MCP entries.
5. Phone WORK/MCP/AUTHORITY tabs.
6. Cardputer readiness overview and MCP aggregate.
7. Presentation-only removal of duplicate navigation paths.
8. Stable-ID, shortcut, palette and deep-link compatibility.

### Next: v0.9.x

- Start Work wrapper for Plan/Crew.
- Start Focus wrapper for Focus/Sprint.
- Notes & Text wrapper.
- OpenCode telemetry.
- Richer project/model detail and optional MCP diff preview.
- Portal refinement after real usage proves the hierarchy.

### Later

- Hermes telemetry and automatic desktop time capture.
- Heatmaps, forecasts, tool latency, cache efficiency and subagent overhead.
- Detailed Git/check/file evidence and context/quota warnings.
- Transcript replay, cross-device sync or remote telemetry only after separate
  privacy/product decisions.

## Explicit v0.9 cuts

- No OpenCode or Hermes telemetry; their MCP adapters remain required.
- No automatic productive-hour surveillance or universal productivity score.
- No arbitrary MCP discovery, marketplace installation or tool invocation as
  a health check.
- No Cardputer secret entry, MCP mutation or cross-client secret copying.
- No full app mergers, stable-ID renames or implementation deletion.
- No complete portal redesign, desktop analytics suite or unrelated polish.

## Success test

The release earns continuation when the owner can understand today from WORK
without opening the laptop, and can turn an intended existing MCP setup into a
verified ready state with one phone-side batch action plus only unavoidable
OAuth/approval steps.
