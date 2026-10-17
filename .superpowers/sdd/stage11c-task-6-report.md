# Stage 11-C Task 6 Report — Objective, Navigation and Context HUD

## Status

GREEN

## RED evidence

The Task 6 tests were added before production API/plan implementation. The
first ordinary build was not accepted as RED because the sandbox could not
create Ninja/PDB locks. Re-running in the installed VS x64 environment reached
the intended missing-interface failures:

```text
hud_render_plan_tests.cpp: NavigationHudModel has no member primary
hud_render_plan_tests.cpp: ContextHudModel is not a member of arpg::platform
hud_render_plan_tests.cpp: make_objective_panel_plan not found
hud_view_model_tests.cpp: attach_notice_view not found
hud_view_model_tests.cpp: HudViewModel has no member context
```

After the initial GREEN build, the direct runner correctly caught the stale
registered count (`actual 223 != expected 218`). Updating the Task 6 count to
223 restored the suite. A focused rebind regression then caught unintended
level/passive notices shadowing the inventory fixture; the fixture was made
single-signal and the implementation was not weakened.

## Changes

- Added fixed-capacity objective labels for combat, wave delay, cleared/exit,
  committing, and abyss room snapshots. The wave-delay label contains no
  ungenerated encounter information.
- Added bounded navigation text for depth/floor-room extrema and four Chinese
  element labels with the shared fire/water/lightning/chaos palette values.
- Added `ContextHudModel`, `attach_notice_view`, and fixed plan types for the
  objective, navigation, and two context notice rectangles.
- Moved `HudNoticeKind` into the view-model header as the minimal shared type;
  `HudNoticeView` remains forward-declared there and is included only in the
  implementation, avoiding a header cycle.
- Added `DrawTextEx` rendering for the three new panels. Existing abyss
  confirmation is represented by the already-prioritized abyss notice and is
  no longer drawn by the legacy HUD helper; `abyss_hud_values` was not changed.
- Replaced the old always-visible legacy HUD diagnostic lines with player-safe
  objective/navigation output. `Budget`, `Saturation`, `Invalid owner`, and
  seed text are absent from the normal HUD source/plan.
- Added RED-to-GREEN tests for all requested room states, numeric extremes,
  Chinese element colors, abyss context attachment, E/I/P rebinding, plans,
  and platform test count.

## GREEN evidence

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Result: build passed; direct platform runner reported `223 cases, 0 failures`.

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|stage10.formal_game.capture_content_validator|stage11b.settings_formal|platform.input_latency_source)$" --output-on-failure
```

Result: 4/4 passed.

```powershell
git diff --check
```

Result: passed. Source audit found no forbidden normal-HUD diagnostic terms.

## Commit

`64c65c02164f9c8e8814ab4ccc9f2513797b939b` —
`feat: render room navigation and context hud`

## Self-review

- Scope is limited to Task 6 sources/tests plus the minimal shared notice enum
  relocation. No host/F1 integration, Session, Store, RNG, input sampling,
  fixed tick, combat rules, or Task 7+ files were changed.
- All new view text and plans use fixed arrays/scalars; no dynamic allocation
  or dynamic containers were added.
- Context visibility is limited to populated notice kinds. Door/hole context
  remains sourced from `HudNoticeState`, which publishes only committed
  awaiting-exit interaction state.

## Concerns

The new `HudRenderer` plans/drawing are intentionally not connected to the
production host until Task 7. The retained legacy host path now uses only safe
objective/navigation text and contains no developer diagnostics.

---

## Review Fixes

Status: GREEN

### Added coverage and implementation

- The shared single font plan now explicitly covers every Task 6 Chinese text
  family, including navigation, phase, abyss reward, committing, retreat and
  fault labels. Its fixed codepoint capacity is 320 and the test requires it
  to remain strictly below capacity.
- Abyss context notices now read `abyss_hud_values(current)` and preserve the
  existing descent/door confirmation meaning after an `Interact` rebind. The
  true abyss path emits the rebound key exactly once; the legacy non-visible
  fallback remains only for existing compatibility fixtures.
- Introduced the raylib-free `hud_color.hpp` as the sole RGBA authority.
  Navigation stores `HudPaletteId`, the ViewModel stays raylib-free, and the
  renderer converts the ID to `Color` through `hud_palette()`.
- Added a fixed-buffer `HudTextDrawPlan` with injected measurement for
  headless tests. Rendering measures with `MeasureTextEx`, reduces font size,
  UTF-8-safely ellipsizes if needed, and clips every panel text draw with a
  scissor rectangle.

### RED evidence accuracy

- C/D had valid compile RED before implementation: missing navigation
  `color_id`/shared palette ID and missing `HudTextDrawPlan`/fit API.
- B's real `HudNoticeState` descent/door test was written first, but could not
  run independently against the old implementation because C/D's compile RED
  stopped the executable. After C/D compiled, it exposed the old descent text
  missing `remaining rewards`; the final implementation turns it green.
- A was added before production font changes but was likewise blocked by C/D.
  To prove the production coverage test is effective, the repaired shared
  codepoint source was temporarily mutated to remove `撤`; both the existing
  and Task6 font coverage tests then failed at `covers_required_text`. The
  exact character was restored before GREEN. This is mutation-validity
  evidence, not an original TDD RED claim.

### GREEN evidence

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|stage10.formal_game.capture_content_validator|stage11b.settings_formal|platform.input_latency_source)$" --output-on-failure
git diff --check
```

