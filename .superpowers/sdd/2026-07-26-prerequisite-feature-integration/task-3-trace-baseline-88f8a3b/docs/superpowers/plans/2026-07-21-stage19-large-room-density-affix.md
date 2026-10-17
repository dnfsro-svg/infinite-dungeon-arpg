# Stage 19 Large Room Density Affix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a four-times-area scrolling combat room whose deterministic room-density affix selects 12–30 normal monsters or 18–45 abyss monsters and spawns the whole encounter at once.

**Architecture:** `dungeon` owns a new deterministic room-density roll and passes its exact count into a single-batch encounter director. `combat` continues to own fixed-capacity simulation and shared room bounds. The raylib platform derives a pure camera view from the interpolated player position, projects every world object through that view, and renders density/count text from the dungeon snapshot.

**Tech Stack:** C++17, raylib 6.0, CMake 3.25+, Ninja, MSVC, CTest, fixed-step deterministic simulation.

## Global Constraints

- Room bounds are exactly X `[-24, 24]` and Y `[-11, 11]`.
- A 16:9 viewport shows exactly 24×11 world units; wider viewports expand horizontal coverage without stretching.
- Camera tracking adds no smoothing or chase delay and never exposes space outside the room.
- Every room has exactly one density affix: crowded 50%/12–16, dense 35%/17–22, horde 15%/23–30.
- Abyss count is `ceil(base_count * 1.5)` and is never greater than 45.
- All monsters spawn in one batch; no later wave is generated.
- Density, count, composition, positions, and monster affixes are deterministic for the same room seed.
- Spawn positions avoid radius 4.0 around player spawn, radius 3.0 around each door, and radius 3.0 around an active hole.
- Existing save bytes and checkpoint schema do not change.
- No heap allocation is added to room generation, encounter generation, combat update, or render hot paths.
- Do not modify unrelated user files or clean existing untracked directories.

---

### Task 1: Expand shared room geometry and entry spawns

**Files:**
- Modify: `src/combat/room_bounds.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Modify: `tests/combat/movement_jump_tests.cpp`
- Modify: `tests/combat/monster_special_tests.cpp`
- Modify: `tests/combat/monster_affix_trigger_tests.cpp`
- Modify: `tests/dungeon/room_generation_tests.cpp`
- Modify: `tests/dungeon/dungeon_navigation_tests.cpp`
- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Modify: `tests/dungeon/dungeon_affix_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_reward_tests.cpp`

**Interfaces:**
- Produces: `combat::room_bounds::{min_x,max_x,min_y,max_y,width,depth}` equal to `-24,24,-11,11,48,22`.
- Produces: entry spawns at left/right X `-22.5/22.5` and top/bottom Y `-10.25/10.25`.
- Consumes: existing `make_combat_lab_config(EntrySide, rules_version)` interface unchanged.

- [ ] **Step 1: Write failing geometry and navigation assertions**

```cpp
static_assert(arpg::combat::room_bounds::min_x == -24.0F);
static_assert(arpg::combat::room_bounds::max_x == 24.0F);
static_assert(arpg::combat::room_bounds::min_y == -11.0F);
static_assert(arpg::combat::room_bounds::max_y == 11.0F);
static_assert(arpg::combat::room_bounds::width == 48.0F);
static_assert(arpg::combat::room_bounds::depth == 22.0F);

ARPG_REQUIRE(requested_exit(Vec3{-24.0F, 0.90F, 0.0F}, {-1, 0})
    == ExitDirection::left);
ARPG_REQUIRE(requested_exit(Vec3{24.0F, -0.90F, 0.0F}, {1, 0})
    == ExitDirection::right);
ARPG_REQUIRE(requested_exit(Vec3{1.50F, -11.0F, 0.0F}, {0, -1})
    == ExitDirection::up);
ARPG_REQUIRE(requested_exit(Vec3{-1.50F, 11.0F, 0.0F}, {0, 1})
    == ExitDirection::down);
```

- [ ] **Step 2: Build and run focused tests to verify RED**

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_combat_tests arpg_dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure
```

Expected: FAIL on old `±12/±5.5` bounds or old entry spawn values.

- [ ] **Step 3: Change shared bounds and entry locations**

