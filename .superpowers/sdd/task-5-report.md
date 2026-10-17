# Stage 7 Task 5 Report

## Status

Completed. `DungeonSession` now owns one `PendingSave` transaction for both
room transitions and permanent passive-tree changes. The old transition API is
kept as a compatibility wrapper and only exposes transition-kind saves.

## Delivered

- Added allocation/refund requests gated to a cleared room with active combat
  and no pending save.
- Added committed, not-committed, indeterminate, and receipt-mismatch handling
  for passive mutations without letting an uncommitted mutation alter the
  stable run state or the current room's combat build.
- Added passive tree, pending-save state, and latest passive error to the
  read-only dungeon snapshot.
- Projected the stable tree's evaluated player modifiers into each newly
  constructed `CombatEncounterConfig`.
- Included passive bits in `same_run_state`, so a mismatched save receipt is
  rejected rather than silently accepting a different permanent tree.
- Added six dungeon tests for stage gating, rollback, commit, indeterminate
  faulting, disconnected refunds, and next-room combat-build projection.

## TDD Evidence

The new dungeon passive-tree test target was first built before the API existed.
It failed at compilation for the absent `PendingSaveKind`, `pending_save`,
passive request methods, and snapshot fields. After the implementation, the
dungeon suite passed with 76 cases.

## Verification

- `cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests`
- `ctest --test-dir out/build/windows-msvc-debug -R "dungeon.units|architecture.dungeon" --output-on-failure`
  - `dungeon.units`: passed (64.18 s)
  - `architecture.dungeon_no_raylib`: passed
  - `architecture.dungeon_no_persistence`: passed
- `ctest --test-dir out/build/windows-msvc-debug -R "^combat.units$|^persistence.units$" --output-on-failure`
  - 2/2 passed
- `git diff --check`

## Scope

No Runtime, host, raylib, or persistence service code was changed. Task 6 is
responsible for driving `pending_save()` through the durable save store.

## Important Review Follow-up

Added three direct regression cases without changing production code:

- A committed passive receipt with the wrong generation faults with
  `save_receipt_mismatch`.
- A committed passive receipt with a different passive bitset faults with
  `save_receipt_mismatch`.
- A committed passive allocation leaves the current room's `CombatWorld`,
  combat tick, barrier, and resistance snapshot unchanged; after a real
  descent transaction, the next room receives the new barrier build.

The dungeon suite now has 79 cases. The direct executable run and the targeted
CTest run both completed; CTest recorded 79 cases and 0 failures, plus both
Dungeon architecture boundary checks passing.
