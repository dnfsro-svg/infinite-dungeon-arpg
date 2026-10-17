# Stage 10 dense fixture GREEN failure — second-stage root cause

## Verdict

The primary limiter is **spatial discovery/residency in the fixture driver**, not
damage, attack speed, movement speed, incoming damage, death, or the 12,000-tick
budget itself.

The current driver selects only the nearest living entry in
`CombatSnapshot::monsters`. That array contains only the active resident pool,
not all monsters in the 162.48 x 162.48 room. When the currently streamed region
has no living resident but `remaining_targets` is still nonzero,
`nearest_monster()` returns null and `drive_clear()` submits zero movement for
every remaining tick. The stronger item hunk makes this failure more visible:
it kills the local residents sooner and stops the player before the next cells
stream in. This explains the otherwise paradoxical fresh result: health rises
from 507 to 729, yet remaining targets regress from 560 to 577.

Fixture-only use of the two already-equipped production active skills is also
needed for comfortable dense-room throughput, but it is secondary to the
no-target streaming deadlock. Skills alone do not make nonresident monsters
visible; the driver also needs deterministic scouting movement when the local
resident set is empty.

## Evidence

### 1. Exact RED and rejected hypotheses

Fresh supplied evidence after the exact validated equipment hunk:

```text
direct executable: exit 8
exact CTest:       exit 8
clear failed phase=1 abyss=1 rule=8 lifecycle_pending= room=1 hp=729 targets=577
```

The recorded pre-hunk baseline was `hp=507 targets=560`. Both executions reach
the complete 12,000-tick loop in combat with positive HP. Therefore:

* death is not the termination path;
* save/abyss reward processing has not begun;
* survivability improved materially;
* clear progress did not improve, so lack of damage or defense is not the
  leading explanation.

The fixture itself makes the deadlock possible at
`tests/dungeon/stage10_validation_fixture.cpp:155-169,192-250`: it searches only
the snapshot monster array; if no target is returned, movement remains `{}` and
no action is queued.

### 2. What the equipment actually produces

The six items are valid and project through the production path
`project_equipment_detailed -> evaluate_player_modifiers -> PlayerCombatBuild`
(`src/items/item_modifiers.cpp:286-374`,
`src/dungeon/dungeon_session.cpp:1487-1530`). Static evaluation of the exact
fixture data gives the offensive/mobility values below:

* Siege Greatblade tier-8 base 22 at reinforcement 15 becomes 64; tier-8 local
  flat 12 and +42% local physical yield `weapon_physical = 107`.
* The weapon's -1500 bp local speed plus its +1200 bp suffix gives
  `local_attack_speed_bp = -300`.
* The gloves' +1200 bp attack-speed suffix gives global attack speed 11200.
  Effective basic-attack speed is `11200 * 9700 / 10000 = 10864`, 8.64% above
  baseline.
* The boots give +1000 bp movement speed, so normal movement is 110% of the
  5.4 units/s production base: 5.94 units/s before abyss/status modifiers.
* Three tier-8 fire-flat rolls give 30 fire; three tier-8 fire-increased rolls
  give +126%; the weapon gives +30% melee damage.
* Because the fixture queues `light` only while `active_attack == none`, it
  never buffers the j1 cancel into j2/j3. Every basic attack is j1. Its packet
  is approximately `(28 + 107) * 1.30 = 175` physical plus
  `30 * 2.26 * 1.30 = 87` fire, total 262 before a monster's physical-only
  armor/shield. Base monster HP is 100--315
  (`src/combat/monster_catalog.cpp:42-78`), so this is commonly a one-hit local
  kill.
* j1 startup/recovery scale from 5/9 to 4/8 ticks; active remains 3 ticks
  (`src/combat/attack_catalog.cpp:8-14`,
  `src/combat/combat_world.cpp:297-303`). Thus neither damage nor input cadence
  is worse after the item change.

