# Task 1 Report: Stage 9/10 Fixture Stack Recovery

Status: DONE_WITH_CONCERNS. The target-local MSVC stack contract is implemented,
linked, and verified in PE headers. The required behavioral acceptance of each
selected CTest regression passing 1/1 is not met; no fixture, rule, value, or
timeout was changed because those are outside this card's scope.

## RED evidence

- Command: the two prebuilt Debug fixture executables were launched from
  `out/build/windows-msvc-debug/bin` serially.
- Stage 9: exit `-1073741571` (`0xC00000FD`).
- Stage 10: exit `-1073741571` (`0xC00000FD`).
- Application Error event 1000 entries at `2026-07-31T10:37:27Z` and
  `2026-07-31T10:37:28Z` named these fixtures and contained `0xc00000fd`
  (two earlier matching RED entries were also present at `10:32Z`).

## Change

- Added an MSVC-only `target_link_options(... PRIVATE /STACK:4194304)` block
  immediately after each of `arpg_stage9_validation_fixture` and
  `arpg_stage10_validation_fixture`, each with a concise local comment.
- No CTest registration, timeout, fixture source, production source, gameplay
  rule, or unrelated executable was modified.

## Build and binary verification

- A first configuration attempt outside the VS developer environment failed as
  expected by the project guard: Windows SDK version was empty. No retry was
  caused by a timeout and no orphan process was present.
- In VS2022 x64 Build Tools environment with `CMAKE_BUILD_PARALLEL_LEVEL=1`,
  `cmake --preset windows-msvc-debug` followed by the two-target `-j1` build
  completed with exit 0 in 9.0 s. Ninja linked exactly the two fixture EXEs.
- `dumpbin /headers` readback: Stage 9 `400000 size of stack reserve`; Stage 10
  `400000 size of stack reserve`.

## GREEN evidence and concern

- Exact serial CTest selector `^stage9\\.validation_fixture\\.real_v4_reward_reload$`:
  exit 8, 0/1 passed, failed in 47.79 s; its CTest output contained no fixture
  stdout. This is recorded as an unlocated ordinary test failure, without
  inference about its cause.
- Exact serial CTest selector `^stage10\\.validation_fixture\\.real_abyss_transactions$`:
  exit 8, 0/1 passed, failed in 1.55 s; output was
  `clear failed phase=1 abyss=1 rule=8 lifecycle_pending= room=1 hp=507 targets=560`.
- Direct post-link runs: Stage 9 exit 2 (`0x00000002`) in 47.57 s; Stage 10
  exit 8 (`0x00000008`) in 1.526 s. Neither was the former stack-overflow exit.
- For both the CTest window beginning `2026-07-31T10:38:57.9184746Z` and the
  direct-run window beginning `2026-07-31T10:40:26.5107943Z`, Application Error
  event 1000 matching either fixture had count 0.

## Scope audit

- `git diff --check`: exit 0, no whitespace errors.
- Tracked change scope before commit: `tests/dungeon/CMakeLists.txt` and the
  requested plan file; this report is under ignored `.superpowers` evidence
  storage.
- Full CTest was not run.

Review correction: the previous report incorrectly stated that the plan file
was already tracked and unchanged; commit `ab9f888` added that plan file.
