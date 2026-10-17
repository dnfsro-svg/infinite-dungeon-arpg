# Task 1 Brief — Pure Slot Text Geometry and Fixed-Buffer Plan

## Global Constraints

- Use C++17 and raylib 6.0.
- Work only in `E:/game/.worktrees/loot-suction-animation` on branch `codex/full-chinese-ui`; preserve unrelated user changes and never reset Git.
- Implement only the pure active-skill combat-HUD text geometry and plan in this task. Do not modify the renderer or source guards yet.
- Keep `ActiveSkillHudLayout::bounds` as the existing five-slot strip; do not move slot artwork or change input, cooldown, icons, materials, terminology, glyph catalog, status tags, global HUD backings, or inventory layouts.
- Plate geometry at reference scale is key `{+4,-20,width-8,18}`, cooldown `{+16,+28,width-32,20}`, and name `{+7,+77,width-14,18}`.
- Plate color is exactly `Color{5, 9, 16, 255}`; text uses the existing opaque primary color and must have luma contrast at least 4.5.
- Preferred text size is 16px, minimum 9px, preferred spacing 0.5px.
- Empty slots show key only; ready equipped slots show key and name; cooling equipped slots show all three.
- Fixed caller-owned buffers only; no default font, heap allocation, self-referential pointer, per-frame formatting, or partial UTF-8 truncation.
- Do not add a platform `TestCase`; extend existing cases so MENU/HUD remains exactly `183 cases, 0 failures`.
- Respect `尽量不测试`: run only the debug platform target and MENU/HUD selector for this task. Do not run full suites or formal executables.

## Files

- Modify: `src/platform/raylib/active_skill_view.hpp`
- Modify: `src/platform/raylib/active_skill_view.cpp`
- Modify: `src/platform/raylib/ui_text_bounds_audit.hpp`
- Test: `tests/platform/active_skill_view_tests.cpp`

## Interfaces

- Consumes: `UiTextMeasureFn`, `UiTextAuditRole`, `fit_centered_ui_text`, `ActiveSkillHudSlot`.
- Produces: `ActiveSkillHudTextRegions`, `ActiveSkillTextPlatePlan`, `ActiveSkillHudTextPlan`, `ActiveSkillHudLayout::valid`, `ActiveSkillHudLayout::scale`, `ActiveSkillHudLayout::text_regions`, `active_skill_text_plate_backing()`, and `make_active_skill_hud_text_plan(...)`.

## Step 1 — Write the failing behavior tests

Add `ui_text_layout.hpp`, `ui_text_bounds_audit.hpp`, and `ui_text_contrast.hpp` includes. Add deterministic and failing measure callbacks:

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

Extend `hud_layout_is_bottom_centered_with_scaled_slot_geometry()` so the viewport table is exactly `{{800,450},{1280,720},{1920,1080}}`. For every slot require `layout.valid`, the visual bounds inside the viewport, and the key/cooldown/name rectangles pairwise separated.

At 800x450 require slot strip `{194,354,412,76}`, key y-range `334..352`, cooldown `382..402`, name `431..449`, and cooldown/icon overlap ratio `44*20/(60*60) <= 0.25`.

Extend `hud_cooldown_ratios_are_clamped_and_empty_slots_stay_zero()` to build empty, ready, and cooling plans. Require key-only, key+name, and all-three visibility respectively; exact roles `hud_skill_key`, `hud_skill_name`, and `hud_skill_cooldown`; valid UTF-8; centered extents inside their plate; approved labels not truncated; `active_skill_text_plate_backing().a == 255`; and contrast against `ui_text_contrast_style().primary >= 4.5F`.

Also require empty aggregate plans for `font_ready=false`, invalid UTF-8, invalid region geometry, and `fail_skill_text_measure`. Warm once, then run layout plus all three state plans 10,000 times and require `allocation_count()` unchanged.

## Step 2 — Verify RED

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

The valid RED is compiler failure naming the missing types, fields, roles, backing accessor, and `make_active_skill_hud_text_plan`. Fix any test-source syntax error before counting RED.

## Step 3 — Implement the pure API

Include `ui_text_bounds_audit.hpp` and `ui_text_layout.hpp` from `active_skill_view.hpp`. Append `hud_skill_key` and `hud_skill_cooldown` immediately before `UiTextAuditRole::count`, preserving all existing ordinals.

Define:

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

Extend `ActiveSkillHudLayout` with `float scale{}`, `bool valid{}`, and an array `text_regions` of `ActiveSkillHudTextRegions`.

Declare and implement:

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

Calculate regions with the exact geometry and set `visual_bounds` to the union of the slot and all three regions. Validate finite positive slot, regions, scale, and viewport; invalid layouts keep `valid=false`.

Build each plate through `fit_centered_ui_text` into its own 48-byte array with `16.0F*scale`, `9.0F*scale`, and `0.5F*scale`. Copy only scalar/vector fields and never retain `UiCenteredTextLayout::text`. Key is always attempted; name only when not empty; cooldown only when not empty, ratio positive, and label non-empty.

## Step 4 — Focused GREEN

Build the same target with `--parallel 2`. Snapshot and remove every `^ARPG_.*_ONLY$` environment variable, set only `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`, run `out/build/windows-msvc-debug/bin/arpg_platform_tests.exe`, require the unique summary `183 cases, 0 failures`, then restore every original selector in `finally`.

## Step 5 — Self-review, report, and commit

- Run `git diff --check` and `git diff --cached --check`.
- Confirm the diff contains only the four task files and no new `TestCase`.
- Commit exactly those files with `feat(ui): plan centered active skill labels`.
- Write the full RED/GREEN commands, outputs, allocation evidence, diff scope, commit hash, self-review, and concerns to `.superpowers/sdd/2026-08-11-active-skill-text-plates/task-1-report.md` using `apply_patch`.
- Return only: status, commit hash, one-line test summary, and concerns.
