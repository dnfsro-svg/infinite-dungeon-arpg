### Task 1: Compose collision-free combat HUD geometry

**Files:**
- Modify: `src/platform/raylib/hud_layout.hpp`
- Modify: `src/platform/raylib/hud_layout.cpp`
- Modify: `src/platform/raylib/active_skill_view.hpp`
- Modify: `src/platform/raylib/active_skill_view.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Test: `tests/platform/active_skill_view_tests.cpp`

**Interfaces:**
- Consumes: `HudLayout make_hud_layout(int, int, bool)`, `ActiveSkillHudLayout active_skill_hud_layout(int, int)`, `ActiveSkillHudLayout::visual_bounds`.
- Produces: `HudLayout hud_layout_avoiding_bottom_blocker(HudLayout, HudRect) noexcept`, `CombatHudLayout make_combat_hud_layout(int, int, bool) noexcept`, and one cached `ActiveSkillHudLayout` in `CombatRenderer`.

- [ ] **Step 1: Write the failing collision matrix in the existing active-skill layout case**

Add `#include "hud_layout.hpp"` to `tests/platform/active_skill_view_tests.cpp`. Extend `hud_layout_is_bottom_centered_with_scaled_slot_geometry()`; do not create a new TestCase. Use literal expected values:

```cpp
struct CollisionCase final {
    int width;
    int height;
    float expected_scale;
    float expected_x;
};
constexpr std::array<CollisionCase, 4U> kViewports{{
    {800, 450, 1.0F, 284.5F},
    {960, 540, 1.0F, 339.0F},
    {1280, 720, 1.0F, 448.0F},
    {1920, 1080, 1.5F, 672.0F},
}};

const platform::CombatHudLayout composition =
    platform::make_combat_hud_layout(values.width, values.height, false);
ARPG_REQUIRE(composition.valid);
ARPG_REQUIRE(arpg::test::near(
    composition.active_skill.bounds.x, values.expected_x));
const Rectangle player{
    composition.hud.player_panel.x,
    composition.hud.player_panel.y,
    composition.hud.player_panel.width,
    composition.hud.player_panel.height,
};
ARPG_REQUIRE(!overlaps(composition.active_skill.visual_bounds, player));
ARPG_REQUIRE(composition.hud.primary_notice.y
        + composition.hud.primary_notice.height
    <= composition.active_skill.visual_bounds.y
        - 12.0F * composition.hud.scale);
ARPG_REQUIRE(!overlaps(composition.active_skill.visual_bounds,
    Rectangle{composition.hud.primary_notice.x,
        composition.hud.primary_notice.y,
        composition.hud.primary_notice.width,
        composition.hud.primary_notice.height}));
ARPG_REQUIRE(!overlaps(composition.active_skill.visual_bounds,
    Rectangle{composition.hud.secondary_notice.x,
        composition.hud.secondary_notice.y,
        composition.hud.secondary_notice.width,
        composition.hud.secondary_notice.height}));
ARPG_REQUIRE(platform::hud_rect_inside(
    composition.hud.primary_notice, composition.hud.safe_area));
ARPG_REQUIRE(platform::hud_rect_inside(
    composition.hud.secondary_notice, composition.hud.safe_area));
```

Keep the existing assertions for slot width/height, key/cooldown/name regions, viewport containment, separation, and 800×450 cooldown-to-icon ratio. Replace only the old centered-X and old 800×450 `x == 194.0F` expectations with the collision-free literals above. Also assert `active_skill_hud_layout(width, height)` returns the same `bounds` and `visual_bounds` as `composition.active_skill`; this locks the door-prompt entry point to the rendered geometry.

- [ ] **Step 2: Run the focused selector and verify RED**

Run in one PowerShell process so the MSVC environment is retained:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Remove-Item Env:ARPG_*_ONLY -ErrorAction SilentlyContinue
$env:ARPG_ACTIVE_SKILL_VIEW_ONLY = '1'
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
$code = $LASTEXITCODE
Remove-Item Env:ARPG_ACTIVE_SKILL_VIEW_ONLY -ErrorAction SilentlyContinue
exit $code
```

Expected: compile failure because `CombatHudLayout` / `make_combat_hud_layout()` do not exist, or the first new literal collision assertion fails against the centered production layout. Environment/include failures are not an acceptable RED.

- [ ] **Step 3: Add the notice-only blocker helper**

Declare in `hud_layout.hpp`:

```cpp
[[nodiscard]] HudLayout hud_layout_avoiding_bottom_blocker(
    HudLayout layout, HudRect blocker) noexcept;
