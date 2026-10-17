# Task 5 implementation report

## Scope

- Baseline: `59c71a6184a65d8508049ffa4c3dfa6e53e30730`
- Branch: `task/m01-combat-core`
- Commit subject: `feat: resolve deterministic combat hits`

## TDD evidence

### RED

- Added five `hit_resolution` cases and changed the combat case contract from 15 to 20 before touching production code.
- Fresh `out/build/task5-red` configure succeeded, then MSVC failed with C1083 because `combat/combat_collision.hpp` did not exist. This was the expected missing-feature RED.
- Added explicit invalid-`DummyKind` and defeated-target assist assertions before their guards. The invalid enum reproduced an out-of-bounds debug failure at the exact collision case; the defeated-target scenario required the living target to win assist selection.

### GREEN

- Direct combat test: `20 cases, 0 failures`.
- Final fresh build: `out/build/task5-final` compiled all 24 build steps without project warnings.
- CTest: `4/4` passed (`core.units`, `combat.units`, `architecture.core_no_raylib`, `architecture.combat_no_raylib`).
- `git diff --check`: clean.
- `src/combat` and `tests/combat` dynamic-allocation token scan: empty.
- `src/combat` raylib/raymath/rlgl token scan: empty.
- Temporary `[RUN]` / `[TRACE]` diagnostics: removed.

## Files

- Added `src/combat/combat_collision.hpp/.cpp`.
- Added `src/combat/hit_resolution.cpp`.
- Added `tests/combat/hit_resolution_tests.cpp`.
- Updated combat types/world/player simulation and combat CMake wiring.
- Updated attack-state tests to use isolated far-away dummies so Task 4 timeline/lunge assertions remain independent of Task 5 assist and hits.
- Updated combat test main/CMake for 20 cases.

## Semantic checks

- AABB overlap is inclusive on X/Y/Z; local X alone mirrors with facing; Y/Z translate unchanged.
- Dummy half extents are light `0.45/0.35/1.40`, normal `0.55/0.40/1.60`, heavy `0.70/0.50/1.90`; invalid enum values return a safe degenerate box.
- Assist runs once at attack start after lunge, only for living targets in front, chooses smallest surface gap then index, requires gap `<=0.35` and `abs(dy)<=0.45`, changes X only by at most `0.18`, and clamps to the room.
- Tick freeze decisions use tick-start counters. Player and each dummy decrement locally; player freeze skips player state/collision while world tick and dummy counters continue. Input aging uses the tick-start player-frozen value.
- Active collision collects indices 0 through 2 before applying any mutation. Per-serial hit flags prevent repeated hits, including across frozen active ticks.
- Task 5 applies HP, raw break damage, pending impact, target/player Hit Stop, and events only; it does not implement reaction motion, armor suppression, break transitions, or defeat state.
- Event order is `swing`, per-target `hit` in index order, then one `impact_summary`; failed queue pushes increment diagnostics without blocking rules. Queue capacity is 64 and reset drains it.
- J1/J2 connected cancel thresholds are 8/9; whiff thresholds remain 13/15.

## Debugging note

An incremental test run initially produced `0xC0000409` because the localized Ninja MSVC dependency prefix left old test objects compiled against the previous `CombatWorld` layout. A separate fresh build removed that stale-object failure. The later invalid-enum RED exposed and fixed the independent public collision helper OOB. Final evidence uses a full fresh build, not the stale incremental tree.

## Formal review fix: landing event

- Important finding: the event enum exposed `landing`, but the transition did not emit it.
- RED: extended the existing landing case without increasing the 20-case contract; after 44 ticks plus the following grounded tick, draining events produced zero landing events and failed at `landing_events == 1` (`20 cases, 1 failures`).
- Fix: the first `Z <= 0` airborne clamp emits one `CombatEventKind::landing` with current pre-increment `tick_`, ground position, default `AttackId::none`, and target sentinel `0xFF`. Subsequent grounded ticks cannot re-enter that branch and do not repeat the event.
- GREEN fresh build `out/build/task5-landing-final`: Combat `20 cases, 0 failures`; CTest `4/4`; `git diff --check` clean; no temporary RUN/TRACE diagnostics.
