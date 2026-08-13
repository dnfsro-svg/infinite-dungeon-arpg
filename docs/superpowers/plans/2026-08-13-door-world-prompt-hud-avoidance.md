# Door World Prompt HUD Avoidance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prevent off-screen or bottom-edge door labels such as “水流” from appearing inside the active-skill HUD while keeping visible door labels readable.

**Architecture:** Extend the pure active-skill layout with one aggregate visual blocker and add a door-only world-prompt layout that reasons in final screen coordinates, including the live camera shake offset. Wire only door labels through that path; generic hole and abyss prompts keep their current layout path.

**Tech Stack:** C++17, raylib 6.0, CMake 3.25+, MSVC Debug preset, repository test framework, CMake source guards.

## Global Constraints

- Work only in `E:\game\.worktrees\loot-suction-animation` on branch `codex/full-chinese-ui`; preserve unrelated changes and never reset Git.
- Use C++17 and raylib 6.0; do not add dependencies, a second font, heap allocation, or a new render stage.
- Keep “水流” and every existing door/input/map/save rule unchanged.
- Keep active-skill slot positions, empty-slot key-only behavior, hole prompts, abyss prompts, and room → actors → HUD draw order unchanged.
- `door_world_prompt_layout` must be pure, fixed-size, `noexcept`, and fail closed on non-finite required geometry.
- A finite empty HUD blocker means “no blocker”; a blocker containing NaN/Inf means invalid and must return invisible.
- Use the actual material destination when the door material draw succeeds and the 82×70 fallback frame only when it fails.
- Apply the live screen offset before visibility/clamp/collision calculations and subtract it from returned draw coordinates so `BeginMode2D` applies it exactly once.
- Support 800×450, 1280×720, and 1920×1080; use a `12 * ui_viewport_scale` safety margin and prove final geometry for all four `{±9, ±9}` shake extremes.
- Do not add a platform `TestCase`; extend existing cases so the MENU/HUD selector remains exactly `183 cases, 0 failures`.
- Honor “尽量不测试”: run only required incremental build, the MENU/HUD selector, and the exact hole-prompt source guard/self-test. Do not run full, formal, `scripts/Test.ps1`, Release, or a game window in these tasks.

## File Map

- `src/platform/raylib/active_skill_view.hpp/.cpp`: expose the aggregate five-slot visual blocker.
- `src/platform/raylib/dungeon_view_math.hpp/.cpp`: own the pure door-only final-screen placement rules.
- `src/platform/raylib/room_renderer.cpp`: compute the actual door draw bounds and consume the pure placement.
- `src/platform/raylib/combat_renderer.hpp/.cpp`: forward the live camera offset to room-door rendering.
- `tests/platform/active_skill_view_tests.cpp`: prove the aggregate blocker without adding a case.
- `tests/platform/hud_layout_tests.cpp`: prove culling, material/fallback visibility boundaries, HUD avoidance, shake extremes, invalid inputs, centering, and zero allocation.
- `tests/platform/hole_prompt_binding_source_test.cmake`: lock the real renderer wiring while preserving generic hole/abyss calls.
- `tests/platform/hole_prompt_binding_source_self_test.cmake`: prove the source guard with reversible single-cause mutations.

---

### Task 1: Pure door prompt and HUD geometry

**Files:**
- Modify: `src/platform/raylib/active_skill_view.hpp`
- Modify: `src/platform/raylib/active_skill_view.cpp`
- Modify: `src/platform/raylib/dungeon_view_math.hpp`
- Modify: `src/platform/raylib/dungeon_view_math.cpp`
- Test: `tests/platform/active_skill_view_tests.cpp`
- Test: `tests/platform/hud_layout_tests.cpp`

**Interfaces:**
- Consumes: existing `active_skill_hud_layout(int, int)`, `world_prompt_layout(Vector2, Vector2, float, float)`, `ui_viewport_scale(int, int)`, and raylib `Rectangle`/`Vector2`.
- Produces: `ActiveSkillHudLayout::visual_bounds` and the exact function below for Task 2:

```cpp
[[nodiscard]] WorldPromptLayout door_world_prompt_layout(
    Vector2 anchor, Vector2 measured,
    Rectangle door_bounds, Rectangle hud_blocker,
    Vector2 screen_offset,
    float screen_width, float screen_height) noexcept;
```

