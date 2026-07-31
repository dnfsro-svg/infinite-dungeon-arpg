# Stage 10 Continuous Melee Target-Chain Design

## Scope

Repair only the Stage 10 standalone validation fixture's per-target navigation
overhead. Preserve the inherited deterministic equipment, event lease,
close-for-light fallback, corrected five-lane sweep, production combat and
streaming paths, assertions, 12,000-tick budget, and registered timeout.

This card does not add J1/J2/J3 buffering. Combo input is independently useful
to validate, but current timing analysis does not show it can solve the clear
budget and it may displace higher-area skills.

## Evidence and root cause

The latest direct run reaches 144/600 after 12,000 ticks with 7 Storm, 35 Draw,
51 light actions, 75 lock acquisitions, 68 absent releases, and 1,623 sweep
ticks. The waypoint deadband is gone, the player remains alive at full HP, and
the driver is not parked in sweep escape.

`close_for_light` currently belongs to one target lease. Every release clears
it. The next target acquisition freezes another interior stance exactly
`kDrawSlashRange` from the target, so a driver that just reached melee distance
backs away roughly five units before it can engage again. At the current legal
movement rate, dozens of these target-chain breaks consume thousands of ticks;
active skills can overlap some movement, but the repeated retreat remains the
largest fixture-controlled navigation cost visible in the evidence.

## Considered approaches

- Persist close mode across target handoffs. This directly removes the repeated
  five-unit retreat while retaining the existing stall/sweep safety net. It is
  selected.
- Prefer any currently attackable resident but otherwise rebuild the ranged
  stance. This is smaller but preserves most retreat churn when geometry
  briefly empties.
- Add unconditional combo buffering. Production supports it, but a full
  hit-confirm combo averages about 20 ticks per swing versus about 19 for
  repeated J1 under the current build, and sparse follow-up swings can delay
  area skills. It is deferred to a separate evidence card.

## Persistent engagement mode

Add a fixture-owned `melee_chain` flag. It starts false and becomes true at the
existing public action-decision transition that currently sets
`close_for_light`: ranged stance reached, player action-ready, neither skill
accepted, and no light geometry.

`melee_chain` is run-level driver intent, not per-lock state:

- acquiring or releasing a monster lock does not clear it;
- target death, target streaming absence, action completion, cooldown, hurt,
  hit stop, and input-buffer state do not clear it;
- a movement stall may release/exclude the current ordinal and enter sweep, but
  the next resident acquisition remains melee;
- an empty resident snapshot enters the existing deterministic sweep; when a
  resident becomes available, acquisition remains melee;
- the flag lives only in the fixture driver and never enters save/checkpoint or
  production state.

## Deterministic melee handoff

When `melee_chain` is true and no valid lock exists, choose from active living
residents in two deterministic tiers:

1. residents satisfying the existing real `in_attack_lane()` geometry;
2. if tier 1 is empty, all active living residents.

Within a tier choose lower squared XY player distance, then lower monster
ordinal on equal distance. Honor the existing one-attempt stalled-ordinal
exclusion before applying the tier/distance/ordinal order.

Freeze the selected ordinal, but set its per-lock `close_for_light` immediately
and navigate with the existing
`movement_toward(player.position, target.position)`. Do not construct or walk a
new ranged stance. Continue the existing Storm, Draw, light action priority.
If an in-lane target is selected, movement may be zero and the real light
request can occur immediately.

Before `melee_chain` first becomes true, preserve the existing frozen ranged
stance behavior exactly.

## Safety and diagnostics

Retain the existing controllable close-movement no-progress check,
one-attempt ordinal exclusion, and sweep escape. Add failure-only counters for:

- melee-chain entry;
- close handoffs after an absent lock;
- in-light-lane versus nearest-resident acquisitions while melee-chain is
  active;
- ranged acquisitions after melee-chain entry, which must remain zero;
- cumulative initial distance to each newly acquired melee target.

Diagnostics are observational only. No target HP, defeat/remaining count,
phase, cooldown, input buffer, save state, or production data is written.

## Invariants

- Exactly one `session.tick(movement)` remains per driver iteration.
- All movement/actions use existing public APIs and real production geometry.
- Storm then Draw then light remains the idle action priority.
- Selection is deterministic by tier, squared distance, then ordinal.
- No route, threshold, equipment, monster, budget, timeout, assertion,
  production source, or Stage B-G behavior changes.

## Focused verification

Preserve the latest RED and inherited fixture diff; do not rerun before the
change. Build only `arpg_stage10_validation_fixture` serially. Run the native
fixture once with its true exit code. Only after direct exit 0, run exactly
`^stage10\.validation_fixture\.real_abyss_transactions$` with `-j1` and require
1/1. Audit forbidden writes/injection, unique driver-loop tick, whitespace,
and exact scope. Do not run full CTest and do not commit RED code.
