# Stage 4 Monster Director Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the three static combat dummies with eight fixed-capacity monster roles and a deterministic, budget-constrained one/two-wave encounter director while preserving Stage 3 save and room-transition semantics.

**Architecture:** `combat` owns monster definitions, runtime pools, AI, attacks, player health, projectiles, and hazards. `dungeon` deterministically builds a fixed-size `RoomEncounterPlan` from stable room data and owns wave progression. The raylib layer renders snapshots and events only; no gameplay decisions move into rendering.

**Tech Stack:** C++17, raylib 6.0.0, CMake 3.25+, Ninja, CTest, MSVC 19.44 x64, Windows SDK 10.0.26100.0.

## Global Constraints

- Work only on branches/worktrees based on `milestone/m04-monster-director`; do not modify or merge `main`.
- `core`, `combat`, `dungeon`, and `persistence` must not include or link raylib.
- Keep dependency direction `dungeon -> combat`; do not create a general ECS or a new `actors` module.
- Simulate only the current room; do not retain old rooms or pre-generate unchosen doors.
- Use fixed-step 60 Hz gameplay and deterministic RNG domains derived from the committed room seed.
- Monster, projectile, hazard, event, and encounter storage must be fixed-capacity with no sustained heap allocation in update/hit/director hot paths.
- Stage 4 implements no death, rewards, XP, levels, equipment, loot, affixes, dedicated abyss combat, summoners, or sniper monsters.
- Player HP clamps at 1 and resets to full on committed room entry; this is a Stage 4 acceptance rule, not a permanent death rule.
- Abyss remains a visual placeholder and uses the normal encounter director.
- Every production change follows RED -> GREEN TDD and ends in an atomic commit.

---

## File Map

**New combat files**

- `src/combat/monster_catalog.hpp/.cpp`: eight immutable definitions, stable IDs, tags, stats, attack timing, and catalog validation.
- `src/combat/monster_pool.hpp/.cpp`: fixed monster/projectile/hazard runtime storage and snapshot conversion.
- `src/combat/monster_ai.cpp`: fixed-step role state machines and attack-object creation.
- `tests/combat/monster_catalog_tests.cpp`: catalog uniqueness and data contracts.
- `tests/combat/player_health_tests.cpp`: damage, protection, HP clamp, and room reset.
- `tests/combat/monster_melee_tests.cpp`: chaser/bulwark behavior.
- `tests/combat/monster_ranged_tests.cpp`: shooter/support/projectile behavior.
- `tests/combat/monster_special_tests.cpp`: bomber/charger/dasher/hazard behavior.

**New dungeon files**

- `src/dungeon/encounter_director.hpp/.cpp`: deterministic budget calculation and constrained wave composition.
- `tests/dungeon/encounter_director_tests.cpp`: budgets, ecology weights, constraints, fallback, and determinism.
- `tests/dungeon/dungeon_wave_tests.cpp`: one/two-wave lifecycle and door/hole gating.

**Modified shared files**

- `src/combat/combat_types.hpp`: monster IDs/tags, encounter spawn specs, player/monster/projectile/hazard snapshots, and diagnostics.
- `src/combat/combat_world.hpp/.cpp`: pool ownership, encounter initialization, player health, tick orchestration, and active-monster count.
- `src/combat/hit_resolution.cpp`, `player_simulation.cpp`, `target_simulation.cpp`: migrate dummy-specific loops to active monster slots while retaining player attack behavior.
- `src/dungeon/dungeon_rules.hpp/.cpp`: director configuration and validation.
- `src/dungeon/dungeon_session.hpp/.cpp`, `dungeon_types.hpp`, `room_combat_template.hpp/.cpp`: encounter-plan construction, wave delay, wave spawn, and snapshot fields.
- `src/platform/raylib/combat_renderer.hpp/.cpp`, `combat_feedback.cpp`, `combat_audio.cpp`: monster visuals, warnings, player HP, waves, projectiles, hazards, and event feedback.
- Combat/dungeon/platform `CMakeLists.txt` and `*_test_main.cpp`: register new files and exact case-count guards.

---

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

### Task 2: Deterministic budget encounter director

