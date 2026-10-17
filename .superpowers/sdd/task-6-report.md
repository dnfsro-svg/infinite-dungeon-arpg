# Task 6 implementation report

## Status

- Baseline: 2697027 (localized MSVC dependency fix)
- Branch: task/m01-combat-core
- Intended commit: feat: add combat target reactions

## TDD evidence

### RED

Five reaction cases and the exact case-count gate were added before production changes. A fresh MSVC 19.44 build completed and the focused runner reported:

```text
25 cases, 5 failures
```

The five failures were exactly the missing behaviors: J1 hitstun, J2 knockback, launcher airborne state, knockdown, and immediate defeated state.

### GREEN

- Fresh build directory: E:/game/out/combat-task6-final
- Full fresh compile: 26 steps, no new project warnings
- Focused Combat: 25 cases, 0 failures
- Core-only CTest: 4/4
- git diff --check: clean
- Combat/test raylib and dynamic-allocation token scan: empty

## Production changes

- Added src/combat/target_simulation.cpp.
- Wired target simulation into actor-local tick-start Hit Stop handling.
- Added same-hit reaction application, scaled fixed timers/impulses, horizontal integration/clamp, launch gravity, landing, knockdown/rising, defeated/respawning, and target events.
- Hit resolution now ignores defeated/respawning targets and applies each collected impact after its hit event, preserving hit -> defeated -> summary ordering.
- Added target_simulation.cpp to arpg_combat.

## Frozen semantics

- A hit establishes reaction, full timer, velocity, and Hit Stop in the hit tick; it does not move or decrement the timer in that tick.
- A target whose Hit Stop is nonzero at tick start decrements once and skips all local motion/timers even when the counter becomes zero.
- Light uses integer ceil(base * 1.25): hitstun 10->13, medium 16->20, knockdown 45->57, rising 30->38. Normal/heavy use 1.0.
- J2 horizontal impulse is 2.75 light / 2.2 normal. Launcher normal starts at vx=1.2, vz=9.5; the first active tick integrates z then applies gravity to 9.1.
- Airborne motion continues under later non-launch hits; landing enters a full knockdown window and emits one target landing event.
- Normal knockdown stays through active tick 44, transitions after tick 45 to Rising 30, then to Idle after active rising tick 30.
- HP zero wins immediately: Defeated 90, zero velocity, no ordinary reaction, defeated event after hit. Hit Stop pauses the 90 active-tick timer.
- Active tick 90 restores spawn/full HP/full break/initial armor, enters Respawning, and emits one respawned event; next active tick enters Idle.
- Armored suppression, breaking-blow priority, Broken 180, break_started, and reset/stress behavior remain deferred to Task 7.

## Test compatibility update

The prior Task 5 defeated-target Assist test used repeated heavy attacks while targets were static. Task 6 correctly makes heavy attacks knock targets away, so the test setup no longer guaranteed repeated contact. It now kills the same target with eleven zero-knockback J1 attacks and places a living second target so the final Assist correction still distinguishes dead-target filtering. The original assertion purpose is preserved.

## Formal review follow-up

The first formal review found no production defect but required stronger regression evidence. Existing five cases were extended without changing the 25-case contract to prove:

- an airborne target hit again by J1 preserves vertical motion after its local Hit Stop;
- target landing emits exactly one indexed landing event;
- an unhit airborne normal target continues simulating while the player and a different light target are frozen;
- knockback clamps at room X=8 and clears outward velocity;
- Defeated ignores a spatially overlapping active attack;
- an overlapping active attack also cannot hit on the Respawning tick, but can hit on the following tick after the target becomes live.

Review-fix regression remained Combat 25/0 and CTest 4/4.
