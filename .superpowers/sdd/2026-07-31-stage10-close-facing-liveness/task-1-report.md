# Task 1 report — Stage 10 close-facing liveness

Status: **BLOCKED (direct gate remained RED; stopped before CTest)**

## Preserved baseline

- HEAD: `feadac993c32a59428dc5de705d974a50824f9b0` (`feadac9`).
- Staging area: empty.
- Inherited fixture+CMake diff hash before this card:
  `0e0846cdaf04a0eccb540b6396376a08aec0556c` (exact match).
- Recorded RED was retained without rerunning unchanged code:
  `exit 8; budget 48,914; 486/600 defeated`, with the live close lock at
  player `(-32.9425,81.0109)` facing left and target
  `(-32.4962,81.2404)`, `geometry_light=false`, empty action/skill/input,
  and `sweep_escape=false`.

## Minimal implementation

Only `tests/dungeon/stage10_validation_fixture.cpp` was newly changed by this
card.

- Added per-lock `close_facing_turn_attempted`, reset by both target acquire
  and release, plus failure-only `close_facing_corrections` diagnostics.
- Recognized a zero-movement, controllable, close, facing-away target outside
  the existing light lane using the specified bounds.
- Preserved Storm -> Draw -> light action priority. Only an action-ready tick
  with no requested action may send one public horizontal movement input.
- A second candidate on the same lock uses the existing movement-stall release
  accounting, excludes that ordinal once, enters existing light-lane recovery,
  and immediately uses deterministic sweep movement.
- The correction tick does not arm ordinary close-movement progress.
- No CMake, budget, timeout, production, population, threshold, reward, save,
  or evidence-semantic change was made in this card.

## Static audit

- `git diff --check`: exit 0.
- Staging area remained empty.
- Forbidden injection search
  (`force_defeat|defeat_monster|#define private|reinterpret_cast|const_cast`):
  no matches.
- `drive_clear()` contains exactly one `session.tick(movement)`.
- Current inherited-plus-card hashes:
  - combined fixture+CMake diff:
    `a8accd8782bd98656efb493902a2aba5c65002b4`;
  - fixture diff: `26473647626280e81adaa3303e36b1849e6311ec`;
  - unchanged inherited CMake diff:
    `d7328a662189e4ee5747bbb8c0c68357f628df5c`.

## Serial build

VS2022 BuildTools x64 environment, SDK override
`WindowsSDKVersion=10.0.26100.0\`, and
`CMAKE_BUILD_PARALLEL_LEVEL=1` were used.

The first target-only serial build exited 0 but reported C4244 for the local
correction direction. The local type was narrowed to `std::int8_t`, then the
same target-only serial build was repeated:

```text
[1/2] Building CXX object ...stage10_validation_fixture.cpp.obj
[2/2] Linking CXX executable bin\arpg_stage10_validation_fixture.exe
```

Final build exit: 0, with no compiler warning.

## One direct gate

The fixture executable was run exactly once after the final build.

- Exit: 8.
- Wall time: 19.4 seconds.
- Budget: 48,914 ticks.
- Population: 600 initial, 562 defeated, 38 remaining.
- Player: HP 760/768, barrier 0, position `(24.7048,4.13412)`, facing right.
- Lock terminal: none; stance reset; `close_for_light=false`;
  `recover_until_light_lane=false`; `sweep_escape=true`; waypoint 5.
- Geometry terminal: Storm/Draw/light = `0/0/0`; no locked target.
- Input/action terminal: input size 0, active skill/attack both `none` enum
  values (255), hurt 0, hit-stop 0.
- Lock counters: acquired 361; absent 349; stale geometry 0; movement stall 12.
- Sweep: 17,278 ticks.
- Accepted actions: Storm 24; Draw 98; light 288.
- Melee: chain entries 1; handoffs after absent 336; in-lane acquisitions 16;
  nearest acquisitions 331; ranged after chain 0; accumulated initial distance
  squared 13,181.
- Light-lane recovery: entries 12; ticks 14,709; reacquired 12.
- Close-facing corrections: 29; per-lock attempted flag false at terminal.
- Cooldowns: Draw 57; Storm 1,257.
- Potion restored: 192.

The former fixed-point signature is absent: the terminal state has no live
lock, no locked close target, and `sweep_escape=true`. The correction increased
same-budget progress from 486 to 562 defeats, but full clear still did not fit
the unchanged budget.

Per the card gate, no CTest was run and no commit/staging was performed after
direct exit 8. No budget or timeout adjustment was attempted.
