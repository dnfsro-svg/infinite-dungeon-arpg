# Task 1 report — Stage 10 sparse-tail budget rebaseline

Status: **BLOCKED — direct GREEN gate failed**

## Baseline

- HEAD: `e6793c60fb14b3a0c5638dc37bec7510cd8f6aed`.
- Staging: empty.
- Inherited fixture+CMake diff hash:
  `a8accd8782bd98656efb493902a2aba5c65002b4`.
- Preserved recorded RED: exit 8; 48,914 ticks; 562/600 defeated;
  38 remaining; 19.4 seconds; no live lock; sweep active; all 12
  recoveries reacquired.

## Implementation

- Replaced only the checked clear budget calculation with exact
  `12000 + 96 * initial_monster_count` using `uint64_t` arithmetic and the
  existing checked narrowing to `uint32_t`.
- Changed only `stage10.validation_fixture.real_abyss_transactions` timeout
  from 60 to 75 seconds.
- Driver, production code, gameplay, assertions, diagnostics, and callers
  remain unchanged.

## Verification

### Static audit

- `git diff --check`: exit 0.
- Tracked diff names: only the inherited
  `tests/dungeon/stage10_validation_fixture.cpp` and
  `tests/dungeon/CMakeLists.txt`.
- Staging: empty.
- Exact formula shape: confirmed.
- Exact transaction timeout block: confirmed at 75 seconds.
- Forbidden injection pattern matches: zero.
- `drive_clear()` `session.tick(movement)` count: exactly one. The fixture's
  separate environment probe retains its pre-existing tick and is outside the
  driver loop.

### Serial target build

Environment: VS2022 BuildTools x64, Windows SDK `10.0.26100.0`,
`CMAKE_BUILD_PARALLEL_LEVEL=1`.

Command:

```text
cmake --build --preset windows-msvc-debug --target arpg_stage10_validation_fixture --parallel 1
```

Result: exit 0. CMake regenerated after the timeout edit, then compiled and
linked `bin/arpg_stage10_validation_fixture.exe` without a compiler warning.

### Single direct gate

The fixture executable was invoked exactly once after the successful build.

- Exit: 8.
- Wall time: 29.638 seconds.
- Budget: 69,600 ticks (`12000 + 96 * 600`).
- Population: 600 initial, 599 defeated, one remaining.
- Player: HP 760/768, barrier 0, position `(-16.12,3.86859)`, facing left.
- Terminal lock: none; stance reset; `close_for_light=false`;
  `recover_until_light_lane=false`; `sweep_escape=true`; waypoint 4.
- Accepted actions: Storm 26; Draw 105; light 310.
- Lock counters: acquired 388; absent 376; stale geometry 0; movement stall
  12.
- Sweep: 35,172 ticks.
- Light-lane recovery: entries 12; ticks 14,709; reacquired 12.
- Close-facing corrections: 31.
- Potion restored: 192.

Complete terminal diagnostics (the native byte-valued
`lifecycle_pending` field rendered as NUL):

```text
clear failed phase=1 abyss=1 rule=8 lifecycle_pending=<NUL> room=1 hp=760 targets=1 budget=69600 initial_population=600
dense driver failed reason=tick_budget driver_tick=69600 driver_budget=69600 initial_population=600 session_tick=69600 phase=1 initial=600 defeated=599 remaining=1 hp=760 max_hp=768 barrier=0 x=-16.12 y=3.86859 potion_restored=192 lock=none stance_x=0 stance_y=0 stance_facing=1 stance_reached=0 close_for_light=0 melee_chain=1 recover_until_light_lane=0 close_facing_turn_attempted=0 close_facing_corrections=31 stance_distance_squared=0 lock_acquired=388 release_absent=376 release_stale_geometry=0 release_movement_stall=12 sweep_escape=1 sweep_ticks=35172 sweep_waypoint=4 geometry_storm=0 geometry_draw=0 geometry_light=0 locked_x=0 locked_y=0 locked_hp=-1 player_facing=-1 cooldown_draw=0 cooldown_storm=0 active_skill=255 active_attack=255 hurt=0 hit_stop=0 input_size=0 storm_accepted=26 draw_accepted=105 light_queued=310 melee_chain_entries=1 melee_handoffs_after_absent=363 melee_in_lane_acquired=16 melee_nearest_acquired=358 ranged_acquired_after_melee_chain=0 melee_initial_distance_squared=15314.7 light_lane_recovery_entries=12 light_lane_recovery_ticks=14709 light_lane_reacquired=12
__DIRECT_EXIT=8
__DIRECT_ELAPSED_SECONDS=29.638
```

### Gate result

Per the plan, execution stopped immediately after the failed direct gate. The
two exact CTests were not run. The approved formula was not enlarged; no
additional driver, production, or CMake change was attempted; nothing was
staged or committed.

Final read-only preservation check:

- HEAD remains `e6793c60fb14b3a0c5638dc37bec7510cd8f6aed`.
- Staging remains empty.
- Only the inherited fixture+CMake pair is modified.
- Final combined fixture+CMake diff hash:
  `811392e3c7d0e5857f68feb43fbba45e44cbfc97`.
- Final `git diff --check`: exit 0.
