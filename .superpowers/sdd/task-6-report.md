# Task 6 Implementation Report

## Result

- Implemented atomic dungeon equipment, unequipment, and three-to-one recipe requests.
- Added stable-state-only item queries, pending `resume_phase`, item request result/fault contracts, and commit/not-committed/indeterminate handling.
- Added combined passive + equipment build projection using one fixed 256-modifier buffer and one modifier evaluation.
- Equipment and recipe commits hot-apply the combined build without resetting combat or healing HP/barrier.
- Added lightweight inventory/equipment/pending snapshot fields and zeroed fixed-capacity ground-drop scaffolding.
- Extended save-receipt equality to compare item records in acquisition order, equipment, claimed bits, and sequence without allocation.
- Derived combat evasion RNG seed from the room seed through a named deterministic domain.
- Enabled MSVC `/EHsc` for the dungeon target so required `std::bad_alloc` handling has correct unwind semantics.
- Added detailed equipment/build results that distinguish `valid`, `invalid_state`, and `allocation_failure`; item requests fault invalid builds but reject transient allocation failures.
- Added a thread-local, nested-scope-safe allocation failure injector for deterministic transaction OOM tests.

## TDD Evidence

- Initial RED: dungeon test compilation failed on the intentionally missing `RequestResult`, item request APIs, snapshot fields, pending kinds, and item faults.
- Focused GREEN after the main transaction implementation: 8/8 cases passed.
- Second RED: explicit `item_sequence_overflow` fault was absent; added the fault and behavior, then 9/9 passed.
- Third RED: an inconsistent loaded progression/passive tree was not faulted; tightened `build_for` validation, then 10/10 passed.
- Final focused Task 6 suite: 12/12 cases passed.
- Review-fix RED: `project_equipment_detailed` and its status enum were absent, and the items test failed to compile for the expected missing API.
- Review-fix GREEN: detailed projection/build status implemented; final focused Task 6 suite passed 19/19.
- Durable-commit RED: a committed equipment receipt under `ScopedAllocationFailure(0)` increased the allocation counter and failed to publish.
- Durable-commit GREEN: accepted item requests cache the verified fixed-size combat build in the session; committed receipts now publish from that cache without allocation. Final focused Task 6 suite passed 20/20.
- OOM coverage injects failures at ownership scratch, stable item-vector copy, post-copy recipe validation scratch, and combined-build projection scratch. Every case returns `rejected` with phase, pending state, acquisition order, equipment, sequence, generation, combat build, HP, and barrier unchanged.
- The legal catalog cannot produce a candidate-invalid combined build after ownership, passive-tree, and catalog validation succeed. The candidate-invalid branch is retained and reviewed through its detailed status handling; the regression is accurately scoped to invalid loaded equipment ownership rather than claiming a legal candidate-invalid fixture.
- Recipe mutation performs no second container allocation: copying the stable vector preserves capacity `N`, removing three produces size `N-3`, and appending one produces `N-2 <= N`; therefore the append cannot grow or throw `bad_alloc`.

## Verification

- `cmake --build build-release --target arpg_dungeon_tests arpg_combat_tests arpg_item_tests`: passed, no Task 6 warnings.
- `ctest --test-dir build-release -R dungeon.units --output-on-failure`: passed (1/1, 203.82s; 100 dungeon cases).
- `ctest --test-dir build-release -R "core.units|combat.units|items.units" --output-on-failure`: passed (3/3; core 25 cases, combat passed, items 28 cases).
- Full `cmake --build build-release`: passed, including `arpg_game.exe`, persistence, platform, and raylib consumers.
- `git diff --check`: passed.
- Full-build note: the pre-existing `tests/core/allocation_probe.cpp` C4530 warning remains outside Task 6; the dungeon target itself is compiled with `/EHsc` and is clean.

## Commit

`ba52ba4cd1889712e7be46363eaf15d6babc497e` — `feat: add atomic equipment and recipe transactions`

`446cf9b5c0417402ea646cbb02d3512f85ab98c3` — `fix: harden atomic item transactions`

`44ab3ad` — `fix: publish committed equipment without allocation`

## Scope Notes

- Kept the existing by-value `pending_save()` query for Task 8 as required.
- Did not modify platform, persistence, or raylib production sources.
- Review-authorized support changes are limited to detailed item projection and the shared test allocation injector/core test wiring.
- Worktree is clean after the commit; this report lives in the gitignored SDD report directory.
