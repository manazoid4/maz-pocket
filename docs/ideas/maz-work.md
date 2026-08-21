# MAZ Work

## Problem Statement

How might MAZ show its owner what work they and their AI agents actually did,
using a glanceable Cardputer interface without confusing activity, tokens or
agent uptime with productivity?

## Recommended Direction

MAZ Work is the private work-and-agent telemetry surface inside MAZ Pocket.
The laptop collects and normalizes local Codex, Claude Code and later OpenCode
events; the Cardputer presents six large tiles and short drill-down screens.

The product keeps four ideas separate: human focus time, AI consumption,
useful output and efficiency/health. This is more honest than producing a
single productivity score and more useful than another dense token dashboard.
Its distinctive value is ambient visibility on dedicated pocket hardware.

## Key Assumptions to Validate

- [ ] The six-tile view is useful enough to check several times per workday — test with a seven-day carry trial.
- [ ] Codex and Claude Code local logs can be normalized without silently treating missing fields as zero — test against real and fixture logs.
- [ ] `FOCUS`, `DONE` and `VALUE` remain understandable without a desktop dashboard — test each label and detail page on the physical 240x135 display.
- [ ] Completed tasks, tests and commits are better evidence of useful output than token volume — compare the daily summary with the owner's own assessment.

## MVP Scope

- Host-side, local-only collectors for Codex and Claude Code logs.
- One normalized session/event schema with explicit unknown values.
- Six Cardputer home tiles: Focus, Agents, Model, Tokens, Value and Done.
- Five compact detail views: Now, Today, Mix, Efficiency and History.
- Agent state from the existing Agent Nudge integration.
- Git/test/task evidence for the Done count.
- Seven-day local history and comparison.

## Not Doing (and Why)

- Employer or team surveillance — MAZ Work is owner-facing and private.
- A universal productivity score — it would falsely combine unlike signals.
- Screenshots, keylogging or background transcript capture — invasive and unnecessary for the core question.
- Full transcripts, replay and dense charts on the Cardputer — the display is a glance surface, not an analytics workstation.
- OpenCode, Cursor and Gemini collectors in the first slice — validate the schema with the two locally active sources first.
- Cloud accounts or hosted storage — local data proves the experience with less risk and setup.
- Public release before the seven-day carry test — daily usefulness is the continuation gate.

## Open Questions

- Should focus time require an explicit start/stop action, or combine a timer with AFK heartbeats?
- What exactly increments `DONE`: commits, passed checks, manually completed tasks, or a weighted subset?
- Should API-equivalent value appear by default for subscription users?
- Which missing-data conditions deserve a warning rather than an `unknown` label?
