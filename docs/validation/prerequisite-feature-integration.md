# Prerequisite Feature Integration Validation

- Date: 2026-07-26
- Branch: `codex/integration-large-square-rooms`
- Task 5 commit: `b5e40acd6e1ac62a189aa23fe11352be93626159`
- Build concurrency: one compiler/test worker (`CMAKE_BUILD_PARALLEL_LEVEL=1`, `CTEST_PARALLEL_LEVEL=1`, `-j1`)

## Result

PASS. The launcher, approved first-three-depth balance, atomic HP-potion
gameplay, high-resolution potion presentation, and their compatibility guards
have complete bounded regression evidence. No production gameplay code was
changed while closing this validation task.

## Core Debug gate

Fresh configuration and build:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
./scripts/Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

- Fresh configure succeeded.
- Serial build succeeded: 63/63 build actions.
- The first run stopped at test 19 because the Stage 11 death evidence guard
  still matched the previous `void` capture helper. The production helper was
  already stricter: it returned export success and captured only after
  `EndDrawing`.
- A second full CTest run passed tests 1-49 and exposed another formatting-bound
  source guard at test 50. The guard was rebound to the recovery
  `static_cast<void>` path and the normal `capture_succeeded` assignment.
- The bounded continuation exposed five Stage11C/11D guard assumptions at
  tests 54-58. Their production semantics were preserved: capture failure is
  propagated, one production render plan is used, and dense label dropping
  never removes world icons. Only the guards and their negative mutations were
  updated.

Post-fix serial evidence:

| Coverage | Result | Elapsed |
|---|---:|---:|
| Core tests 1-49 | 49 passed | included in 835.79 s run |
| `platform.host_input_source` (test 50) | 1/1 passed | 147.81 s |
| Core tests 51-52 and 59-74 | 18 passed | included in 21.19 s continuation |
| Stage11C tests 53-54 | 2/2 passed | 0.53 s |
| Stage11D renderer tests 55-56 | 2/2 passed | 0.70 s |
| Stage11D loot tests 57-58 | 2/2 passed | 80.15 s |
| Stage11 death production + mutation gates | 2/2 passed | 0.97 s |

All 74 registered core tests therefore have post-build pass evidence. The
expensive tests 1-49 were not run a third time after changes limited to the
independent Stage11C/11D CMake guards.

## Focused graphical truth gate

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug -- -j1
$env:CTEST_PARALLEL_LEVEL='1'
ctest --test-dir out/build/windows-msvc-debug `
  -R '^(platform\.units|launcher\.|stage12\.material_(formal|evidence_validator)|stage17\.skill_stones\.)' `
  --output-on-failure
```

- Fresh serial graphical build: 150/150 actions passed.
- Focused CTest: 12/12 passed in 462.64 s.
- `platform.units`: passed in 12.61 s and retained the approved 488 cases.
- Stage 17 real raylib plus two evidence gates: 3/3 passed in 23.69 s.
- Stage 12 formal capture: passed in 222.94 s.
- Stage 12 evidence validator: passed in 9.72 s.
- Stage 12 80-mutation self-test: passed in 186.56 s.
- Launcher unit, icon, manifest, real-window smoke, and deploy gates: 5/5 passed.

The full graphical CTest suite was intentionally not run. The integration plan
specifies this focused set as the proportional truth gate; it includes the
actual raylib capture, pixel evidence, negative mutation, launcher window, and
platform unit paths touched by Tasks 1-5.

## Scope and repository health

```powershell
git diff main...HEAD --check
git diff main...HEAD --name-status
git status --short --untracked-files=all
git diff --check
```

- Both diff checks passed.
- The branch delta is limited to the launcher, approved monster balance,
  atomic HP-potion gameplay, potion material presentation, their tests/assets,
  plans, and owned validation evidence.
- Task 6 adds only capture/presentation guard compatibility and negative
  mutations plus this record; it does not alter gameplay.
- `tools/__pycache__/build_item_material_atlas.cpython-313.pyc` is a generated,
  untracked local cache. It is intentionally excluded from staging and left
  untouched.

## Resource and process checks

- Before core regression: about 6.15 GiB free of 15.71 GiB.
- The graphical build used one compiler process; its largest translation unit
  briefly used about 3.1 GiB, then released it.
- After graphical validation: about 7.14 GiB free of 15.71 GiB.
- Final check found no `arpg_game`, launcher, `ctest`, `cmake`, compiler, or
  linker process left running.
