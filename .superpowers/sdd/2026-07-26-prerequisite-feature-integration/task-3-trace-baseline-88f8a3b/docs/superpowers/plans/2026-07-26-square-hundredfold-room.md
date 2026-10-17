# Square Hundredfold Room Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current 24×11 arena with a deterministic square room of 26,400 world-area units, preserve the approved monster density through room affixes, unlock all exits after 25% permanent kills while combat continues, and render the room through a zero-delay player-follow camera.

**Architecture:** A room-owned immutable population/environment blueprint is generated once from the room seed. `RoomMonsterField` owns up to 1,152 persistent monster states, while the existing slot-based `MonsterPool` is expanded only to 128 nearby residents and is repacked in stable ordinal order. `dungeon` owns permanent kill/exit/drop progression and V9 persistence; raylib consumes camera-filtered snapshots and shared material atlases. Exit unlock is orthogonal to `RoomPhase`, so 25% progression never impersonates a full clear.

**Tech Stack:** C++17, raylib 6.0, OpenGL 3.3, CMake 3.25+, Ninja, MSVC, fixed-step 60 Hz authority, CTest.

## Global Constraints

- The baseline room is the current `24×11` room with area `264`; new area is exactly `26,400` within `0.01` float tolerance.
- New bounds use `half_extent=81.24038696F`, producing a square side of approximately `162.48077393`.
- A 16:9 viewport shows 24×11 world units; wider viewports expand horizontal coverage without stretching actors, capped at 32×11 world units.
- The previous approved Stage 19 density is the density reference: its 48×22 room has area 1,056, so the new room uses scale factor 25.
- Density affixes remain 50% crowded, 35% dense, 15% horde. Scaled normal counts are 300–400, 425–550, and 575–750; abyss counts are `ceil(normal*1.5)`, producing 450–1125.
- `kRoomMonsterCapacity=1152`; generation must fault rather than truncate when a requested count exceeds capacity.
- All monster blueprints, room props, obstacles, doors, and the optional hole are determined on room entry from independent seed domains. No route-dependent RNG is allowed.
- At most 128 monsters are simulated at once; off-region monsters remain in the room field and preserve state but do not execute AI. The immutable home-cell layout plus one-cell roaming leash proves a maximum resident demand of `7*5*3=105`, leaving 23 safety slots.
- The full `RoomMonsterField` storage is allocated once behind an owning pointer during room construction; it is never placed in per-frame stack values or copied into snapshots. Normal fixed-tick/render paths allocate nothing.
- Exit threshold is `required_kills=(generated_count+3)/4`; summoned/non-blueprint entities do not enter the numerator or denominator.
- At 25%, all four doors and an existing hole become usable, but combat, skills, AI, drops, death handling, and the remaining-monster counter continue.
- Only 100% permanent kills produce `room_cleared`, clear XP, material vacuum, abyss-clear rewards, and the existing full-clear transaction.
- Leaving an uncleared abyss after 25% records failed/abandoned abyss resolution and grants no abyss-clear reward.
- Existing texture atlases are shared. No room-sized RenderTexture, per-monster texture, or per-chunk texture copy is permitted; material residency remains at or below 256 MiB.
- Authority, RNG, checkpoint capture/result application, raylib/OpenGL calls, audio, and final draw submission remain on the main thread. One persistent sleeping save worker may encode, write, read back, decode, and verify only an immutable captured checkpoint; it never reads or mutates live authority.
- Do not begin this plan until `2026-07-26-prerequisite-feature-integration.md` is fully complete and the integration branch is clean.
- Every created `.cpp`, test `.cpp`, and CMake guard is added to its owning `src/*/CMakeLists.txt` or `tests/*/CMakeLists.txt`; every new custom test body is also declared and invoked by the owning `*_test_main.cpp` in the same task.

---

### Task 1: Freeze geometry, spatial limits, and scaled density

**Files:**
- Create: `src/core/gameplay_limits.hpp`
- Modify: `src/combat/room_bounds.hpp`
- Create: `src/combat/room_spatial_grid.hpp`
- Create: `src/combat/room_spatial_grid.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `src/dungeon/room_affix.hpp`
- Create: `src/dungeon/room_affix.cpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Modify: `src/dungeon/room_navigation.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/room_affix_tests.cpp`
- Modify: `tests/dungeon/dungeon_navigation_tests.cpp`
- Modify: `tests/dungeon/room_generation_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/combat/movement_jump_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Produces `arpg::limits::{kRoomMonsterCapacity=1152,kActiveMonsterCapacity=128,kProjectileCapacity=512,kHazardCapacity=128,kCombatEventCapacity=512,kDefeatLedgerCapacity=128,kRoomEquipmentClaimWords=18,kRoomSecondaryClaimWords=37}`.
- Produces `combat::room_bounds::{min_x,max_x,min_y,max_y,width,depth}` with square bounds.
- Produces `RoomDensityRoll roll_room_density(std::uint64_t room_seed, bool abyss) noexcept` with `std::uint16_t` counts.
- Produces a square 20×20 spatial grid with `side_exact/20` cells, at most three generated monsters per cell, and `RoomStreamingRegion make_room_streaming_region(Vec3 player_position) noexcept` covering the maximum 32×11 view plus one home-cell halo.

- [ ] **Step 1: Write exact geometry and capacity RED tests**

```cpp
static_assert(arpg::limits::kRoomMonsterCapacity == 1152U);
static_assert(arpg::limits::kActiveMonsterCapacity == 128U);
static_assert(arpg::limits::kProjectileCapacity == 512U);
static_assert(arpg::limits::kHazardCapacity == 128U);
static_assert(arpg::limits::kCombatEventCapacity == 512U);
static_assert(arpg::limits::kDefeatLedgerCapacity == 128U);
ARPG_REQUIRE_NEAR(combat::room_bounds::width, 162.48077393F, 0.0001F);
ARPG_REQUIRE_NEAR(combat::room_bounds::depth, 162.48077393F, 0.0001F);
ARPG_REQUIRE_NEAR(combat::room_bounds::width * combat::room_bounds::depth,
    26400.0F, 0.01F);
ARPG_REQUIRE(room_spatial::columns == 20U);
ARPG_REQUIRE(room_spatial::rows == 20U);
ARPG_REQUIRE(room_spatial::maximum_monsters_per_cell == 3U);
```

- [ ] **Step 2: Write density RED tests**

```cpp
const RoomDensityDefinition crowded{
    RoomDensityAffix::crowded, 50U, 300U, 400U};
const RoomDensityDefinition dense{
    RoomDensityAffix::dense, 35U, 425U, 550U};
const RoomDensityDefinition horde{
    RoomDensityAffix::horde, 15U, 575U, 750U};
