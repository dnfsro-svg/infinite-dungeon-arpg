### Task 6: 完整技能帧、玩家抑制与专属图标

**Files:**
- Modify: `src/platform/raylib/active_skill_view.hpp`
- Modify: `src/platform/raylib/active_skill_view.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.hpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `tests/platform/active_skill_view_tests.cpp`
- Modify: `tests/platform/active_skill_asset_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes material-backed frame metadata from Task 5.
- Produces `make_active_skill_effect_plan(const combat::CombatSnapshot&, const combat::CombatEvent*, bool)` once per rendered snapshot; both actor and world passes consume the same `ActiveSkillEffectPlan`.

- [ ] **Step 1: Write RED tests for full frame reachability and positions.**

Define and test:

```cpp
enum class ActiveSkillVisualMode : std::uint8_t {
    none, material, procedural_fallback,
};

[[nodiscard]] std::size_t active_skill_visual_frame_index(
    skills::ActiveSkillId id, std::uint16_t elapsed_ticks) noexcept;
```

For draw slash ticks 0..89, collect returned indices and require exactly 0..35. For storm ticks 0..359, require exactly 0..23 including 16 and 20..23. Assert `player_position == snapshot.player.position`, `effect_center == snapshot.active_skill.locked_center`, and changing only the lock center never moves `player_position`.

- [ ] **Step 2: Write RED tests for healthy/fallback exclusivity and HUD icons.**

Use this plan contract:

```cpp
struct ActiveSkillEffectPlan final {
    ActiveSkillVisualMode mode{ActiveSkillVisualMode::none};
    combat::Vec3 player_position{};
    combat::Vec3 effect_center{};
    MaterialAtlasId atlas{MaterialAtlasId::count};
    std::size_t atlas_frame{};
    bool suppress_base_player{};
    std::size_t procedural_main_visual_count{};
    DrawSlashVisualPlan draw_slash{};
    StormSwordsVisualPlan storm_swords{};
    float screen_flash_alpha{};
};

[[nodiscard]] ActiveSkillEffectPlan make_active_skill_effect_plan(
    const combat::CombatSnapshot& snapshot,
    const combat::CombatEvent* last_event,
    bool material_ready) noexcept;
```

Healthy material requires `mode == material`, `suppress_base_player`, and `procedural_main_visual_count == 0`. Missing material requires `procedural_fallback`, no suppression, and a nonzero fallback main-visual count. Add `skill_icon_draw_slash` and `skill_icon_storm_swords` tests: occupied slots have distinct non-missing icons; empty slots use `missing`.

After constructing the snapshot once, call `make_active_skill_effect_plan` 10,000 times inside the existing allocation probe and require zero heap allocations.

- [ ] **Step 3: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: current draw slash exposes only the hit window and storm omits frames.

- [ ] **Step 4: Implement exact visual frame mapping.**

Use integer math:

```cpp
// draw slash: tick 0..89
return (std::min<std::size_t>)(35U,
    static_cast<std::size_t>(elapsed_ticks) * 36U / 90U);

// storm swords
if (elapsed_ticks < 72U)  return elapsed_ticks * 5U / 72U;
if (elapsed_ticks < 324U) return 5U + (elapsed_ticks - 72U) * 15U / 252U;
if (elapsed_ticks < 342U) return 20U + (elapsed_ticks - 324U) * 2U / 18U;
return 22U + (elapsed_ticks - 342U) * 2U / 18U;
```

Clamp storm to 23. Keep combat timeline `kStormSwordsClip.frame_count` unchanged; visual atlas frames are deliberately decoupled from its logical event frame count.

- [ ] **Step 5: Draw material skills once and suppress duplicate player.**

Build the plan once in `CombatRenderer::draw` after residency synchronization. `material_ready` is true only when the currently active skill ID is not `none` and `material_pack_.available(active_skill_material_atlas(id))`; another equipped skill’s healthy atlas cannot mask this skill’s failure. Pass the plan to `draw_actors` and `ActiveSkillRenderer::draw_world(const ActiveSkillEffectPlan&, const MaterialPack&, float, float)`. The actor pass omits the base player only when `plan.suppress_base_player`; the skill renderer calls:

```cpp
const ScreenProjection player = project_combat_position(
    plan.player_position, width, height);
material_pack.draw_frame(frame.atlas, frame.source, frame.foot_anchor,
    {player.x, player.ground_y}, flip_x, scale, WHITE);
```

Use `effect_center` only for damage-area flashes, pull lines and the 24 sword placements. Material mode does not draw the fan, triangle-sword or finisher ellipse as the main visual; fallback mode retains all three. Resource failure never hides the base player.

- [ ] **Step 6: Add skill-specific material icons.**

Register representative frames as sprites: draw slash frame 18 source `{0,627,209,209}`, storm frame 20 source `{0,1280,256,256}`. Add:

```cpp
[[nodiscard]] MaterialSpriteId active_skill_icon_sprite(
    skills::ActiveSkillId id) noexcept;
```

Add `MaterialSpriteId icon` to `ActiveSkillHudSlot`; draw it inside the slot’s inset bounds through `material_pack.draw_to`, then draw the cooldown mask and text. Retain the generic slot frame, not the generic blue-ball main icon.

Register exactly four new C++ cases from Steps 1–2 and change the platform expected case count from 458 to 462.

- [ ] **Step 7: Run GREEN tests.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: 36/24 index coverage, position separation, healthy/fallback exclusivity, no duplicate player, and distinct HUD icons pass.

- [ ] **Step 8: Commit.**

```powershell
git add src/platform/raylib/active_skill_view.* src/platform/raylib/active_skill_renderer.* src/platform/raylib/combat_renderer.* src/platform/raylib/actor_renderer.cpp src/platform/raylib/material_asset_types.hpp src/platform/raylib/material_manifest.hpp tests/platform/active_skill_view_tests.cpp tests/platform/active_skill_asset_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: render complete active skill material timelines"
```

