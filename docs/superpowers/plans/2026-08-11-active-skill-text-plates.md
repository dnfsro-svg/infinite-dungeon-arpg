# Active Skill Text Plates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every active-skill key, name, and cooldown label a fitted centered label on an opaque high-contrast local plate without hiding the skill icon or allocating per frame.

**Architecture:** `active_skill_view` owns a pure geometry layout and a fixed-buffer aggregate text plan. `ActiveSkillRenderer` consumes that plan after slot/icon/cooldown artwork, records the exact plan rectangles, and draws only the visible opaque plates. Shared text helpers gain backward-compatible zero-shadow and actual-shadow audit semantics.

**Tech Stack:** C++17, raylib 6.0, CMake 3.25, the existing custom platform test framework, PowerShell/MSVC presets.

## Global Constraints

- Use C++17 and raylib 6.0.
- Work only in `E:/game/.worktrees/loot-suction-animation` on branch `codex/full-chinese-ui`; preserve unrelated user changes and never reset Git.
- Implement only active-skill combat-HUD text plates plus the two backward-compatible shared-helper corrections required by the approved design.
- Keep `ActiveSkillHudLayout::bounds` as the existing five-slot strip; do not move slot artwork or change input, cooldown, icons, materials, terminology, glyph catalog, status tags, global HUD backings, or inventory layouts.
- Plate geometry at reference scale is key `{+4,-20,width-8,18}`, cooldown `{+16,+28,width-32,20}`, and name `{+7,+77,width-14,18}`.
- Plate color is exactly `Color{5, 9, 16, 255}`; text uses the existing opaque primary color and must have luma contrast at least 4.5.
- Preferred text size is 16px, minimum 9px, preferred spacing 0.5px; active-skill text uses no shadow.
- Empty slots show key only; ready equipped slots show key and name; cooling equipped slots show all three.
- Fixed caller-owned buffers only; no default font, heap allocation, self-referential pointer, per-frame formatting, or partial UTF-8 truncation.
- Do not add a platform `TestCase`; extend existing cases so MENU/HUD remains exactly `183 cases, 0 failures`.
- Respect the user's `尽量不测试` instruction: run only the focused build, MENU/HUD selector, affected direct guard/self-test, Release `arpg_game` target build, and isolated manual screenshots. Do not run the full platform suite or formal executables.

---

## File Structure

- `src/platform/raylib/active_skill_view.hpp`: public fixed-buffer layout and text-plan types and builder declaration.
- `src/platform/raylib/active_skill_view.cpp`: pure geometry and text-plan construction.
- `tests/platform/active_skill_view_tests.cpp`: existing-case behavior, geometry, failure, contrast, and allocation coverage.
- `src/platform/raylib/ui_text_renderer.cpp`: make an explicitly non-positive shadow request skip the shadow draw.
- `src/platform/raylib/ui_text_bounds_audit.hpp/.cpp`: accept and measure the actual shadow size while preserving the default two pixels.
- `src/platform/raylib/active_skill_renderer.cpp`: consume and draw the pure plan using the shared font.
- `tests/platform/hud_renderer_structured_model_source_test.cmake`: require the real early guard, opaque plate, plan audit, fit-consumer, and zero-shadow paths.
- `tests/platform/hud_renderer_structured_model_source_self_test.cmake`: reversible mutations for the strengthened source guard.
- `tests/platform/CMakeLists.txt`: pass the two shared-helper source paths to the existing guard and self-test.

---

### Task 1: Pure Slot Text Geometry and Fixed-Buffer Plan

**Files:**
- Modify: `src/platform/raylib/active_skill_view.hpp`
- Modify: `src/platform/raylib/active_skill_view.cpp`
- Modify: `src/platform/raylib/ui_text_bounds_audit.hpp`
- Test: `tests/platform/active_skill_view_tests.cpp`

**Interfaces:**
- Consumes: `UiTextMeasureFn`, `UiTextAuditRole`, `fit_centered_ui_text`, `ActiveSkillHudSlot`.
- Produces: `ActiveSkillHudTextRegions`, `ActiveSkillTextPlatePlan`, `ActiveSkillHudTextPlan`, `ActiveSkillHudLayout::valid`, `ActiveSkillHudLayout::scale`, `ActiveSkillHudLayout::text_regions`, `active_skill_text_plate_backing()`, and `make_active_skill_hud_text_plan(...)`.

- [ ] **Step 1: Extend existing tests with the wished-for geometry and text-plan behavior**

