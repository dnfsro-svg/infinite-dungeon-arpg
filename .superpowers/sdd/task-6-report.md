# Stage 16 Task 6 Report - Currency Crafting and Recipe Transactions

## Delivered

- Added `DungeonSession::request_craft(MaterialId, item_id, optional<DirectedCategory>)`.
  It validates the stable ownership state and material count, runs pure
  `items::craft_item` before making a next state, then consumes exactly one
  material only when the pure operation was applied and consumed.  The item ID
  is retained and the existing item-save path recalculates an equipped target's
  combat build.
- Added `PendingSaveKind::craft` and included it in pending-build consistency
  and commit publication.
- Tightened three-to-one inputs to exact base ID and rarity.  Recipe output
  keeps that base, uses the existing integer floor average level, and starts at
  reinforcement zero.
- Added inventory material interaction: select an owned material, click an
  item to submit the craft transaction, right-click to cancel; selected
  Directed Core can cycle damage/defense/speed/element with right-click on its
  material slot.  A reinforced recipe input shows a loss warning.

## TDD evidence

- RED: the new dungeon transaction test failed to compile because
  `DungeonSession::request_craft` was absent.
- GREEN: `stage16.crafting_transaction.units` passes 2/2, including deferred
  material consumption, failed-save rollback, invalid no-consume behavior,
  exact-base recipe input, floor average item level, and reinforcement reset.
- A material-bag cancellation test was also added RED-first and now passes.

## Verification

- Built: `arpg_dungeon_tests`, `arpg_item_tests`, `arpg_platform_tests`,
  `arpg_game` (Debug/MSVC x64).
- `stage16.crafting_transaction.units`: PASS, 2/2.
- `items.units`: PASS, 45/45.
- `platform.units`: PASS, 356/356.
- `git diff --check`: clean before commit.

## Notes

- Existing recipe unit tests were updated from the superseded same-slot
  contract to the approved same-base contract.  This was required for the
  tightened public rule, not a relaxation of the implementation.
