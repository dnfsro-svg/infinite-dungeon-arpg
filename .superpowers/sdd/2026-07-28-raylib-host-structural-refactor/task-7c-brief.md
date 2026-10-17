# Task 7C Presentation, Capture, and Summary Boundary Plan

> **Execution contract:** use subagent-driven development, TDD source guards,
> serial MSVC builds, independent production and guard reviews, and verification
> before commit.

**Goal:** Finish the `HostValidationRuntime` boundary by moving all remaining
validation-only HUD, active-skill draw, ground-loot presentation, completion,
capture-result, and summary state behind the opaque facade without changing
gameplay, rendering, input, screenshot, clean-shutdown, or evidence behavior.

**Task BASE:** `adacc409c16a5ed6e936b614651d588a19c6e0d1`

**Architecture:** `raylib_host.cpp` keeps production input, simulation,
renderer, `BeginDrawing`/`EndDrawing`, screenshot-path arbitration, capture I/O,
and clean-shutdown authority. `HostValidationRuntime` owns validation state and
validation-only decisions. Stage files keep their existing algorithms and
report writers. The temporary `HostValidationStateAccess` shim is removed.

## Hard constraints

- Work only in `E:/game/.worktrees/whole-game-refactor-2026-07` on
  `codex/whole-game-refactor-2026-07`; never modify controller `E:/game`.
- Structural refactor only. Preserve exact gameplay, fixed-step, input,
  death/pause, renderer, ground-loot, capture priority, summary, persistence,
  RNG, audio, and exit behavior.
- Do not delete `host_validation_state.hpp`; retain `HostValidationStates` and
  remove only the temporary accessor type.
- Never edit or stage the tracked progress ledger. Stage only the exact Task 7C
  files after review.
- Before every build or long test, prove there is no second
  `cmake|ctest|ninja|cl|link` chain. Build/test with `-j1`.
- If a wrapper times out while the original CTest PID is alive, monitor that PID
  and its redirected logs; never launch a duplicate selector.
- Do not relabel an observed pre-Task-7C failure as a clean-project BASE proof.

## Corrected file scope (28 files)

### Production (6)

- `src/platform/raylib/host_validation.hpp`
- `src/platform/raylib/host_validation_state.hpp`
- `src/platform/raylib/host_validation_runtime.cpp`
- `src/platform/raylib/host_validation_stage11d.hpp`
- `src/platform/raylib/host_validation_stage11d_report.cpp`
- `src/platform/raylib/raylib_host.cpp`

### Central, input, and test registration guards (6)

- `tests/platform/CMakeLists.txt`
- `tests/platform/host_validation_source_test.cmake`
- `tests/platform/host_validation_sequence_guard_test.cmake`
- `tests/platform/host_validation_sequence_guard_self_test.cmake`
- `tests/platform/host_input_source_test.cmake`
- `tests/platform/input_latency_source_test.cmake`

### Stage owner guards (15)

- `tests/dungeon/stage10_evidence_guard_test.cmake`
- `tests/dungeon/stage10_evidence_guard_self_test.cmake`
- `tests/dungeon/stage11_death_evidence_guard_test.cmake`
- `tests/dungeon/stage11_death_evidence_guard_self_test.cmake`
- `tests/platform/stage11b_architecture_guard_test.cmake`
- `tests/platform/stage11b_settings_evidence_guard_test.cmake`
- `tests/platform/stage11b_settings_evidence_guard_self_test.cmake`
- `tests/platform/stage11c_hud_architecture_guard_test.cmake`
- `tests/platform/stage11c_hud_architecture_guard_self_test.cmake`
- `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`
- `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`
- `tests/platform/stage11d_renderer_integration_guard_test.cmake`
- `tests/platform/stage11d_renderer_integration_guard_self_test.cmake`

### Baseline document (1)

- `docs/validation/raylib-host-refactor-baseline.md`