```

Across 100,000 seeds, require all affixes and inclusive endpoints, absolute weight deviation below one percentage point, equal affix/base count between normal and abyss, and `abyss_count=(base*3+1)/2` with maximum 1125.

- [ ] **Step 3: Run RED**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests arpg_dungeon_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^(combat|dungeon)\.units$' --output-on-failure
```

Expected: compile failures for the missing limits/density APIs and old room geometry.

- [ ] **Step 4: Implement shared limits and independent density RNG domains**

```cpp
namespace arpg::limits {
inline constexpr std::size_t kRoomMonsterCapacity = 1152U;
inline constexpr std::size_t kActiveMonsterCapacity = 128U;
inline constexpr std::size_t kProjectileCapacity = 512U;
inline constexpr std::size_t kHazardCapacity = 128U;
inline constexpr std::size_t kCombatEventCapacity = 512U;
inline constexpr std::size_t kDefeatLedgerCapacity = 128U;
inline constexpr std::size_t kRoomEquipmentClaimWords = 18U;
inline constexpr std::size_t kRoomSecondaryClaimWords = 37U;
}
```

Use separate 64-bit domains for affix selection and reference-count selection. Multiply the selected Stage 19 reference count by exactly 25 before applying the abyss multiplier. Add static assertions that 1125 fits the room capacity, `20*20*3 >= 1125`, `7*5*3 < 128`, and the defeat ledger holds every active resident. Keep existing combat capacity names as compatibility aliases to the shared limits while migrating call sites.

- [ ] **Step 5: Replace old boundary literals only where they are fixtures**

Use shared bounds in movement, projectile, hazard, navigation, stress, and formal fixture setup. Preserve exact old-room literals only in migration tests proving that the new baseline intentionally changed.

- [ ] **Step 6: Run GREEN and commit**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests arpg_dungeon_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^(combat|dungeon)\.units$' --output-on-failure
git add src/core/gameplay_limits.hpp src/combat/room_bounds.hpp src/combat/room_spatial_grid.* src/combat/CMakeLists.txt src/dungeon/room_affix.* src/dungeon/room_combat_template.cpp src/dungeon/room_navigation.cpp src/dungeon/CMakeLists.txt tests/combat tests/dungeon
git commit -m "feat: define hundredfold square room limits"
```

---

### Task 2: Generate immutable monster and environment blueprints once

**Files:**
- Create: `src/combat/room_monster_plan.hpp`
- Create: `src/combat/room_environment_plan.hpp`
- Create: `src/dungeon/room_monster_plan_builder.hpp`
- Create: `src/dungeon/room_monster_plan_builder.cpp`
- Create: `src/dungeon/room_environment.hpp`
- Create: `src/dungeon/room_environment.cpp`
- Modify: `src/dungeon/encounter_director.hpp`
- Modify: `src/dungeon/encounter_director.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/room_population_tests.cpp`
- Create: `tests/dungeon/room_environment_tests.cpp`
- Modify: `tests/dungeon/encounter_director_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Produces `combat::RoomMonsterPlan` with fixed `std::array<RoomMonsterBlueprint,1152>`, `cell_offsets[401]`, `cell_counts[400]`, monster count, threat total, generator version, and field-wise FNV-1a blueprint hash. Each monster stores its immutable home-cell index and one-cell roaming leash.
- Produces small `RoomMonsterPlanBuildResult { DungeonFault fault; RoomDensityRoll density; }` and `build_room_monster_plan(const checkpoint::RoomDescriptor&, const DungeonRules&, std::uint32_t generator_version, combat::RoomMonsterPlan& out_plan) noexcept`. The 1,152-entry plan is caller-owned heap-backed room storage; it is never returned or accepted by value.
- Produces platform-neutral `combat::RoomObstacleSpec`/`RoomObstaclePlanView` in `room_environment_plan.hpp`. Non-copyable `RoomEnvironmentBlueprint` owns those specs plus decorative records, its own nonzero generator version, canonical field-wise hash, fixed `kRoomEnvironmentRecordCapacity=1200` storage, explicit count, stable ordinals, and 20×20 cell offsets/counts capped at three combined records per cell (at most one colliding obstacle). After the monster plan is sealed, allocation-free out-parameter `build_room_environment(..., const combat::RoomMonsterPlan&, RoomEnvironmentBlueprint&)` uses independent environment RNG domains and rejects colliding obstacle candidates against every initial monster bound; it is never returned by value or copied into a snapshot.
- Produces stable global `spawn_ordinal` equal to the blueprint array index.
- Consumes room seed, depth, ecology, entry side, hole flag, abyss flag, and Task 1 density roll.

- [ ] **Step 1: Write RED tests for exact one-time population generation**

```cpp
checkpoint::RoomDescriptor room{};
room.seed = 0xA11CEULL;
room.depth = 40U;
room.ecology = DungeonElement::lightning;
room.entry = EntrySide::left;
room.has_hole = true;
auto a = std::make_unique<combat::RoomMonsterPlan>();
auto b = std::make_unique<combat::RoomMonsterPlan>();
const auto a_result = build_room_monster_plan(room, DungeonRules{}, 1U, *a);
const auto b_result = build_room_monster_plan(room, DungeonRules{}, 1U, *b);
ARPG_REQUIRE(a_result.fault == DungeonFault::none);
ARPG_REQUIRE(a->monster_count == a_result.density.total_count);
ARPG_REQUIRE(room_monster_plan_equal_fields(*a, *b));
ARPG_REQUIRE(a->blueprint_hash == b->blueprint_hash);
ARPG_REQUIRE(a->monster_count <= limits::kRoomMonsterCapacity);
```

Require ordinals `0..count-1`, every spawn in bounds, no cell above three monsters, at least one direct target in every nonempty local group, and avoidance radii 4.0 around entry spawn plus 3.0 around doors/hole.

Build the environment after each sealed population and require no colliding obstacle overlap with entry/doors/hole or any initial monster bounds, a traversable connection from entry to all four doors, at most three combined prop/obstacle records per home cell, and equal canonical environment hashes for equal inputs. Changing only environment RNG domains must not change monster bytes/hash; changing only monster identity/position must deterministically re-run environment rejection without consuming monster RNG.

