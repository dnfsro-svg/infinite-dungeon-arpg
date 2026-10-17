# Material / recipe mode exclusion

## Scope

- Tracked changes are limited to `tests/platform/inventory_view_math_tests.cpp`,
  `src/platform/raylib/inventory_view_math.cpp`, and
  `src/platform/raylib/inventory_renderer.cpp`.
- The material-target and recipe-combine modes are now mutually exclusive.

## RED

- First appended behavior assertion to the existing inventory recipe/status
  case (no new TestCase): a recipe-ready equipment page with `chaos` selected
  must have neither a visible nor an enabled combine action.
- The production mutation this catches is: ignore `selected_material` and
  continue displaying the recipe action.  Before the production change, the
  existing full-Chinese inventory selector compiled and failed precisely at
  `!material_target_status.action_visible` with `120 cases, 1 failures`.
- The initial direct build lacked the MSVC standard-library environment and
  failed before testing at `<cstddef>`; it is not counted as RED.  Reloading
  the repository's `scripts/Configure.ps1` MSVC setup produced the valid RED.

## GREEN

- `inventory_status_presentation` only creates the combine action when no
  material is selected, while preserving the recipe-ready no-material path.
- `InventoryRenderer::process_input` independently requires no selected
  material before it can call `request_recipe`.
- Executed only the requested focused command after each phase:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_INVENTORY_ONLY -ErrorAction SilentlyContinue }
```

- Final output: `120 cases, 0 failures`.

## Range and self-review

- `git diff --check` passed before staging.
- Reviewed the diff: the new test fails against the old visible action,
  no-material recipe-ready behavior remains asserted and GREEN, presentation
  hides/disables combine while a material is selected, and renderer request
  submission has a separate selected-material guard.
- No CTest, `Test.ps1`, Release build, window run, or long guard/self-test was
  run.  This report is ignored by `.superpowers/` and is not committed.

## Commit

- Read-only review found no Critical issue and confirmed the only renderer
  `request_recipe` call has the selected-material guard.  It noted that the
  focused view-math regression test cannot mutate-test that renderer guard.
  Adding a renderer input test would exceed this task's explicit minimal-test
  and no-new-TestCase constraints, so this is recorded as a handoff concern.
- Commit: `5abefcbe15fa7651fad5ea2829e4f85c87556fba`

## Review fix round 1

- Verified the open finding: before this round, a selected material did not
  affect pointer arbitration.  A left click in the legacy action bounds still
  reached `page_action`; only the request-layer guard stopped submission.
- Added the data-only `material_selected` input seam, then extended the
  existing `inventory layouts` pointer-priority case (no new TestCase).  It
  retains the no-material `page_action` assertion and requires the same click
  with a selected material to resolve to `background_clear` instead.
- Valid RED: after the test, the focused selector built and failed exactly at
  the new `background_clear` expectation in `inventory_view_math_tests.cpp:158`.
- Minimal GREEN: `inventory_pointer_layer` only returns `page_action` when
  `material_selected` is false; renderer supplies the selected-material state.
  The earlier `request_recipe` `!selected_material.has_value()` guard remains
  as the independent second layer.
- Re-ran only the same configured full-Chinese inventory selector; final
  output: `120 cases, 0 failures`.  `git diff --check` passed before staging.
- Tracked round scope is exactly four allowed files: `inventory_view_math.hpp`,
  `inventory_view_math.cpp`, `inventory_renderer.cpp`, and
  `inventory_view_math_tests.cpp`.  No CTest, window, Release, or long guard
  ran.
- Review-fix commit: `92f36ec8a6d2b6ae7dc5b1858a744c819e2cd126`.