- [ ] **Step 1: Extend the existing active-skill responsive-layout case with a failing aggregate-bound contract**

Add a local rectangle-union helper to `tests/platform/active_skill_view_tests.cpp`, then extend `hud_layout_is_bottom_centered_with_scaled_slot_geometry()` after its per-slot assertions:

```cpp
Rectangle expected_visual = layout.text_regions.front().visual_bounds;
for (std::size_t index = 1U; index < layout.text_regions.size(); ++index) {
    expected_visual = rectangle_union(
        expected_visual, layout.text_regions[index].visual_bounds);
}
ARPG_REQUIRE(same_rectangle(layout.visual_bounds, expected_visual));
ARPG_REQUIRE(layout.visual_bounds.x >= 0.0F);
ARPG_REQUIRE(layout.visual_bounds.y >= 0.0F);
ARPG_REQUIRE(layout.visual_bounds.x + layout.visual_bounds.width
    <= static_cast<float>(viewport[0]));
ARPG_REQUIRE(layout.visual_bounds.y + layout.visual_bounds.height
    <= static_cast<float>(viewport[1]));
```

Do not add an entry to `kCases`.

- [ ] **Step 2: Extend the existing world-prompt case with failing door-layout behavior**

In `tests/platform/hud_layout_tests.cpp`, add includes for `active_skill_view.hpp`, `allocation_probe.hpp`, `<cmath>`, and `<limits>`. Add strict positive-area overlap, translation, finite-rectangle, and centered-text helpers. Extend `world_prompt_backings_fit_supported_viewports()` with this matrix:

```cpp
constexpr std::array<Vector2, 4U> kShakeOffsets{{
    {-9.0F, -9.0F}, {-9.0F, 9.0F},
    {9.0F, -9.0F}, {9.0F, 9.0F},
}};
for (const auto viewport : kViewports) {
    const float width = static_cast<float>(viewport[0]);
    const float height = static_cast<float>(viewport[1]);
    const platform::ActiveSkillHudLayout skill =
        platform::active_skill_hud_layout(viewport[0], viewport[1]);
    ARPG_REQUIRE(skill.valid);

    const Vector2 measured{48.0F * skill.scale, 16.0F * skill.scale};
    const Rectangle far_below{width * 0.5F - 92.0F,
        height + 40.0F, 184.0F, 184.0F};
    ARPG_REQUIRE(!platform::door_world_prompt_layout(
        {width * 0.5F, height + 15.0F}, measured,
        far_below, skill.visual_bounds, {}, width, height).visible);

    // The material destination is visible while the smaller fallback frame
    // is still fully below the viewport.
    const Rectangle material_entering{width * 0.5F - 92.0F,
        height - 12.0F, 184.0F, 184.0F};
    const Rectangle fallback_still_offscreen{width * 0.5F - 41.0F,
        height + 2.0F, 82.0F, 70.0F};
    ARPG_REQUIRE(platform::door_world_prompt_layout(
        {width * 0.5F, height - 6.0F}, measured,
        material_entering, skill.visual_bounds, {}, width, height).visible);
    ARPG_REQUIRE(!platform::door_world_prompt_layout(
        {width * 0.5F, height - 6.0F}, measured,
        fallback_still_offscreen, skill.visual_bounds, {}, width, height).visible);

    const Rectangle visible_door{width * 0.5F - 92.0F,
        height - 184.0F, 184.0F, 184.0F};
    for (const Vector2 offset : kShakeOffsets) {
        const platform::WorldPromptLayout logical =
            platform::door_world_prompt_layout(
                {width * 0.5F, skill.visual_bounds.y + 4.0F}, measured,
                visible_door, skill.visual_bounds, offset, width, height);
        ARPG_REQUIRE(logical.visible);
        const Rectangle final_backing = translated(logical.backing, offset);
        const Vector2 final_text{
            logical.text_position.x + offset.x,
            logical.text_position.y + offset.y};
        ARPG_REQUIRE(inside(final_backing, {0.0F, 0.0F, width, height}));
        ARPG_REQUIRE(!overlaps(final_backing, skill.visual_bounds));
        ARPG_REQUIRE(centered(final_text, logical.measured, final_backing));
    }
}
```

Also add:

- a non-colliding upper prompt whose final-screen result equals `world_prompt_layout(anchor + offset, measured, width, height)`;
- a door rectangle that is outside before offset but inside after `{0, -9}` and must be visible;
- NaN/Inf probes for anchor, measured, door, blocker, offset, and viewport, all invisible;
- a finite empty blocker probe that remains visible;
- a 10,000-iteration warmed allocation probe over all four offsets.

Do not add an entry to `kCases`.

- [ ] **Step 3: Build to verify the interface RED**

Run in PowerShell:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

Expected: build fails in the modified tests because `ActiveSkillHudLayout::visual_bounds` and/or `door_world_prompt_layout` do not exist. Record the first business failure; do not count a missing MSVC environment as RED.

- [ ] **Step 4: Implement `ActiveSkillHudLayout::visual_bounds` minimally**

Add the field after `bounds` in `active_skill_view.hpp`:

```cpp
struct ActiveSkillHudLayout final {
    Rectangle bounds{};
    Rectangle visual_bounds{};
    std::array<Rectangle, skills::kActiveSkillSlotCount> slots{};
    float scale{};
    bool valid{};
    std::array<ActiveSkillHudTextRegions,
        skills::kActiveSkillSlotCount> text_regions{};
};
```

In `active_skill_hud_layout`, after each `regions.visual_bounds` is validated, initialize on index zero and union every later region:

```cpp
if (index == 0U) {
    result.visual_bounds = regions.visual_bounds;
} else {
    result.visual_bounds = rectangle_union(
        result.visual_bounds, regions.visual_bounds);
}
```

Before setting `result.valid`, require `valid_rectangle(result.visual_bounds)` and `ui_text_bounds_inside(result.visual_bounds, viewport)`; otherwise return `{}`.

- [ ] **Step 5: Implement `door_world_prompt_layout` minimally**

Declare the exact interface in `dungeon_view_math.hpp`. In `dungeon_view_math.cpp`, include `ui_typography.hpp` and add local pure helpers for finite rectangles/vectors, positive-area intersection, translation, and strict rectangle overlap. Implement this flow exactly:

```cpp
WorldPromptLayout door_world_prompt_layout(
    Vector2 anchor, Vector2 measured,
    Rectangle door_bounds, Rectangle hud_blocker,
    Vector2 screen_offset,
    float screen_width, float screen_height) noexcept {
    if (!finite(anchor) || !finite(measured) || !finite(door_bounds)
            || !finite(hud_blocker) || !finite(screen_offset)
            || !std::isfinite(screen_width) || !std::isfinite(screen_height)
            || screen_width <= 0.0F || screen_height <= 0.0F
            || !has_area(door_bounds)) {
        return {};
    }
    const Rectangle viewport{0.0F, 0.0F, screen_width, screen_height};
    const Rectangle final_door = translated(door_bounds, screen_offset);
    if (!overlaps(final_door, viewport)) return {};

    const Vector2 final_anchor{
        anchor.x + screen_offset.x, anchor.y + screen_offset.y};
    WorldPromptLayout layout = world_prompt_layout(
        final_anchor, measured, screen_width, screen_height);
    if (!layout.visible) return {};

    if (has_area(hud_blocker)) {
        const float margin = 12.0F * ui_viewport_scale(
            static_cast<int>(screen_width), static_cast<int>(screen_height));
        const Rectangle expanded{
            hud_blocker.x - margin, hud_blocker.y - margin,
            hud_blocker.width + margin * 2.0F,
            hud_blocker.height + margin * 2.0F};
        if (overlaps(layout.backing, expanded)) {
            layout.backing.y = expanded.y - layout.backing.height;
            layout.text_position.y = layout.backing.y
                + (layout.backing.height - layout.measured.y) * 0.5F;
            if (!inside(layout.backing, viewport)
                    || overlaps(layout.backing, expanded)) return {};
        }
    }

    layout.backing.x -= screen_offset.x;
    layout.backing.y -= screen_offset.y;
    layout.text_position.x -= screen_offset.x;
    layout.text_position.y -= screen_offset.y;
    return layout;
}
```

The helper named `finite(hud_blocker)` must accept a finite empty rectangle and reject NaN/Inf; `has_area(hud_blocker)` alone decides whether avoidance is active.

- [ ] **Step 6: Build and run the one focused selector for GREEN**

Rebuild with the Step 3 command. Then run the MENU/HUD selector in a clean selector environment:

