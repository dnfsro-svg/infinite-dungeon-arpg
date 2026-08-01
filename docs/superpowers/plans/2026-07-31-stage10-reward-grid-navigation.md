# Stage 10 Reward Grid Navigation Implementation Plan

**Goal:** Complete the now-reachable Stage 10 reward claim through bounded,
obstacle-safe public movement after the hundredfold room expansion.

**Design:**
`docs/superpowers/specs/2026-07-31-stage10-reward-grid-navigation-design.md`

## Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery`.
- Modify only `tests/dungeon/stage10_validation_fixture.cpp` and this plan.
- Preserve `12000 + 96N`, CTest timeout 75, production code and all reward,
  pickup, persistence and abandonment assertions.
- Use only public snapshot, movement, request and session APIs.
- Build/test serially; no full CTest and no game launch.

## Task 1: Reuse the grid route for reward claim

- [x] Preserve the recorded exit-10 RED; do not rerun unchanged code.
- [x] Extract a target-oriented grid movement helper from the proven sweep
      route without changing its waypoint behavior.
- [x] Extract the blocked-progress state transition so sweep and claim use the
      same near/far/route rules with independent state.
- [x] Replace claim's straight-line movement with the grid helper.
- [x] Change only claim's finite bound from 500 to 2,400 ticks.
- [x] Add failure-only position/navigation diagnostics.

## Task 2: Verify

- [x] Run `git diff --check` and evidence-token scan.
- [x] Serial-build only `arpg_stage10_validation_fixture`.
- [x] Run standalone once; require exit 0.
- [x] Then run the exact transaction CTest and evidence guard, each with `-j1`.
- [x] Obtain independent spec and code-quality reviews before staging.