`tests/platform/stage12_environment_render_tests.cpp` is regression-only: its
renderer HUD/Stage12 ownership stays Host-owned and requires no Task 7C edit.
No new production source is created, so the raylib production CMake list is not
modified. `tests/platform/CMakeLists.txt` sets
`platform.host_validation_sequence_self_test` to `TIMEOUT 2400` because the
BASE self-test already takes about 1479 seconds and Task 7C adds mutations. It
also raises the mutation-bearing Stage11C evidence self-test to 600 seconds and
Stage11D evidence self-test to 1500 seconds. These are bounded metadata changes,
not permission to add unbounded work to the guards.

## Corrected facade contract

Add the required forward declarations and public API:

```cpp
enum class CaptureOwner : std::uint8_t {
    none,
    generic_validation,
    stage17,
};

struct PresentationDecision final {
    bool validation_complete{};
    bool generic_capture_visible{};
    bool generic_capture_complete{};
    CaptureOwner capture_owner{CaptureOwner::none};
    std::optional<std::string> stage17_capture_path{};
};

[[nodiscard]] const PhysicalKeySnapshot&
death_input_snapshot() const noexcept;
void observe_pause_transition(bool was_open, bool is_open) noexcept;
void prepare_hud_snapshot(dungeon::DungeonSnapshot&) noexcept;
void observe_hud(const dungeon::DungeonSnapshot&,
    const HudViewModel&, HudNoticeView, bool draw_debug,
    int screen_width, int screen_height) noexcept;
void observe_active_skill_draw(const dungeon::DungeonSnapshot&,
    const ActiveSkillDrawRuntimeStatus&) noexcept;
void observe_ground_loot(const dungeon::DungeonSnapshot&,
    const PauseMenuState&, const DungeonRenderStatus&,
    settings::LootFilterMode, const GroundLootView&, HudNoticeView,
    const items::ItemOwnershipState*, int screen_width,
    int screen_height) noexcept;
[[nodiscard]] PresentationDecision observe_presented_frame(
    const dungeon::DungeonSnapshot&, const PauseMenuState&,
    bool pause_cjk_ready) noexcept;
void observe_capture_result(CaptureOwner, bool succeeded) noexcept;
void write_summaries(CleanShutdownState,
    const PauseMenuState&) noexcept;
```

`PresentationDecision::generic_capture_complete` exposes the prior successful
generic validation capture. It replaces the Host-local
`stage10_validation_captured` lock and lets Host preserve same-frame clean exit
with:

```text
prior generic complete OR selected generic validation capture succeeded now
```

The decision fields are exact, pre-capture values:

```cpp
validation_complete = stage10_reached || stage11_reached
    || stage11b_reached || stage11c_reached
    || stage11d_reached || stage17_reached;
generic_capture_visible = validation_complete
    || states.stage11d.target_visible
    || stage11b_visible_capture
    || stage11b_paused_visible_capture;
generic_capture_complete = impl_->generic_capture_complete;
```

After the single Stage17 path query, desired `capture_owner` is `stage17` when
that path exists; otherwise it is `generic_validation` only when
`generic_capture_visible && !generic_capture_complete` and the configured
generic capture file exists. Host still selects generic ownership only if no
higher-priority Stage12/material path already occupies the real capture path.

## Exact semantic mapping

### Death input and pre-fixed pause transition

- `death_input_snapshot()` returns the same Stage11D-after/Stage17-before
  cached physical snapshot as the old state shim; the Stage17-injected return
  value must not replace it.
- `observe_pause_transition(pause_was_open, pause_open)` executes at the old
  location after pause state settlement and before `GetFrameTime()` /
  `gate_host_frame()` / any fixed tick. It alone records Stage11B
  `resume_observed` and `resume_ticks_before`.

### HUD and active-skill draw

- `prepare_hud_snapshot()` runs after `audio.update()` and before
  `presented_snapshot = current`. It performs only the existing Stage11C
  low-health barrier fixture mutation on the current presentation copy.
