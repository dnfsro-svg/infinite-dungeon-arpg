# Stage 8 Task 8 implementation report

## Result

- Added `DungeonSession::pending_save_view()` as a zero-copy, zero-allocation temporary const view.
- Generalized `DungeonRuntime::service_pending_save()` to read that view once and use one `SaveStore::commit(next_state)` path for every pending kind.
- Mapped committed/not_committed/indeterminate exactly and moved `verified_state` into the local session receipt after synchronizing render status.
- Added runtime-only narrow pickup/equip/unequip/recipe forwarding and stable `item_state()` access, guarded by running state and session presence.
- Preserved `service_pending_transition()` as a compatibility wrapper and did not modify raylib host, renderer, UI, or V4 checkpoint ABI.

## TDD evidence

- Initial RED: platform build failed on the missing `pending_save_view`, four runtime request functions, and runtime `item_state` API.
- Initial GREEN: 54 platform cases passed after the minimal API and generic service implementation.
- Expanded fault matrix GREEN: pickup/equip/unequip/recipe x committed/not_committed/indeterminate passed with state, indicator, phase, stable ownership, ground state, materials, generation, and build assertions.
- Mutation proof:
  - Replacing `pending_save_view()` with by-value `pending_save()` made `generic service adds no large state copies` fail.
  - Replacing `std::move(saved.verified_state)` with a copy made the same test fail.
  - Restoring both produced 56/56 platform cases.

## Large inventory evidence

- Constructed one complete valid 65,535-item ownership state and `DungeonSession`; item count was not reduced.
- Repeated `snapshot()`, `item_state()`, and empty `pending_save_view()` calls without changing allocation count.
- Created a 65,535-item equipment pending transaction and repeated `pending_save_view()` without changing allocation count.
- Compared allocation deltas for a direct real `SaveStore::commit` and runtime generic service on the same 65,535-item next state; deltas were equal, proving no runtime-layer item-vector copy.
- No OOM occurred; focused platform suite completed in about two seconds.

## Persistence and restart evidence

- Real `SaveStore + DungeonSession` integration independently covers equipment committed/not_committed/indeterminate without platform/raylib dependency.
- Restart compares acquisition order and all 40 bytes of every item, six equipment IDs, three claimed words, next sequence, commit generation, and rebuilt `PlayerCombatBuild`.
- Existing seven-point variable-length SaveStore fault matrix now compares complete ownership state instead of item count only.

## Verification

- `arpg_platform_tests`: 56 cases, 0 failures.
- `arpg_persistence_tests`: 48 cases, 0 failures.
- CTest integration selection: 9/9 passed.
  - `dungeon.units`: passed in 388.09 seconds.
  - `persistence.units`: passed.
  - `platform.view_math`: passed.
  - persistence/dungeon/combat boundary tests selected by the regex: passed.
- Full Release build: passed and linked `arpg_game.exe`.
- `git diff --check`: passed; only Git's existing Windows LF-to-CRLF conversion notices were printed.

## Scope

- No V4 ABI changes.
- No raylib host/renderer/UI changes.
- No progress-ledger changes.