The health result is consistent with the same projection: five +46 flat-health
prefixes, three +24% health suffixes, five +38 barrier prefixes, reinforced
armor/evasion, and elemental reduction all improve survival. It is evidence
that equipment is live, not evidence that the room is close to clearing.

### 3. Why the driver loses the rest of the room

The current room contract is spatially streamed:

* Bounds are `[-81.24038696, +81.24038696]` on both axes
  (`src/combat/room_bounds.hpp:5-11`).
* The room is 20 x 20 cells, at most three monsters per cell. A streaming region
  is only the camera footprint plus a one-cell halo, bounded by 7 x 5 cells and
  105 residents (`src/combat/room_spatial_grid.hpp:12-30`).
* Only 128 monsters may be active even though the room owns up to 1152
  (`src/core/gameplay_limits.hpp:7-12`). Abyss density is 450--1125
  (`src/dungeon/room_affix.cpp:13-24,48-63`).
* `RoomMonsterField::required_residents()` collects only undefeated blueprints
  in the current region; `synchronize_active_region()` writes back and replaces
  the active pool when the player's streaming region changes
  (`src/combat/room_monster_field.cpp:150-202,231-273`).
* `CombatWorld::tick()` changes that region from the player's position
  (`src/combat/combat_world.cpp:606-647`).
* `CombatWorld::snapshot()` exports only active pool entries in
  `snapshot.monsters`, while total living/defeated counts come separately from
  `RoomMonsterField` (`src/combat/combat_snapshot.cpp:30-52`).

Therefore the fixture's `nearest_monster()` is not a whole-room query. Once its
local query returns null, the only operation that could reveal more targets is
player movement, but the null branch produces none. The stronger one-hit build
can stop about 1.7 units short of the last local target (the fixture's attack
lane), whereas the weaker build spends longer following mobile targets. The
17-target regression is directionally diagnostic of earlier local exhaustion,
not inadequate damage.

Evidence level for this causal chain: **high**. It follows directly from the
snapshot boundary and null branch, and the fresh A/B result falsifies the
damage/defense hypothesis. No extra focused runtime was used; the single-run
allowance remains available for the TDD GREEN attempt.

### 4. History and contract compatibility

`git log --all -- tests/dungeon/stage10_validation_fixture.cpp` contains only:

```text
cadbb833 feat: define hundredfold square room limits
d57c2e8 test: adapt Stage 10 fixture to dense rooms
fb7f0f3 feat: expand combat room to four times area
a7e0fc6 fix: close stage 10 evidence gaps
73e9200 fix: harden stage 10 validation evidence
76c166b test: add formal stage 10 abyss validation
```

There is no later standalone-fixture fix on any ref. `d57c2e8` is not an
ancestor of HEAD (merge-base `58a8238...`) and lives on parallel large-density
branches. Its full 168-add/21-delete hunk includes equipment and later-stage
diagnostics/navigation hardening, but its `drive_clear()` still uses the same
12,000-tick nearest-resident/light-only policy. More importantly, its parent
contract was only a 48 x 22 room with 12--30 base monsters (18--45 abyss) and
had no `RoomMonsterField`/spatial grid. It was valid for that branch but is not
a complete repair for the current streamed 450--1125-monster square.

The later behavioral precedent is outside this file: `b57930e` added real
Storm Swords / Draw Slash use to the Stage10 host validation driver, and
`67b1574` extracted it to
`src/platform/raylib/host_validation_stage10_11.cpp:35-119`. It tries slot 1
Storm Swords when the target is within the real storm radius, then slot 0 Draw
Slash inside its real cone, then light as fallback. That logic is compatible
with current production APIs and default loadout, but its scenario gate excludes
most abyss scenarios; it cannot simply be called unchanged by this standalone
life-sacrifice fixture.

Evidence level for history: **high**. Evidence that active skills plus scouting
will fit the fixed budget is **medium until the focused GREEN run**.

## Minimal TDD repair (not applied)

Keep the current equipment diff. Change only
`tests/dungeon/stage10_validation_fixture.cpp`.