```cpp
namespace arpg::combat::room_bounds {
inline constexpr float min_x = -24.0F;
inline constexpr float max_x = 24.0F;
inline constexpr float min_y = -11.0F;
inline constexpr float max_y = 11.0F;
inline constexpr float width = max_x - min_x;
inline constexpr float depth = max_y - min_y;
}
```

Update `make_combat_lab_config` entry values to `±22.5F` and `±10.25F`; keep the initial spawn at `{0,0,0}` and preserve facing behavior.

- [ ] **Step 4: Replace test-only old boundary literals with shared constants where the literal is not the subject of the test**

Use `combat::room_bounds::min_x/max_x/min_y/max_y` in stress, abyss-reward, and validation fixtures. Keep exact numeric assertions only in the new geometry test.

- [ ] **Step 5: Run focused tests to verify GREEN**

Run the Step 2 commands. Expected: both suites PASS.

- [ ] **Step 6: Commit**

```powershell
git add src/combat/room_bounds.hpp src/dungeon/room_combat_template.cpp tests/combat tests/dungeon
git commit -m "feat: expand combat room to four times area"
```

---

### Task 2: Add deterministic room-density affixes

**Files:**
- Create: `src/dungeon/room_affix.hpp`
- Create: `src/dungeon/room_affix.cpp`
- Create: `tests/dungeon/room_affix_tests.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Produces: `enum class RoomDensityAffix : std::uint8_t { crowded, dense, horde, count };`
- Produces: `struct RoomDensityRoll { RoomDensityAffix affix; std::uint8_t base_count; std::uint8_t monster_count; };`
- Produces: `RoomDensityRoll roll_room_density(std::uint64_t room_seed, bool is_abyss) noexcept;`
- Produces: `const RoomDensityDefinition* room_density_definition(RoomDensityAffix) noexcept;`

- [ ] **Step 1: Register the new test file and write RED tests**

```cpp
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::crowded)->weight == 50U);
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::crowded)->minimum == 12U);
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::crowded)->maximum == 16U);
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::dense)->weight == 35U);
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::horde)->weight == 15U);

for (std::uint64_t seed = 0; seed < 10000U; ++seed) {
    const RoomDensityRoll normal = roll_room_density(seed, false);
    const RoomDensityRoll abyss = roll_room_density(seed, true);
    ARPG_REQUIRE(abyss.affix == normal.affix);
    ARPG_REQUIRE(abyss.base_count == normal.base_count);
    ARPG_REQUIRE(abyss.monster_count
        == static_cast<std::uint8_t>((normal.base_count * 3U + 1U) / 2U));
    ARPG_REQUIRE(abyss.monster_count <= 45U);
}
```

Also count all three affixes over 100,000 stable seeds and require absolute deviation below 1 percentage point; record whether every inclusive interval endpoint appears.

- [ ] **Step 2: Run the dungeon target to verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure
```

Expected: compile FAIL because `room_affix.hpp` and its APIs do not exist.

- [ ] **Step 3: Implement the fixed catalog and domain-separated rolls**

```cpp
struct RoomDensityDefinition final {
    RoomDensityAffix id{RoomDensityAffix::crowded};
    std::uint8_t weight{};
    std::uint8_t minimum{};
    std::uint8_t maximum{};
};

constexpr std::array<RoomDensityDefinition, 3> kDefinitions{{
    {RoomDensityAffix::crowded, 50U, 12U, 16U},
    {RoomDensityAffix::dense, 35U, 17U, 22U},
    {RoomDensityAffix::horde, 15U, 23U, 30U},
}};
```

Use separate fixed 64-bit domains for affix selection and count selection. Select with `next_bounded(100U)`, select the inclusive count with `next_bounded(maximum-minimum+1)`, and compute abyss count as `(base_count * 3U + 1U) / 2U`. Do not accept configuration or depth as input.

- [ ] **Step 4: Prove deterministic output and no allocation**

Repeat the same seeds twice and compare `RoomDensityRoll` fields. Wrap 100,000 rolls with the existing allocation probe and require zero allocations.

- [ ] **Step 5: Run the dungeon suite to verify GREEN**

Run Step 2. Expected: PASS with the dungeon suite count updated in `dungeon_test_main.cpp`.

- [ ] **Step 6: Commit**

```powershell
git add src/dungeon/room_affix.* src/dungeon/CMakeLists.txt tests/dungeon/room_affix_tests.cpp tests/dungeon/CMakeLists.txt tests/dungeon/dungeon_test_main.cpp
git commit -m "feat: add deterministic room density affixes"
```