Result: build passed; direct platform runner reported `227 cases, 0 failures`;
specified CTest gate passed 4/4; `git diff --check` passed.

### Review self-check

- No dynamic allocation was added; all text candidates are fixed 96-byte
  buffers and UTF-8 truncation only moves to preceding codepoint boundaries.
- `hud_view_model.hpp/.cpp` does not include raylib or duplicate element RGB
  values. `hud_palette.hpp` is the only raylib conversion layer.
- Task7 host/F1 integration remains untouched.

### Review commit

`6ec4a26a59a93bb97fe82d40b3a0d1e92369e889` —
`fix: harden task 6 hud text and notices`

---

## Final Review Fixes

Status: GREEN

### Production text coverage

Added a production-driven font test that builds `HudViewModel` snapshots for
idle fallback, combat, wave delay, cleared, committing, death-pending,
faulted and abyss rooms, then checks every generated `HudText96` against the
single shared death-overlay font plan. It also checks real player/status draw
labels. This exposed missing normal-HUD text glyphs for `目标`、`个敌人`、
`待结算经验`, and then the actual navigation separator `·`.

The corpus now contains all four and the shared fixed capacity was increased
from 320 to 384 after the 320-slot array silently reached capacity. The test
requires the final codepoint count to remain strictly below capacity.

### Other review fixes

- Abyss descent and door tests now use `F Interact`, requiring the rebound F
  in both notices alongside their original semantic phrases.
- The 270-pixel maximum-navigation fit test now proves zero allocations around
  plan creation and validates the final fixed buffer as strict UTF-8 plus NUL
  termination.

### RED and GREEN evidence

Before adding the missing corpus glyphs, the new production-driven test failed:

```text
hud_font.production ViewModel text coverage:
model_texts_are_covered(plan, model)
```

Temporary test diagnostics identified the first omitted real output as
`深度 9 · 层房间 7`; the diagnostic was removed before the final commit.

Final GREEN:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|stage10.formal_game.capture_content_validator|stage11b.settings_formal|platform.input_latency_source)$" --output-on-failure
git diff --check
```

Result: build passed; direct runner `228 cases, 0 failures`; specified CTest
gate 4/4 passed; diff check passed.

### Final review commit

`ea7063994abd8acc4b127b81a1a95d675be939e2` —
`fix: cover production hud text glyphs`