### RED first

Use the existing exact failing CTest as the RED. Extend only its exit-8
diagnostic with fixture counters (`initial_monster_count`,
`defeated_monster_count`, resident living count, player x/y, consecutive
no-target ticks, accepted skill counts). Do not add a new test target or alter
the tick/CTest timeout.

The expected pre-fix signature is positive `remaining_targets` together with a
long final no-target streak. This makes the residency boundary directly visible
if the final implementation review requires runtime confirmation.

### Smallest robust driver diff

1. Include `combat/active_skill_runtime.hpp`,
   `combat/room_spatial_grid.hpp`, and `skills/active_skill_types.hpp`.
2. Add a tiny fixture-owned `DenseDriverState` containing only a sweep lane and
   direction. Add `sweep_movement(player, state)` with five alternating
   horizontal lanes. Lane centers are rows 2, 6, 10, 14, and 18 of the existing
   20-row spatial grid; adjacent passes overlap the maximum five-row streamed
   footprint. Advance a lane only after reaching its far-x waypoint. This is
   deterministic traversal of production room cells, not a density change.
3. In `drive_clear()`:
   * if a resident target exists, retain `movement_toward(player, target)`;
   * if no resident target exists and `remaining_targets != 0`, use
     `sweep_movement` instead of `{}` so new cells become resident;
   * when a target exists and the player is available, copy the geometry/readiness
     checks from `stage10_validation_input`: request slot 1 Storm Swords first,
     then slot 0 Draw Slash, then use `Action::light` only as fallback;
   * always pass the chosen movement to `session.tick()`. Production active
     skills continue applying movement during their timeline
     (`src/combat/active_skill_runtime.cpp:29-56`), unlike basic attacks, which
     freeze horizontal movement while active
     (`src/combat/player_simulation.cpp:155-194`).

Exact action policy:

```text
target absent, remaining > 0 -> deterministic five-lane scout movement
target present, Storm geometry + ready -> request_active_skill_slot(1)
else target present, Draw cone + ready -> request_active_skill_slot(0)
else target in existing light lane + idle/input empty -> queue_action(light)
else -> move toward resident target
```

Use the existing constants (`kStormCenterForward=3.5`,
`kStormStrikeRadius=2.8`, `kDrawSlashRange=5.0`,
`kDrawSlashHalfWidthAtEnd=3.2`) rather than fixture-owned damage/range values.
The default loadout already owns/equips Draw Slash in slot 0 and Storm Swords in
slot 1 (`src/skills/skill_loadout.cpp:26-36`). Existing production tests cover
slot requests/cooldowns (`tests/dungeon/dungeon_skill_cast_tests.cpp:21-63`),
Draw Slash multi-target geometry/damage
(`tests/combat/draw_slash_skill_tests.cpp:86-208`), and the 12-strike plus
finisher Storm timeline (`tests/combat/active_skill_timeline_tests.cpp:55-103`,
`tests/combat/storm_swords_skill_tests.cpp`).

### GREEN check

Use the one allowed focused execution after the diff:

1. Build only `arpg_stage10_validation_fixture` serially.
2. Run its executable directly once and require exit 0.
3. Then run the exact CTest selector only if the direct run is GREEN; it is the
   required registered 30-second verification, not another hypothesis run.

Do not run the full suite in this task.

## Why this still tests real production combat

The fixture continues to construct a real `DungeonSession`, use the production
streaming region, active skill APIs, input buffer, attack timelines, collision,
damage, monster death ledger, all-target clear transition, save/reward
transactions, and abyss lifecycle. The only new fixture behavior is choosing
legal movement/actions that a player can issue. It never writes monster HP,
never calls a defeat helper, never edits the defeat ledger, and never changes
`remaining_targets` or room phase. Clear can still occur only after production
combat has defeated every generated monster.

No production source, golden value, density, 12,000-tick cap, 30-second CTest
timeout, or Stage B--G contract needs to change.