Add `ui_text_layout.hpp`, `ui_text_bounds_audit.hpp`, and `ui_text_contrast.hpp` includes. Add a deterministic UTF-8 code-point measure callback and a failing measure callback:

```cpp
platform::UiTextExtent measure_skill_text(const char* text, float size,
    float spacing, void*) noexcept {
    std::size_t count{};
    for (const unsigned char* cursor =
             reinterpret_cast<const unsigned char*>(text);
         cursor != nullptr && *cursor != 0U; ++cursor) {
        if ((*cursor & 0xC0U) != 0x80U) ++count;
    }
    return {count == 0U ? 0.0F
                        : static_cast<float>(count) * size
                            + static_cast<float>(count - 1U) * spacing,
        size};
}

platform::UiTextExtent fail_skill_text_measure(
    const char*, float, float, void*) noexcept {
    return {};
}
```

Extend `hud_layout_is_bottom_centered_with_scaled_slot_geometry()` so its viewport table is exactly `{{800,450},{1280,720},{1920,1080}}`. For every slot assert:

```cpp
ARPG_REQUIRE(layout.valid);
ARPG_REQUIRE(platform::ui_text_bounds_inside(
    layout.text_regions[index].visual_bounds,
    {0.0F, 0.0F, static_cast<float>(viewport[0]),
        static_cast<float>(viewport[1])}));
ARPG_REQUIRE(!overlaps(layout.text_regions[index].key,
    layout.text_regions[index].cooldown));
ARPG_REQUIRE(!overlaps(layout.text_regions[index].key,
    layout.text_regions[index].name));
ARPG_REQUIRE(!overlaps(layout.text_regions[index].cooldown,
    layout.text_regions[index].name));
```

At 800x450 require slot strip `{194,354,412,76}`, key y-range `334..352`, cooldown `382..402`, name `431..449`, and cooldown/icon overlap ratio `44*20/(60*60) <= 0.25`.

Extend `hud_cooldown_ratios_are_clamped_and_empty_slots_stay_zero()` to build empty, ready, and cooling plans. Require key-only, key+name, and all-three visibility respectively; exact roles `hud_skill_key`, `hud_skill_name`, and `hud_skill_cooldown`; valid UTF-8; centered extents inside their plate; approved labels not truncated; `active_skill_text_plate_backing().a == 255`; and contrast against `ui_text_contrast_style().primary >= 4.5F`.

Also require empty aggregate plans for `font_ready=false`, invalid UTF-8, invalid region geometry, and `fail_skill_text_measure`. Warm once, then run layout plus all three state plans 10,000 times and require `allocation_count()` is unchanged.

- [ ] **Step 2: Build to verify the new API tests fail**

Run from a VS Developer PowerShell environment:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

Expected: compiler errors name the missing types, fields, roles, backing accessor, and `make_active_skill_hud_text_plan`; no unrelated test-source syntax error may remain.

- [ ] **Step 3: Implement the pure geometry and text-plan API**

Include `ui_text_bounds_audit.hpp` and `ui_text_layout.hpp` from
`active_skill_view.hpp`. Append `hud_skill_key` and `hud_skill_cooldown` immediately before
`UiTextAuditRole::count` in `ui_text_bounds_audit.hpp`, preserving every
existing role ordinal, and define the following in `active_skill_view.hpp`:

```cpp
inline constexpr std::size_t kActiveSkillTextCapacity = 48U;

struct ActiveSkillHudTextRegions final {
    Rectangle key{};
    Rectangle cooldown{};
    Rectangle name{};
    Rectangle visual_bounds{};
};

struct ActiveSkillTextPlatePlan final {
    Rectangle backing{};
    Vector2 position{};
    Vector2 extent{};
    std::array<char, kActiveSkillTextCapacity> text{};
    float font_size{};
    float spacing{};
    UiTextAuditRole role{UiTextAuditRole::count};
    bool visible{};
    bool truncated{};
};

struct ActiveSkillHudTextPlan final {
    ActiveSkillTextPlatePlan key{};
    ActiveSkillTextPlatePlan cooldown{};
    ActiveSkillTextPlatePlan name{};
};
```

Extend `ActiveSkillHudLayout` with `float scale{}`, `bool valid{}`, and
`std::array<ActiveSkillHudTextRegions, skills::kActiveSkillSlotCount>
text_regions{}`. Declare:

```cpp
[[nodiscard]] constexpr Color active_skill_text_plate_backing() noexcept {
    return {5, 9, 16, 255};
}

[[nodiscard]] ActiveSkillHudTextPlan make_active_skill_hud_text_plan(
    const ActiveSkillHudSlot& slot,
    const ActiveSkillHudTextRegions& regions,
    float scale, bool font_ready,
    UiTextMeasureFn measure, void* measure_context) noexcept;
```

