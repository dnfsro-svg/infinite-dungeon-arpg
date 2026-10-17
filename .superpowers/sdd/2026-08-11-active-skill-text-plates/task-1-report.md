# Task 1 Report — Pure Slot Text Geometry and Fixed-Buffer Plan

## Scope

- Worktree: `E:/game/.worktrees/loot-suction-animation`
- Branch: `codex/full-chinese-ui`
- Commit: `c33317a11c6b264dd0f7021985c1570ad1783940 feat(ui): plan centered active skill labels`
- Committed files only:
  - `src/platform/raylib/active_skill_view.hpp`
  - `src/platform/raylib/active_skill_view.cpp`
  - `src/platform/raylib/ui_text_bounds_audit.hpp`
  - `tests/platform/active_skill_view_tests.cpp`
- No renderer, source guard, input, material, icon, cooldown-overlay, or task-2 file was modified.

## RED

Command run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

Result: expected compile failure, exit code 2. The compiler reported missing
`ActiveSkillHudTextRegions`, `ActiveSkillHudTextPlan`,
`ActiveSkillTextPlatePlan`, `ActiveSkillHudLayout::text_regions`,
`ActiveSkillHudLayout::scale`, `UiTextAuditRole::hud_skill_key`,
`UiTextAuditRole::hud_skill_cooldown`,
`active_skill_text_plate_backing`, and
`make_active_skill_hud_text_plan`. The first error was
`active_skill_view_tests.cpp(266): error C2039:
"ActiveSkillHudTextRegions": 不是 "arpg::platform" 的成员`.
No test-source syntax error was present.

## GREEN

Final successful build command:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

Result: exit code 0; CMake configured and generated successfully, then Ninja
reported `no work to do.` The only earlier transient build issue was a new
PowerShell process missing the MSVC `INCLUDE` environment; rerunning the
required Configure script in the same process restored that environment. A
background Ninja process then briefly held the build directory; it ended
naturally before the final successful build. Neither issue was a source or
test failure.

Focused selector command run (the `try/finally` saved, removed, and restored
every `^ARPG_.*_ONLY$` variable; only
`ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1` was active):

