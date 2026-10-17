# Raylib host structural-refactor acceptance baseline

Captured before Task 0 test-only changes, from revision
`3b3ac0a58d1037505a36d3cccd791a241e250d8d` (the Task 0 review base).
The worktree was clean at capture time.

## Host scale and state ownership

- `src/platform/raylib/raylib_host.cpp`: 4,135 lines.
- The six validation-state definitions are `Stage10ValidationState` (328-333),
  `Stage11ValidationState` (335-340), `Stage11BValidationState` (342-361),
  `Stage11CHudValidationState` (363-373), `Stage11DLootValidationState`
  (376-430), and `Stage17SkillStonesValidationState` (483-552).
- `HostValidationStates` owns the Stage 10 through Stage 11D values at
  433-439. `run_raylib_host` binds the current Stage 11C runtime reference at
  3166-3167; it is not an independent local value.

## Stage validation function regions

| Stage | Host region |
| --- | --- |
| Stage 17 | draw/runtime and validation support 554-1539; physical injection begins 898; summary begins 1343 |
| Stage 11B | physical injection 1541-1602; completion/hash/summary 1604-1691 |
| Stage 11C | physical binding/injection 1857-1935; capture hash/reached/summary 2145-2297 |
| Stage 11D | physical injection begins 1937; evidence semantics begin 2300; summary begins 2495 |
| Stage 10 | fixed-step validation input begins 2625 |
| Stage 11 | fixed-step validation input begins 2735 |

`run_raylib_host` spans 3035-4124. Its relevant call sites are:

- state-reference binding: 3159-3176;
- physical-input chain and host-frame mapping: 3250-3276;
- fixed-step validation movement priority: 3612-3622;
- Stage 11C production capture assignments: 3731-3747 and 4073-4084;
- ordered validation-summary writes: 4104-4114.

## Frozen acceptance order

After comments are removed, the host loop must contain one instance of each
token in these orders:

1. `sample_physical_keys()` → `inject_stage11b_physical_edges(` →
   `inject_stage11c_physical_edges(` → `inject_stage11d_physical_edges(` →
   `inject_stage17_physical_edges(` → `map_host_frame_input(`.
2. `stage11_validation_input(` → `stage10_validation_input(` →
   `step_movement = movement;` in the fixed-step if/else-if/else priority.
3. `write_stage11b_validation_summary(` →
   `write_stage11c_hud_validation_summary(` →
   `write_stage11d_loot_validation_summary(` →
   `write_stage17_validation_summary(`.

## Existing Stage11C guard drift

Before this task, the command below failed with the known baseline error:

```powershell
cmake "-DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07" -P tests/platform/stage11c_hud_evidence_guard_test.cmake
```

```text
Stage11C evidence guard cannot bind complete host capture surface
```

The failure was caused by the guard's removed-text anchor
`Stage11CHudValidationState stage11c_validation_state{};`. The actual runtime
binding is the `Stage11CHudValidationState&` alias above. It predates this
structural-refactor baseline and is not attributed to the refactor.

## Task 7C completion metrics

Task 7C uses `adacc409c16a5ed6e936b614651d588a19c6e0d1` as its frozen BASE.
The final-candidate counts below were measured from the reviewed Task 7C
worktree before staging. Physical counts include blank lines; nonblank counts
exclude whitespace-only lines.

| Surface | BASE physical | Final physical | BASE nonblank | Final nonblank |
| --- | ---: | ---: | ---: | ---: |
| Six Task 7C production files | 2,317 | 2,360 | 2,210 | 2,245 |
| `run_raylib_host` | 1,039 | 898 | 1,032 | 891 |
| Public facade plus runtime implementation | 277 | 512 | 238 | 462 |

The six-file surface comprises `host_validation.hpp`,
`host_validation_state.hpp`, `host_validation_runtime.cpp`,
`host_validation_stage11d.hpp`, `host_validation_stage11d_report.cpp`, and
`raylib_host.cpp`. The facade/runtime row comprises `host_validation.hpp` and
`host_validation_runtime.cpp`.

