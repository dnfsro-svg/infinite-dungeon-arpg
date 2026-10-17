### Task 5: Direct-target melee AI (chaser and bulwark)

**Files:**
- Create: `src/combat/monster_ai.cpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/target_simulation.cpp`
- Modify: `src/combat/hit_resolution.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `tests/combat/monster_melee_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Adds `MonsterAiPhase { idle, move, telegraph, active, recovery, cooldown, defeated }`.
- Adds `simulate_monster(std::size_t slot)`, `resolve_monster_contact_attack`, and catalog-driven player-hitbox collision.
- Player attacks iterate active monster slots and retain per-attack `hit_targets[96]` semantics.

- [ ] **Step 1: Write RED behavior tests**

For `chaos_chaser`, assert distance decreases during move, movement stops during telegraph, damage occurs only during active, and cooldown prevents repeated damage. For `water_bulwark`, assert lower speed, higher HP/break, front armor before break, and normal hit reaction after break.

- [ ] **Step 2: Verify RED**

Expected: monsters remain inert or required AI phases do not exist.

- [ ] **Step 3: Implement the shared fixed-step attack state machine**

Use catalog tick fields for transitions. Horizontal facing follows the player only outside active/recovery. Movement clamps to existing room bounds. Contact attacks use a catalog-derived AABB and call Task 4 damage once per attack serial.

Migrate player hit resolution from three dummy slots to active monster slots; attacker hit-stop remains aggregated once per player attack regardless of targets hit.

- [ ] **Step 4: Run all combat tests**

Expected: legacy J/K/L, launcher, knockdown, armor, multi-hit aggregation, and new melee AI tests all pass.

- [ ] **Step 5: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: add melee monster AI"
```

---