- `observe_hud()` runs immediately after the real
  `renderer.observe_presented_hud_frame()` and before `BeginDrawing()`. It
  records the real HUD model, notice view, production snapshot hash, full HUD
  layout, target frames, and current debug visibility. The debug value is
  synchronized every frame so the next frame's injector sees the same state.
- `observe_active_skill_draw()` receives `presented_snapshot` and the real
  `renderer.active_skill_draw_status()` immediately after the real renderer
  draw and before overlays/capture.

### Ground-loot and presented-frame phases

The BASE has two distinct positions and they remain distinct:

1. `observe_ground_loot()` stays at the existing Stage11D seam: after the real
   `GroundLootView`, the Stage17 draw observer, and the Stage12 material runtime
   status block, but before passive/inventory/pause overlays. It receives a
   fresh real `renderer.hud_notice_view()` and performs Stage11D target
   visibility plus, while visible and uncaptured, semantic recording. It must
   not cache a previous-frame notice or rebuild a second GroundLootView.
2. `observe_presented_frame()` runs after passive/inventory/pause overlays and
   before capture-path selection and `EndDrawing()`.

This split preserves the BASE order instead of moving Stage11D observation
across overlay drawing. Within `observe_presented_frame()` preserve this order:

1. Stage11B resume-after, recovery notice, paused tick/hash observations.
2. Stage10 and Stage11 target checks, Stage11 stable-frame increment/reset,
   and Stage10 chaos-only increment (no new reset).
3. Stage10/11/11B/11C/11D/17 completion values.
4. Stage11B and Stage11D visible-capture conditions.
5. Exactly one `stage17_capture_path()` call per ordinary presented frame; its
   frame-counter side effect must not be duplicated or skipped.

`pause_cjk_ready` is true only when the pause menu was actually drawn this
frame and its CJK font is ready. Stage12 background/icons-only frames pass
false.

Change `stage11d_target_visible()` to accept explicit screen width/height and
replace its internal `GetScreenWidth/Height()` calls with those exact values.
Do not duplicate its layout algorithm in the facade.

### Capture arbitration and result

- Host remains the only owner of capture paths, `EndDrawing()`, image export,
  Stage12 baseline/material captures, manual screenshots, and periodic capture.
- `capture_owner` in the decision is a desired validation owner. Host records a
  selected validation owner only if that validation path wins the existing
  arbitration point.
- Stage17 path remains first. If Stage17 requested a capture, a later manual
  screenshot path override still reports Stage17 success exactly as before.
- Stage12/material/pre-existing paths prevent selecting generic ownership.
- When a generic validation path was selected and a manual screenshot later
  overrides it, retain `generic_validation` for the result callback but pass
  `succeeded=false`. This preserves the old Stage11B paused-capture latch while
  preventing the manual image from marking Stage10/11C/11D captured.
- `observe_capture_result(generic_validation, ...)` always performs the exact
  BASE assignment
  `pause_capture_while_paused = pending_stage11b_paused_visible_capture`;
  this is assignment, not OR-latching. Only success marks global generic
  completion and the pending Stage11C/Stage11D targets captured.
- `observe_capture_result(stage17, true)` alone marks Stage17 capture complete.
- `none` changes no validation state.
- On each ordinary presentation path, the callback runs exactly once after
  `present_frame_and_maybe_capture()` / `EndDrawing()`, using the same call's
  effective validation result. The separate recovery presentation path remains
  validation-observer-free exactly as BASE.
- The Host's same-frame exit formula is exact:

  ```cpp
  const bool effective_generic_complete =
      decision.generic_capture_complete
      || selected_generic_capture_succeeded_now;
  if (decision.validation_complete
          && (!config.validation_capture_file.has_value()
              || effective_generic_complete)) {
      begin_clean_exit();
  }
  ```

  A Stage11D target is not complete until its capture result has been observed,
  so its capture frame still has `decision.validation_complete == false` and it
  exits on the following frame exactly as BASE. Manual override or export
  failure blocks Stage10/11C/11D captured-state promotion while the Stage11B
  paused flag still receives the exact pending-value assignment above.