In `active_skill_view.cpp`, calculate the three regions from each slot with the
exact reference geometry and set `visual_bounds` to the union of the slot and
all three regions. Validate finite positive slot, region, scale, and viewport
geometry; invalid inputs return the default layout with `valid=false`.

Build each plate through `fit_centered_ui_text` into the plate's own 48-byte array with `16.0F*scale`, `9.0F*scale`, and `0.5F*scale`. Copy only the returned scalar/vector fields; never retain `UiCenteredTextLayout::text`. Mark the plate visible only when the fit is drawable. Key is always attempted; name only when `!slot.empty`; cooldown only when `!slot.empty && slot.cooldown_ratio > 0.0F && slot.cooldown_label[0] != '\0'`.

- [ ] **Step 4: Build and run only the MENU/HUD selector**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$savedSelectors = @{}
Get-ChildItem Env: | Where-Object Name -Match '^ARPG_.*_ONLY$' |
    ForEach-Object {
        $savedSelectors[$_.Name] = $_.Value
        Remove-Item "Env:$($_.Name)"
    }
try {
    $env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY = '1'
    & .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
    if ($LASTEXITCODE -ne 0) { throw "MENU/HUD selector failed" }
} finally {
    Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue
    foreach ($entry in $savedSelectors.GetEnumerator()) {
        Set-Item "Env:$($entry.Key)" $entry.Value
    }
}
```

Expected unique summary: `183 cases, 0 failures`; allocation probe unchanged.

- [ ] **Step 5: Self-review and commit Task 1**

```powershell
git diff --check
git add -- src/platform/raylib/active_skill_view.hpp `
    src/platform/raylib/active_skill_view.cpp `
    src/platform/raylib/ui_text_bounds_audit.hpp `
    tests/platform/active_skill_view_tests.cpp
git diff --cached --check
git commit -m "feat(ui): plan centered active skill labels"
```

---

### Task 2: Opaque Renderer Plates, Accurate Audit, Guard, and Manual Proof

**Files:**
- Modify: `src/platform/raylib/ui_text_renderer.cpp`
- Modify: `src/platform/raylib/ui_text_bounds_audit.hpp`
- Modify: `src/platform/raylib/ui_text_bounds_audit.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `tests/platform/hud_renderer_structured_model_source_test.cmake`
- Modify: `tests/platform/hud_renderer_structured_model_source_self_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1's `ActiveSkillHudLayout::valid`, `text_regions`, `scale`, `ActiveSkillHudTextPlan`, `active_skill_text_plate_backing()`, and `make_active_skill_hud_text_plan(...)`.
- Produces: zero-shadow `draw_crisp_ui_text`, actual-shadow `record_ui_text_bounds`, and the real opaque centered active-skill renderer path.

- [ ] **Step 1: Write source-guard mutations for the wished-for real renderer path**

Pass `UI_TEXT_RENDERER_SOURCE` and `UI_TEXT_BOUNDS_AUDIT_SOURCE` from both existing CTest registrations in `tests/platform/CMakeLists.txt`.

Strengthen the source guard so the extracted `ActiveSkillRenderer::draw_hud` body requires:

```cpp
if (!layout.valid) return;
const ActiveSkillHudTextPlan text_plan = make_active_skill_hud_text_plan(
    slot, layout.text_regions[index], layout.scale,
    hud_font_ready && IsFontValid(hud_font),
    &measure_active_skill_text, &measure_context);
draw_active_skill_text_plate(hud_font, text_plan.key);
draw_active_skill_text_plate(hud_font, text_plan.cooldown);
draw_active_skill_text_plate(hud_font, text_plan.name);
```

Require the extracted plate helper to draw `DrawRectangleRec(plan.backing,
active_skill_text_plate_backing())`, then call `record_ui_text_bounds` with
`plan.role`, `plan.backing`, and final `shadow_pixels=0`, then call
`draw_crisp_ui_text` with `shadow_pixels=0`.

Require `ui_text_renderer.cpp` to guard its shadow `DrawTextEx` with
`if (shadow_pixels > 0)`, and require the audit implementation to clamp the
explicit shadow to zero before measuring. Add reversible self-test mutants for:

- moving `if (!layout.valid) return;` after the first material draw;
- removing each key/cooldown/name draw;
- replacing `plan.position` with a fixed offset;
- replacing either final zero-shadow argument with `2`;
- making the shadow draw unconditional;
- making the audit ignore the explicit shadow size;
- hiding required calls in `if(false)` or an uncalled lambda.

