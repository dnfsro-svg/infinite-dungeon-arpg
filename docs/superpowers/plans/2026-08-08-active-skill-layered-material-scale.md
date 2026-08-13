# Active Skill Layered Material Scale Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the player at normal scale during active skills while restoring the large baked draw-slash and storm-swords effects from the existing material atlases.

**Architecture:** The existing color/material texture pair is drawn twice only while a material skill is active. The first draw keeps the current actor-normalized scale; the second draw uses a shader emissive-only mode and the original skill-effect scale. Both passes reuse the same resident textures, source frame, foot anchor, facing and camera projection.

**Tech Stack:** C++17, raylib 6.0, OpenGL 3.3 fragment shader, CMake/Ninja, MSVC 19.44.

## Global Constraints

- Do not change skill damage, hit boxes, invulnerability, cooldowns, timelines, camera behavior or input rules.
- Do not add texture files, texture residency, disk reads, shader objects or per-frame heap allocation.
- Keep the existing procedural path only as the unavailable-material fallback.
- Use at most four parallel compile jobs and run only the listed targeted validation.
- Preserve every unrelated dirty-worktree change; do not reset, delete or overwrite files.
- Do not commit from this shared dirty worktree; leave selective integration to the root task.

## File Map

- `src/platform/raylib/active_skill_assets.hpp/.cpp`: actor and effect scale policy.
- `src/platform/raylib/material_pack.hpp/.cpp`: material composite mode, shader uniforms and emissive frame draw API.
- `src/platform/raylib/active_skill_renderer.cpp`: invoke actor and effect passes for the same active-skill frame.
- `tests/platform/active_skill_asset_tests.cpp`: scale contract, extending the existing scale case.
- `tests/platform/material_asset_validation_tests.cpp`: same-texture emissive draw contract, extending the existing material pair case.
- `docs/superpowers/specs/2026-08-08-active-skill-layered-material-scale-design.md`: approved design source.

---

### Task 1: Separate actor and effect scale policy

**Files:**
- Modify: `tests/platform/active_skill_asset_tests.cpp`
- Modify: `src/platform/raylib/active_skill_assets.hpp`
- Modify: `src/platform/raylib/active_skill_assets.cpp`

**Interfaces:**
- Consumes: `active_skill_material_draw_scale(skills::ActiveSkillId, float)`.
- Produces: `float active_skill_material_effect_scale(skills::ActiveSkillId id, float projection_scale) noexcept`.

- [ ] **Step 1: Extend the existing failing scale test**

In `active_skill_actor_scale_matches_base_player_geometry()`, add exact effect-scale assertions without adding a test case:

```cpp
const float draw_effect_scale = platform::active_skill_material_effect_scale(
    skills::ActiveSkillId::draw_slash, projection_scale);
const float storm_effect_scale = platform::active_skill_material_effect_scale(
    skills::ActiveSkillId::storm_swords, projection_scale);
ARPG_REQUIRE(arpg::test::near(draw_effect_scale, 0.72F * projection_scale));
ARPG_REQUIRE(arpg::test::near(storm_effect_scale, 0.70F * projection_scale));
ARPG_REQUIRE(draw_effect_scale > draw_scale);
ARPG_REQUIRE(storm_effect_scale > storm_scale);
ARPG_REQUIRE(platform::active_skill_material_effect_scale(
    skills::ActiveSkillId::none, projection_scale) == 0.0F);
ARPG_REQUIRE(platform::active_skill_material_effect_scale(
    skills::ActiveSkillId::draw_slash, 0.0F) == 0.0F);
```