```powershell
$saved = @{}
Get-ChildItem Env: | Where-Object Name -match '^ARPG_.*_ONLY$' | ForEach-Object {
    $saved[$_.Name] = $_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY = '1'
    & .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
    if ($LASTEXITCODE -ne 0) { throw "MENU/HUD selector failed: $LASTEXITCODE" }
} finally {
    Get-ChildItem Env: | Where-Object Name -match '^ARPG_.*_ONLY$' | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach ($entry in $saved.GetEnumerator()) {
        Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value
    }
}
```

Expected: one exact summary `183 cases, 0 failures`; allocation probe remains unchanged.

- [ ] **Step 7: Review scope and commit Task 1**

Run:

```powershell
git diff --check
git diff --stat
git diff -- tests/platform/active_skill_view_tests.cpp tests/platform/hud_layout_tests.cpp src/platform/raylib/active_skill_view.hpp src/platform/raylib/active_skill_view.cpp src/platform/raylib/dungeon_view_math.hpp src/platform/raylib/dungeon_view_math.cpp
```

Stage only those six files and commit:

```powershell
git add -- src/platform/raylib/active_skill_view.hpp src/platform/raylib/active_skill_view.cpp src/platform/raylib/dungeon_view_math.hpp src/platform/raylib/dungeon_view_math.cpp tests/platform/active_skill_view_tests.cpp tests/platform/hud_layout_tests.cpp
git diff --cached --check
git commit -m "feat(ui): plan door prompts around active skill HUD"
```

---

### Task 2: Wire actual door bounds and camera offset

**Files:**
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Test: `tests/platform/hole_prompt_binding_source_test.cmake`
- Test: `tests/platform/hole_prompt_binding_source_self_test.cmake`

**Interfaces:**
- Consumes: `ActiveSkillHudLayout::visual_bounds` and `door_world_prompt_layout(...)` exactly as produced by Task 1; `default_material_manifest()`; `CameraOffset` from `CombatFeedback`.
- Produces: door-only renderer wiring that supplies actual material/fallback bounds, the current screen offset, and the full active-skill blocker. Generic hole/abyss calls remain source-compatible.

- [ ] **Step 1: Write source-guard expectations and reversible mutations first**

Extend `hole_prompt_binding_source_test.cmake` so the extracted live bodies require:

```text
GroundLootView CombatRenderer::draw: draw_room(..., interact_binding_label, {camera_offset.x, camera_offset.y})
CombatRenderer::draw_room: draw_doors(..., world_ui_font_ready_, screen_offset)
draw_doors: active_skill_hud_layout(...), active_skill_layout.visual_bounds,
            default_material_manifest(), material_drawn,
            material_drawn ? material_destination : frame,
            draw_world_prompt(..., &door_bounds, hud_blocker, screen_offset)
draw_world_prompt: door_bounds != nullptr ? door_world_prompt_layout(...) : world_prompt_layout(...)
```

Keep the two exact `draw_hole` calls unchanged and continue forbidding `DrawText`, `DrawTextEx`, `MeasureText`, and `GetFontDefault` in the protected functions.

Extend `hole_prompt_binding_source_self_test.cmake` with `replace_required`-style anchors that fail if no replacement occurs, then add these single-cause mutants:

```text
material_success_uses_fallback_frame  material_drawn ? material_destination : frame -> frame
slot_only_blocker                    active_skill_layout.visual_bounds -> active_skill_layout.bounds
camera_offset_dropped                {camera_offset.x, camera_offset.y} -> {}
door_layout_bypassed                 door_world_prompt_layout(...) -> world_prompt_layout(...)
door_requirements_in_constexpr_false keep required tokens in if constexpr(false), use frame/{}/bounds live
door_requirements_in_uncalled_lambda keep required tokens in an uncalled auto&& lambda, use frame/{}/bounds live
```

The dead-decoy mutants must be valid C++17 and the live path must still contain the generic draw so rejection is caused by the door-specific contract, not by unrelated missing text drawing.

- [ ] **Step 2: Run the direct guard to verify RED**

Run:

```powershell
cmake -DRAYLIB_SOURCE_DIR="$PWD/src/platform/raylib" -P tests/platform/hole_prompt_binding_source_test.cmake
```

Expected: exit nonzero with the first missing door-specific wiring requirement. Do not run the long self-test before production exists.

- [ ] **Step 3: Forward the exact live camera offset**