**Files:**
- Create: `src/dungeon/encounter_director.hpp`
- Create: `src/dungeon/encounter_director.cpp`
- Modify: `src/dungeon/dungeon_rules.hpp`
- Modify: `src/dungeon/dungeon_rules.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/encounter_director_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Consumes: Task 1 `MonsterId`, `MonsterDefinition`, catalog lookup, deterministic RNG, and `checkpoint::DungeonElement`.
- Produces:

```cpp
struct EncounterDirectorConfig final {
    std::uint8_t base_budget{8};
    std::uint8_t depth_step{5};
    std::uint8_t budget_per_step{1};
    std::uint8_t max_budget{24};
    std::uint8_t two_wave_threshold{12};
    std::uint8_t matching_ecology_weight{4};
    std::uint8_t off_ecology_weight{1};
    std::uint8_t normal_high_priority_limit{1};
    std::uint8_t high_budget_priority_limit{2};
    std::uint8_t ranged_limit{4};
    std::uint8_t support_limit{2};
    std::uint8_t ground_hazard_limit{3};
};

struct RoomEncounterPlan final {
    std::array<combat::EncounterWave, combat::kEncounterWaveCapacity> waves{};
    std::uint8_t wave_count{};
    std::uint8_t total_budget{};
};

EncounterPlanResult build_encounter_plan(std::uint64_t room_seed,
    std::uint64_t depth, checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig&) noexcept;
```

- [ ] **Step 1: Write RED tests for budget math, determinism, ecology and constraints**

```cpp
arpg::test::Failure director_budget_is_bounded_and_depth_driven() noexcept {
    EncounterDirectorConfig config{};
    ARPG_REQUIRE(encounter_budget(1U, config) == 8U);
    ARPG_REQUIRE(encounter_budget(5U, config) == 8U);
    ARPG_REQUIRE(encounter_budget(6U, config) == 9U);
    ARPG_REQUIRE(encounter_budget(10000U, config) == 24U);
    return {};
}

arpg::test::Failure encounter_plan_is_deterministic_and_legal() noexcept {
    const auto a = build_encounter_plan(0xA11CEULL, 18U,
        DungeonElement::lightning, EncounterDirectorConfig{});
    const auto b = build_encounter_plan(0xA11CEULL, 18U,
        DungeonElement::lightning, EncounterDirectorConfig{});
    ARPG_REQUIRE(a.fault == DungeonFault::none);
    ARPG_REQUIRE(same_encounter_plan(a.plan, b.plan));
    ARPG_REQUIRE(encounter_plan_legal(a.plan, EncounterDirectorConfig{}));
    ARPG_REQUIRE(a.plan.wave_count == 2U);
    return {};
}
```

Add a 4096-seed ecology distribution test asserting matching-element selections exceed off-element selections without requiring every spawn to match.

- [ ] **Step 2: Verify RED**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

Expected: compilation fails on the missing director interfaces.

- [ ] **Step 3: Implement budget calculation and constrained deterministic selection**

Use a dedicated domain constant:

```cpp
constexpr std::uint64_t kEncounterDirectorDomain = 0x454E434F554E5434ULL;
```

Algorithm:

1. Validate all config fields are nonzero where required and limits do not exceed fixed capacities.
2. Calculate the bounded budget with checked integer operations.
3. Set wave count to 2 only when budget is greater than 12; split budget deterministically with the first wave receiving the extra point.
4. For each wave, start with the cheapest `direct_target` candidate that fits.
5. Build a fixed candidate array from catalog entries that fit remaining budget and tag limits; weight matching ecology by 4 and off ecology by 1.
6. Select with `DeterministicRng::next_bounded(total_weight)`; stop when no legal candidate fits.
7. If no legal direct target was produced, replace the wave with a deterministic `chaos_chaser` safe fallback.
8. Generate spawn positions from a separate encounter-position substream and clamp them to the existing room bounds.

- [ ] **Step 4: Run dungeon and full core-only tests**

Run:

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
```

Expected: director cases pass, dungeon deterministic tests pass, all 12 core-only CTest entries pass.

- [ ] **Step 5: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: add deterministic encounter director"
```

---

### Task 3: Fixed runtime pools and snapshot migration

**Files:**
- Create: `src/combat/monster_pool.hpp`
- Create: `src/combat/monster_pool.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `tests/combat/monster_pool_tests.cpp`
- Modify: `tests/combat/attack_state_tests.cpp`
- Modify: `tests/combat/break_stress_tests.cpp`
- Modify: `tests/combat/combat_config_tests.cpp`
- Modify: `tests/combat/dummy_reaction_tests.cpp`
- Modify: `tests/combat/hit_resolution_tests.cpp`
- Modify: `tests/combat/movement_jump_tests.cpp`
- Modify: `tests/combat/combat_test_support.hpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Consumes: Tasks 1-2 `MonsterId`, `MonsterSpawnSpec`, `EncounterWave`.
- Produces fixed runtime slot APIs:

```cpp
struct MonsterHandle final { std::uint16_t index{0xFFFF}; std::uint16_t generation{}; };