```powershell
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Result: exit code 0, unique summary:

```text
183 cases, 0 failures
```

The extended existing HUD case warms all three state plans, then executes the
layout plus empty, ready, and cooling plans 10,000 times. Its allocation probe
asserted that `allocation_count()` was unchanged; the focused selector passed,
which is the allocation evidence.

## Self-review

- Exact reference-scale text regions are key `{+4,-20,width-8,18}`;
  cooldown `{+16,+28,width-32,20}`; name `{+7,+77,width-14,18}`.
- `visual_bounds` is the union of the slot and all three regions. Layout
  validation requires finite positive viewport, scale, slots, regions, and
  visual bounds inside the viewport; failures return `valid=false`.
- Text plans use separate 48-byte arrays and copy only scalar/vector fit
  results, never retaining the fit result's text pointer. Invalid UTF-8,
  invalid geometry, or absent font fail closed to an empty aggregate plan;
  failed key measurement does the same, while failed name/cooldown measurement
  hides only that plate and preserves already successful plates.
- Visibility is key-only for empty slots, key+name for ready equipped slots,
  and all three for cooling equipped slots with a nonempty cooldown label.
- `hud_skill_key` and `hud_skill_cooldown` were appended immediately before
  `UiTextAuditRole::count`; all existing role ordinals remain unchanged.
- `active_skill_text_plate_backing()` returns exactly `Color{5, 9, 16, 255}`;
  the test verifies opaque alpha and primary-text luma contrast at least 4.5.
- `git diff --check` and `git diff --cached --check` both exited 0 before the
  commit. Staged scope contained exactly the four files above, and the staged
  diff contained no added `TestCase`.

## Concerns

None. Renderer wiring and visual acceptance are intentionally deferred to
task 2; this task is only the pure layout and fixed-buffer plan API.

## Review Fix Round 1

### Scope and commit

- Review-fix commit: `31da080ec9f78d4333961c3cd6a8e45d79cedda5 fix(ui): harden active skill text plan`
- Committed files only:
  - `src/platform/raylib/active_skill_view.cpp`
  - `tests/platform/active_skill_view_tests.cpp`
- The changes remain within the four Task 1 files. No renderer, source guard,
  Task 2 file, new platform `TestCase`, full suite, formal executable,
  release build, or manual visual test was used.

### RED

Focused build command (after adding the existing-case assertions):

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

Result: exit code 0. The focused selector then produced the first valid RED:

```text
[FAIL] active_skill_view.active skill HUD cooldown clamp: !non_finite_plan.cooldown.visible
[FAIL] active_skill_view.active skill HUD responsive layout: arpg::test::near(layout.scale, scale)
183 cases, 2 failures
```

The scale assertion was corrected from the initially assumed values to the
actual independent viewport policy values: 800x450 = 1.0, 1280x720 = 1.0,
and 1920x1080 = 1.5 (the defined maximum). After the minimal finite-cooldown
gate, the next focused RED was:

```text
[FAIL] active_skill_view.active skill HUD cooldown clamp: name_failure.cooldown_calls > 0U
183 cases, 1 failures
```

This proved the prior name-fit failure returned before attempting the cooldown
plate. A transient test-helper declaration-order compiler failure occurred
before the valid RED and was repaired before accepting a RED result.

### GREEN

Final focused build command:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

Result: exit code 0.

Focused selector run used the same `try/finally` selector snapshot/restore
procedure as the initial task and set only
`ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`. Result: exit code 0 and unique output:

```text
183 cases, 0 failures
```

### Review self-check

- Cooldown text now requires `std::isfinite(cooldown_ratio)`, strictly
  positive ratio, and a nonempty label. NaN, positive infinity, and negative
  infinity preserve key/name while hiding cooldown.
- Stage-aware measurement context independently fails key, name, or cooldown.
  Key failure keeps the aggregate empty and prevents later calls; failed name
  keeps key and still allows cooldown; failed cooldown keeps key/name.
- The 10,000-iteration allocation assertion now runs actual empty, ready, and
  cooling plans and asserts each state every iteration; the passing selector
  is fresh allocation evidence.
- Layout tests assert literal scales at 800x450, 1280x720, and 1920x1080 and
  every key/cooldown/name x/y/width/height formula for every slot.
- Plan tests traverse `小键盘1` through `小键盘5`, `拔刀斩`, `暴风式`, and `30秒`,
  checking exact source text, no truncation, and the corresponding backing
  region.
- `git diff --check` and `git diff --cached --check` passed before committing;
  the staged range contained only the two listed Task 1 files and no added
  `TestCase`.

### Concerns

None. Renderer integration remains intentionally deferred to Task 2.

## Review Fix Round 2

- Commit: `34eed403e459e490110f200fb2886d9a9c3bffdc test(ui): warm active skill ready plan`
- The allocation-test warm-up now invokes empty, actual ready (`ready_slot`),
  and cooling states before sampling allocations; it matches the already
  asserted three-state 10,000-iteration loop.
- Corrected the initial self-review to distinguish key-measure failure
  (empty aggregate) from name/cooldown-measure failure (only the failed plate
  is hidden and successful plates remain).
- Scope: `tests/platform/active_skill_view_tests.cpp` plus this ignored report;
  no production code, Task 2 file, or TestCase change.
- Verification ran only:

  ```powershell
  . .\scripts\Configure.ps1 -Preset windows-msvc-debug
  cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
  & .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
  ```

  The build exited 0 and the selector, with only
  `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1` set under the existing
  snapshot/restore `try/finally`, exited 0 with the unique summary
  `183 cases, 0 failures`.