### Summary

`write_summaries()` runs after clean-shutdown state is final and before audio,
renderer, pause renderer, and window shutdown. Its exact order is:

1. set Stage17 `clean_shutdown_exact_ready` from `CleanShutdownState::ready`;
2. Stage11B summary;
3. Stage11C summary;
4. Stage11D summary;
5. Stage17 summary.

After migration, `raylib_host.cpp` includes only the public facade for
validation. It has zero `HostValidationStateAccess`, zero Stage state
type/field access, zero direct `inject_stage*`, zero direct Stage presentation
observer/capture/report call, and zero direct `write_stage*_validation_summary`.

## TDD execution order

### Step 0: freeze BASE and test metadata

- Record BASE line counts and legacy symbol counts in the task report.
- Save the corrected final regex's exact 52 test names as a manifest; compare
  names and per-test results, never only total counts.
- Reuse Task 7B's completed 38-test baseline and run the 14-test addendum before
  production edits. First configure and serially relink the three executable
  targets used by this addendum at `adacc40`:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --build --preset windows-msvc-debug --target arpg_stage11c_hud_stress arpg_stage11c_hud_formal arpg_stage17_skill_stones_game_validation -- -j1
$baseAddendumRx = '^(platform\.(host_input_source|input_latency_source)|stage11c\..*|stage17\..*)$'
ctest --test-dir out/build/windows-msvc-debug -R $baseAddendumRx --output-on-failure -j1 --no-tests=error
```

Record actual results; do not predeclare a pass/fail count. Compare Stage11C
scenario summaries/evidence, not only process exit codes.

### Step 1: central/input RED

Update source, input, latency, sequence, and sequence-self guards to require the
new exact declarations/definitions and Host data flow. Require active lexical
definitions in the real namespace and method; prohibit comment/string/raw,
inactive preprocessor, lambda, dead-code, duplicate, wrong argument, discarded
decision, fabricated owner/result, and cross-file-position decoys.

Confirm each direct guard is RED for the expected missing Task 7C seam before
production edits. Record the BASE durations that justify all three bounded
timeout metadata changes (sequence 2400, Stage11C evidence 600, Stage11D
evidence 1500 seconds).

### Step 2: Stage owner RED

Retarget each Stage guard and self-test from direct Host state ownership to the
real facade method that owns the behavior:

- Stage10/11: target frames, thresholds, completion, capture result.
- Stage11B: pre-fixed resume, paused presentation/capture, summary.
- Stage11C: presentation-copy fixture, true HUD model/notices/layout/hash,
  four-frame completion, post-present capture, summary.
- Stage11D: true shared GroundLootView and notices, semantics, completion,
  capture, summary, explicit dimensions.

Run each direct guard separately and record its expected RED diagnostic. Do not
run long mutation self-tests until production is GREEN.

The Task 7C adversarial inventory is fixed at these 25 named mutation families.
Every independently listed slash/or/comma variant within a family must be
applied at least once to the real owner file by the corresponding central or
Stage self-test, must fail, and must match its dedicated diagnostic. One family
may therefore map to multiple concrete cases. Record the family-to-case-to-
self-test mapping and the final concrete case count in the report:

1. `T7C-M01`: ground-loot observer before the Stage17 draw observer.
2. `T7C-M02`: ground-loot observer before Stage12 runtime-status collection.
3. `T7C-M03`: ground-loot observer after passive/inventory/pause overlays.
4. `T7C-M04`: rebuild/pass a second `GroundLootView`.
5. `T7C-M05`: pass cached/previous-frame HUD notices.
6. `T7C-M06`: pass constants, swapped values, or global calls instead of the
   real screen dimensions.
7. `T7C-M07`: move pause-transition observation after `GetFrameTime`, frame
   gate, or a fixed tick.
8. `T7C-M08`: call HUD observation before the real renderer HUD observation.
9. `T7C-M09`: call HUD observation after `BeginDrawing`.
10. `T7C-M10`: pass fabricated HUD model/notices arguments or corrupt the
    facade's internal authoritative layout/hash computation.
11. `T7C-M11`: call active-skill observation before the real renderer draw.
12. `T7C-M12`: pass fabricated or stale active-skill draw status.
13. `T7C-M13`: call the Stage17 path query twice.
14. `T7C-M14`: skip the Stage17 path query on an ordinary frame.
15. `T7C-M15`: select generic ownership when a Stage12/material path already
    occupies the capture path.
16. `T7C-M16`: lose Stage17 ownership when a manual screenshot overrides its
    path.
17. `T7C-M17`: report generic success when a manual screenshot overrides the
    selected generic path.
18. `T7C-M18`: call capture-result observation before `EndDrawing`.
19. `T7C-M19`: omit the ordinary-frame capture-result callback.
20. `T7C-M20`: call the ordinary-frame capture-result callback twice.
21. `T7C-M21`: pass fabricated/constant owner or success values.
22. `T7C-M22`: OR-latch `pause_capture_while_paused` instead of assigning the
    pending paused-visible value.
23. `T7C-M23`: omit current-frame selected generic success from the clean-exit
    predicate, delaying exit.
24. `T7C-M24`: bypass `decision.validation_complete`, causing premature
    Stage11D exit or unrelated capture-driven exit.
25. `T7C-M25`: reorder, duplicate, omit, or move the 11B/11C/11D/17 summary
    chain past resource shutdown.

Existing harmless-decoy inventories remain required. A mutation implemented as
comment/string/raw text, inactive preprocessor code, uncalled lambda, dead code,
or cross-file concatenation does not satisfy an ID.

To keep the central sequence self-test bounded under its 2400-second timeout,
its Task 7C additions are exactly eight concrete cases: M11; both M12
variants; M13; M14; M16; and the M25 reorder and post-shutdown variants.
Distribute the remaining concrete cases to the real Stage owners:

- Stage11B evidence self-test: all M07 variants and M22.
- Stage11C architecture/evidence self-tests: M08, M09, all M10 variants, M20,
  and the M25 duplicate-summary variant.
- Stage11D loot/renderer self-tests: M01, M02, all three M03 overlay variants,
  all M04, M05, and M06 variants, M19, M24, and the M25 omitted-summary
  variant.
- Stage10 evidence self-test: all M15 variants, M17, M18, all M21 variants,
  and M23.

If measured runtime still approaches a configured timeout, optimize mutation
setup reuse without dropping cases; do not silently raise timeouts or coverage.

### Step 3: production migration

Implement the facade contract and remove `HostValidationStateAccess`. Preserve
the exact call locations, branch reachability, capture precedence, and
same-frame exit semantics above. Replace the Host's fixed-step Stage11D helper
call only with the equivalent public config enum comparison; do not expose a
new private Stage header.

### Step 4: focused GREEN and adversarial self-tests

Configure, build all targets needed by the 52-test selector serially, then run
the 30 focused guard tests with stop-on-failure. Because their BASE aggregate
already approaches one hour, run this selector as one hidden redirected process
with a unique PID and the same no-relaunch/liveness/log-completeness rules as
the final selector. Run all affected mutation self-tests only against their real
owner files.

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests arpg_dungeon_tests arpg_settings_stress_tests arpg_stage10_validation_fixture arpg_stage10_validation_game arpg_stage10_formal_game_validation arpg_stage11_death_validation_fixture arpg_stage11_death_formal_game_validation arpg_stage11b_settings_formal arpg_stage11c_hud_stress arpg_stage11c_hud_formal arpg_stage11d_loot_formal arpg_stage17_skill_stones_game_validation -- -j1
$focusedRx = '^(platform\.(units|host_input_source|input_latency_source|host_validation_source|host_validation_sequence|host_validation_sequence_self_test)|stage10\.evidence\..*|stage11\.(architecture\.death_boundaries|death_evidence\..*)|stage11b\.(architecture\.settings_boundaries|settings_evidence_guard|settings_evidence_guard_self_test)|stage11c\.(architecture\.hud_boundaries|architecture\.hud_boundaries_self_test|hud_evidence_guard|hud_evidence_guard_self_test)|stage11d\.(architecture\.loot_boundaries|architecture\.loot_boundaries_self_test|renderer_integration_guard|renderer_integration_guard_self_test|loot_evidence_guard|loot_evidence_guard_self_test))$'
$focusedOut = '.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task7c-focused.out.log'
$focusedErr = '.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task7c-focused.err.log'
$focused = Start-Process -FilePath (Get-Command ctest).Source -ArgumentList @('--test-dir','out/build/windows-msvc-debug','-R',$focusedRx,'--output-on-failure','-j1','--stop-on-failure','--no-tests=error') -WorkingDirectory (Get-Location).Path -RedirectStandardOutput $focusedOut -RedirectStandardError $focusedErr -WindowStyle Hidden -PassThru
$focused.Id
```

