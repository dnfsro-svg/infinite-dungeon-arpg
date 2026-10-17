# Task 2 review fix brief

## Scope

Address the independent review finding only. Do not change production code,
launcher evidence, deployment, saves, display settings, or Task 3 files.

## Required change

Extend the existing
`production_startup_auto_continues_pending_death` case in
`tests/platform/dungeon_runtime_tests.cpp` so the same registered case directly
proves both missing idempotence requirements without changing the 477-case
registry count:

1. After the first successful auto-continuation, initialize again against the
   same save with `continue_pending_death_on_initialize = true`.
2. Prove the second initialization succeeds, death remains absent, the stored
   death lifecycle is `none`, and the commit generation does not increase.
3. This second initialization is also the direct `continue=true` +
   already-clear-save case.

Use a fresh `DungeonRuntime` object for the second launch unless the existing
runtime contract explicitly requires same-object reinitialization. Preserve all
existing first-launch assertions.

## Verification

Run serially:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
```

Expected: 1/1 CTest and `477 cases, 0 failures`.

Then run `git diff --check`, self-review the exact diff, and commit only
`tests/platform/dungeon_runtime_tests.cpp` with:

`test: cover idempotent death recovery initialization`

Update `task-2-report.md` by replacing the contradictory opening status with a
short current DONE summary and append the review-fix evidence. Update
`progress.md` to record the review finding and fix commit. Those SDD files are
ignored and must not be staged.

