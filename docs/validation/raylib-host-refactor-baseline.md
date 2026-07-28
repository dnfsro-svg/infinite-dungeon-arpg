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