```

Implement in `hud_layout.cpp` with finite/positive-area checks. Preserve the primary-to-secondary gap by applying one `delta_y` to both rectangles:

```cpp
const float blocker_gap = 12.0F * layout.scale;
const float notice_gap = 6.0F * layout.scale;
const float desired_primary_y = blocker.y - blocker_gap
    - layout.primary_notice.height;
const float minimum_primary_y = layout.safe_area.y
    + layout.secondary_notice.height + notice_gap;
const float target_primary_y = std::max(minimum_primary_y,
    std::min(layout.primary_notice.y, desired_primary_y));
const float delta_y = target_primary_y - layout.primary_notice.y;
layout.primary_notice.y += delta_y;
layout.secondary_notice.y += delta_y;
```

An empty, non-finite, zero-area, or non-positive-scale input returns the original layout unchanged.

- [ ] **Step 4: Shift the complete active-skill layout without changing scale**

Include `hud_layout.hpp` from `active_skill_view.hpp`. Define after `ActiveSkillHudLayout`:

```cpp
struct CombatHudLayout final {
    HudLayout hud{};
    ActiveSkillHudLayout active_skill{};
    bool valid{};
};

[[nodiscard]] CombatHudLayout make_combat_hud_layout(
    int width, int height, bool debug_visible) noexcept;
```

In `active_skill_view.cpp`, extract the current layout construction into an internal helper that accepts a `Rectangle player_blocker`. Keep `scale = ui_viewport_scale(width, height)`. Before filling slots, compute the candidate visual rectangle from `y - 20 * scale` through `y + 95 * scale`; if it overlaps the player blocker, set:

```cpp
x = player_blocker.x + player_blocker.width + 12.0F * scale;
```

Reject the layout if the shifted complete visual rectangle leaves the viewport. The public two-argument `active_skill_hud_layout()` obtains the player blocker from `make_hud_layout(width, height, false).player_panel`, then delegates to the internal helper. `make_combat_hud_layout()` builds one base HUD, delegates with that HUD's player panel, adjusts notices with `hud_layout_avoiding_bottom_blocker()`, and sets `valid` only when both layouts are valid and all moved notices remain in the safe area.

- [ ] **Step 5: Wire one composed frame into CombatRenderer**

Add this member next to `active_skill_hud_model_` in `combat_renderer.hpp`:

```cpp
ActiveSkillHudLayout active_skill_hud_layout_{};
```

In `CombatRenderer::observe_hud()`, replace the standalone `make_hud_layout()` call with:

```cpp
if (IsWindowReady()) {
    const CombatHudLayout composition = make_combat_hud_layout(
        GetScreenWidth(), GetScreenHeight(), false);
    hud_layout_ = composition.hud;
    active_skill_hud_layout_ = composition.active_skill;
} else {
    hud_layout_ = {};
    active_skill_hud_layout_ = {};
}
```

In the normal-HUD draw stage, pass `active_skill_hud_layout_` to `active_skill_renderer_.draw_hud()` instead of recomputing `active_skill_hud_layout(GetScreenWidth(), GetScreenHeight())`. Do not change the ordering: ordinary HUD first, active skills second.

- [ ] **Step 6: Run focused GREEN verification**

Run the exact PowerShell command from Step 2 once. Expected final output:

```text
11 cases, 0 failures
```

Also run:

```powershell
git diff --check
git status --short --untracked-files=all
```

Expected: no whitespace error; exactly the seven implementation/test files listed in this task are modified, while this committed plan remains clean. Do not run CTest, `scripts/Test.ps1`, Release, windowed producers, or the long source-guard self-tests.

- [ ] **Step 7: Request read-only review and commit**

Request an independent read-only review for collision math, final-layout ownership, viewport/safe-area containment, door-prompt consistency, and regression-test strength. Resolve every Critical or Important finding with a new focused RED/GREEN cycle. Then stage exactly the seven task files and commit:

```powershell
git add -- `
  src/platform/raylib/hud_layout.hpp `
  src/platform/raylib/hud_layout.cpp `
  src/platform/raylib/active_skill_view.hpp `
  src/platform/raylib/active_skill_view.cpp `
  src/platform/raylib/combat_renderer.hpp `
  src/platform/raylib/combat_renderer.cpp `
  tests/platform/active_skill_view_tests.cpp
git diff --cached --check
git commit -m "fix(ui): keep combat HUD clear of active skills"
```