- [ ] **Step 2: Verify RED**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^dungeon\.units$' --output-on-failure
```

- [ ] **Step 3: Implement deterministic slot selection**

Deterministically Fisher-Yates shuffle the 400 cell indices. Give every cell `count/400` monsters and the first `count%400` shuffled cells one extra; this proves a maximum of three per cell at 1125. Write blueprints in `(cell_index,local_index)` order and assign contiguous stable ordinals. Within a cell, try at most 32 independent jitter positions, then scan a fixed 17×17 subgrid.

- [ ] **Step 4: Implement count-first monster composition**

Choose monster IDs with the existing ecology weights, but enforce per-cell limits of at most one support, ranged, high-priority, or ground-hazard monster. If any nonempty cell lacks a direct target, replace its last stable slot with the ecology-compatible direct-target fallback, including one- and two-monster cells. Apply normal/abyss monster affixes after identity selection without consuming position/density streams.

- [ ] **Step 5: Prove capacity, distribution, and no allocation**

Test all three normal maximums and abyss maximum 1125, all four entry sides, hole/no-hole, every ecology, and 4096 seeds. Both builders must allocate zero heap memory. The monster builder returns `population_capacity` rather than a partial blueprint at count 1153; the environment builder returns a placement/capacity fault rather than overlapping, truncating, or changing the sealed population.

- [ ] **Step 6: Run GREEN and commit**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^dungeon\.units$' --output-on-failure
git add src/combat/room_monster_plan.hpp src/combat/room_environment_plan.hpp src/dungeon/room_monster_plan_builder.* src/dungeon/room_environment.* src/dungeon/encounter_director.* src/dungeon/CMakeLists.txt tests/dungeon
git commit -m "feat: generate deterministic room blueprints"
```

---

### Task 3: Add a persistent room field above the bounded active pool

**Files:**
- Create: `src/combat/monster_persistent_state.hpp`
- Create: `src/combat/monster_persistent_state.cpp`
- Create: `src/combat/combat_defeat_ledger.hpp`
- Create: `src/combat/combat_defeat_ledger.cpp`
- Create: `src/combat/room_monster_field.hpp`
- Create: `src/combat/room_monster_field.cpp`
- Create: `src/combat/room_obstacle_runtime.hpp`
- Create: `src/combat/room_obstacle_runtime.cpp`
- Modify: `src/combat/fire_room_obstacle.hpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/monster_pool.hpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/combat_effects.cpp`
- Modify: `src/combat/combat_snapshot.cpp`
- Modify: `src/combat/abyss_environment.cpp`
- Modify: `src/combat/hit_resolution.cpp`
- Modify: `src/combat/active_skill_runtime.hpp`
- Modify: `src/combat/active_skill_runtime.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/monster_ai_ranged.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Modify: `src/combat/target_simulation.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: `tests/combat/monster_pool_tests.cpp`
- Create: `tests/combat/monster_residency_tests.cpp`
- Create: `tests/combat/room_obstacle_runtime_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Produces `using MonsterOrdinal=std::uint16_t` and `kInvalidMonsterOrdinal=0xFFFFU`; `MonsterHandle` remains a local active-slot index plus generation and is never persisted.
- Produces field-wise `MonsterPersistentState` containing position, velocity, facing, reaction, armor, AI phase/timers, HP/break/shield, affix timers/effects, and a `touched` flag; it contains no active-slot handle.
- Produces `RoomMonsterField::plan_storage_for_construction()`, `seal_plan(RoomMonsterPlanBuildResult)`, `synchronize_active_region(RoomStreamingRegion)`, `mark_defeated(ordinal)`, `total_count()`, `defeated_count()`, and allocation-free persistent-state accessors used later by the checkpoint encoder. Copy construction/assignment for both `RoomMonsterPlan` and `RoomMonsterField` is deleted.
- Keeps `MonsterPool` fixed at 128 active slots and adds an ordinal-to-handle map owned by `RoomMonsterField`; `CombatWorld` holds the field through one owning pointer so `DungeonSession`/test stack frames remain bounded. Resident slots are rebuilt in increasing ordinal order after dirty runtimes are written back.
- Changes projectile/hazard owner identity and authoritative combat-event target identity from reusable active slots to `MonsterOrdinal`; renderer-only slot indices never cross the `CombatWorld` boundary.
- Produces a non-lossy 128-record `CombatDefeatLedger` drained once per authority tick. Reward/progress defeat records never depend on the bounded visual `CombatEvent` queue; a ledger overflow is a hard combat fault.
- Produces neutral `combat::RoomObstaclePlanView` over Task 2 stable obstacle specs and fixed `RoomObstacleRuntime` keyed by environment ordinal. `CombatWorld` consumes the view without depending on dungeon types, owns mutable `intact/hp/broken_tick/effects` records, exposes O(1) const state lookup for visible rendering/checkpoint capture, and replaces the two hard-coded fire-crate slots with their blueprint ordinals before V9 exists.

- [ ] **Step 1: Write persistence/residency RED tests**

Initialize a 1125-monster field and require 1125 living states but at most 105 active pool residents for any camera cell. Move, damage, break, shield, affix-tick, and retarget selected ordinals, cross a cell boundary, return, and require every field to round-trip exactly. Give 105 simultaneous residents distinct nonempty `EffectSet`s, churn every active slot, then require each effect/timer to remain attached to its persistent ordinal; remove the old eight-entry slot/generation `effect_owners_` authority. Build all Task 2 obstacle specs, break the two crate ordinals plus boundary ordinals, churn monster residency, and require obstacle state to remain stable and independent from local monster slots. A recycled active slot must never change a projectile/hazard owner or a defeat event from its original global ordinal. Defeat all 105 residents in one tick and require 105 unique ledger records even when visual hit/feedback events approach their independent capacity.

- [ ] **Step 2: Write active-capacity and deterministic-order RED tests**

```cpp
const auto residents = field.required_residents(streaming_region_for_cell(10U, 10U));
ARPG_REQUIRE(residents.count <= 105U);
ARPG_REQUIRE(std::is_sorted(residents.ordinals.begin(),
    residents.ordinals.begin() + residents.count));
ARPG_REQUIRE(field.synchronize_active_region(
    streaming_region_for_cell(10U, 10U)));