- [ ] **Step 2: Build to observe RED**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 4
```

Expected: compilation fails because `active_skill_material_effect_scale` does not exist. Do not run another target.

- [ ] **Step 3: Add the minimal scale function**

Declare the produced interface in `active_skill_assets.hpp`. Implement it in `active_skill_assets.cpp`:

```cpp
float active_skill_material_effect_scale(
    skills::ActiveSkillId id, float projection_scale) noexcept {
    if (projection_scale <= 0.0F) return 0.0F;
    switch (id) {
    case skills::ActiveSkillId::draw_slash:
        return 0.72F * projection_scale;
    case skills::ActiveSkillId::storm_swords:
        return 0.70F * projection_scale;
    case skills::ActiveSkillId::none:
    case skills::ActiveSkillId::count:
        return 0.0F;
    }
    return 0.0F;
}
```

- [ ] **Step 4: Rebuild to prove the scale contract compiles**

Run the same target build once. Expected: exit `0`. Do not run the full executable yet; Task 2 adds the second test contract before the one combined GREEN run.

---

### Task 2: Add an emissive-only MaterialPack draw

**Files:**
- Modify: `tests/platform/material_asset_validation_tests.cpp`
- Modify: `src/platform/raylib/material_pack.hpp`
- Modify: `src/platform/raylib/material_pack.cpp`

**Interfaces:**
- Consumes: the existing resident atlas texture pair and `MaterialTextureApi::draw_material`.
- Produces: `MaterialCompositeMode::emissive_only` and `MaterialPack::draw_frame_emissive(...) const noexcept` with the same geometry arguments as `draw_frame(...)`.

- [ ] **Step 1: Extend the existing material-pair test**

Immediately after the current successful `draw_frame` assertions in `material_pack_loads_and_draws_color_material_pairs()`, add:

```cpp
const std::size_t loads_before_emissive = fake.load_count;
ARPG_REQUIRE(pack.draw_frame_emissive(
    MaterialAtlasId::water_bulwark,
    {0.0F, 0.0F, 96.0F, 96.0F}, {48.0F, 93.0F},
    {100.0F, 100.0F}, false, 0.70F));
ARPG_REQUIRE(fake.draw_count == 2U);
ARPG_REQUIRE(fake.drawn_color_ids[1] == fake.drawn_color_ids[0]);
ARPG_REQUIRE(fake.drawn_material_ids[1] == fake.drawn_material_ids[0]);
ARPG_REQUIRE(fake.composites[1].mode
    == MaterialCompositeMode::emissive_only);
ARPG_REQUIRE(fake.composites[1].emissive_mask_start == 0.08F);
ARPG_REQUIRE(fake.composites[1].emissive_mask_end == 0.24F);
ARPG_REQUIRE(fake.load_count == loads_before_emissive);
```

Add `using arpg::platform::MaterialCompositeMode;` beside the existing material aliases.

- [ ] **Step 2: Build to observe RED**

Run the Task 1 build command once. Expected: compilation fails on the missing mode, fields and method.

- [ ] **Step 3: Define the fixed-size composite mode**

In `material_pack.hpp`, add:

```cpp
enum class MaterialCompositeMode : std::uint8_t {
    surface,
    emissive_only,
};
```

Append these defaults to `MaterialCompositeParameters` so every existing aggregate default remains surface rendering:

```cpp
MaterialCompositeMode mode{MaterialCompositeMode::surface};
float emissive_mask_start{0.08F};
float emissive_mask_end{0.24F};
```

Declare these exact members:

```cpp
[[nodiscard]] bool draw_frame_emissive(
    MaterialAtlasId atlas, Rectangle source, Vector2 foot_anchor,
    Vector2 foot_position, bool flip_x, float scale = 1.0F,
    Color tint = WHITE) const noexcept;

[[nodiscard]] bool draw_frame_composited(
    MaterialAtlasId atlas, Rectangle source, Vector2 foot_anchor,
    Vector2 foot_position, bool flip_x, float scale, Color tint,
    MaterialCompositeParameters parameters) const noexcept;
```

The first declaration is public and the second is private.

- [ ] **Step 4: Add shader uniforms and emissive-only output**

Extend `MaterialShaderState` with `int emissive_only_location{-1};` and `int emissive_mask_range_location{-1};`. Add these GLSL uniforms:

```glsl
uniform float emissiveOnly;
uniform vec2 emissiveMaskRange;
```

Replace the final assignment with:

```glsl
if (emissiveOnly > 0.5) {
    float effectMask = smoothstep(
        emissiveMaskRange.x, emissiveMaskRange.y, emissive);
    vec3 effectColor = base.rgb + emissiveLight;
    finalColor = vec4(effectColor,
        base.a * materialSample.a * effectMask);
    return;
}
finalColor = vec4(base.rgb * diffuseLight + metalLight + emissiveLight,
    base.a * materialSample.a);
```

In `begin_material_composite`, upload:

```cpp
const float emissive_only = parameters.mode
        == MaterialCompositeMode::emissive_only ? 1.0F : 0.0F;
const float emissive_mask_range[2]{parameters.emissive_mask_start,
    parameters.emissive_mask_end};