class MonsterPool final {
public:
    void clear() noexcept;
    [[nodiscard]] std::optional<MonsterHandle> spawn(
        MonsterId id, Vec3 position) noexcept;
    [[nodiscard]] bool destroy(MonsterHandle) noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] MonsterRuntime* get(MonsterHandle) noexcept;
    [[nodiscard]] const std::array<MonsterRuntime, kMonsterCapacity>& slots() const noexcept;
};
```

`CombatSnapshot` changes from a fixed `dummies[3]` presentation to `monsters[96]` plus `monster_count`. All existing combat tests are migrated in this task to use active monster slots; no duplicate legacy snapshot is retained.

- [ ] **Step 1: Write RED pool tests**

Test first-free deterministic allocation, stale handle rejection after destroy/reuse, full-capacity rejection, clear reset, and snapshot active count. Fill all 96 slots and assert allocation 97 returns `nullopt` without modifying existing slots.

- [ ] **Step 2: Verify RED**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

Expected: missing `MonsterPool` and snapshot fields fail compilation.

- [ ] **Step 3: Implement pool and migrate CombatWorld initialization**

`MonsterRuntime` must contain only bounded state: active/generation, definition ID, spawn/position/velocity, facing, reaction/armor, AI phase/ticks, HP/break/shield, hit-stop, and owner-linked transient counters. No owning pointers or vectors.

Add:

```cpp
struct CombatEncounterConfig final {
    Vec3 player_spawn{};
    Facing initial_facing{Facing::right};
    EncounterWave wave{};
    bool reset_player_health{true};
};

explicit CombatWorld(CombatEncounterConfig config) noexcept;
[[nodiscard]] bool load_wave(const EncounterWave&) noexcept;
[[nodiscard]] std::size_t active_monster_count() const noexcept;
```

Keep the old `CombatLabConfig` constructor temporarily as a test compatibility adapter until Task 7 removes its dungeon use.

- [ ] **Step 4: Run combat tests**

Run:

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
```

Expected: all pool tests and all legacy combat tests pass; no persistent allocation regression.

- [ ] **Step 5: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: add fixed monster runtime pool"
```

---

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

### Task 6: Ranged, support, projectile and shield behavior

**Files:**
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/monster_pool.hpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/combat_world.cpp`
- Create: `tests/combat/monster_ranged_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Adds fixed `ProjectileRuntime[384]`, `ProjectileSnapshot[384]`, active count, owner handle, position/velocity, lifetime, damage, and collision radius.
- Adds bounded shield points/duration to monster runtime/snapshot.

- [ ] **Step 1: Write RED tests**

Cover shooter range maintenance, telegraph-before-projectile, deterministic projectile trajectory, projectile removal on hit/bounds/lifetime, pool saturation diagnostics, support selecting the lowest-index legal ally, shield non-stacking refresh, and support never being the only direct target.

- [ ] **Step 2: Verify RED**

Expected: missing projectile and shield behavior failures.

- [ ] **Step 3: Implement shooter and support**

The shooter moves toward/away from its preferred range, freezes during telegraph, and creates one projectile in active. The support chooses an active non-support ally without shield; if none exists it repositions and enters a short cooldown. Shield is a separate bounded integer absorbed before HP and never stacks above the definition cap.

- [ ] **Step 4: Run core-only tests**

Expected: combat and architecture tests pass with no capacity overflow under legal fixtures.

- [ ] **Step 5: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: add ranged and support monsters"
```

---

### Task 7: High-priority bomber, charger, dasher and hazard behavior

**Files:**
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/monster_pool.hpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/combat_world.cpp`
- Create: `tests/combat/monster_special_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Adds fixed `HazardRuntime[96]`/snapshot with owner, center, radius, telegraph/active/lifetime ticks, damage interval, and per-player attack latch.
- Adds monster phase payload for captured target position and attack vector.

- [ ] **Step 1: Write RED tests for all four roles**

Bomber: long warning, no early damage, one explosion, self defeated, no repeated damage. Charger: line capture at telegraph start, no retarget during active, boundary clamp. Dasher: shorter warning, burst movement, recovery gap. Hazard caster: warning marker then bounded active area, periodic damage protected by interval/invulnerability, owner defeat removes non-death hazards.

- [ ] **Step 2: Verify RED**

Expected: required phases/hazards do not exist.

- [ ] **Step 3: Implement high-priority attacks using shared phase transitions**