ARPG_REQUIRE(world.active_monster_count() == residents.count);
```

Require byte-identical resident ordinals independent of camera traversal history. Inject malformed plans that demand 129 residents and require a `monster_residency_capacity` fault rather than truncation.

- [ ] **Step 3: Verify RED, then refactor `MonsterPool` storage**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

Keep only 128 `MonsterRuntime` slots in `MonsterPool`. Allocate one `RoomMonsterField` behind `std::unique_ptr` during room construction, pass its internal construction-only plan storage to the builder, and seal it without a large return, move, or copy. It stores 1,152 persistent records and 1,152 ordinal-to-local-handle entries, writes changed residents plus their `EffectSet`s back before eviction, and installs required residents in increasing ordinal order. Initialize `RoomObstacleRuntime` once from the sealed neutral obstacle view; collision/break logic addresses stable environment ordinals and queries blueprint cell spans rather than iterating every obstacle each tick. Normal ticks iterate only active monster slots and local obstacle candidates; room construction and explicit checkpoint capture are the only full-field traversals. No snapshot or automatic local variable may copy the plan, field, or obstacle state.

- [ ] **Step 4: Convert all authority references to global ordinals**

Replace authoritative `CombatEvent::target_index` with `target_ordinal`; convert hit-index scratch arrays to local handles only inside one tick, and convert attack/active-skill hit latches to fixed 1,152-bit ordinal bitsets. Store `owner_ordinal` in projectiles/hazards so they remain valid while their monster is dormant. Support AI searches only active pool slots, but all emitted events expose stable ordinals. Resize the presentation event queue to 512 and keep its saturation diagnostic, while every defeat is also appended exactly once to the independent 128-record ledger before any presentation push.

- [ ] **Step 5: Build camera-neighborhood residency**

Derive a maximum 32×11 home-cell span from the authoritative player position (`<=5×3` cells after room-edge clamping), expand by one home-cell halo (`<=7×5`), and gather through `RoomMonsterPlan::cell_offsets` without scanning the full field. Monsters may move only inside their immutable home cell expanded by one cell; therefore a monster outside the halo cannot enter any supported viewport and resident demand is at most 105. Refresh only when the streaming span changes, without allocations. A traversal stress test crosses all 400 cells and proves no overflow, duplicate activation, or history-dependent state hash.

- [ ] **Step 6: Keep snapshots bounded**

`CombatSnapshot` packs only active residents into a 128-entry array with an explicit `std::uint16_t monster_count`; total living/defeated counts are separate 32-bit fields. Projectile and hazard snapshots use the 512/128 capacities and preserve owner global ordinals.

- [ ] **Step 7: Run GREEN and commit**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
git add src/combat tests/combat
git commit -m "refactor: stream a bounded monster residency set"
```

---

### Task 4: Replace waves with room population lifecycle

**Files:**
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/room_environment.hpp`
- Modify: `src/dungeon/encounter_wave_builder.cpp`
- Modify: `tests/dungeon/dungeon_wave_tests.cpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Adds session-owned `RoomProgressState room_progress_` and heap-owned `std::unique_ptr<RoomEnvironmentBlueprint> room_environment_`; `CombatWorld::RoomMonsterField` is the sole owner of the in-place-built `combat::RoomMonsterPlan`. Both immutable blueprints expose const version/hash views to `DungeonSession`; neither large blueprint is an automatic/member value of a stack-created session fixture.
- Replaces wave counts with `std::uint32_t initial_monster_count`, `defeated_monster_count`, and `remaining_targets` in `DungeonSnapshot`.
- Produces exactly one `population_generated`/`combat_started` sequence and no `wave_delay` transition.

- [ ] **Step 1: Write RED lifecycle tests**

Require exactly one monster build followed by one environment build on room construction, field-wise-equal rebuilds and equal hashes from the same state, all blueprint monsters present in `RoomMonsterField` immediately, no later wave/environment generation after local residency is cleared, and `remaining=initial-defeated` even when most monsters are dormant.

- [ ] **Step 2: Remove wave authority from the session path**

`construct_current_room()` allocates the owning `RoomMonsterField` and environment blueprint, builds directly into their construction-only heap storage, seals the monster plan first and the environment against that const plan second, then constructs `CombatWorld` with the environment's neutral obstacle view, initializes progress, and activates the entry neighborhood. Partial allocation/build failure cleans both owners and faults before the first combat tick. Retain old wave fields only in V8 migration/test compatibility until Task 7; production state must not enter `wave_delay`.

- [ ] **Step 3: Relay permanent defeat exactly once**

Drain `CombatDefeatLedger` in ordinal emission order and use each global ordinal to set a defeat bit. A duplicate ledger record or ledger overflow is a fault. Increment `defeated_monster_count` once and derive remaining count from the immutable total; never derive progression from the visual event queue or active slot count.

- [ ] **Step 4: Run and commit**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^dungeon\.units$' --output-on-failure
git add src/dungeon tests/dungeon
git commit -m "refactor: drive rooms from immutable populations"
```

---

### Task 5: Add V9 room-progress persistence before new transactions

**Files:**
- Modify: `src/core/deterministic_rng.hpp`
- Modify: `src/core/deterministic_rng.cpp`
- Create: `src/combat/room_combat_checkpoint.hpp`
- Create: `src/combat/room_combat_checkpoint.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/player_damage_history.hpp`
- Modify: `src/combat/player_damage_history.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `src/dungeon/room_progress_checkpoint.hpp`
- Create: `src/dungeon/room_progress_checkpoint.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Create: `src/persistence/room_progress_codec.hpp`
- Create: `src/persistence/room_progress_codec.cpp`
- Create: `src/persistence/save_commit_worker.hpp`
- Create: `src/persistence/save_commit_worker.cpp`
- Create: `src/persistence/save_commit_storage.hpp`
- Create: `src/persistence/save_commit_storage.cpp`
- Modify: `src/persistence/CMakeLists.txt`
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/save_slot.cpp`
- Modify: `src/dungeon/dungeon_progression.cpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/core/deterministic_rng_tests.cpp`
- Modify: `tests/core/test_main.cpp`
- Create: `tests/combat/room_combat_checkpoint_tests.cpp`
- Modify: `tests/combat/player_damage_history_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`
- Create: `tests/persistence/checkpoint_v9_tests.cpp`
- Create: `tests/persistence/save_commit_worker_tests.cpp`
- Create: `tests/persistence/save_commit_storage_tests.cpp`
- Create: `tests/persistence/save_commit_stack_guard_test.cmake`
- Modify: `tests/persistence/CMakeLists.txt`
- Modify: `tests/persistence/persistence_test_main.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/stage17_skill_stones_game_validation.cpp`
- Modify: `tests/platform/stage17_skill_stones_validator.ps1`
- Modify: `tests/platform/stage17_skill_stones_validator_self_test.ps1`

