# Stage 16 SDD Progress Ledger

Plan: `docs/superpowers/plans/2026-07-20-stage16-loot-currency-reinforcement.md`

Task 1: complete (commits 6d6fb3a..946bdf4, review clean; minor: add frozen legacy item-stream regression in final validation).
Task 2: complete (commits 946bdf4..3def7ad, re-review clean; materials copy/publish, divine rolls, uint64 counters and dungeon regressions fixed).
Task 3: complete (commits 3def7ad..a006446, review clean; V7 material/reinforcement persistence and V1-V6 migration verified).
Task 4: complete (commits a006446..672e723, re-review clean; deterministic material ground pool, V7 claim persistence, atomic pickup/vacuum retry, death cleanup, 12/12 focused + 266/266 dungeon + 85/85 persistence verified).
Task 5: complete (commits 672e723..7ae0ad6, re-review clean; material labels, beams, 14-slot paused bag, pickup aggregation, and 800x450 detail clipping verified by platform.units).
Task 6: complete (commits 7ae0ad6..e6f0c21, re-review clean; atomic currency crafting and strict same-base three-to-one with +0 output, 268/268 dungeon + 45/45 items + 357/357 platform verified).
Task 7: complete (commits e6f0c21..a965a09, re-review clean; reinforcement/coupons, battle-stat projection, receipts, destruction and bounded extreme arithmetic verified; final full dungeon suite remains required in Task 8).
Task 8: complete (commits a965a09..f05f195, final review clean; million-roll statistics, frozen equipment RNG stream, real V7 SaveStore/DungeonRuntime production scenario, fresh screenshots, 272/272 dungeon, Stage16 4/4 and release evidence verified).

# Stage 17 SDD Progress Ledger

Plan: `docs/superpowers/plans/2026-07-20-stage17-swordmaster-skill-stones.md`

Confirmed execution order: 1 → 4 → 5 → 2 → 3 → 6 → 7 → 8. Occupied active slots require remove-then-equip or swap; direct overwrite is rejected.
Task 1: complete (commits 43cc049..ce9fe4e, re-review clean; default slots 1/2 corrected, skills.units and combat.units passed).
Task 4: complete (commits ce9fe4e..7ab4fb6, re-review clean; draw slash uses shared hit resolution, world-tick cooldown/timing and real-death clearing verified; minor: freeze 0.22 knockback and x=5.0001 boundary in final review).
Task 5: complete (commit 7ab4fb6..5da5355, review clean; storm swords fixed center, 12 strikes, finisher launch and cancellation verified; minor: freeze exact 0.26/0.24 finisher impulse in final review).
Task 2: complete (commits 5da5355..cc281d6, re-review clean; V8 codec/migration and skill-loadout AB conflict detection verified; persistence.units 94/94).
Task 3: complete (commits cc281d6..c4c97fe, re-review clean; atomic loadout transactions preserve checkpoint-local progression and retain runtime progression; focused 9/9, dungeon.units and Stage16 crafting passed).
Task 6: complete (commits c4c97fe..6863000, re-review clean; number-key routing, fallback priority and input gates verified; platform 364/0, dungeon 285/0, source guards 2/2).
Task 7: complete (commits 6863000..9d7ccd3, re-review clean; five-slot HUD, skill-stone loadout page and snapshot-only effects verified; platform 374/0, persistence 94/0, combat 228/0, HUD zero-alloc passed).
Task 8: complete (commits 9d7ccd3..ece7409, re-review and final whole-branch review clean; real Raylib evidence, safe validator cleanup, Chinese skill UI and exact combat boundaries verified; Debug/Release Stage17 3/3 passed).
