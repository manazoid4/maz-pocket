# Personal work and AI-agent telemetry landscape

Research date: 2026-08-21  
Scope: 15 open-source projects demonstrated on GitHub and, where available, in first-party Reddit posts by their makers. The goal is to identify the data worth presenting in Maz Pocket before reducing it to an M5Stack Cardputer interface.

## Executive finding

The useful product is not an "employer monitor." It is a private, glanceable work console for its owner: **what is running, what it is costing, how long focused work has lasted, and what was actually produced**.

Existing products are strongest at consumption telemetry: sessions, models, tokens, cost, cache and tool calls. They are much weaker at productivity. Only a few connect AI activity to commits, accepted edits, lines changed, completed tasks or errors. Maz Pocket should keep these concepts separate:

- **Attention/time:** Was I active and focused, and for how long?
- **AI activity:** Which agent/model ran, for how long, using how many tokens/tools?
- **Output:** What completed or changed?
- **Efficiency/health:** How much activity was overhead, cached, failing, blocked or nearing a limit?

High token use is not productive time. A long session is not necessarily useful output.

The word "cost" also needs care. Most local tools multiply logged tokens by public API prices. For subscription users this is hypothetical, not money charged. Maz Pocket should call it **API value** or **estimated API equivalent**, with actual subscription cost tracked separately.

## Fifteen-project scan

### 1. Claud-ometer

