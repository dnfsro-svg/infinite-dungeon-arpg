# Task 3 Brief — Fixed-percent player health restoration

## Scope

Implement only Task 3 from:
`E:\game\.worktrees\balance-hp-potions\docs\superpowers\plans\2026-07-23-extreme-monster-nerf-health-potions.md`

Worktree: `E:\game\.worktrees\balance-hp-potions`

Allowed files:

- `src/combat/combat_world.hpp`
- `src/combat/combat_world.cpp`
- `tests/combat/player_health_tests.cpp`
- `tests/combat/combat_test_main.cpp`

Report:
`E:\game\.worktrees\balance-hp-potions\.superpowers\sdd\task-3-report.md`

## Required interface and behavior

Add this public API:

```cpp
[[nodiscard]] int restore_player_health_percent(
    std::uint16_t maximum_health_basis_points) noexcept;
```

- Compute the requested heal from the player's actual `max_hp` using `scale_basis_points(..., BasisPointRounding::ceil)`.
- Clamp to missing health and return the actual restored amount.
- Return zero for 0 basis points, invalid/non-positive max HP, a full-health player, or a defeated player.
- Never revive a defeated player or alter its frozen death snapshot.
- This API must bypass the abyss `player_resource_restore_bp` multiplier; do not call `restore_player_resources()`.

Minimal implementation:

```cpp
int CombatWorld::restore_player_health_percent(
    std::uint16_t maximum_health_basis_points) noexcept {
    if (maximum_health_basis_points == 0U || player_.max_hp <= 0
            || player_defeated()) {
        return 0;
    }
    const int requested = scale_basis_points(player_.max_hp,
        maximum_health_basis_points, BasisPointRounding::ceil);
    const int missing = (std::max)(0, player_.max_hp - player_.hp);
    const int actual = (std::min)(requested, missing);
    player_.hp += actual;
    return actual;
}
```

Place the declaration in the public section and implementation beside `restore_player_resources()`.

## Required tests

Add the plan's two cases to `player_health_tests.cpp`:

```cpp
arpg::test::Failure percent_health_restore_uses_actual_max_and_bypasses_abyss_multiplier() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.player_build.values.max_health = 10000;  // actual max becomes 1001
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 500, 0);
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 251);
    ARPG_REQUIRE(world.snapshot().player.hp == 751);
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 900, 0);
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 101);
    ARPG_REQUIRE(world.snapshot().player.hp == 1001);
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 0);

    config = single_chaser_encounter();
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::exhausted_recovery);
    CombatWorld abyss{config};
    arpg::test::CombatWorldTestAccess::set_player_resources(abyss, 600, 0);
    ARPG_REQUIRE(abyss.restore_player_health_percent(2500U) == 250);
    ARPG_REQUIRE(abyss.snapshot().player.hp == 850);
    ARPG_REQUIRE(abyss.restore_player_health_percent(0U) == 0);
    return {};
}

arpg::test::Failure percent_health_restore_never_revives_defeated_player() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.wave = {};
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::apply_damage(world,
        world.snapshot().player.max_hp, Vec3{}, FeedbackLevel::heavy);
    const auto death = world.death_snapshot();
    ARPG_REQUIRE(death.has_value());
    const std::uint64_t death_tick = death->tick;
    const std::uint64_t final_damage = death->final_damage;
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 0);
    ARPG_REQUIRE(world.snapshot().player.hp == 0);
    ARPG_REQUIRE(world.death_snapshot().has_value());
    ARPG_REQUIRE(world.death_snapshot()->tick == death_tick);
    ARPG_REQUIRE(world.death_snapshot()->final_damage == final_damage);
    return {};
}
```

Register both cases. Expected suite total after Task 2: `243 -> 245`.

## TDD and verification

Read and follow:
`E:\codex1\.codex\plugins\cache\openai-curated-remote\superpowers\6.1.1\skills\test-driven-development\SKILL.md`

1. Write/register both tests first.
2. Build and capture RED: compilation must fail because `CombatWorld` lacks the API.
3. Implement only the minimal API.
4. Run:

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

Expected final result: `245 cases, 0 failures`.

## Hard boundaries

- Do not implement potion drop/pools/persistence/auto-use/UI/assets.
- Do not change existing abyss resource restore behavior; only the new API bypasses it.
- Do not change Task 1 monster values or Task 2 affix-depth behavior.
- Do not change player max-health formulas, damage, defenses, or death behavior.
- Do not touch `E:\game`.

## Completion

- Commit exactly the scoped files with message:
  `feat: restore fixed percent health after committed potion use`
- Write the report with status, commit hash, RED/GREEN evidence, changed files, concerns.
- Return `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`.