```

Set both uniforms every draw. Require all two new locations during shader initialization so a partial shader cannot report ready.

- [ ] **Step 5: Reuse one validated frame draw path**

Move the current `draw_frame` body into `draw_frame_composited`. Make `draw_frame` call it with default parameters. Make `draw_frame_emissive` create default parameters, set `mode = MaterialCompositeMode::emissive_only`, and call the same helper. Do not touch residency or load counters.

- [ ] **Step 6: Build and run the one combined platform GREEN**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 4
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: build exit `0` and `638 cases, 0 failures`. This single executable run covers both modified existing cases plus the prior HUD/input/menu batch; do not run platform CTest guards.

---

### Task 3: Draw the large effect layer without enlarging the actor

**Files:**
- Modify: `src/platform/raylib/active_skill_renderer.cpp`

**Interfaces:**
- Consumes: `active_skill_material_draw_scale`, `active_skill_material_effect_scale`, and `MaterialPack::draw_frame_emissive`.
- Produces: material-mode `draw_world` reports `material_frame_drawn=true` only when both layers draw.

- [ ] **Step 1: Record the existing visual RED evidence**

Use the already captured internal screenshots in `out/play-visual-20260808-stage17`: draw hit and storm phases show a normal-sized actor but unreadably small effects. Do not rerun the old binary.

- [ ] **Step 2: Draw actor then emissive effect using one frame**

In `draw_material_active_skill`, keep all existing ID/frame/projection/facing logic. Replace its final single draw with:

```cpp
const Vector2 foot_position{player.x, player.ground_y};
const float actor_scale = active_skill_material_draw_scale(id, player.scale);
const bool actor_drawn = material_pack.draw_frame(
    frame->atlas, frame->source, frame->foot_anchor,
    foot_position, flip_x, actor_scale, WHITE);
if (!actor_drawn) return false;
const float effect_scale = active_skill_material_effect_scale(
    id, player.scale);
return material_pack.draw_frame_emissive(
    frame->atlas, frame->source, frame->foot_anchor,
    foot_position, flip_x, effect_scale, WHITE);
```

Do not enable `draw_draw_slash` or `draw_storm_swords` in material mode. Do not change `suppress_base_player`, screen flash or status types.

- [ ] **Step 3: Rebuild the narrow platform target**

Run the Task 2 build command without executing the test binary again. Expected: build exit `0`.

---

### Task 4: Release and visual verification

**Files:**
- Verify: `out/build/windows-msvc-release/bin/arpg_game.exe`
- Verify: a fresh Stage 17 evidence root under `out/`

**Interfaces:**
- Consumes: all Task 1–3 outputs.
- Produces: a Release binary and internally captured PNG evidence.

- [ ] **Step 1: Build only the Release game and Stage 17 validator**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-release
cmake --build --preset windows-msvc-release `
    --target arpg_game arpg_stage17_skill_stones_game_validation --parallel 4
```

Expected: exit `0`. Do not build unrelated test targets.

- [ ] **Step 2: Run Stage 17 once in a fresh evidence root**

Run the built validator with one new absolute evidence root:

```powershell
$evidenceRoot = Join-Path (Resolve-Path out).Path `
    ("stage17-layered-material-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& .\out\build\windows-msvc-release\bin\arpg_stage17_skill_stones_game_validation.exe `
    $evidenceRoot
if ($LASTEXITCODE -ne 0) { throw "Stage17 failed: $LASTEXITCODE" }
$evidenceRoot
```

Expected: `stage17 raylib skill stones scenario=PASS`, with production, storm and restart child scenarios all accepted.

- [ ] **Step 3: Inspect the internal PNGs**

Open the new draw and storm PNGs with the local image viewer. Confirm:

- the player silhouette height matches the ordinary player;
- the draw hit has a broad blue-white arc instead of only small sparks;
- storm ground, aerial and finisher frames visibly surround the locked combat area;
- no second full-size enlarged player silhouette appears.

If the enlarged silhouette is visible, adjust only `emissive_mask_start/end` and repeat the Stage 17 run; do not change scale, assets or gameplay.

- [ ] **Step 4: Final lightweight checks**

Run:

```powershell
git diff --check -- `
    src/platform/raylib/active_skill_assets.hpp `
    src/platform/raylib/active_skill_assets.cpp `
    src/platform/raylib/material_pack.hpp `
    src/platform/raylib/material_pack.cpp `
    src/platform/raylib/active_skill_renderer.cpp `
    tests/platform/active_skill_asset_tests.cpp `
    tests/platform/material_asset_validation_tests.cpp
Get-Process arpg_game,cl,cmake,ctest -ErrorAction SilentlyContinue
```

Expected: no whitespace errors and no listed processes. Update `out/play-continuous-20260808-120721/issues.md` with the exact evidence path and result.