All target capture happens once at telegraph entry. All spawned hazards/projectiles include a valid owner handle. On monster defeat, scan bounded transient arrays once and remove non-death-owned objects. Saturation increments diagnostics and does not allocate or overwrite active slots.

- [ ] **Step 4: Run combat Debug and Release direct executables**

Expected: every role test and legacy combat case has zero failures in both configurations.

- [ ] **Step 5: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: add high priority monster attacks"
```

---

### Task 8: Dungeon wave lifecycle integration

**Files:**
- Modify: `src/dungeon/dungeon_rules.hpp`
- Modify: `src/dungeon/dungeon_rules.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/room_combat_template.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Create: `tests/dungeon/dungeon_wave_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: existing dungeon lifecycle/transaction/stress helpers that assume exactly three dummies.

**Interfaces:**
- `DungeonRules` contains `EncounterDirectorConfig encounter{}` and validation delegates to director validation.
- `DungeonSnapshot` adds `wave_index`, `wave_count`, `wave_delay_ticks`, `remaining_targets`, and encounter diagnostics.
- `RoomPhase` adds `wave_delay` between combat waves.

- [ ] **Step 1: Write RED wave tests**

```cpp
arpg::test::Failure two_wave_room_keeps_exits_closed_until_last_wave() noexcept {
    DungeonRules rules = rules_with_budget(16U);
    DungeonSession session{rules, state_for_seed(two_wave_seed())};
    clear_current_wave(session);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::wave_delay);
    ARPG_REQUIRE(!any_exit_open(session.snapshot()));
    tick_wave_delay(session);
    ARPG_REQUIRE(session.snapshot().wave_index == 1U);
    clear_current_wave(session);
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(all_exits_open(session.snapshot()));
    return {};
}
```

Add tests that a hole stays sealed through wave delay, reset reproduces the same plan, reload reproduces the same plan, committed room entry restores player HP, and abyss uses the normal director config.

- [ ] **Step 2: Verify RED**

Expected: session clears after wave one and missing snapshot fields fail.

- [ ] **Step 3: Integrate director and waves**

`construct_current_room()` builds the plan from `stable_state_.current_room.seed/depth/ecology`, constructs CombatWorld from wave 0, and records plan/wave index. When active monsters reach zero, either start fixed `kWaveDelayTicks=45` or enter `cleared`. During delay, no combat simulation or exit requests are accepted. Loading wave 1 does not reset player HP; committed new-room construction does.

- [ ] **Step 4: Update dungeon test support and run core-only CTest**

Replace three-dummy helpers with nearest-living-monster helpers over active snapshot slots. Expected: all dungeon progression, transaction, navigation, lifecycle, and wave cases pass.

- [ ] **Step 5: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: integrate deterministic monster waves"
```

---

### Task 9: Raylib presentation, warnings, HP and director HUD

**Files:**
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/combat_feedback.cpp`
- Modify: `src/platform/raylib/combat_audio.cpp`
- Modify: `src/platform/raylib/combat_view_math.hpp`
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Create: `tests/platform/monster_view_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Pure `monster_visual(MonsterId, MonsterAiPhase, ecology)` returns body/accent/warning colors, shape ID, icon label, and warning mode.
- Pure projectile/hazard projection functions reuse existing 2.5D projection.

- [ ] **Step 1: Write RED pure-view tests**

Assert all eight IDs produce distinct role labels, matching ecology accent colors, high-priority phases expose a warning, inactive slots are hidden, player HP ratio clamps to `[0,1]`, and hazard warning/active modes differ.

- [ ] **Step 2: Verify RED**

Expected: missing visual functions fail compilation.

- [ ] **Step 3: Implement visuals and feedback**

Render active monsters in depth order using simple low-cost silhouettes: bomber round/explosive, charger long weapon, bulwark wide body/shield, support cross/shield ring, shooter ranged barrel, dasher pointed shape, chaser claw, hazard caster staff/circle. Draw warnings before active effects. Add player HP bar and HUD lines for `Wave current/total`, budget, active monster/projectile/hazard counts, and saturation diagnostics. Reuse aggregated shake/audio rules; one monster attack emits one low-frequency layer regardless of targets.

- [ ] **Step 4: Run platform tests and launch Debug**

Run:

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: all platform tests pass and the executable opens with readable silhouettes/HUD.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render stage 4 monster encounters"
```

---

### Task 10: Deterministic stress, allocation and capacity gates

**Files:**
- Modify: `tests/combat/break_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Consumes complete combat/director/session implementation.
- Produces no product API; adds release-grade invariants and count guards.

- [ ] **Step 1: Add stress tests and observe failures before final hardening**

Add:

1. 10,000 director plans across all four ecologies with zero illegal plans, deterministic replay equality, and no capacity overflow.
2. 1,000 complete rooms using real monster AI/player attacks and explicit save confirmation after each transition.
3. Snapshot allocation counter before/after the steady-state portion; assert zero net live allocation growth and zero allocations inside instrumented combat ticks/director generation.
4. Force each pool to capacity and verify one extra creation is rejected, existing slots remain unchanged, and diagnostic counters saturate instead of wrapping.

- [ ] **Step 2: Run Debug and Release stress executables**

Expected before hardening: any hidden capacity, event queue, timeout, or allocation assumptions fail with a named invariant rather than hanging.

- [ ] **Step 3: Make only the minimal bounded fixes exposed by RED**

Allowed fixes are capacity constants already approved, bounded event draining, saturating diagnostics, or deterministic tick limits. Do not weaken assertions or add dynamic containers.

- [ ] **Step 4: Run full build matrix**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
```

Expected: Core-only, Debug, and Release CTest all pass; direct combat/dungeon executables report zero failures; stress logs report 1000 rooms and zero sustained allocations/overflows.

- [ ] **Step 5: Commit**

```powershell
git add tests/combat tests/dungeon src
git commit -m "test: verify stage 4 encounter endurance"
```

---

### Task 11: Final integration review and visual acceptance

**Files:**
- Modify only files required by review findings; every production fix requires a new RED regression.
- Record scratch evidence under ignored `.superpowers/sdd/`; do not commit generated build or acceptance artifacts.

**Interfaces:**
- Consumes all prior tasks.
- Produces reviewed `milestone/m04-monster-director` only; no `main` merge, push, PR, or Stage 5 branch.

- [ ] **Step 1: Run source-boundary scans**

Verify no raylib includes in `src/core`, `src/combat`, `src/dungeon`, `src/persistence`; no new `combat -> dungeon/persistence` or `persistence -> combat/dungeon` reachability; `git diff --check` produces no output.

- [ ] **Step 2: Run direct test executables in Debug and Release**

Run combat, dungeon, persistence, core, and platform executables individually. Every executable must exit 0 and report zero failures.

- [ ] **Step 3: Visual acceptance matrix**

Use isolated save directories and representative deterministic seeds. Confirm:

- all eight monster silhouettes and role labels;
- fire/water/lightning/chaos ecology colors;
- bomber, charge, dash, projectile, support shield, and hazard warnings before damage;
- player HP drops but never below 1 and restores only on committed room entry;
- one-wave and two-wave HUD/door behavior;
- normal hole remains sealed through all waves and ready after final clear;
- abyss remains presentation-only;
- F12 screenshot and window/Esc exit paths leave zero `arpg_game` processes.

- [ ] **Step 4: Independent whole-range review**

Review from `f3f442d` (Stage 4 design commit) to current HEAD against the design and this plan. Classify findings as Critical/Important/Minor. Fix all Critical/Important findings with RED tests, rerun Steps 1-3, and obtain a clean re-review.

- [ ] **Step 5: Final branch state**

```powershell
git status --short
git rev-parse main
git rev-parse milestone/m04-monster-director
git log --oneline --decorate --graph --max-count 40
```

Expected: worktree clean; `main` remains `bb74bd48b42435d798b2b6ce9968d1d2d3f8a9da`; Stage 4 branch contains only approved Stage 4 work. Preserve all M04 task worktrees and wait for the user's integration choice.

---

## Plan Self-Review Checklist

- [ ] All eight approved roles map to one stable catalog ID and one preferred ecology.
- [ ] Director defaults are exactly 8 base budget, +1 per 5 depths, cap 24, matching/off weights 4/1, two waves above 12.
- [ ] Normal/high-budget high-priority limits are 1/2 and active dangerous-area limit is 3.
- [ ] Every wave has a direct target and illegal all-support/all-ranged compositions have a deterministic fallback.
- [ ] Monster, projectile, hazard, wave, spawn, event, and diagnostic paths are bounded.
- [ ] Player HP clamp/reset is explicitly temporary and no death state is introduced.
- [ ] Same stable room data reproduces the same plan without changing ecology/hole/abyss RNG.
- [ ] Doors and holes remain sealed until the last wave clears.
- [ ] Abyss receives no Stage 4 gameplay multiplier or dedicated content.
- [ ] No rewards, XP, levels, equipment, loot, affixes, summoners, snipers, bosses, ECS, or Stage 5 work appears.
- [ ] Final review covers the full Stage 4 range and all Critical/Important findings are re-reviewed.