---

### Task 3: Generate an exact single-batch encounter

**Files:**
- Modify: `src/dungeon/encounter_director.hpp`
- Modify: `src/dungeon/encounter_director.cpp`
- Modify: `src/dungeon/dungeon_rules.hpp`
- Modify: `src/dungeon/dungeon_rules.cpp`
- Modify: `src/dungeon/encounter_budget.cpp`
- Modify: `tests/dungeon/encounter_director_tests.cpp`
- Modify: `tests/dungeon/dungeon_rules_tests.cpp`
- Modify: `tests/dungeon/stage9_validation_fixture.cpp`
- Modify: `tests/dungeon/stage10_formal_game_validation.cpp`
- Modify: `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`

**Interfaces:**
- Produces:

```cpp
struct EncounterBuildRequest final {
    std::uint64_t room_seed{};
    std::uint64_t depth{1U};
    checkpoint::DungeonElement ecology{checkpoint::DungeonElement::fire};
    checkpoint::EntrySide entry{checkpoint::EntrySide::initial};
    bool has_hole{};
    std::uint8_t target_monster_count{};
};

EncounterPlanResult build_encounter_plan(
    const EncounterBuildRequest&, const EncounterDirectorConfig&) noexcept;
EncounterPlanResult build_abyss_encounter_plan(
    const EncounterBuildRequest&, const EncounterDirectorConfig&) noexcept;
```

- Produces: `RoomEncounterPlan::initial_monster_count` and exactly one populated wave.
- Consumes: `make_combat_lab_config(request.entry, 1U)` for the player safe-zone center.

- [ ] **Step 1: Replace budget-oriented tests with exact-count RED tests**

```cpp
EncounterBuildRequest request{0xA11CEULL, 40U,
    DungeonElement::lightning, EntrySide::left, true, 30U};
const EncounterPlanResult result = build_encounter_plan(request, {});
ARPG_REQUIRE(result.fault == DungeonFault::none);
ARPG_REQUIRE(result.plan.wave_count == 1U);
ARPG_REQUIRE(result.plan.initial_monster_count == 30U);
ARPG_REQUIRE(result.plan.waves[0].spawn_count == 30U);
ARPG_REQUIRE(result.plan.waves[1].spawn_count == 0U);
```

For each spawn, require room containment and squared distance outside the player, four door, and active-hole exclusion circles. Across 4096 seeds require at least one valid position in each newly exposed band: X below -12, X above 12, Y below -5.5, Y above 5.5.

- [ ] **Step 2: Run the encounter suite to verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure
```

Expected: compile FAIL on the missing request type/new signature, then encounter assertions FAIL until one batch and safe positions are implemented.

- [ ] **Step 3: Simplify director configuration to count-era safety fields**

```cpp
struct EncounterDirectorConfig final {
    std::uint8_t matching_ecology_weight{4};
    std::uint8_t off_ecology_weight{1};
    std::uint8_t high_priority_limit{2};
    std::uint8_t ranged_limit{4};
    std::uint8_t support_limit{2};
    std::uint8_t ground_hazard_limit{3};
};
```

Remove the obsolete depth budget fields, `encounter_budget`, and `abyss_encounter_legality_config`. Repurpose the private helper in `encounter_budget.cpp` as checked threat-cost accumulation used by the director. Keep `RoomEncounterPlan::total_budget` and `EncounterWave::spent_budget` as the summed threat-cost diagnostic; maximum is 180, which fits `std::uint8_t`.

- [ ] **Step 4: Implement count-first selection**

Set `wave_count=1` and `initial_monster_count=request.target_monster_count`. Append one direct-target fallback first, then repeatedly use the existing ecology-weighted candidate selection while enforcing safety tag limits until exact count is reached. The unrestricted `chaos_chaser` guarantees completion. Apply existing monster affixes after composition; abyss generation then supplements existing affixes exactly as before.

- [ ] **Step 5: Implement bounded safe-position sampling**

Draw X uniformly from 48,001 millisteps and Y from 22,001 millisteps. Reject excluded points for at most 32 attempts per spawn. If all attempts fail, scan a deterministic 1-unit grid from a seed-derived offset and take the first legal point. Return `invalid_rules` if no legal point exists; never truncate or reroll the room.

- [ ] **Step 6: Update legality checks**

Require `wave_count==1`, `initial_monster_count==waves[0].spawn_count`, count in `[12,45]`, exact threat sums, sequential ordinals, valid affixes, valid safe positions, at least one direct target, and all safety tag limits. Remove all budget-split and two-wave legality branches.

- [ ] **Step 7: Run focused and full dungeon suites**

Run Step 2, then:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure
```

