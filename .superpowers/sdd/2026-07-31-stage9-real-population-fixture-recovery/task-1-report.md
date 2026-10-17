# Task 1 Report: Stage 9 real-population fixture recovery

## Status

PASS. The Stage 9 fixture now proves the production chain:

`RoomMonsterPlan -> RoomMonsterField residency -> CombatWorld::defeat_monster -> DungeonSession::relay_combat_events -> real ground drop -> public request_pickup -> committed save -> v4 reload`

The focused selector passed 1/1. No full CTest suite or independent Stage 9 evidence guard was run, per the binding task scope.

## Commit

- Commit: `ccc1ed01c00b3eb626722cb109ca13f2e50f49a9`
- Message: `test: restore stage9 real population fixture`
- Branch: `codex/task9-gate-recovery`

## Build and test evidence

Successful configure command, run after entering the VS2022 x64 Developer PowerShell environment:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --preset windows-msvc-debug
```

- Result: exit 0
- Elapsed: 7.282 seconds

Exact serial build command:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --build --preset windows-msvc-debug --target arpg_stage9_validation_fixture -- -j1
```

- Result: exit 0
- Elapsed: 2.659 seconds

Exact focused test command:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
ctest --test-dir out/build/windows-msvc-debug -R '^stage9\.validation_fixture\.real_v4_reward_reload$' --output-on-failure -j1
```

- Result: exit 0; 1/1 passed; 0 failed
- CTest test time: 0.05 seconds
- CTest total real time: 0.20 seconds
- Command wall time: 0.243 seconds

## Audits and scope

- Fixture injection scan for `relay_defeated(` and `events_.try_push(`: 0 matches.
- Added support-code injection scan: 0 matches.
- `git diff --check`: exit 0 before staging.
- `git diff --cached --check`: exit 0 before commit.
- Exact staged/committed paths:
  - `docs/superpowers/plans/2026-07-31-stage9-real-population-fixture-recovery.md`
  - `tests/dungeon/dungeon_test_support.hpp`
  - `tests/dungeon/stage9_validation_fixture.cpp`
- No production source, gameplay/drop/capacity/golden/timeout, CMake, or Stage 10 path was committed.
- Protected `tests/dungeon/stage10_validation_fixture.cpp` diff hash remained `8bcc00efda5fc9fd4c6dd030800c3875e8853947` before staging, after staging, and after commit.
- Post-commit remaining workspace state is only:
  - modified `tests/dungeon/stage10_validation_fixture.cpp`
  - untracked `docs/superpowers/plans/2026-07-31-stage10-dense-fixture-recovery.md`

## Concerns

- A plain PowerShell configure attempt exited 1 in 0.070 seconds because the VS Developer environment was not loaded (`Windows SDK 10.0.26100.0; detected ''`). Loading the installed VS2022 Build Tools Developer PowerShell resolved the environment issue; the successful configure/build/test results above are authoritative.
- The independent `stage9.evidence.no_injected_defeat_trace` selector remains outside this task and was intentionally not run or modified.
- The unrelated Stage 10 dirty fixture and plan remain present and untouched for their blocked task card.
