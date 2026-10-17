### Task 9: Formal Raylib Evidence — review-fix scope

This is a fix round for the existing Stage 11-D formal evidence implementation.

Global constraints:

- Windows x64; C++17 and raylib 6.0.0 static.
- Do not change character V6 checkpoint bytes or loot generation/drop probabilities.
- Scenarios and gameplay must use the production host, production snapshot, real settings slots, and real save transactions.
- Do not add public snapshot setters, direct view injection, private member access, pre-present screenshots, fake renderers, direct pickup completion, or expected-answer injection.
- Hot-path state remains fixed-capacity and allocation-free.
- Preserve unrelated dirty files and do not commit.

Required review fixes:

1. Record and validate that the player was damaged but survived. `monster_damage_observed` is not evidence of player damage. Add production-observed player maximum HP and/or a monotonic `player_damage_observed` field; formal validation must require a real `0 < hp < max_hp` observation and final survival.
2. Validate exact pickup conservation: in the pickup scenario, remaining ground IDs must equal the original three expected ordinary IDs minus the exact picked ID, not merely have count two.
3. Validate exact per-rarity tuples. The manifest must identify the expected `(item_id, monster_ordinal)` for normal, magic, and rare separately, and each observed rarity tuple must match its own expected tuple. Separate unordered ID and ordinal sets are insufficient.
4. Determine from the existing specification and persistence tests whether literal same-scenario A/B determinism is required here. If required by the formal contract, implement the smallest production-faithful deterministic comparison without doubling all six graphical scenarios. If persistence A/B is already independently proven and Task 9 never requires literal replay, document that conclusion in the report and do not invent a fake claim.
5. Strengthen the evidence guard and its self-test only as needed to protect the new load-bearing checks. Token-only checks that can be moved into comments or unrelated functions are insufficient for these conditions.

Cover the changed behavior with focused unit/validator/self-tests before running the long six-scenario formal gate. Do not launch multiple high-load builds or tests concurrently.

