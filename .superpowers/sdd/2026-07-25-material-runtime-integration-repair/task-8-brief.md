### Task 8: 死亡材质面板与高清战斗文字

**Files:**
- Modify: `src/platform/raylib/death_overlay_view.hpp`
- Modify: `src/platform/raylib/death_overlay_view.cpp`
- Modify: `src/platform/raylib/death_overlay_renderer.hpp`
- Modify: `src/platform/raylib/death_overlay_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/hud_font.hpp`
- Modify: `src/platform/raylib/hud_font.cpp`
- Modify: `tests/platform/death_overlay_view_tests.cpp`
- Modify: `tests/platform/ui_material_slice_tests.cpp`
- Modify: `tests/platform/hud_font_tests.cpp`
- Modify: `tests/platform/stage12_actor_render_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes common `ui_warning_modal`, `ui_label_plate` and the already loaded HUD font.
- Produces `DeathOverlayMaterialPlan` and `CombatTextStyle` pure plans.

- [ ] **Step 1: Write RED plan and source-path tests.**

```cpp
struct DeathOverlayMaterialPlan final {
    bool visible{};
    MaterialSpriteId panel{MaterialSpriteId::ui_warning_modal};
    MaterialSpriteId title_plate{MaterialSpriteId::ui_label_plate};
    float panel_border_pixels{32.0F};
};

[[nodiscard]] DeathOverlayMaterialPlan death_overlay_material_plan(
    bool visible) noexcept;

struct CombatTextStyle final {
    float damage_font_size{};
    float defeated_font_size{};
    float spacing{1.0F};
};

[[nodiscard]] CombatTextStyle combat_text_style(
    int screen_width, int screen_height) noexcept;
```

Require visible death mode selects both UI sprites and hidden mode performs zero material draws. Require font coverage for ASCII digits, `-+%`, and `DEFEATED`; require sizes increase monotonically from 800×450 to 1920×1080. Add source guards requiring `MeasureTextEx`/`draw_crisp_ui_text` in damage and defeated paths and forbidding their old direct `MeasureText`/`DrawText` calls.

- [ ] **Step 2: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: death renderer does not accept `MaterialPack` and actor text uses the default font.

- [ ] **Step 3: Integrate existing UI materials.**

Change the renderer signature to:

```cpp
void DeathOverlayRenderer::draw(
    const dungeon::DungeonSnapshot& snapshot,
    const MaterialPack& material_pack) const noexcept;
```

Draw the full-screen dark overlay programmatically, the central panel with `draw_nine_slice(plan.panel, bounds, 32.0F)`, and the title plate with `draw_region_fit`. Keep dynamic title/body, separator and continue/retreat text programmatic. Call it as `death_overlay_.draw(current, material_pack_)`.

If either material draw returns `false`, draw the existing dark panel/plate geometry for only that missing element; the full-screen dark overlay and text remain visible in both paths.

- [ ] **Step 4: Route combat text through the shared HUD font.**

Extend `draw_effects` to receive `Font hud_font, bool hud_font_ready`. Use `combat_text_style(width,height)` based on `ui_viewport_scale`, `MeasureTextEx`, and `draw_crisp_ui_text`; fallback to `GetFontDefault()` only when the loaded font is invalid. Keep the text color pure/opaque at the center (`alpha=255`) and use only a dark one-pixel shadow for contrast; do not introduce gradient text.

Register exactly four new C++ cases from Step 1 and change the platform expected case count from 465 to 469.

- [ ] **Step 5: Run GREEN tests.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: material plan, nine-slice source guard, font coverage/scaling and actor text route pass.

- [ ] **Step 6: Commit.**

```powershell
git add src/platform/raylib/death_overlay_view.* src/platform/raylib/death_overlay_renderer.* src/platform/raylib/combat_renderer.cpp src/platform/raylib/actor_renderer.cpp src/platform/raylib/hud_font.* tests/platform/death_overlay_view_tests.cpp tests/platform/ui_material_slice_tests.cpp tests/platform/hud_font_tests.cpp tests/platform/stage12_actor_render_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: integrate death and combat text materials"
```