The final Host has zero occurrences of `HostValidationStateAccess`, all six
private Stage validation-state type names, direct `inject_stage*` or
`observe_stage*` calls, and direct Stage validation-summary writers. Its only
validation include is the public `host_validation.hpp` facade.

## Task 8C window lifetime boundary

Task 8C adds `HostWindowLifetime`, with a replaceable `HostWindowBackend`, as
the only owner of the `SetConfigFlags` → `InitWindow` → `IsWindowReady`
transaction and of `CloseWindow`.  `run_raylib_host` creates one lifetime
object before its `try` block, explicitly closes it only after audio, combat
renderer, and pause renderer shutdown, and relies on the destructor for all
exceptional or early-return paths.  Its post-ready `ChangeDirectory`,
`SetWindowMinSize`, `SetExitKey`, and validation foreground policy remain in
the Host immediately after successful initialization.

The Host remains above the 1,200-line target after this focused extraction.
That remaining surface is intentional coordination rather than a mechanical
split: startup owns save/settings/runtime and validation-facade construction;
the main loop retains its frozen input, fixed-step, persistence, event-drain,
and clean-exit transaction; and the presentation section retains draw order,
capture ownership, evidence observation, and audio behavior.  These blocks
cross the public runtime and renderer contracts, so moving them merely to
reduce the line count would violate the structural-refactor behavior freeze.

## Task 9 full-validation result (2026-07-30)

Task 9 validated local revision
`ba1c93f4fab37108d2da079cb14ab1dc748aa741`.  The result is **BLOCKED**:
both configurations build, but neither full CTest run is green and the real
window checklist is only partially proven.  No golden value, timeout, expected
image, or production implementation was changed to hide these results.

### Production metrics

The tracked `src/platform/raylib/*.{cpp,hpp}` surface changed from 25,136 lines
in the frozen Task 0 review base to 26,033 lines at the validated revision
(+897).  `raylib_host.cpp` changed from 4,135 to 1,240 lines (-2,895).  The
largest current file in that surface is `raylib_host.cpp` at 1,240 lines,
followed by `inventory_renderer.cpp` at 1,222 lines.

Files added to that surface since the frozen base, with physical line counts:

| File | Lines |
| --- | ---: |
| `host_frame_gate.cpp` | 19 |
| `host_settings_runtime.cpp` / `.hpp` | 156 / 32 |
| `host_validation.hpp` | 114 |
| `host_validation_input.cpp` / `.hpp` | 49 / 29 |
| `host_validation_navigation.cpp` / `.hpp` | 111 / 29 |
| `host_validation_runtime.cpp` | 400 |
| `host_validation_stage10_11.cpp` / `.hpp` | 260 / 45 |
| `host_validation_stage11b.cpp` / `.hpp` | 170 / 52 |
| `host_validation_stage11c.cpp` / `.hpp` | 225 / 49 |
| `host_validation_stage11d.hpp` / report / runtime | 108 / 333 / 427 |
| `host_validation_stage17.hpp` / report / runtime | 169 / 329 / 585 |
| `host_validation_state.hpp` | 20 |
| `host_window_lifetime.cpp` / `.hpp` | 41 / 40 |

The Host still has no private Stage validation-state type or direct
`inject_stage*`, `observe_stage*`, or Stage summary-writer use; it coordinates
validation through `HostValidationRuntime`.

### Build and full-test gates

- Debug full build: PASS, 134 actions, 146.837 s.  The first attempt failed in
  0.575 s because the shell did not have the Visual Studio include environment
  (`C1083: cstddef`); loading `Launch-VsDevShell.ps1` resolved the environment
  without a source change.
