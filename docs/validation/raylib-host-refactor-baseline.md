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

## Task 9 gate recovery (2026-08-01)

This recovery pass repaired the Stage 10/11 formal paths without changing the
public host CLI, scenario enum, golden image contract, or production game
rules. The Stage 11 fixture now uses the production V9 commit worker and real
double-slot arbitration: normal/deep paths publish revisions A1, B2 and A3;
the abyss path preserves its constructor-owned `abyss_start` pending save and
publishes A1, B2, A3 and B4. After death and continue it stops and destroys the
old worker/storage, initializes fresh storage, verifies the V9 envelope and
A/B winner, compares both durable and room-progress checkpoints, restores the
session, releases the loaded slots, and starts a new worker. Persistence
revision and gameplay commit generation remain separate receipt fields. The
Stage 10 driver retains a stalled target lease through a
bounded local/grid detour, releases a sole melee target only after the global
detour completes or reaches its rejoin limit, and retries that released target
when it becomes resident again. The exact mixed 450-monster hole fixture is
generated by the production room-plan builder on checked, heap-backed nothrow
storage.

Final focused Windows results from
`E:/game/.worktrees/finish-unfinished-branches-20260801`:

| Gate | Result |
| --- | --- |
| `platform.units` | PASS; 544 cases, 0 failures; 14.57 s test time |
| Exact root `408182` hole-only witness | PASS; 450/450 cleared, depth 2, descent transition; temporary diagnostic entry removed afterward |
| CTest #19 `stage10.formal_game.capture_after_present` | PASS; 125.78 s; all seven captures and death/reset/restart/hole semantic checks accepted |
| `stage10.evidence.no_private_injection` | PASS; 17.04 s |
| `stage10.evidence.mutation_self_test` | PASS; 776.63 s, including a real `generated_mask = 0U` rejection witness |
| `platform.host_validation_source` | PASS; 364.96 s |

The evidence guard mask-assignment regex now excludes `==` comparisons while
its mutation suite proves an actual assignment is still rejected. Temporary
hole/death/navigation diagnostics and the hole-only command-line branch have
zero remaining source occurrences.

This focused pass does not claim the Task 9 completion gate: full Debug and
Release suites, current real-window interaction/idle-CPU acceptance, and final
submission remain pending. No file was staged, committed, or pushed.
