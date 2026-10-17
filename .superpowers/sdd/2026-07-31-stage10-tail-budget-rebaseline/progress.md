# SDD ledger — Stage 10 sparse-tail budget rebaseline

Plan: `docs/superpowers/plans/2026-07-31-stage10-tail-budget-rebaseline.md`

- Baseline committed HEAD: `e6793c6`.
- Preserved inherited fixture+CMake diff hash:
  `a8accd8782bd98656efb493902a2aba5c65002b4`.
- RED: 48,914 ticks, 562/600 defeated, exit 8, 19.4s; no live lock and
  deterministic sweep active.
- Approved design: `12000 + 96N`, timeout 75 seconds, no further driver change.
- Task 1 Step 1: complete; HEAD/staging/hash/recorded RED verified.
- Task 1 Steps 2-3: complete; formula is `12000 + 96N`, timeout is 75 seconds.
- Task 1 Step 4: complete; `git diff --check` passed, staging remains empty,
  tracked scope remains the inherited fixture+CMake pair, forbidden injection
  matches are zero, and `drive_clear()` has exactly one
  `session.tick(movement)`.
- Task 1 Step 5 build: VS2022 BuildTools x64, Windows SDK 10.0.26100.0,
  `CMAKE_BUILD_PARALLEL_LEVEL=1`, target-only `--parallel 1`; exit 0.
- Task 1 Step 5 direct gate: BLOCKED; the single allowed native run exited 8
  after 29.638 seconds at the exact 69,600-tick budget, with 599/600
  defeated, one remaining, HP 760, no live lock, and deterministic sweep
  active.
- Task 1 Step 6: not run because direct execution was not GREEN.
- Task 1 status: BLOCKED at the direct gate; formula and timeout remain exactly
  as approved, with no staging, commit, CTest, or further implementation.
