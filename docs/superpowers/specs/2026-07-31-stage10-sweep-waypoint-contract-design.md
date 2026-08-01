# Stage 10 Sweep Waypoint Contract Design

## Scope

Repair only the Stage 10 standalone validation fixture's five-lane sweep
waypoint transition. Preserve the event lease, close-pursuit fallback,
equipment, production combat/streaming/save paths, assertions, 12,000-tick
budget, and registered timeout.

## Root cause

The round-5 fixture reaches sweep waypoint 0 at approximately
`(-76.7703, -60.7451)`. Its waypoint X is about `-77.178`, leaving an absolute
X error near `0.408`. `sweep_movement()` advances only inside a 0.25-unit
arrival box, but the reused `movement_toward()` deliberately emits zero X input
inside 0.45 units and zero Y input inside 0.35 units. The result is zero
movement outside the separate arrival box: no progress check is armed, the
waypoint never advances, and sweep escape remains true indefinitely.

## Considered approaches

- Duplicate the movement helper's 0.45/0.35 thresholds in the arrival check.
  This works today but permits future threshold drift.
- Treat the movement helper's returned zero input as arrival, advance once,
  and recompute movement. This is selected because actual legal input is the
  single source of truth.
- Replace sweep movement with the 0.25-unit stance helper. This changes the
  established sweep behavior and is rejected.

## Selected transition

For the current deterministic waypoint:

1. Call the existing `movement_toward(player, waypoint)` exactly once.
2. If either axis is nonzero, return that movement unchanged.
3. If both axes are zero, advance the waypoint exactly once modulo the existing
   ten-waypoint count, compute the new waypoint, and return
   `movement_toward(player, new_waypoint)`.

Do not retain a separate positional arrival threshold in `sweep_movement()`.
Do not loop over multiple waypoints in one tick. A nonzero input that collision
prevents from moving remains covered by the existing controllable sweep
progress check; it is not misclassified as arrival.

## Invariants

- The route, waypoint order, row centers, room coordinates, and movement helper
  remain unchanged.
- Exactly one `session.tick(movement)` remains in the driver loop.
- No production source, target/HP/defeat state, action geometry, cooldown,
  budget, timeout, or assertion changes.
- The fixture still earns every result through real production movement,
  combat, defeat, save, and abyss lifecycle paths.

## Verification

Preserve the existing RED; do not rerun before the change. Build only
`arpg_stage10_validation_fixture` serially, then run the native executable once
with its real exit code. Only after direct exit 0, run exactly
`^stage10\.validation_fixture\.real_abyss_transactions$` with `-j1` and require
1/1 passed. Audit the added lines, unique driver-loop tick, whitespace, and
exact scope. Do not run the full CTest suite and do not commit RED code.