**Interfaces:**
- Produces checkpoint format 9 with magic `ARPGSV9\0`, explicit little-endian fields, CRC, and an 8 MiB decode ceiling while keeping decoders for V1-V8.
- Produces `RoomProgressCheckpoint` containing lifecycle, room identity/seed, separate nonzero monster/environment generator versions and canonical hashes, generated/defeated/required counts, exits/full-clear/reward flags, 18-word defeat and equipment-claim bitsets, a 37-word secondary-claim bitset, `RoomCombatCheckpoint`, and ordered unclaimed equipment/material/potion records. Its storage is fixed: at most 1,152 touched monsters, 1,152 equipment ground records, and 2,320 tagged secondary ground records (`2*1152+16`), each with an explicit count; no checkpoint-owned `std::vector` or other growth allocation is allowed.
- Produces field-wise `combat::PlayerCombatCheckpoint`, ordered `combat::MonsterCombatCheckpoint` records for every touched living ordinal, and `combat::RoomCombatCheckpoint` containing `CombatWorld::tick_`, exported 4×`uint64_t` evasion RNG state, `AbyssEnvironmentRuntime`, ordinary `AttackRuntime` including the 1,152-ordinal hit latch, all stateful/destructible environment records including fire-crate `intact/broken_tick`, `PlayerDamageHistory`'s 300 buckets/active tick/initialized flag, and the authoritative death snapshot when present. No raw struct bytes, padding, pointers, active-slot handles, renderer state, or presentation diagnostics enter the file.
- Produces checked `DeterministicRng::export_state()`/`import_state()` and equivalent field-wise `PlayerDamageHistory` checkpoint APIs. Import rejects the forbidden all-zero RNG state, invalid active/history ticks, overflowed damage totals, invalid attack phase/latch bits, and malformed obstacle/death state.
- Produces allocation-free `CombatWorld::capture_room_checkpoint(RoomCombatCheckpoint& out) const noexcept` and `restore_room_checkpoint(const RoomCombatCheckpoint&)`; `DungeonSession` writes directly into the caller-selected heap job slot. Checkpoint/job copy and move operations are deleted, and no API returns a checkpoint by value. Restore rebuilds and validates both immutable blueprint versions/hashes before applying any player, monster, or obstacle position/state.
- Produces heap-owned `SaveCommitStorage` containing two preallocated checkpoint/job slots and two preallocated 8 MiB codec buffers. `DungeonRuntime` allocates it once through `std::unique_ptr` during initialization; no host/runtime/worker automatic object embeds those arrays. `SaveCommitWorker` contains only stable pointers and small synchronization/state members, with `sizeof(SaveCommitWorker)<=1024` enforced.
- Produces a persistent `SaveCommitWorker` with one in-flight/pending queue entry, condition-variable sleep, revision-tagged completion, and `stop_and_join()` before storage teardown. The exact computed `SaveCommitStorage` resident bytes are recorded and hard-capped at 32 MiB outside the texture budget.
- V8 migration preserves durable inventory/ownership, sets `state.room_progress={}`, marks the state migrated, and starts the current room fresh rather than inventing partial large-room progress.
- Until Task 7 removes legacy room-drop claims from production, a checked adapter mirrors only the existing low 3 equipment words and low 7 secondary words into/out of V9; V9 is already the canonical save representation and the remaining high words stay zero.

- [ ] **Step 1: Write V9 round-trip and corruption RED tests**

```cpp
ARPG_REQUIRE(decoded.state.room_progress.generated_monsters == 1125U);
ARPG_REQUIRE(decoded.state.room_progress.defeated_monsters == 282U);
ARPG_REQUIRE(decoded.state.room_progress.required_kills == 282U);
ARPG_REQUIRE(decoded.state.room_progress.exits_unlocked);
ARPG_REQUIRE(decoded.state.room_progress.monster_blueprint_hash != 0U);
ARPG_REQUIRE(decoded.state.room_progress.environment_blueprint_hash != 0U);
```

Reject seed/room mismatch, either blueprint hash mismatch, either generator version zero/unsupported, CRC/length overflow, popcount/count mismatch, required-count mismatch, out-of-bounds or colliding player/monster position against intact environment obstacles, set bits beyond generated count, unlocked state below threshold, duplicate/out-of-order monster or ground records, defeated ordinals with live records, invalid RNG/attack/history/abyss/obstacle/death state, invalid enums/effects, and any capacity excess.

- [ ] **Step 2: Define exact reload semantics**

On V9 reload, rebuild the monster blueprint, then the environment blueprint against it, and require both recorded versions/hashes before mutating the live session. Restore defeat/claim bits, all ordered ground records, combat tick/RNG/abyss/ordinary-attack/obstacle/damage-history/death state, player position/velocity/facing/HP/barrier/status/cooldowns, and every touched monster's position/velocity/facing/reaction/armor/AI/timers/HP/break/shield/affix effects. Clear only explicitly transient buffered/input edges, currently executing active-skill instance, projectiles, hazards, hit-stop presentation, visual event queue, and audio/renderer queues; buffered gameplay input is discarded and must be physically released before rearming. Those clear rules are versioned and tested, not accidental omissions.

- [ ] **Step 3: Implement fixed binary layout and migration**

Encode records in strictly increasing ordinal order with explicit little-endian fields and CRC, never raw struct writes. After finite/range validation, preserve every authoritative `float` losslessly by copying its IEEE-754 binary32 bit pattern into `std::uint32_t` with `std::memcpy` and writing that integer explicitly little-endian; decode reverses the operation and rejects non-finite or out-of-contract values. Encode integer timers/resources with checked widths. Preserve V8 magic/decoder and mark migrated states for rewrite on the next normal save. Change only the Stage 17 save-version assertions/validator fixture from 8 to 9; its skill timing, frame, hit, and material expectations stay unchanged.

- [ ] **Step 4: Add bounded save triggers before exposing exit/drop features**

Capture immutable checkpoint revision `N` on the main thread between fixed ticks; capture is allocation-free and targets P99 at or below 0.50 ms. The save worker alone performs codec work, temporary-file write/flush/replace, readback decode, and exact verification. It may touch only the submitted slot and returns `{revision,exact_result}`; worker completion time is measured separately and cannot choose RNG, advance a tick, or emit gameplay events.

Mark progress dirty on authoritative combat/drop changes and coalesce background saves to once per 300 fixed ticks (5 seconds). A background save does not pause authority: commit of revision `N` marks only `<=N` durable, and a newer authority revision remains dirty. If the one-entry worker queue is busy, background requests coalesce without allocating or replacing a transactional request.

Full clear, death, existing pickup/heal transactions, room transition, and clean host shutdown are exact transactional requests; Tasks 6-7 add threshold and new pickup/heal hooks to the same API. Ordinary monster death and ground-drop creation only bump the revision/dirty flag for the five-second background cadence; they never pause authority for an exact transaction. At an exact boundary, authority enters a deterministic `committing` state and stops before publishing the mutation, while presentation continues from the last complete immutable frame. Gameplay input is gated during this state: pending edges are cleared, new gameplay edges are discarded, and held controls must be physically released before rearming, so variable I/O duration cannot queue future authority actions. If a background job is active, the transaction waits in the sole pending slot and supersedes only redundant background work. Apply the exact result between ticks, then resume authority so wall-clock completion order cannot alter tick/event/RNG ordering. Closing remains inside the host loop until the transaction completes and never synchronously encodes/writes on the render thread. `not_committed` cancels the requested mutation (and shutdown closing) while keeping the game running; `indeterminate` enters the existing fault path without pretending the state was saved. Do not write every tick or every ordinary kill.

