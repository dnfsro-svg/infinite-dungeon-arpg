# Stage 11-C Task 7 Report

## RED

Added `hud_host_integration_tests.cpp` before production integration.  The first
real build used `scripts\Build.ps1 -Preset windows-msvc-debug` and failed in
`hud_host_integration_tests.cpp`: `CombatRenderer` had no `observe_hud`,
`hud_observation_count`, `hud_notice_view`, `hud_binding_revision`, or
`hud_model`.  The initial sandbox could not write CMake/Ninja temporary files;
the verified RED used the approved elevated build directory rather than an old
binary.

## Changes

- `CombatRenderer::observe_hud` builds the fixed-capacity HUD model, advances
  presentation notices only when unpaused, attaches the notice view, records
  the committed binding revision, and never touches gameplay state.
- `raylib_host` observes exactly once after all fixed steps and before every
  normal presented frame.  The recovery presentation branch also observes once
  before its own present path.
- `HudRenderer` is initialized and shut down alongside `DeathOverlayRenderer`.
  Normal HUD draws before transition/death overlays.  `CombatRenderer::draw`
  accepts no `ControlHints` and consumes the prebuilt model only.
- Production diagnostics moved into `DebugOverlayRenderer`, invoked only from
  the existing F1-controlled `draw_debug` branch.  Normal HUD no longer draws
  the old permanent debug text wall or F1 status label.
- Added headless integration coverage for per-frame observation/read-only
  snapshots, paused notification freeze, committed settings revision, and
  prebuilt-model publication.

## GREEN

All commands were run in `E:\game\.worktrees\stage11c-complete-hud`:

```powershell
scripts\Build.ps1 -Preset windows-msvc-debug
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|stage11.death_formal.five_paths|stage11b.settings_formal)$" --output-on-failure
git -c safe.directory=E:/game/.worktrees/stage11c-complete-hud diff --check
```

The direct platform executable reported `232 cases, 0 failures`.  The focused
CTest selection passed all 5/5 tests: platform units, host input source, input
latency source, death formal five paths, and Stage 11-B settings formal.

## Lifecycle, order and input evidence

- HUD observation runs after the single physical input snapshot has been
  mapped and after host pause/death gates, so no second keyboard sample is
  introduced.
- The committed `input_settings` revision refreshes `control_hints` before
  `observe_hud`; the observation stores that revision for debug output.
- Pause uses `pause_blocks_gameplay` when advancing HUD notices, freezing their
  real-time lifetime.  Recovery observations also pass paused.
- Normal HUD renders before transition and death overlays; debug diagnostics
  are F1-gated and draw after normal HUD.

## Self-review and concerns

No Task8 source/stress guard or Task9 formal evidence was added.  The new
headless contract deliberately does not invoke `CombatRenderer::draw`, because
world drawing requires an initialized raylib window; it verifies the complete
prebuilt draw input instead.  Formal raylib screenshot validation remains
Task9 scope.

## Review remediation

Review requested four Task7-only corrections. New RED coverage first failed to
compile because the production presentation seam, recovery status, complete
debug diagnostics plan and fallback draw-mode API did not exist. After the
minimal implementation, the recovery assertion initially failed because the
existing priority policy correctly puts `save_error` before the persistent
`recovery_required` secondary notice; the test was corrected to assert both.

- `HudPresentedFrame` and `observe_presented_hud_frame` now form the production
  presentation seam. The host source test requires exactly two host calls: one
  recovery branch and one normal/death branch; the latter selects the death
  owner from the committed snapshot. Unit coverage invokes normal, recovery and
  death owners directly and checks exact counts plus committed revisions.
- `DungeonRenderStatus::recovery_required` is derived from the real runtime
  state at readback, then passed into `HudNoticeState`.
- `DebugOverlayDiagnosticsPlan` is a fixed-value, headless-testable F1 plan. It
  carries budgets, active M/P/H, projectile/hazard saturation and invalid owner
  counts, ground saturation, HUD diagnostics, notice drops, binding revision,
  last event and CJK readiness. `DebugOverlayRenderer` is a persistent
  `CombatRenderer` member and is not invoked when F1 is false.
- HUD fallback selects the default raylib font for drawing whenever complete
  CJK coverage is unavailable; `font_ready()` remains the strict CJK coverage
  signal. The F1 plan renders `CJK READY` or `CJK FALLBACK`.

Final remediation GREEN: `arpg_platform_tests.exe` reported `235 cases, 0
failures`; the focused CTest selection passed 5/5.

## Final review remediation

Final review added one more RED cycle. The initial build failed because the
presentation enum had no sentinel-safe index API, the debug plan omitted room
index overflow, and the fallback mode was not tied to a production font
selection/ownership helper.

- `HudPresentedFrame::count` and `hud_presented_frame_index` reject both the
  sentinel and arbitrary cast values. Invalid owners still perform the one HUD
  observation but cannot increment or read outside the fixed frame-count array.
- The existing host input source test now strips comments and string literals,
  validates exactly two real presentation paths and their order, and self-tests
  comment-only tokens, a third present call, and a seam moved after
  `BeginDrawing`.
- `room_index_overflow` is now carried by the pure debug diagnostics plan and
  rendered alongside ground saturation.
- `HudFontSelectionPlan` is used by both the production draw choice and
  shutdown ownership path: fallback borrows the default font and cannot unload
  it; CJK mode uses and owns the loaded font.

Final RED/GREEN evidence: `arpg_platform_tests.exe` reported `236 cases, 0
failures`; the focused five-test CTest run passed 5/5.

## Owner and invalid-frame final fix

The final RED run proved that an invalid `HudPresentedFrame` still called
`observe_hud` and incremented the observation count. The presentation seam now
validates its index first and returns before any model, notice, layout, revision
or counter mutation for invalid values and the `count` sentinel.

The stripped-source host structure check now verifies owners inside their real
branch regions: recovery passes `HudPresentedFrame::recovery`; the shared
normal/death branch derives its variable from `current.death.has_value()` with
the true `death_overlay` and false `normal` arms, then supplies that variable
to the second seam call. Self mutations reject a recovery-to-normal change, a
death-overlay-to-normal change, and swapped ternary arms.

Final GREEN: `arpg_platform_tests.exe` reported `236 cases, 0 failures`; the
focused CTest selection passed 5/5.
