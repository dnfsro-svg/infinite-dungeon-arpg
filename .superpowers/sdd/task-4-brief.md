### Task 4: Player health and monster-hit contract

**Files:**
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/player_simulation.cpp`
- Create: `tests/combat/player_health_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Produces `PlayerSnapshot::hp`, `max_hp`, `hurt_ticks`, and `invulnerability_ticks`.
- Adds `CombatEventKind::player_hit`, `player_hurt_started`, `player_health_reset`.
- Adds private `CombatWorld::apply_player_damage(int, Vec3, FeedbackLevel) noexcept`.

- [ ] **Step 1: Write RED tests**

```cpp
arpg::test::Failure player_damage_clamps_at_one_and_uses_protection() noexcept {
    CombatWorld world{single_chaser_encounter()};
    const int max_hp = world.snapshot().player.max_hp;
    tick_until_first_monster_hit(world);
    const int after_first = world.snapshot().player.hp;
    ARPG_REQUIRE(after_first < max_hp);
    tick_for(world, 1U);
    ARPG_REQUIRE(world.snapshot().player.hp == after_first);
    tick_until_invulnerability_ends(world);
    repeat_monster_hits_until_clamped(world);
    ARPG_REQUIRE(world.snapshot().player.hp == 1);
    return {};
}
```

Do not add a permanent public test-only method. Drive damage through a minimal monster attack fixture or a private test friend already used by combat support.

Add tests for hit event payload, hurt movement lock, and `load_wave(..., reset_player_health=true)` restoring max HP.

- [ ] **Step 2: Verify RED**

Expected: tests fail because snapshots and player-damage events do not exist.

- [ ] **Step 3: Implement health with Stage 4 clamp semantics**

Use constants in `combat_world.cpp`:

```cpp
constexpr int kStage4PlayerMaxHp = 1000;
constexpr std::uint16_t kPlayerHurtTicks = 12;
constexpr std::uint16_t kPlayerInvulnerabilityTicks = 30;
```

Damage is ignored during invulnerability, otherwise `hp = max(1, hp-damage)`. Emit one `player_hit` and one aggregated feedback event per accepted hit. No death state is added.

- [ ] **Step 4: Run combat tests and `git diff --check`**

Run the core-only test preset and direct combat executable; expect zero failures.

- [ ] **Step 5: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: add stage 4 player health"
```

---

