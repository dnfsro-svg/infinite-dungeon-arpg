### Task 1: Monster catalog and fixed public data contracts

**Files:**
- Create: `src/combat/monster_catalog.hpp`
- Create: `src/combat/monster_catalog.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `tests/combat/monster_catalog_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Consumes: existing `Vec3`, `Aabb`, `FeedbackLevel`, `ImpactKind`, and fixed-step tick conventions.
- Produces:
  - `enum class MonsterId : std::uint8_t { fire_bomber, fire_charger, water_bulwark, water_support, lightning_shooter, lightning_dasher, chaos_chaser, chaos_hazard, count };`
  - `enum class MonsterTag : std::uint16_t` bit values for `melee`, `ranged`, `support`, `high_priority`, `ground_hazard`, `direct_target`.
  - `struct MonsterDefinition` containing ID, preferred ecology index `0..3`, tags, threat cost, HP, break, speed, attack ranges/ticks/damage, projectile/hazard fields, and feedback level.
  - `struct MonsterSpawnSpec { MonsterId id; Vec3 position; };`
  - `struct EncounterWave { std::array<MonsterSpawnSpec, kEncounterSpawnCapacity> spawns; std::uint8_t spawn_count; std::uint8_t spent_budget; };`
  - `monster_definition(MonsterId) noexcept` and `monster_catalog_valid() noexcept`.
  - Capacity constants `kMonsterCapacity=96`, `kProjectileCapacity=384`, `kHazardCapacity=96`, `kEncounterWaveCapacity=2`, `kEncounterSpawnCapacity=96`.

- [ ] **Step 1: Add catalog tests that cannot compile before the API exists**

```cpp
arpg::test::Failure catalog_contains_eight_unique_stable_ids() noexcept {
    std::array<bool, static_cast<std::size_t>(MonsterId::count)> seen{};
    for (std::uint8_t raw = 0; raw < static_cast<std::uint8_t>(MonsterId::count); ++raw) {
        const auto id = static_cast<MonsterId>(raw);
        const auto* definition = monster_definition(id);
        ARPG_REQUIRE(definition != nullptr);
        ARPG_REQUIRE(definition->id == id);
        ARPG_REQUIRE(!seen[raw]);
        seen[raw] = true;
    }
    ARPG_REQUIRE(monster_catalog_valid());
    return {};
}

arpg::test::Failure catalog_has_two_preferred_roles_per_ecology() noexcept {
    std::array<std::uint8_t, 4> counts{};
    for (std::uint8_t raw = 0; raw < static_cast<std::uint8_t>(MonsterId::count); ++raw) {
        const auto* definition = monster_definition(static_cast<MonsterId>(raw));
        ARPG_REQUIRE(definition->preferred_ecology < counts.size());
        ++counts[definition->preferred_ecology];
        ARPG_REQUIRE(definition->threat_cost > 0U);
        ARPG_REQUIRE(definition->max_hp > 0);
    }
    ARPG_REQUIRE(counts == std::array<std::uint8_t, 4>{{2, 2, 2, 2}});
    return {};
}
```

- [ ] **Step 2: Register the suite and verify RED**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

Expected: compilation fails because `MonsterId`, `MonsterDefinition`, and `monster_definition` do not exist.

- [ ] **Step 3: Add the exact catalog types and eight definitions**

```cpp
enum class MonsterId : std::uint8_t {
    fire_bomber,
    fire_charger,
    water_bulwark,
    water_support,
    lightning_shooter,
    lightning_dasher,
    chaos_chaser,
    chaos_hazard,
    count,
};

enum class MonsterTag : std::uint16_t {
    none = 0,
    melee = 1U << 0U,
    ranged = 1U << 1U,
    support = 1U << 2U,
    high_priority = 1U << 3U,
    ground_hazard = 1U << 4U,
    direct_target = 1U << 5U,
};

struct MonsterDefinition final {
    MonsterId id{MonsterId::chaos_chaser};
    std::uint8_t preferred_ecology{};
    std::uint16_t tags{};
    std::uint8_t threat_cost{1};
    int max_hp{100};
    int max_break{};
    float move_speed{0.04F};
    float preferred_range{1.0F};
    std::uint16_t telegraph_ticks{20};
    std::uint16_t active_ticks{4};
    std::uint16_t recovery_ticks{20};
    std::uint16_t cooldown_ticks{60};
    int contact_damage{10};
    float projectile_speed{};
    std::uint16_t hazard_ticks{};
    FeedbackLevel feedback{FeedbackLevel::light};
};
```

Use one `constexpr std::array<MonsterDefinition, 8>` in catalog order. Validation must reject zero costs/HP, ecology above 3, missing direct combat targets, and IDs not matching their array index.

- [ ] **Step 4: Run the combat executable and architecture test**

Run:

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
```

Expected: catalog suite passes; all pre-existing combat cases remain green; architecture tests remain green.

- [ ] **Step 5: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: define stage 4 monster catalog"
```

---