Expected: PASS; the same input is byte-equivalent and 12, 16, 17, 22, 23, 30, and 45 counts are legal.

- [ ] **Step 8: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: build exact single-batch encounters"
```

---

### Task 4: Integrate density with session, abyss, snapshot, and reload

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `tests/dungeon/dungeon_wave_tests.cpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_death_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_transaction_tests.cpp`

**Interfaces:**
- Produces in `DungeonSnapshot`: `RoomDensityAffix density_affix`, `std::uint8_t base_monster_count`, `std::uint8_t initial_monster_count`.
- Produces in `DungeonEncounterDiagnostics`: exact initial count plus summed threat budget.
- Consumes: `roll_room_density(current_room.seed, current_room.is_abyss)`.

- [ ] **Step 1: Write RED lifecycle and reload tests**

```cpp
const DungeonSnapshot entered = session.snapshot();
ARPG_REQUIRE(entered.wave_count == 1U);
ARPG_REQUIRE(entered.initial_monster_count >= 12U);
ARPG_REQUIRE(entered.initial_monster_count <= 30U);
ARPG_REQUIRE(entered.combat->monster_count == entered.initial_monster_count);

DungeonSession reloaded{rules, stable_state};
const DungeonSnapshot replay = reloaded.snapshot();
ARPG_REQUIRE(replay.density_affix == entered.density_affix);
ARPG_REQUIRE(replay.base_monster_count == entered.base_monster_count);
ARPG_REQUIRE(replay.initial_monster_count == entered.initial_monster_count);
```

For an abyss seed, require `initial == (base*3+1)/2`, `initial<=45`, and all monsters present before the first combat tick. After defeating the last target, require direct transition toward room clear and no `wave_delay` phase.

- [ ] **Step 2: Run dungeon tests to verify RED**

Run `ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure`. Expected: compile FAIL on snapshot fields/new director calls.

- [ ] **Step 3: Store only transient derived density state**

Add `RoomDensityRoll room_density_{};` to `DungeonSession`. Recompute it in normal construction, abyss construction, death retreat, and reload from stable room seed/is-abyss. Do not add it to `checkpoint::RoomDescriptor`, `DungeonRunState`, or persistence serialization.

- [ ] **Step 4: Build normal and abyss encounter requests**

```cpp
const EncounterBuildRequest request{
    room.seed, room.depth, room.ecology, room.entry,
    room.has_hole, room_density_.monster_count};
const EncounterPlanResult plan = room.is_abyss
    ? build_abyss_encounter_plan(request, rules_.encounter)
    : build_encounter_plan(request, rules_.encounter);
```

Load `plan.waves[0]` once. Keep legacy phase enum values for source compatibility, but ensure production plans never enter `wave_delay` and never call `start_next_wave`.

- [ ] **Step 5: Publish immutable snapshot values**

Populate `density_affix`, `base_monster_count`, and `initial_monster_count` from `room_density_`/plan. Set `wave_index=0`, `wave_count=1`, and `wave_delay_ticks=0` for active Stage 19 rooms. Preserve `remaining_targets()` as the allocation-free live monster count.

- [ ] **Step 6: Verify stable save bytes**

Run existing persistence golden/round-trip tests before and after constructing density-enabled rooms. Expected: serialized checkpoint version and byte layout remain unchanged; reloaded derived density equals the original.

- [ ] **Step 7: Run dungeon and persistence suites**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests
ctest --test-dir out/build/windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

Expected: PASS.

- [ ] **Step 8: Commit**

```powershell
git add src/dungeon tests/dungeon tests/persistence
git commit -m "feat: integrate density encounters with dungeon lifecycle"
```

---

### Task 5: Add a deterministic, zero-lag scrolling camera

**Files:**
- Modify: `src/platform/raylib/combat_view_math.hpp`
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Modify: `src/platform/raylib/render_layout.hpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.hpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/debug_renderer.cpp`
- Modify: `src/platform/raylib/ground_loot_view.cpp`
- Modify: `src/platform/raylib/material_loot_view.cpp`
- Modify: `tests/platform/combat_view_math_tests.cpp`

**Interfaces:**
- Produces:

```cpp
struct CombatCameraView final {
    combat::Vec3 center{};
    float visible_width{24.0F};
    float visible_depth{11.0F};
};

