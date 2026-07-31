# Stage 10 Dense Driver Event-Lease Design

## Status and scope

This design repairs only the standalone Stage 10 validation fixture driver.
It does not change production combat, room streaming, monster behavior,
equipment rules, density, gameplay thresholds, the 12,000-tick fixture budget,
or the registered 30-second timeout.

The preserved round-5 fixture is RED after 12,000 real-combat ticks with
55/600 monsters defeated, full health, a fixed player position, and only 27
accepted or queued attacks. Its navigation lock survives while its monster is
resident even after that moving monster leaves every attack geometry. The
driver therefore neither reacquires a useful stance nor resumes spatial
scouting.

## Considered approaches

### Fixed-duration lock

Release the target after a fixed number of ticks. This is small, but the
threshold is arbitrary and can expire during cooldown, hit stop, hurt, an
active attack, or an active-skill timeline. It couples the fixture to timing
details instead of observable navigation state and is rejected.

### Defeat or damage progress timeout

Release after no defeated-count or damage progress for a bounded interval.
This provides useful failure diagnostics, but high-health targets and attacks
that legitimately require multiple hits create false positives. Progress
counters remain diagnostic only and never drive navigation.

### Event-driven engagement lease

Retain a frozen target and stance only for one approach/engagement episode.
Release it on observable invalidation and immediately choose a new deterministic
course. This preserves intentional navigation without permitting a resident
monster to hold the driver indefinitely. This is the selected approach.

## Driver state

The fixture-owned state contains:

- the existing deterministic sweep waypoint;
- an optional locked monster ordinal;
- the frozen stance and facing computed when the lock is acquired;
- whether that stance has been reached with the required facing;
- the prior commanded stance distance and player position needed to detect one
  controllable movement that made no progress;
- a one-attempt stalled ordinal used only to avoid immediately reacquiring the
  same unreachable stance;
- failure-only counters for lock acquisitions/releases, geometry-stale
  releases, movement-stall releases, sweep ticks, and accepted/queued actions.

No driver state contains or mutates monster HP, defeat ledgers, target counts,
room phase, production cooldowns, ground state, or save state.

## Deterministic state machine

### Acquire

When no valid lock exists, choose the nearest active living resident that is
not the one-attempt stalled ordinal. Break equal-distance ties by lower monster
ordinal. Freeze that monster's current derived interior stance and facing. The
stalled exclusion is cleared after exactly this acquisition attempt, whether
or not another candidate was found.

If no eligible active living resident exists while targets remain, run the
existing five-lane boustrophedon sweep for at least one controllable movement
tick. The sweep is also the escape for an unreachable stance. Once that sweep
movement changes player position, normal acquisition may resume.

### Approach

Move toward the frozen stance with the existing 0.25-unit axis dead zone. The
lease remains valid while the target is alive/resident and distance to the
stance decreases on controllable movement ticks.

A tick is controllable for movement-stall detection only when horizontal
movement was requested and the player is alive, not hurt, not in hit stop, not
inside a basic attack, and not inside an active-skill timeline. Cooldown alone
does not affect controllability. If such a commanded tick fails to reduce
squared distance by at least `1e-4F`, release the lease, exclude its ordinal
from the next acquisition attempt, and sweep. Suppressed movement never counts
as a stall.

When both axes are within 0.25 units, establish the frozen facing with the
existing one-tick horizontal input when required. Mark the stance reached only
after position and facing both match.

### Engage

Action selection scans every active living resident, independent of the
navigation target, using only production geometry constants and APIs:

1. request Storm Swords in slot 1 when its real circular geometry contains a
   target;
2. otherwise request Draw Slash in slot 0 when its real cone contains a
   target;
3. otherwise queue light attack when its existing lane contains a target.

Rejected cooldown requests fall through to the next legal action. Accepted
actions, cooldown, hit stop, hurt, basic-attack animation, and active-skill
animation do not expire the lease.

After the frozen stance and facing have been reached, if no active living
resident lies in any of the three current attack geometries, release the lease
immediately. Reacquisition may select the same ordinal at its new position,
but it must compute a new frozen stance; distance/ordinal ordering remains
stable.

### Immediate invalidation

At any point, release without a timeout when the locked ordinal is defeated,
absent from the active snapshot, inactive, or no longer alive. If no resident
can be acquired and remaining targets are positive, sweep so new room cells
become resident.

## Invariants

- Exactly one `session.tick(movement)` occurs per driver-loop iteration.
- The driver uses public movement/action/skill requests and production combat;
  it never calls a defeat helper or writes HP, target counts, defeat bits,
  room phase, cooldowns, or combat events.
- A resident target cannot retain a lease indefinitely after the frozen stance
  has become geometrically stale.
- Cooldown and animation timing cannot by themselves invalidate a useful
  engagement.
- Every selection and sweep transition is deterministic for the same snapshot.
- The existing real clear, save, abyss reward/lifecycle, abandonment, and
  reload assertions remain unchanged.

## Failure evidence

On failure, print the existing terminal snapshot plus:

- acquisition and release counts by reason;
- sweep ticks and current waypoint;
- whether a lock exists, its ordinal, stance, facing, reached state, and current
  squared stance distance;
- accepted Storm/Draw counts and queued light count;
- initial, defeated, and remaining target counts.

Diagnostics are observational only and are printed only on failure.

## Focused verification

Use the existing RED as the baseline and retain the current round-5 diff.
After implementing the lease:

1. configure in the VS2022 x64 environment with
   `CMAKE_BUILD_PARALLEL_LEVEL=1`;
2. build only `arpg_stage10_validation_fixture` with `-j1`;
3. run the executable directly once and require native exit 0;
4. only after direct GREEN, run exactly
   `^stage10\.validation_fixture\.real_abyss_life_sacrifice$` with `-j1` and
   require exactly 1/1 passed;
5. audit added lines for forbidden HP/defeat/remaining-target mutation,
   injected defeat/events, changed tick/timeout values, and more than one
   `session.tick` in the driver loop;
6. require `git diff --check` and exact Stage 10 fixture/design/plan scope.

Do not run the full CTest suite in this card. A failed direct run stops the
card before CTest and must retain its diagnostics without committing RED code.