Change the private declaration and definition of `CombatRenderer::draw_room` to append `Vector2 screen_offset`. In `CombatRenderer::draw`, change only the room call:

```cpp
draw_room(world, render_plan.ground_loot,
    render_plan.material_loot, camera, interact_binding_label,
    {camera_offset.x, camera_offset.y});
```

In `CombatRenderer::draw_room`, pass the same value only to `draw_doors`; do not pass it to `draw_hole` and do not modify `BeginMode2D`.

- [ ] **Step 4: Compute the actual material destination and choose it only on draw success**

In the anonymous namespace of `room_renderer.cpp`, add a pure helper that scans `default_material_manifest().frames` for `visual.sprite` and mirrors `MaterialPack::draw` for `flip_x == false`:

```cpp
Rectangle material_destination_bounds(MaterialSpriteId id,
    Vector2 foot_position, float scale) noexcept {
    if (!(scale > 0.0F) || !std::isfinite(scale)) return {};
    const MaterialManifestDefinition manifest = default_material_manifest();
    for (std::size_t index = 0U; index < manifest.frame_count; ++index) {
        const MaterialFrameDefinition& frame = manifest.frames[index];
        if (frame.id != id) continue;
        return {
            foot_position.x - frame.foot_anchor.x * scale,
            foot_position.y - frame.foot_anchor.y * scale,
            frame.source.width * scale,
            frame.source.height * scale,
        };
    }
    return {};
}
```

At the start of `draw_doors`, compute one `ActiveSkillHudLayout` and choose `hud_blocker = valid ? visual_bounds : Rectangle{}`. Replace the inline draw condition with:

```cpp
const float material_scale = 0.72F * projected.scale;
const Rectangle material_destination = material_destination_bounds(
    visual.sprite, {projected.x, projected.ground_y}, material_scale);
const bool material_drawn = material_pack.draw(visual.sprite,
    {projected.x, projected.ground_y}, false, material_scale, body_tint);
if (!material_drawn) {
    DrawRectangleLinesEx(frame, 5.0F * projected.scale, text_color);
}
const Rectangle door_bounds = material_drawn
    && material_destination.width > 0.0F
    && material_destination.height > 0.0F
    ? material_destination : frame;
```

Do not duplicate 256/244 manifest constants in renderer code.

- [ ] **Step 5: Route only door labels through the pure door layout**

Append optional door parameters to the local `draw_world_prompt` helper:

```cpp
const Rectangle* door_bounds = nullptr,
Rectangle hud_blocker = {}, Vector2 screen_offset = {}
```

Select the layout without changing measure/backing/text order:

```cpp
const WorldPromptLayout layout = door_bounds != nullptr
    ? door_world_prompt_layout(anchor, measured, *door_bounds,
        hud_blocker, screen_offset, screen_width, screen_height)
    : world_prompt_layout(anchor, measured, screen_width, screen_height);
```

The door call passes `&door_bounds`, `hud_blocker`, and `screen_offset`. Leave both `draw_hole` calls and the abyss call with their existing argument lists so they take the default generic branch.

- [ ] **Step 6: Build and run the minimal GREEN evidence**

Run the same incremental platform build and clean MENU/HUD selector from Task 1. Expected:

```text
183 cases, 0 failures
```

Then run only the exact room-prompt guard pair:

```powershell
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error --parallel 1 -R '^platform\.hole_prompt_binding_source(_self_test)?$'
```

Expected: `2/2` passed. Do not run any other CTest or executable.

- [ ] **Step 7: Review scope, write the ignored task report, and commit Task 2**

Create `.superpowers/sdd/2026-08-13-door-world-prompt-hud-avoidance/task-2-report.md` with `apply_patch`. Record:

- exact RED first failure;
- incremental build and `183/0` output;
- exact guard `2/2` result and duration;
- confirmation that full/formal/Test.ps1/Release/window were not run;
- allocation result and any concern.

Run `git diff --check`, inspect the five tracked Task 2 files, stage only them, and commit:

```powershell
git add -- src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp src/platform/raylib/room_renderer.cpp tests/platform/hole_prompt_binding_source_test.cmake tests/platform/hole_prompt_binding_source_self_test.cmake
git diff --cached --check
git commit -m "fix(ui): keep door prompts clear of active skill HUD"
```

Do not stage ignored reports or any unrelated file.
