# Stage 10 Sweep Waypoint Recovery — Task 1 Report

## Status

BLOCKED after direct round 1. The prescribed sweep-waypoint transition compiled
and removed the recorded waypoint-0 zero-input deadlock, but the unchanged
12,000-tick budget still expired at 144/600 defeated. CTest was not run and
nothing was staged or committed because native direct GREEN is mandatory.

## Protected baseline

- Worktree: `E:\game\.worktrees\task9-gate-recovery`
- Branch: `codex/task9-gate-recovery`
- Committed HEAD: `f60767722bfa0a54922bfe874b67db42adbfeffc`
- Initial staging area: empty
- Initial tracked dirty scope: only
  `tests/dungeon/stage10_validation_fixture.cpp`
- Preserved pre-card fixture diff hash:
  `3af6eec94e088467368c72c250c44c3c4447340f`
- Established RED was consumed without rerunning it: native exit 8 at tick
  12,000, 119/600 defeated, 481 remaining, sweep escape active for 3,599
  ticks at waypoint 0, player at `(-76.7703,-60.7451)`.

## Implementation

In `sweep_movement()` only:

1. Called the existing `movement_toward(player, target)` for the current
   waypoint.
2. Returned any nonzero movement unchanged.
3. On `(0,0)` only, advanced `state.waypoint` exactly once modulo the existing
   waypoint count, recomputed the target, and returned the existing movement
   helper's result.
4. Removed the separate 0.25-unit sweep arrival branch.

No loop, movement-helper change, route change, lease/action change, or direct
combat/state mutation was added.

## Static audit before execution

Commands:

```powershell
git diff --check
git diff -- tests/dungeon/stage10_validation_fixture.cpp
rg -n "relay_defeated\s*\(|events_\.try_push\s*\(" `
  tests/dungeon/stage10_validation_fixture.cpp
```

- `git diff --check`: exit 0, with only Git's existing LF-to-CRLF notice.
- Forbidden injection search: no matches.
- `drive_clear()` contains exactly one `session.tick(movement)`.
- The revised `sweep_movement()` contains no positional arrival threshold.
- Added card lines contain only the existing movement-helper call, nonzero
  return, one modulo waypoint increment, target recomputation, and final
  helper call.

## VS2022 x64 configure and build

Developer PowerShell setup and exact commands:

```powershell
$vswhere = Join-Path ${env:ProgramFiles(x86)} `
  'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = (& $vswhere -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath | Select-Object -First 1)
Import-Module (Join-Path $vs `
  'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation `
  -DevCmdArguments '-arch=x64 -host_arch=x64'
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage10_validation_fixture -- -j1
```

- Visual Studio: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`
- `VSCMD_ARG_TGT_ARCH=x64`
- Configure: exit 0, 7,684 ms.
- Target-only serial build: exit 0, 3,140 ms.

## Direct rounds

### Round 1 — prescribed sweep transition

Exact command:

```powershell
$process = Start-Process `
  'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe' `
  -Wait -PassThru -NoNewWindow
$process.ExitCode
```

- Native exit: 8
- Elapsed: 3,047 ms
- Terminal evidence:

  ```text
  clear failed phase=1 abyss=1 rule=8 lifecycle_pending=0 room=1 hp=768 targets=456
  dense driver failed reason=tick_budget driver_tick=12000 session_tick=12000 phase=1 initial=600 defeated=144 remaining=456 hp=768 max_hp=768 barrier=51 x=36.6444 y=-47.3234 potion_restored=0 lock=142 stance_x=43.7442 stance_y=-47.5059 stance_facing=1 stance_reached=0 close_for_light=0 stance_distance_squared=50.4409 lock_acquired=75 release_absent=68 release_stale_geometry=0 release_movement_stall=6 sweep_escape=0 sweep_ticks=1623 sweep_waypoint=1 geometry_storm=0 geometry_draw=0 geometry_light=0 locked_x=48.7442 locked_y=-47.5004 locked_hp=110 player_facing=1 cooldown_draw=0 cooldown_storm=1681 active_skill=1 active_attack=255 hurt=0 hit_stop=0 input_size=0 storm_accepted=7 draw_accepted=35 light_queued=51
  ```

Diagnosis: the original deterministic sweep deadlock is no longer present.
The driver advanced beyond waypoint 0, left sweep escape, and reacquired real
targets 75 times. The validation still exhausted the fixed budget with 456
targets remaining. The observed remaining failure is outside this card's
single authorized sweep-arrival change; further changes would require touching
the protected lease/action/throughput contract or another new root-cause card.
No evidence-driven in-scope round-2 change exists, so the same deterministic
binary was not rerun.

## CTest and commit gates

- CTest: not run because direct exit was nonzero.
- Staging area after the failed direct run: empty.
- Commit: none.
- Required commit message `test: recover stage10 sweep waypoint` was not used.

## Final scope audit

- `git diff --check`: exit 0.
- Post-card fixture diff hash:
  `8e80991684cab8297d072a5b23e0504f36d51f78`
- Tracked dirty scope remains only
  `tests/dungeon/stage10_validation_fixture.cpp`.
- The two superseded untracked Stage 10 plans remain untracked and unstaged.
- The new sweep-waypoint plan also remains untracked and unstaged because the
  direct GREEN gate was not met.
- No production, Stage 9, route/order/rows, threshold, density, bounds,
  golden, budget, timeout, assertion, Stage B-G, lease, or action file was
  edited, staged, or committed.

## Concerns / next card

The sweep-waypoint contract repair is locally evidenced by the changed terminal
state, but it is insufficient to make the registered validation GREEN. A new
root-cause/design card must explain why real combat throughput remains at
144/600 within the immutable 12,000-tick budget before authorizing any further
fixture change. The current uncommitted fixture contains both the inherited
event-lease implementation and this card's sweep repair and must be preserved.