- [ ] **Step 5: Run persistence, reload, death, and deterministic tests**

```powershell
. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --target arpg_core_tests arpg_combat_tests arpg_persistence_tests arpg_dungeon_tests arpg_platform_tests arpg_stage17_skill_stones_game_validation -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(core|combat|persistence|dungeon|platform)\.units$|^persistence\.save_commit_stack_guard$|^stage17\.skill_stones\.' --output-on-failure
```

Inject worker completions before capture, after capture, after 1/7/31 presentation frames, and behind an active background job. Require identical next authority tick, event order, RNG count, transaction result, and V9 bytes for every schedule. At capture boundaries where the explicitly cleared transient sets are empty, compare uninterrupted versus save/reload for at least 10,000 following authority ticks and require identical per-tick authority hashes, exported RNG state, damage-history totals, obstacle state, and event sequence; separately seed each declared transient and prove only that named field clears. Require idle worker CPU sleep, bounded queue/slot reuse, exact shutdown join, `sizeof`/resident-budget gates, partial-allocation and worker-start failure cleanup, no large checkpoint/job automatic variables via a source guard, no post-initialization allocation, and no raylib/live-authority calls from the worker translation unit. Storage allocation failure makes runtime initialization fail explicitly as persistence unavailable; it never falls back to an unsafe stack buffer.

- [ ] **Step 6: Commit**

```powershell
git add src/core/deterministic_rng.* src/combat src/dungeon src/persistence src/items src/platform/raylib/dungeon_runtime.* src/platform/raylib/raylib_host.cpp tests/core/deterministic_rng_tests.cpp tests/core/test_main.cpp tests/combat tests/persistence tests/dungeon tests/platform/dungeon_runtime_tests.cpp tests/platform/platform_test_main.cpp tests/platform/stage17_skill_stones_game_validation.cpp tests/platform/stage17_skill_stones_validator*.ps1
git commit -m "feat: persist large room progress in checkpoint v9"
```

---

### Task 6: Unlock exits at 25% without clearing combat

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/room_progress_checkpoint.cpp`
- Modify: `src/platform/raylib/dungeon_view_math.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/hud_notice_state.cpp`
- Create: `tests/dungeon/dungeon_exit_unlock_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/persistence/checkpoint_v9_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`

**Interfaces:**
- Produces `RoomProgressState { uint32_t initial, defeated, required; bool exits_unlocked; bool full_clear; }`.
- Produces `DungeonSnapshot::exits_unlocked` independent from `RoomPhase`.
- Changes `attempt_exit` and `request_descent` to accept `combat && exits_unlocked` or existing full-clear/awaiting states.
- Produces one exactly committed `DungeonEventKind::exits_opened` at threshold and one later `room_cleared` at 100%.

- [ ] **Step 1: Write threshold and save RED tests**

```cpp
ARPG_REQUIRE(required_kills(300U) == 75U);
ARPG_REQUIRE(required_kills(301U) == 76U);
ARPG_REQUIRE(required_kills(1125U) == 282U);
```

At required-minus-one, doors/hole reject transition. Crossing required creates one V9 `room_unlock` transaction; until its exact commit, doors/hole remain closed. After commit, all exits open, phase remains combat, attacks/skills still succeed, AI ticks, and remaining monsters can die. At total kills, the V9 full-clear transaction and rewards occur once.

- [ ] **Step 2: Split exit and clear state**

Replace phase-derived `exits_open` with the committed V9 flag. Do not set `RoomPhase::awaiting_exit` at 25%. Keep `prepare_room_clear()` exclusively behind `defeated==initial`; a failed threshold save retries the same pending state without duplicate events.

- [ ] **Step 3: Define early-exit reward semantics**

Normal early exit atomically commits earned monster XP and claimed loot, clears old `room_progress`, discards living monsters/unpicked ground objects, and grants no clear bonus/vacuum. Abyss early exit atomically commits `AbyssLifecycle::failed`, a failed/abandoned `LastAbyssResolution`, zero clear rewards, the selected next room, and cleared old room progress. `not_committed` leaves the player in the original room; `indeterminate` faults.

- [ ] **Step 4: Update host prompts and hole range**

All four door visuals and an existing hole's E prompt consume `exits_unlocked`, not `awaiting_exit`. Preserve the current widened hole interaction radius and do not change input bindings. Combat/HUD counters continue updating after unlock.

- [ ] **Step 5: Run transaction/death/reload tests and commit**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|persistence|platform)\.units$' --output-on-failure
git add src/dungeon src/platform/raylib tests/dungeon tests/persistence
git commit -m "feat: unlock room exits after one quarter kills"
```

---

### Task 7: Decouple authoritative drops from visible presentation pools

Tasks 7 and 8 are one indivisible implementation slice. Task 7 may reach an internal GREEN state, but do not commit, hand off, deploy, or claim the bounded presentation path is complete until Task 8 connects the camera query and all production render consumers in the same commit.