- Debug full CTest: 124 total, 99 passed, 19 executed failures, 6 fixture-
  dependent not-run tests, 25 non-passing total, 7,550.84 s.  Two tests timed
  out (`stage16.loot_reinforcement.simulation`, 60 s;
  `stage11.death_formal.five_paths`, 300 s) and two validation fixtures
  terminated with access violations.
- Release configure/build: PASS, 8.394 s / 420 actions in 588.247 s.
- Release full CTest: 124 total, 98 passed, 20 executed failures, 6 fixture-
  dependent not-run tests, 26 non-passing total, 7,260.12 s.  It has the same
  timeout/access-violation pattern and additionally fails `dungeon.units` at
  the queue-overflow diagnostic.

The current run reproduces previously recorded failures including the
Stage10/11 fixture/formal group, `stage11b.settings_formal`,
`stage11d.loot_formal`, and `architecture.persistence_checkpoint_only`.
However, the full gate contains additional non-passing tests and therefore is
not accepted merely as baseline drift.  Notably, the persistence checkpoint
failure was already proved at the Task 1 review base, and the Stage11B formal
pause/resume failure was already proved in Tasks 3/7A; that attribution does
not turn the current full suite green.

### Formal evidence comparison

The source/sequence guards for the validation facade and Stage11B/11C/11D
evidence all passed in Debug, so ordered production tokens and report writers
remain bound.  The formal artifacts themselves are incomplete:

| Stage | Task 9 artifact/validator result |
| --- | --- |
| 10 | Formal evidence directory empty; formal capture test failed. |
| 11 | Only `01-normal-death.png` (1280x720) was produced; the five-path formal test timed out and no complete summary exists. |
| 11B | Five 1280x720 PNGs exist, but `swap.png` and `settings.png` are absent; aggregate summary ends `result=fail`; formal failed and validator was not run. |
| 11C | Three of six 1280x720 PNGs exist; `cleared`, `abyss-warning`, and `level-up` images are absent and their fields are invalid/zero; summary ends `result=fail`; validator chain did not run. |
| 11D | Formal evidence directory is empty; formal failed and validator was not run. |
| 17 | Real-raylib, validator, and validator self-test passed.  Seven fresh 1280x720 PNGs and ordered V3 state fields were accepted.  The checked-in validation note still describes an older five-file/23-field contract and needs a separate evidence-documentation reconciliation, not a golden rewrite. |

### Real-window acceptance

The Release game was launched as one visible window with seed `0x8` and an
isolated save/settings/screenshot directory.  The capture API returned a white
OpenGL surface, so every visual claim below uses the game's own post-present
F12 PNG instead.  HUD/Chinese font, pause, the complete Settings page, the
three-column Inventory page, death recap, `E` continue, and same-directory
restart/loading were visibly proven.  Door artwork was visible, but clearing
the 400-monster room and interacting with doors/the next-floor hole was not
achieved.  Movement, light attack, jump, the 0.5-second launch state, two active
skills, and pickup were injected but could not be independently identified in
the dense combat images, so they remain inconclusive rather than passed.

During a confirmed paused screen, a two-second sample consumed 3.047 CPU
seconds (about 154% of one logical core), with 671.29 MiB working set and
611.23 MiB private bytes.  This is an idle/high-CPU concern.  Both game runs
were then closed through the in-game confirmation UI; process readback found
zero `arpg_game` processes and no compiler/build/test processes remained.

Because the full tests, evidence matrix, real-window checklist, paused CPU
criterion, and required zero-finding dual review are not all green, Task 9 did
not commit or push this documentation update.  The two independent reviews
reported, after deduplication, P0=0, P1=3, P2=2.  Their P1 findings cover the
two Task8C-stale input/source guards, the Stage11C architecture self-test's
wrong-reason failure after the shared scanner diagnostic change, and the red
full gate.  Their P2 findings cover
the nullable/throwing `HostWindowBackend` `noexcept` precondition and missing
explicit renderer/material cleanup if allocation throws after resource
initialization.  The detailed evidence and source locations are retained in
the Task 9 report.
