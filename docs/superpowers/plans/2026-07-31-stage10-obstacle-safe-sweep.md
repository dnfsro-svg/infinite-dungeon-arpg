# Stage 10 Obstacle-Safe Sweep Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan task by
> task.

**Goal:** Prove and repair the Stage 10 fixture's deterministic streamed-room
coverage gap without changing production gameplay or adding more time budget.

**Design:**
`docs/superpowers/specs/2026-07-31-stage10-obstacle-safe-sweep-design.md`

## Global Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery` on
  `codex/task9-gate-recovery`.
- Preserve all inherited fixture and CMake work; never reset or edit production
  files.
- Modify only `tests/dungeon/stage10_validation_fixture.cpp`; the registered
  Stage 10 timeout remains exactly 75 seconds.
- Use only public snapshot, event, room-plan builder, streaming-region, action,
  movement and session APIs.
- Never use test-support population builders or private/test access helpers.
- Build and run serially; no full CTest and no game launch.
- Never stage the four superseded untracked recovery plans.

### Task 1: Record one exact coverage RED

**Files:**

- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Add/commit later:
  `docs/superpowers/plans/2026-07-31-stage10-obstacle-safe-sweep.md`

- [x] Add a fixed-capacity trace for public resident snapshots, public defeated
      events, actual streaming-region visits and blocked sweep waypoints.
- [x] Rebuild the production room plan once and validate count/version/hash
      before using it for diagnostic labels.
- [x] Keep movement, actions, budget and return codes unchanged.
- [x] Build only `arpg_stage10_validation_fixture` with one build worker.
- [x] Run the executable exactly once and preserve the missing ordinal/home-cell
      evidence. Do not run CTest while the direct gate is RED.

### Task 2: Make sweep coverage obstacle-safe

- [x] Proceed only if Task 1 proves the missing home cell was not actually
      covered and blocked sweep input occurred.
- [x] Move serpentine lanes to cell boundaries and use room boundaries as the
      horizontal endpoints.
- [x] Add deterministic single-axis grid joining. A blocked join switches probe;
      it never advances the route. Advance a waypoint only after arrival.
- [x] Restore the finite `12000 + 96N` budget; do not change the 75-second CTest
      timeout.
- [x] Preserve the failure-only trace so any future regression names the exact
      missing cell.

### Task 3: Verify and close

- [x] Run `git diff --check` and confirm no production source changed.
- [x] Serial-build only `arpg_stage10_validation_fixture`.
- [x] Run the standalone executable and require exit 0.
- [x] Only after direct GREEN, run exactly:

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.evidence\.no_private_injection$' `
  --output-on-failure -j1
```

- [x] Obtain independent spec-compliance and code-quality reviews.
- [x] Stage only the reviewed fixture, inherited CMake timeout, accepted recovery
      plans, and this plan. Commit as:

```text
test: restore stage10 dense abyss transaction fixture
```
