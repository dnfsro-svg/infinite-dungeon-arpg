# Task 4 — deterministic fixed-capacity health-potion model

Status: `DONE_WITH_CONCERNS`

Commit: `82379ae7ec8ff00e3aaf85c21fc33a1bc439688c`

## RED / GREEN evidence

- RED (after registering the test source and running under the required MSVC
  developer environment): `dungeon_health_potion_tests.cpp` failed with
  `fatal error C1083: cannot open include file: dungeon/health_potion_loot.hpp`.
  The failure was for the missing Task 4 public header, before implementation
  existed.
- GREEN build:
  `cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1`
  completed successfully.
- GREEN focused run with `ARPG_HEALTH_POTION_ONLY=1`:
  `3 cases, 0 failures`.
- Full dungeon executable run completed with the updated total:
  `290 cases, 1 failures`.

## Changed files (committed)

- `src/dungeon/health_potion_loot.hpp`
- `src/dungeon/health_potion_loot.cpp`
- `src/dungeon/CMakeLists.txt`
- `src/dungeon/dungeon_types.hpp`
- `tests/dungeon/dungeon_health_potion_tests.cpp`
- `tests/dungeon/CMakeLists.txt`
- `tests/dungeon/dungeon_test_main.cpp`

## Scope and concerns

- Implemented only the declared model, independent deterministic RNG domain,
  fixed capacities, snapshot/pending-save declarations, and the three frozen
  contract tests. No drop generation, ground-pool population, pickup/healing,
  persistence codec, inventory, or UI behavior was added.
- The full suite's sole failure is reproducible in the existing focused
  `ARPG_STAGE10_ABYSS_STRESS_ONLY` run: its 1000-room abyss trace reports
  hash `0x0d55b41ebb0b1128` while the pre-existing assertion expects
  `0xe102b17e6423351b`. Door hit/trial counts remain the expected
  `9/11/9/7` and `4000`. Task 4 does not call its new potion RNG/model in
  that path, and this report intentionally does not alter the unrelated
  golden baseline.