- [ ] **Step 2: Run the direct guard to verify RED**

```powershell
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
    --parallel 1 -R '^platform\.hud_renderer_structured_model_source$'
```

Expected: one failing test naming the first missing valid-layout or plate-plan contract. Do not count a missing source argument or CMake syntax error as the RED.

- [ ] **Step 3: Implement zero-shadow draw/audit and consume the plan**

Change `draw_crisp_ui_text` so its shadow `DrawTextEx` executes only when
`shadow_pixels > 0`; foreground drawing is unchanged.

Add `int shadow_pixels = 2` as the final parameter of
`record_ui_text_bounds`. Pass it to `measured_bounds`; use
`const int effective_shadow = shadow_pixels > 0 ? shadow_pixels : 0` for the
right/bottom expansion. Existing callers compile unchanged and retain two
pixels.

In `active_skill_renderer.cpp`, remove `draw_skill_text` and
`fit_skill_label`. Add an `UiTextExtent` adapter around `MeasureTextEx`, a
small context containing the shared `Font`, and a plate helper that:

```cpp
if (!plan.visible) return;
DrawRectangleRec(plan.backing, active_skill_text_plate_backing());
record_ui_text_bounds(UiTextAuditPage::hud, plan.role, font,
    plan.text.data(), plan.position, plan.font_size, plan.spacing,
    plan.backing, plan.font_size, nullptr, 0U, 0);
draw_crisp_ui_text(font, plan.text.data(), plan.position,
    plan.font_size, plan.spacing, ui_text_contrast_style().primary, 0);
```

Return before any draw when `!layout.valid`. Keep existing slot, icon, and
cooldown artwork order, then build and draw the three plan plates. Do not call
`GetFontDefault`, `DrawText`, `TextFormat`, `snprintf`, or construct strings.

- [ ] **Step 4: Run only focused GREEN verification**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2

$savedSelectors = @{}
Get-ChildItem Env: | Where-Object Name -Match '^ARPG_.*_ONLY$' |
    ForEach-Object {
        $savedSelectors[$_.Name] = $_.Value
        Remove-Item "Env:$($_.Name)"
    }
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY = '1'
try {
    & .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
    if ($LASTEXITCODE -ne 0) { throw "MENU/HUD selector failed" }
} finally {
    Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue
    foreach ($entry in $savedSelectors.GetEnumerator()) {
        Set-Item "Env:$($entry.Key)" $entry.Value
    }
}

ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
    --parallel 1 -R '^platform\.hud_renderer_structured_model_source$'
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
    --parallel 1 -R '^platform\.hud_renderer_structured_model_source_self_test$'
```

Expected: `183 cases, 0 failures`; direct guard 1/1; self-test 1/1.

- [ ] **Step 5: Build Release game and perform isolated manual proof**

```powershell
cmake --build --preset windows-msvc-release --target arpg_game --parallel 2
```

Safely close the currently paused isolated game with `Alt+F4` and wait until
that exact process exits; never use `taskkill` or mutate the default save.
Copy only the rebuilt `arpg_game.exe` into the existing isolated app root
`C:/Users/dnfsr/AppData/Local/InfiniteDungeon/material-multiplier-20260811-0945/app`, preserving the isolated `--save-dir`, `--settings-dir`, and
`--screenshot-dir`. Launch that exact executable, use F12 screenshots, and
verify:

- ready slots: opaque key plate above and name plate below each equipped slot;
- empty slots: key plate only;
- `Numpad_2` cooldown: opaque centered `N秒` plate over the icon;
- no plate leaves the viewport, no text is clipped, and icon art remains mostly visible.

Do not deploy, touch the default save, run formal executables, or run the full test suite.

- [ ] **Step 6: Final scoped review and commit Task 2**

```powershell
git diff --check
git add -- src/platform/raylib/ui_text_renderer.cpp `
    src/platform/raylib/ui_text_bounds_audit.hpp `
    src/platform/raylib/ui_text_bounds_audit.cpp `
    src/platform/raylib/active_skill_renderer.cpp `
    tests/platform/hud_renderer_structured_model_source_test.cmake `
    tests/platform/hud_renderer_structured_model_source_self_test.cmake `
    tests/platform/CMakeLists.txt
git diff --cached --check
git commit -m "fix(ui): back active skill labels with contrast"
```

After the task-scoped review is clean, run one whole-feature read-only review
over the two implementation commits. Do not broaden verification beyond the
focused commands above.