CombatCameraView make_combat_camera_view(
    combat::Vec3 interpolated_player, float width, float height) noexcept;
ScreenProjection project_combat_position(
    combat::Vec3 position, CombatCameraView view,
    float width, float height) noexcept;
```

- Consumes: the same interpolated player position already used for actor rendering.

- [ ] **Step 1: Write RED camera math tests**

```cpp
const CombatCameraView center = make_combat_camera_view({0,0,0}, 1280, 720);
ARPG_REQUIRE(near(center.visible_width, 24.0F));
ARPG_REQUIRE(near(center.visible_depth, 11.0F));
ARPG_REQUIRE(near(center.center.x, 0.0F));

const CombatCameraView right = make_combat_camera_view({24,0,0}, 1280, 720);
ARPG_REQUIRE(near(right.center.x, 12.0F));
const CombatCameraView front = make_combat_camera_view({0,11,0}, 1280, 720);
ARPG_REQUIRE(near(front.center.y, 5.5F));
```

At 2560×1080 require `visible_depth==11`, `visible_width==24*(64/27)/(16/9)==32`, and unchanged actor pixel scale. Require the player projection to react to the current interpolated position in the same render call; there is no retained camera interpolation state.

- [ ] **Step 2: Run platform tests to verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure
```

Expected: compile FAIL on missing camera APIs.

- [ ] **Step 3: Implement pure camera derivation**

Use 11 world units of visible depth. Compute visible width as `24 * max(1, aspect/(16/9))`, capped at room width 48. Clamp the center so the visible rectangle remains inside room bounds; when a dimension shows the whole room, force that center coordinate to zero. Do not store velocity, smoothing factor, or previous camera center.

- [ ] **Step 4: Make projection camera-relative**

Compute depth from `position.y - (view.center.y - view.visible_depth/2)`, clamp only the visual scale input to `[0,1]`, and preserve the existing `0.70..1.00` perspective scale and `0.38..0.88` ground band. Compute horizontal pixels from `(position.x-view.center.x)/view.visible_width`. Update projectile, hazard, render-layout, active-skill, loot, debug, and actor calls to accept the same view.

- [ ] **Step 5: Derive one camera per frame and share it across every world pass**

In `CombatRenderer::draw`, interpolate the player from `previous.combat->player.position` to `current.combat->player.position` using the existing render alpha, build one `CombatCameraView`, and pass it to room, actors, skills, loot-world anchors, and debug rendering. Camera shake remains a final screen-space offset and must not alter the logical camera center.

- [ ] **Step 6: Run platform tests to verify GREEN**

Run Step 2. Expected: PASS at 16:9, ultrawide, room center, and all four clamped edges.

- [ ] **Step 7: Commit**

```powershell
git add src/platform/raylib tests/platform/combat_view_math_tests.cpp
git commit -m "feat: add zero-lag scrolling combat camera"
```

---

### Task 6: Render the expanded room and density HUD