**Files:**
- Create: `src/dungeon/dungeon_render_snapshot.hpp`
- Create: `src/dungeon/dungeon_render_snapshot.cpp`
- Modify: `src/dungeon/material_loot.hpp`
- Modify: `src/dungeon/health_potion_loot.hpp`
- Create: `src/dungeon/room_drop_state.hpp`
- Create: `src/dungeon/room_drop_state.cpp`
- Create: `src/dungeon/room_drop_spatial_index.hpp`
- Create: `src/dungeon/room_drop_spatial_index.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `src/dungeon/room_progress_checkpoint.hpp`
- Modify: `src/dungeon/room_progress_checkpoint.cpp`
- Modify: `src/dungeon/room_environment.hpp`
- Modify: `src/dungeon/room_environment.cpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/persistence/room_progress_codec.cpp`
- Modify: `src/platform/raylib/ground_loot_view.cpp`
- Modify: `src/platform/raylib/material_loot_view.cpp`
- Create: `tests/dungeon/room_drop_state_tests.cpp`
- Create: `tests/dungeon/dungeon_render_snapshot_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/persistence/checkpoint_v9_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`

**Interfaces:**
- Produces platform-neutral `WorldViewQuery { combat::Aabb world_bounds; int screen_width; int screen_height; uint64_t camera_version; }` and fixed `DungeonRenderSnapshot` storage for current player/skill, at most 128 residents, 512 projectiles, 128 hazards, 128 environment records, bounded visible drops, four doors, and the optional hole.
- Produces allocation-free `DungeonSession::write_render_snapshot(const WorldViewQuery&, DungeonRenderSnapshot&) const noexcept`. It queries the resident set, `RoomDropSpatialIndex`, and Task 2 environment cell spans; it never scans/copies 1,152 monsters, every ground record, or the full environment blueprint.
- Uses the V9 room-progress 18-word equipment-claim and 37-word shared material/coupon/potion secondary-claim bitsets; removes the temporary low-word adapter from Task 5. The legacy V8 `ItemOwnershipState` claim arrays remain decoder compatibility fields and are not enlarged in place.
- Defines common material ordinal `2*spawn`, secondary ordinal `2*spawn+1`, and abyss-reserve ordinals `2304..2319`.
- Stores authoritative per-room equipment/potion/material drops by stable global ordinal and writes them through the V9 room-progress transaction.
- Produces `RoomDropSpatialIndex`: 400 stable home-cell buckets with capacity nine records each (three blueprint monsters times one equipment plus two secondary ordinals), plus a 16-record abyss-reserve bucket. A visible query visits at most the 7×5 camera-span-plus-roaming-halo buckets and the reserve bucket: `35*9+16=331` candidates, independent of total room drops.
- Keeps visible snapshots bounded at 192 equipment labels, 400 material labels, and 192 potion labels.

- [ ] **Step 1: Write RED tests above the old ordinal boundary**

Force deterministic drops at monster ordinals 191, 192, 511, and 1124. Require all to materialize, save, reload, and claim exactly once. Verify material ordinals 2248/2249 for spawn 1124 and no collision with abyss reserve 2304. V8 migration begins with a fresh room drop state and preserves durable inventory.

- [ ] **Step 2: Implement fixed authoritative arrays and V9 bitsets**

Remove all `spawn_ordinal >= 192` early returns. Stop using the legacy V8-sized ownership claim arrays in new-room production code. Authority arrays are never copied into every frame. Insert each monster-derived record into its immutable source monster's home-cell bucket and abyss-reserve records into the separate reserve bucket; update only claim/presence bits, because ground positions do not migrate between buckets. `write_render_snapshot` queries only cells intersecting `WorldViewQuery` plus one roaming halo, then filters exact world rectangles and publishes the Task 2 environment records through the same bounded value. When a visible label pool is full, choose approved rarity priority, then nearest distance, then stable ordinal without deleting authority. Any bucket/environment capacity proof violation is a hard dungeon fault, never truncation or a fallback full-array scan.

- [ ] **Step 3: Preserve transaction ordering**

Equipment, material, coupon, and potion claims update inventory plus the V9 room-progress claims/ground records in the same exact save. Full clear may vacuum according to current rules; 25% early exit discards unpicked authority records only inside the successful transition commit. `not_committed` and `indeterminate` preserve the existing no-duplication guarantees.

- [ ] **Step 4: Prove maximum V9 payload and visible-pool independence**

Round-trip maximum ordered equipment/material/potion records below the 8 MiB file ceiling. Drive synthetic `WorldViewQuery` rectangles across all cells with saturated off-screen drops/environment and require no drop query to inspect more than 331 authority candidates, no environment query more than 105, exact stable ordering/reappearance, no edge-clamped off-screen labels, zero authority loss, and no fallback full-room scan. Fill output with sentinels before each call and prove the API overwrites counts/valid ranges without allocation or stale records.

- [ ] **Step 5: Reach internal GREEN and keep the slice uncommitted for Task 8**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|persistence|platform)\.units$' --output-on-failure
```

---

### Task 8: Add a zero-delay camera and one projection contract

**Files:**
- Modify: `src/platform/raylib/combat_view_math.hpp`
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.hpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/debug_renderer.cpp`
- Modify: `src/platform/raylib/ground_loot_view.hpp`
- Modify: `src/platform/raylib/ground_loot_view.cpp`
- Modify: `src/platform/raylib/material_loot_view.hpp`
- Modify: `src/platform/raylib/material_loot_view.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/combat_view_math_tests.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces `CombatCameraView { Vec3 center; float visible_width; float visible_depth; }`.
- Produces `make_combat_camera_view(interpolated_player,width,height)` and camera-aware `project_combat_position`.
- Produces `make_world_view_query(CombatCameraView,width,height,camera_version)`; `DungeonRuntime` allocates one `DungeonRenderSnapshot` through `std::unique_ptr` during initialization, calls Task 7 `write_render_snapshot` once per presentation frame into that heap value, and passes the same bounded value to every world renderer. No production host/runtime automatic object embeds the snapshot arrays; allocation failure aborts runtime initialization explicitly.
- Every world renderer consumes the same immutable camera value for the frame.

- [ ] **Step 1: Port only the pure camera-math tests from Stage 19 commits `eae3782` and `6b26089`**

Require 24×11 at 16:9, expanded horizontal view capped at 32×11 for wider aspects, edge clamping, centered player away from edges, identical actor pixel height across room size/resolution, and exact projection of the same world point through all wrappers.

- [ ] **Step 2: Verify RED against full-room normalization**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
```

- [ ] **Step 3: Implement the pure camera value**

Use interpolated player position directly with no chase smoothing. Clamp camera center to room bounds minus half visible extents. `Camera2D` remains only the screen-space shake transform; do not double-apply world translation.

- [ ] **Step 4: Thread the camera through every world draw path**

Update actors, projectiles, hazards, skills, debug collision shapes, equipment, materials, potions, doors, hole, obstacles, and props. Build one `WorldViewQuery` from the frame camera and obtain one Task 7 `DungeonRenderSnapshot` in the preallocated heap slot; platform code may not declare a large local snapshot or rebuild/drop-scan/environment-scan authority. Screen-space HUD and label-clamping stages remain outside world shake but use camera-projected anchor points.

- [ ] **Step 5: Add topology discontinuity flags**

Room transition, death/continue, teleport, and reload snap camera/interpolation to current state. Normal movement interpolates previous/current player positions; authority collision and saved position always use the current fixed-tick state.

- [ ] **Step 6: Run the complete Task 7-8 slice and commit it atomically**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|persistence|platform)\.units$' --output-on-failure
git add src/dungeon src/persistence/room_progress_codec.cpp src/platform/raylib tests/dungeon tests/persistence tests/platform
git commit -m "feat: publish camera-bounded large room views"
```

---

### Task 9: Render immutable backgrounds, doors, props, and obstacles as world chunks

**Files:**
- Modify: `src/dungeon/room_environment.hpp`
- Modify: `src/dungeon/room_environment.cpp`
- Modify: `src/platform/raylib/environment_prop_layout.hpp`
- Modify: `src/platform/raylib/environment_prop_layout.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `tests/dungeon/room_environment_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes the Task 2 deterministic `RoomEnvironmentBlueprint`; this task may not regenerate it, consume RNG, change either blueprint hash, or move colliding obstacles.
- Produces `VisibleEnvironmentSet` bounded to 128 props/obstacles. The maximum 7×5 camera span plus halo visits at most `35*3=105` records before exact rectangle filtering, with a compile-time proof `105<=128`.
- Produces camera-aware world positions for four doors and an optional hole.
- Reuses current environment/material atlases; no new texture residency class.

