### Task 7: Transactional Pickup HUD Feedback

Worktree: `E:/game/.worktrees/stage11d-loot-filter`
Start commit: `417f58a`

Implement only Task 7 from `docs/superpowers/plans/2026-07-18-stage11d-ground-loot-filter.md`.
The amended design in `docs/superpowers/specs/2026-07-18-stage11d-ground-loot-filter-design.md` is authoritative.

Core production invariant:

- `DungeonRuntime::fixed_tick()` synchronously calls `service_pending_save()`, so the presented HUD cannot rely on seeing a pending snapshot.
- Before the real synchronous save, cache the exact candidate selected by the production `pending_pickup_ordinal` from the production ground snapshot.
- Publish a fixed-capacity `LootPickupReceipt` in `DungeonRenderStatus` only after store commit succeeded, Session accepted the receipt, commit generation advanced, and the exact item disappeared.
- Save failure, rollback, non-pickup saves, rejected receipts, wrong ordinals, surviving items, fault, or recovery must not publish a new success receipt.
- The presentation observer reads only `DungeonRenderStatus`; it must never inspect `DungeonSession::item_state()`.
- First observation of an already-valid receipt establishes a baseline without replaying it. A newer valid receipt publishes once. Same/older/invalid/error/recovery observations publish nothing.
- Ordinary and abyss pickup sources must be distinguishable; abyss feedback uses abyss styling.
- No heap allocation in unchanged observation hot loops.

Expected files and interfaces are exactly those listed in Task 7 of the plan. Keep the patch within Task 7; do not implement Task 8+.

Required workflow:

1. Write focused failing tests first and capture genuine RED evidence.
2. Implement the minimum production code.
3. Run focused tests, relevant platform regressions, `git diff --check`, and the 100,000-iteration allocation probe.
4. Write `.superpowers/sdd/stage11d-task-7-report.md` with RED/GREEN commands and results, key invariants, files changed, and remaining risks. The tracked `.superpowers/sdd/task-7-report.md` is the historical Stage 10 report and must remain unchanged.
5. Commit all Task 7 source/test changes as `feat: report confirmed loot pickups`.
6. Leave the worktree clean and report the commit hash.

Do not weaken existing tests or guards. Do not modify Stage 11-D design/plan docs or unrelated files.

Review fixes are recorded in `.superpowers/sdd/stage11d-task-7-review-fix-report.md`. The tracked `.superpowers/sdd/task-7-review-fix-report.md` is also historical Stage 10 evidence and must remain unchanged.