### Step 5: full 52-test regression

Run one hidden background/redirected CTest process so an outer one-hour wrapper
cannot terminate or cause relaunch. Record its unique PID and stdout/stderr log
paths, verify PID liveness while waiting, and never restart while it remains
alive. After exit, verify the CTest summary plus `LastTest.log` and
`LastTestsFailed.log` are complete before comparison against the exact BASE
name/result manifest.

```powershell
$finalRx = '^(platform\.units|platform\.host_validation_(source|sequence)|platform\.(host_input_source|input_latency_source)|stage10\.|stage11\.|stage11b\.|stage11c\.|stage11d\.|stage17\.)'
$outLog = '.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task7c-final.out.log'
$errLog = '.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task7c-final.err.log'
$ctest = Start-Process -FilePath (Get-Command ctest).Source -ArgumentList @('--test-dir','out/build/windows-msvc-debug','-R',$finalRx,'--output-on-failure','-j1','--no-tests=error') -WorkingDirectory (Get-Location).Path -RedirectStandardOutput $outLog -RedirectStandardError $errLog -WindowStyle Hidden -PassThru
$ctest.Id
```

Expected acceptance is no new regression relative to the recorded Task 7C BASE
and addendum. Known categories are reported separately: observed pre-Task-7C
Stage10/11 failures, detached-BASE-proven Stage11B/Stage11C/Stage11D failures,
and dependent fixture skips.

### Step 6: two independent reviews, metrics, exact commit

- Review A: production behavior, death input, pre-fixed pause timing, true HUD
  and GroundLootView data, capture owner/override behavior, same-frame exit,
  cleanup and summaries.
- Review B: guard scope and adversarial mutations; reject legacy fixtures,
  comment/inactive/lambda/dead-code/cross-file position bypasses.
- Required gate: Critical 0, Important 0. Fix findings, rerun affected gates,
  and perform a final snapshot re-review.
- Update the baseline document with BASE/final physical and nonblank line counts,
  `run_raylib_host` size, facade/runtime size, and zero legacy-symbol counts.
- Write ignored `task-7c-report.md` with RED/GREEN, build, test, BASE comparison,
  review, metrics, and resource evidence.
- Run `git diff --check`, stage the exact reviewed files one-by-one, inspect
  cached names/diff/check, and commit:

```text
refactor: complete host validation boundary
```

The progress ledger remains the only intended tracked unstaged file after the
commit.