**Files:**
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/hud_view_model.hpp`
- Modify: `src/platform/raylib/hud_view_model.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `tests/platform/hud_view_model_tests.cpp`
- Modify: `tests/platform/hud_render_plan_tests.cpp`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`

**Interfaces:**
- Produces: `const char* room_density_label(RoomDensityAffix) noexcept` returning UTF-8 `拥挤/密集/兽潮`.
- Produces in `RoomHudModel`: affix, initial count, remaining count, and preformatted secondary line.
- Consumes: Task 5 `CombatCameraView` for all room geometry.

- [ ] **Step 1: Write RED HUD tests**

```cpp
snapshot.density_affix = dungeon::RoomDensityAffix::horde;
snapshot.base_monster_count = 27U;
snapshot.initial_monster_count = 27U;
snapshot.remaining_targets = 19U;
build_hud_view_model(output, snapshot, {}, default_hints());
ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"兽潮") != nullptr);
ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"19/27") != nullptr);
```

For abyss, set base 27/final 41 and require the density line shows the actual `19/41` while the existing abyss rule remains visible. Assert no truncation at the minimum supported 1024×704 layout.

- [ ] **Step 2: Run platform tests to verify RED**

Run the Task 5 Step 2 commands. Expected: compile or text assertion FAIL.

- [ ] **Step 3: Replace wave wording with density/count wording**

Format combat objective as `"%s · 怪物 %u/%u"`, using remaining then initial. Put abyss rule/effect in its existing section and density/count in the room objective; do not reintroduce wave text. Add affix and initial count to `ObjectiveKey` so cached text refreshes when a room changes.

- [ ] **Step 4: Make room geometry visibly scroll**

Project the four world corners and all door/hole positions through the shared camera. Draw floor grid lines at fixed 2-world-unit intervals across X `[-24,24]` and Y `[-11,11]`; cull lines whose projected bounding box is outside the viewport. Keep the material floor as the base layer, with the world-space grid and boundary treatment above it so movement is visible without stretching the material asset.

- [ ] **Step 5: Verify door, hole, hazard, and loot alignment**

Add pure projection assertions that each object shares the same projected coordinate when routed through room rendering and actor/loot helpers. At each camera edge, the corresponding door lies inside the viewport and the opposite door lies outside it.

- [ ] **Step 6: Run platform tests to verify GREEN**

Run Step 2. Expected: PASS, no HUD truncation, no stale cached density text.

- [ ] **Step 7: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render large rooms and density encounter HUD"
```

---

### Task 7: Capacity, determinism, full-build, and fullscreen validation

**Files:**
- Create: `docs/validation/stage19-large-room-density-affix.md`
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/platform/hud_stress_tests.cpp`

**Interfaces:**
- Consumes: all prior Stage 19 interfaces.
- Produces: reproducible validation record with commands, suite counts, representative seeds, and visual findings.

- [ ] **Step 1: Add maximum-density stress coverage**

Construct a deterministic abyss horde whose base count is 30 and final count is 45. Require `CombatSnapshot::monster_count==45`, `remaining_targets==45`, no pool saturation diagnostics, and no allocation during 10,000 fixed ticks. Repeat after reload and compare density, spawn IDs, ordinals, positions, and affixes.

- [ ] **Step 2: Add long-run room coverage**

Generate at least 10,000 normal/abyss rooms. Require every normal count in 12–30 according to its affix, every abyss count in 18–45 with the exact ceil formula, one wave only, legal safe positions, and no deterministic-stream drift between two runs.

- [ ] **Step 3: Run full Debug verification**

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Expected: all configured Debug tests PASS.

- [ ] **Step 4: Run full Release verification**

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

Expected: all configured Release tests PASS and `out/build/windows-msvc-release/bin/arpg_game.exe` exists.

- [ ] **Step 5: Perform fullscreen visual validation**

Launch the Release executable. Verify at 16:9 fullscreen: center movement, all four camera clamps, character size unchanged, floor motion visible, doors/hole aligned, normal 12+ encounter, horde 23+ encounter, and an abyss encounter up to 45. Confirm all enemies exist immediately on entry and no second wave appears after clearing any subset.

- [ ] **Step 6: Record exact evidence**

Write the commit hash, Debug/Release test totals, selected room seeds, density/count pairs, maximum live monster count, allocation/saturation result, executable path, resolution, and visual pass/fail observations to `docs/validation/stage19-large-room-density-affix.md`. Do not claim any check that was not actually run.

- [ ] **Step 7: Commit validation and final fixes**

```powershell
git add tests/dungeon/dungeon_abyss_stress_tests.cpp tests/dungeon/dungeon_stress_tests.cpp tests/platform/hud_stress_tests.cpp docs/validation/stage19-large-room-density-affix.md
git commit -m "test: verify large density rooms end to end"
```

---

## Completion Gate

- Every task commit exists and contains only its scoped files.
- Normal rooms always produce 12–30 monsters from the displayed affix.
- Abyss rooms use the displayed base affix and exact 1.5× ceil count, capped at 45.
- One batch is loaded before combat and no production path enters wave delay.
- Same stable room data reproduces affix, count, composition, positions, and monster affixes without save-format changes.
- A 16:9 fullscreen camera shows 24×11 world units, tracks without added delay, and clamps at all four edges.
- Debug and Release CTest are fully green.
- Fullscreen raylib visual validation is recorded with truthful evidence.