- [ ] **Step 1: Write RED visible-set and render-integration tests**

Reuse the Task 2 cross-ecology/4096-seed field/hash/collision/path tests as regression gates. Add queries for every camera cell: each visits at most 105 records, returns exact-rectangle matches in stable `(cell,kind,ordinal)` order, and treats any capacity-proof violation as a hard dungeon fault rather than truncating or scanning the whole blueprint. Rendering the same immutable blueprint through camera moves must not change its versions, hashes, record bytes, or RNG counters.

- [ ] **Step 2: Replace screen-normalized placements**

Replace screen-normalized runtime placements with the already-sealed Task 2 world positions and matching world AABBs. Decorative props remain non-colliding. Render the Task 3 stable obstacle ordinals and mutable intact/broken state without changing combat authority; no render or camera path may rebuild placement or migrate obstacle identity.

- [ ] **Step 3: Tile the existing background without large render targets**

Draw only atlas-backed ground/wall tiles intersecting the camera plus one tile margin. Use a maximum 5×5 tile draw plan and the existing material shader. Do not allocate or cache 100 room textures; no additional full-resolution framebuffer is added.

- [ ] **Step 4: Render current doors/hole through camera geometry**

Preserve the improved non-red element-door material and current hole interaction indicator. Door open/closed visuals consume `exits_unlocked`; full-clear-only decoration consumes `full_clear`.

- [ ] **Step 5: Run and commit**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|platform)\.units$' --output-on-failure
git add src/dungeon/room_environment.* src/platform/raylib tests/dungeon/room_environment_tests.cpp tests/dungeon/dungeon_test_main.cpp tests/platform
git commit -m "feat: render deterministic world-space room chunks"
```

---

### Task 10: Expose density, kill threshold, and visible-set diagnostics in HUD

**Files:**
- Modify: `src/platform/raylib/hud_view_model.hpp`
- Modify: `src/platform/raylib/hud_view_model.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/hud_layout.cpp`
- Modify: `src/platform/raylib/hud_font.cpp`
- Modify: `tests/platform/hud_view_model_tests.cpp`
- Modify: `tests/platform/hud_render_plan_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces Chinese HUD text for density affix, defeated/required/total, remaining, exits-open state, and abyss abandonment warning.
- Preserves current font size/readability, movement/controls text, active-skill UI, and material text-audit path.

- [ ] **Step 1: Write layout/content RED tests at three resolutions**

```cpp
ARPG_REQUIRE(view.density_text == "怪群规模：兽潮");
ARPG_REQUIRE(view.progress_text == "消灭 281/282（总计 1125）");
ARPG_REQUIRE(view.exit_text == "出口尚未开放");
```

At threshold, require `出口已开放，战斗仍可继续`; at full clear require the existing clear copy. Assert opaque/pure colors and non-overlap at 1024×576, 1280×720, and 1920×1080.

- [ ] **Step 2: Implement without importing the old Stage 19 HUD layout**

Only port density/count fields and cache-key inputs from commits `b386182`/`56a497d`. Keep current main font sizes, Chinese glyph pipeline, movement/control rows, abyss text, and native material HUD.

- [ ] **Step 3: Run and commit**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
git add src/platform/raylib/hud_* tests/platform/hud_* tests/platform/platform_test_main.cpp
git commit -m "feat: show large room progression in the hud"
```

---

### Task 11: Validate functionality, determinism, visuals, and bounded cost

**Files:**
- Create: `tests/dungeon/large_room_end_to_end_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Create: `tests/platform/large_room_render_plan_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`
- Create: `docs/validation/square-hundredfold-room.md`
- Create: `docs/validation/evidence/square-hundredfold-room/`

**Interfaces:**
- Consumes Tasks 1-10.
- Produces the clean functional baseline required by the separate 240 FPS/8-core plan.

- [ ] **Step 1: Run deterministic headless traces**

Run normal min/max and abyss max rooms with 0, 1, 7, and 31 scheduled reloads. Repeat each schedule twice and require identical blueprint hashes, defeat bits, exit-open tick, drops/claims, event ordering, transition target, and V9 bytes within that schedule. Across different reload schedules, require equal durable progression/rewards/final target while allowing only the explicitly versioned transient clear behavior from Task 5. After warm-up, require zero allocation in fixed-tick, residency, snapshot, render-preparation, and main-thread checkpoint-capture paths; measure worker encode/write/readback separately rather than hiding them in that gate.

- [ ] **Step 2: Run focused Debug suites serially**

```powershell
. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(core|combat|dungeon|persistence|platform)\.units$|^persistence\.save_commit_stack_guard$' --output-on-failure
```

- [ ] **Step 3: Run formal compatibility gates**

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(stage10\.|stage11c\.|stage11d\.|stage12\.material_|stage16\.|stage17\.)' --output-on-failure
```

Regenerate only owned evidence. Stage 11c must explicitly retain opaque pure-color Chinese HUD text with no overlap at all three Task 10 resolutions. Do not weaken old screenshots/validators because the camera changed; update fixtures to navigate the production room.

- [ ] **Step 4: Perform real fullscreen acceptance**

At 1920×1080, verify all four edges, camera clamping, unchanged actor size, no camera delay, door/hole ranges, readable pure-color HUD, normal 300-monster and abyss 1125-blueprint rooms, 25% early exit, continued combat, full clear, death/continue, save/reload, and no stationary screen-space props.

- [ ] **Step 5: Record bounded-cost evidence**

At equal visible density, compare old-room and new-room CPU active time over at least 10,000 Release frames. Total room population must not make per-frame monster/prop/drop preparation scale with 1,125; visible-set preparation P99 may regress by no more than 10% before the dedicated performance plan.

- [ ] **Step 6: Write validation record and commit**

```powershell
git add tests/dungeon/large_room_end_to_end_tests.cpp tests/dungeon/CMakeLists.txt tests/dungeon/dungeon_test_main.cpp tests/platform/large_room_render_plan_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp docs/validation/square-hundredfold-room.md docs/validation/evidence/square-hundredfold-room
git commit -m "test: validate hundredfold square rooms"
```

- [ ] **Step 7: Independent review and push**

Require one specification review and one code-quality review. Then run:

```powershell
git diff --check
git status --short --branch --untracked-files=all
```

Require the status output to contain only the branch header before pushing `codex/integration-large-square-rooms`. Do not merge to main until both reviews are clean.