Sources: [maintainer's Reddit post](https://www.reddit.com/r/ClaudeCode/comments/1re8vh7/i_built_a_local_dashboard_to_track_my_claude_code/) · [GitHub](https://github.com/deshraj/Claud-ometer)

Data presented:

- Total sessions, messages, tokens and estimated cost
- Usage over time and model split
- Per-project sessions, tokens, cost and last activity
- Per-session duration, messages, tool calls and compactions
- Per-message tokens, cache efficiency and peak-hour distribution

UI pattern: headline KPI cards, model donut, activity heatmap, peak-hours chart, project/session tables and conversation replay. Compactions are marked in the timeline.

Cardputer lesson: model split and peak working hour are good secondary tiles; per-message replay is desktop-only detail.

### 2. ccmon

Sources: [maintainer's Reddit post](https://www.reddit.com/r/ClaudeCode/comments/1rxf34s/i_built_a_terminal_dashboard_to_monitor_claude/) · [GitHub](https://github.com/TheBabaYaga/ccmon)

Data presented:

- Today's tokens, prompts and cost by Opus, Sonnet and Haiku
- Rolling 30-minute cost/hour, tokens/hour and messages/hour
- Active/recent sessions with project, summary, model, tokens, cost, duration and recency

UI pattern: live terminal dashboard refreshed every two seconds, with a top summary, three burn-rate indicators, highlighted active sessions and a compact recent-session table.

Cardputer lesson: this is the closest reference. Its "now + three rates + recent session" hierarchy fits a tiny live display.

### 3. CodeLedger

Sources: [maintainer's Reddit post](https://www.reddit.com/r/ClaudeAI/comments/1rwdahm/i_analyzed_77_claude_code_sessions_233_ghost/) · [GitHub](https://github.com/bhvbhushan/codeledger)

Data presented:

- Tokens and cost by project, session, agent and skill
- Model distribution
- User-requested agents versus automatic/overhead agents
- Session categories, budget alerts and anomalous spend
- Cost-reduction recommendations
- Aggregation across Claude Code, Codex CLI, Cline and Gemini CLI

UI pattern: KPI cards, daily user-versus-overhead spend chart, model pie, top-project list and project → session → agent drill-down.

Cardputer lesson: "useful agent work vs overhead" is more informative than total agent count.

### 4. Tokenmeter

Sources: [maintainer's Reddit post](https://www.reddit.com/r/ClaudeCode/comments/1tctpuo/built_a_free_windows_app_to_track_your_claude/) · [GitHub](https://github.com/DewashishCodes/tokenmeter)

Data presented:

- Total tokens and estimated cost across sessions
- 14-day daily activity and 90-day contribution heatmap
- Peak working hours
- Cache savings versus uncached API equivalent
- 30-day cost forecast from the seven-day average
- Per-project breakdown with sparklines

UI pattern: large desktop KPIs, line chart, heatmap, project cards and a small animated pixel companion.

Cardputer lesson: peak hours, today's total, seven-day comparison and a simple mascot/status expression translate well; the 90-day heatmap does not.

### 5. Specter

Sources: [maintainer's Reddit post](https://www.reddit.com/r/ClaudeCode/comments/1rgnvsi/i_built_specter_an_opensource_local_dashboard_for/) · [GitHub](https://github.com/alizenhom/Specter)

Data presented:

- Total cost, tokens and sessions; 30-day activity
- Model usage and hour-of-day heatmap
- Session cost, duration, context percentage and activity
- Tool calls and subagent timeline
- Per-project session count, tool calls and open tasks
- Cache efficiency

UI pattern: overview KPIs, hour heatmap, sortable sessions with sparklines, project grid, cache donut, transcript and visual subagent timeline.

Cardputer lesson: active context percentage, current subagent count and open-task count belong on health/progress tiles.

### 6. Agent Observability

Sources: [maintainer's Reddit post](https://www.reddit.com/r/ClaudeCode/comments/1u0k01f/claude_code_monitoring_dashboard_may_help_some/) · [GitHub](https://github.com/KB1SLN-Labs/agent-observability)

Data presented:

- Claude Code and Codex sessions; token type, source and model
- Tokens per dollar, real-time burn rate and forecasts
- Main-agent versus subagent cost
- Cache hit/savings; tool calls and approvals; MCP/skill attribution
- p95 response/tool latency, errors and prompts/hour
- Lines added/removed, commits, edit-acceptance rate and modification velocity

UI pattern: Grafana sections for Cost, Tokens & Usage, Productivity & Output, Tools/MCP/Skills and Performance, composed of tiles, gauges, pies, bars, sparklines and an event log.

Cardputer lesson: this has the best separation of **output** from **activity**. Commits, accepted edits and completed checks deserve their own progress page.

### 7. AgentGlass

Sources: [maintainer's Reddit post](https://www.reddit.com/r/ClaudeAI/comments/1v08p2q/made_this_to_see_what_claude_was_doing_now_i/) · [GitHub](https://github.com/SirAllap/agentglass)

Data presented:

- Live agents/sessions across providers
- Tokens and cost per turn and session
- Tool calls and tool-latency percentiles
- Error timeline, context/compaction proximity and session lifecycle
- Spawned subagent tree and project/repository scope

UI pattern: dense real-time mission-control cockpit with session cards, provider filter, subagent drill-down, compaction radar and error timeline.

Cardputer lesson: fleet status matters, but only the exception should be surfaced: "3 running / 1 needs attention," not a miniature cockpit.

### 8. Agent Trail

Source: [GitHub](https://github.com/camtrik/agent-trail)

Data presented:

- Claude Code, Codex, OpenCode, OpenClaw and Qoder in one local dashboard
- Total tokens and estimated USD cost for today/week/all time
- Breakdowns by day, session, project and model, with trends
- Most expensive sessions, live activity feed, tool calls and subagent trees
- Turn-by-turn session replay

UI pattern: unified overview followed by project/model/session drill-down and live feed.

Cardputer lesson: normalize every provider into one small common record: agent, model, project, state, tokens, cost/value and timestamps.

### 9. agent-lens

Source: [GitHub](https://github.com/naimjeem/agent-lens)

Data presented:

- Eight agents, including Claude Code, Codex, Gemini, OpenCode, Cursor and Copilot
- Today versus all-time sessions, messages, tool calls, estimated cost and cache hit rate
- Today's cost/messages, total cost and day-over-day trend
- Daily token/cache/cost totals; hourly and weekday buckets
- Agent totals, model totals and tool use by project

UI pattern: an agent selector over the whole dashboard, sticky KPI pills and responsive inline charts.

Cardputer lesson: a global agent filter plus today/all-time toggle is enough navigation for several providers.

### 10. agent-cockpit

Source: [GitHub](https://github.com/nashory/agent-cockpit)

Data presented:

- Headline tokens/cost, per-agent bars and 30-day trend
- Engine share, model load and output speed
- Token, cost, throughput and velocity trends; efficiency/economics and engaged hours
- Contribution calendar, hour/day activity and top projects
- Daily input/output/cache ledger
- Five-hour billing windows: elapsed/remaining, burn rate, projection and limit use
- Session project, engine, start, active span, tokens, cost and models

UI pattern: keyboard-first TUI with seven tabs, compact/expert density modes and focused-widget zoom.

Cardputer lesson: one tile grid can double as navigation. Selecting a tile opens its one-detail screen; a compact/expert toggle is unnecessary on-device.

### 11. agtop

Source: [GitHub](https://github.com/ldegio/agtop)

Data presented:

- Live Claude Code and Codex sessions
- Per-session spend; hourly/daily breakdown and plan-aware billing
- Context-window pressure
- CPU, memory, PID count, live cost rate and incremental tool count
- Aggregate spend/token/CPU sparklines
- Subagent type, model, description, cost, tools and duration

UI pattern: `top`-style session table with expandable rows and a live-only filter.

Cardputer lesson: context pressure and "needs attention" are better live alerts than CPU/memory unless the host is unhealthy.

### 12. agentsview

Source: [GitHub](https://github.com/kenn-io/agentsview)

Data presented:

- Sessions from Claude Code, Codex and 20+ agents
- Session agent/project/models, output tokens, peak context and cost
- Session archetypes: automation, quick, standard, deep and marathon
- Duration, user-message, context and tools-per-turn distributions
- Cache economics, agent/model/tool mix and hourly activity
- Project activity, recent file edits and daily spend

UI pattern: searchable dashboard with heatmaps, breakdowns, recent-edits feed, live updates and keyboard-first navigation.

Cardputer lesson: session archetypes are a useful summarization strategy—show "deep session" instead of many raw fields.

### 13. AgentMeter

Source: [GitHub](https://github.com/LyleMi/AgentMeter)

Data presented:

- Sessions, tokens and estimated cost by model, day, project and session
- Cache stability, savings and misses
- Cost per session and cost per active hour
- Output expansion, reasoning share and throughput
- Retry/failure pressure, low-confidence cohorts and anomalous sessions
- Model health/service drift and call status when available

UI pattern: dedicated Attention, Analyze, Sessions, Safety and Resources views; the Attention view prioritizes anomalies and health signals.

Cardputer lesson: the home screen should be exception-led. A warning tile can replace several normal metrics when attention is required.

### 14. ActivityWatch

Source: [GitHub](https://github.com/ActivityWatch/activitywatch)

Data presented:

- Active application and window title
- Active browser tab/title/URL through an optional extension
- Keyboard/mouse activity and AFK state
- Time by app, website and user-defined category
- Timelines, date ranges, activity summaries and raw event history

UI pattern: dashboard summaries plus a chronological timeline and category builder. Data stays local and collectors are extensible.

Cardputer lesson: this supplies the missing **human active-time** layer. Productive/focus time should come from explicit categories and AFK exclusion, not from whether an agent process exists.

### 15. Wakapi

Source: [GitHub](https://github.com/muety/wakapi)

Data presented:

- Coding time by project, language, editor, host and operating system
- Time-range summaries, badges and weekly reports
- IDE heartbeat-derived activity with a configurable timeout
- Optional Prometheus export and WakaTime compatibility

UI pattern: minimalist coding-statistics dashboard built around time and categorical breakdowns.

Cardputer lesson: show one dominant "focused coding time" number and rotate the leading project/language/editor rather than reproducing full charts.

## Common data taxonomy to adopt

This is the reusable data presented by the projects above—not a recommendation to copy their code or visual identity.

### Identity and state

- Provider/agent: Codex, Claude Code, OpenCode, Gemini, Cursor, etc.
- Model and model family
- Main agent versus subagent; user-requested versus automatic/overhead agent
- Project/repository/worktree and session identifier
- State: running, waiting for approval/input, idle, completed, failed
- Current action/tool and short session/task summary

### Time and attention

- Session start, elapsed duration, active span and last activity
- Human active time with AFK removed
- Focus/category time, peak hour and day-of-week distribution
- Today/week/all-time windows and day-over-day or week-over-week delta
- Current billing/rate-limit window elapsed and remaining

### Usage and economics

- Input, output, cache-read and cache-write tokens
- Total tokens and context-window percentage/peak
- Estimated API-equivalent value by agent/model/project/session
- Real subscription cost kept as a separate user-entered value
- Burn rate: tokens/hour and API-value/hour
- Forecast, budget/limit percentage and cache hit/savings

### Activity, health and overhead

- Messages/prompts, tool calls, MCP methods, skills and approvals
- Subagent count/tree and subagent share of usage
- Errors, retries, failed calls and latency
- Context compactions/resets and proximity to context limit
- CPU/RAM only as host-health diagnostics

### Productive output

- Tasks completed/open; session goal status
- Commits and tests/checks passed/failed
- Files touched; lines added/removed
- Accepted edits versus reverted/rejected edits
- Modification velocity, but never treated alone as quality

The last group requires joining session telemetry to Git/task/test data. It generally cannot be inferred reliably from token logs alone.

## Cardputer information architecture

The strongest shared UI pattern is 4–6 big KPI tiles first, followed by breakdowns and a drill-down list. On a Cardputer, each tile should contain an icon, one large value and a label; selecting it opens one short detail page.

### Home: six tiles

1. **FOCUS** — productive active time today; delta from seven-day weekday average
2. **AGENTS** — running / waiting / failed counts
3. **MODEL** — current or most-used model, with provider color/icon
4. **TOKENS** — today's total; small burn-rate arrow
5. **VALUE** — estimated API equivalent today, clearly not "spent"
6. **DONE** — completed tasks/checks/commits today

When something needs attention, the affected tile becomes the first/high-contrast tile: waiting approval, failing test, error, quota near limit or context near full.

### Drill-down pages

- **Now:** agent, model, project, state, elapsed time, current action, context percentage and tokens/value per hour
- **Today:** focus time, sessions, tokens, API value, tools, completed outputs and seven-day comparison
- **Mix:** agent share, model share and top three projects
- **Efficiency:** cache hit, overhead-agent share, errors/retries and cost per active hour/output
- **History:** seven small daily bars, streak and peak focus hour

### Avoid on-device

- Full transcripts, session replay, Gantt charts and subagent trees
- Dense ledgers, 30/90-day heatmaps and large sortable tables
- Precise per-token-type detail on the home screen
- Lines-of-code or token volume framed as a productivity score
- More than one mini-chart per detail page

The Cardputer should be the ambient glance/control surface; a host-side local collector should parse and normalize raw logs.

## Naming landscape and collision notes

The existing market clusters around five patterns:

- Literal: `Claude Code Usage Dashboard`, `Codex Usage Dashboard`
- Instrument: `Tokenmeter`, `AgentMeter`, `Claud-ometer`
- Vision/inspection: `Specter`, `AgentGlass`, `agent-lens`, `agentsview`
- Control room: `agent-cockpit`, `Agent Trail`
- Terminal homage: `agtop`, `agentop`, `ccmon`, `ccusage`

These are descriptive but crowded and often tied to one provider. Avoid `Agent Pulse`: several current AI-agent monitors already use that exact name, including [this session monitor](https://useagentpulse.github.io/) and [this observability platform](https://github.com/milan-backend/Agent-Pulse). Avoid `PocketOps`: it is already used by an [offline Android utility dashboard](https://pocketops192.vercel.app/) and a small-business services brand. `WorkPulse` is also already used for work-time tracking and employee-management products.

A preliminary exact-phrase search found no meaningful software collision for **Maz Pocket**, **Maz Meter** or **MazScope**, but this is not trademark or domain clearance. The cleanest naming direction is therefore to retain a distinctive `Maz` parent brand and give the telemetry mode a plain feature label, for example:

- `Maz Pocket — Workboard`
- `Maz Pocket — Shift`
- `Maz Pocket — Console`
- `Maz Meter`
- `MazScope`

The confirmed name is **MAZ Work**. **MAZ Pocket** remains the physical device
and firmware; MAZ Work is its private, provider-neutral work-and-agent
telemetry surface. The plain name is deliberate: it describes the owner's work
rather than implying employee surveillance or tying the product to one model.

## Recommended planning decisions before firmware work

1. Use **MAZ Work** as the telemetry surface name while retaining **MAZ Pocket**
   for the device and firmware.
2. Define "productive time" as user-owned focused time, not agent uptime.
3. Select initial sources: Codex local logs, Claude Code local logs, Git status/history, task state and ActivityWatch/WakaTime-style heartbeats.
4. Specify one normalized session/event schema before writing display code.
5. Prototype the six home tiles with sample data on desktop at the Cardputer's pixel dimensions.
6. Validate the meanings of `FOCUS`, `DONE` and `VALUE` for a week; those are the easiest metrics to misrepresent.
7. Only then implement host-to-Cardputer transport and firmware pages.

## Source-quality notes

- GitHub READMEs and maintainers' Reddit launch posts were treated as descriptions of what each project presents, not as independent verification of accuracy.
- Pricing tables change; estimated API values require versioned model pricing and an "unknown/unpriced model" state.
- Local logs differ by agent and version. Missing token/tool/model data should remain visibly unknown rather than silently becoming zero.
- Activity heartbeat gaps are ambiguous: they can mean a break or time spent reading/thinking. Productive-time rules must be adjustable by the owner.
