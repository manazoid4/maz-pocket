# Astra Handoff — Use Only for High-Value Technical Decisions

Astra should not spend time on PR cleanup, prose cleanup, blog drafting, repository descriptions or broad feature ideation.

## Astra Pass 1 — architecture/reliability decision

Inspect current `main` and open PR #35.

Answer only:

1. Which PR #35 changes are genuinely required for the Element14 competition paths: CALL, CAPTURE, AGENTS->PLAN and CONTROL?
2. For each required change: MERGE AS-IS / PORT SMALLER / REIMPLEMENT / DROP.
3. Identify any regression risk created by pulling the v1/WORK changes from #35 into the competition build.
4. Produce the smallest ordered patch plan that gives a reliable physical demo.

Constraints:
- no v1 expansion;
- no new frameworks/runtimes;
- no unrelated refactors;
- preserve known M5Launcher safety boundaries;
- automated tests are not physical proof;
- favour hiding optional features over rescuing unstable ones.

Required output format:

DECISION
REQUIRED CHANGES
DROP/DEFER
PATCH ORDER
TESTS TO RUN
PHYSICAL ACTION NEEDED FROM MAZ

No implementation unless explicitly asked after the decision is reviewed.

## Astra Pass 2 — final verification/reviewer

Run only after the competition implementation is otherwise complete.

Act as a skeptical release reviewer. Do not add features.

Verify:
- diff is competition-scoped;
- CALL/CAPTURE/PLAN/CONTROL code paths are internally coherent;
- routing/failure handling is bounded;
- no accidental execution path is introduced by PLAN;
- firmware remains inside the real M5Launcher app-image ceiling;
- release/installer guards remain intact;
- public-repo changes do not obviously expose credentials;
- docs do not claim physical behaviour without recorded evidence.

Return only blockers and exact fixes. If there are no blockers, say so and list the remaining physical tests.
