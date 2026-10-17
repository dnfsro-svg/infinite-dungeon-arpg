# Stage 10 standalone fixture: root-cause report

## Scope and evidence

Read-only investigation in `E:\game\.worktrees\task9-gate-recovery` at
`ab9f888`.  No source, test, CMake, Git index, or `HEAD` was changed.  The
only created artifact is this requested report.

Fresh supplied execution evidence is reproducible at the code-path level:

```
arpg_stage10_validation_fixture.exe -> exit 8 (0x8), about 1.526 s
clear failed phase=1 abyss=1 rule=8 lifecycle_pending= room=1 hp=507 targets=560
```

`RoomPhase::combat` is value `1` (`src/dungeon/dungeon_types.hpp:23-33`).
There is no fault, no pending abyss reward, and no Windows Application Error
1000 after the MSVC `/STACK:4194304` restoration.  Thus the previous stack
overflow is resolved and is not part of this failure.

## Exact exit-8 path

1. `main()` finds the first deterministic `life_sacrifice` abyss entry, commits
   it, creates `DungeonSession`, services startup persistence, then calls
   `drive_clear()` (`tests/dungeon/stage10_validation_fixture.cpp:523-547`).
2. `drive_clear()` allows exactly `12000` iterations (`:166-235`).  Each
   iteration first commits any pending save, snapshots the session, and exits
   successfully only when phase is `cleared` or `awaiting_exit` (`:173-195`).
3. While phase is `combat`, it chooses the nearest active/living snapshot
   monster, moves toward it, and queues one `Action::light` only if the player
   is unharmed, idle, has no queued input, faces the target, and is within
   `abs(dx)<=1.70`, `abs(dy)<=0.55` (`:206-223`, attack-lane definition
   `:156-164`).  It then calls precisely one `session.tick(movement)`.
   This is a serial, single-target harness controller; it does not use a
   multi-target clear action or any private defeat injection.
4. At iteration exhaustion it emits `clear failed ... targets=<remaining>` and
   returns `false` (`:225-235`); `main()` maps that to exit code `8` (`:545-547`).

The supplied result therefore means the harness reached its fixed iteration
limit with **560 targets still outstanding**.  Its HP is still 507, so this is
not a death/health termination; `lifecycle_pending` is empty because abyss
clear/reward materialization has not been reached.

## First failed invariant and production flow

The first failed completion invariant is not a save or abyss-lifecycle
invariant.  It is the harness's implicit completion requirement:

```
within 12,000 drive_clear ticks:
    session.phase is cleared/awaiting_exit
```

At the production boundary, `DungeonSession::tick()` calls
`prepare_room_clear()` only when `remaining_targets() == 0`
(`src/dungeon/dungeon_session.cpp:344-352`).  `remaining_targets()` is exactly
`initial_monster_count - defeated_monster_count` (`:2420-2425`).
`prepare_room_clear()` independently requires all generated monsters to be
defeated and remaining targets to be zero (`:2239-2250`).  The supplied 560
remaining targets falsifies that precondition before any abyss clear save,
reward, or exit rule can run.

The quarter-clear value is not the clear condition: `required_kills(n)` is
`(n + 3) / 4` (`src/dungeon/dungeon_types.hpp:110-113`) and only unlocks exits
(`src/dungeon/dungeon_session.cpp:344-351`, `1287-1291`).  It did not change
the all-targets-required rule.  No production fault is indicated.

## Contract drift: confirmed root cause

The current branch contains `cadbb833` (`feat: define hundredfold square room
limits`).  Its direct changes are:

* Room bounds changed from `[-12,12] x [-5.5,5.5]` to a square
  `[-81.24038696,81.24038696]^2` (`src/combat/room_bounds.hpp:5-13`).
* The room plan now has 20x20 cells with at most three monsters each and a
  `kRoomMonsterCapacity` of 1152 (`src/combat/room_spatial_grid.hpp:12-32`,
  `src/core/gameplay_limits.hpp:7-18`).
* Density is now 300--400 / 425--550 / 575--750 before abyss scaling; abyss
  multiplies by 1.5 with ceiling (`src/dungeon/room_affix.cpp:13-24,48-63`).
  Consequently an abyss room contains **450--1125** monsters.

This exact target is deterministically selected by `find_entry()` and
`build_room_monster_plan()`; the fixture reports the authoritative live
residual of 560.  It does not log the initial count, so this investigation does
not claim an unsupported exact starting total.  The valid range and residual
are sufficient: the old fixed-budget, one-light-action driver did not finish
the enlarged encounter.

The current Stage 10 fixture's `install_validation_build()` still creates six
plain generated rare items (`tests/dungeon/stage10_validation_fixture.cpp:60-79`).
The tested fixture is therefore underpowered for the new depth-99 dense abyss
room.  This is not speculative: sibling commit `d57c2e8` is explicitly named
`test: adapt Stage 10 fixture to dense rooms`, is not an ancestor of `HEAD`,
and its diff upgrades only the fixture setup to a validated level-95,
reinforcement-15, six-affix build.  Its `drive_clear()` loop retains the same
12,000-tick limit and the same `Action::light` policy.  That comparison isolates
the missing fixture adaptation rather than a requirement to relax time or game
rules.

## Minimal repair proposal (not applied)

Change only `tests/dungeon/stage10_validation_fixture.cpp`:

1. Include `items/item_catalog.hpp`.
2. In `install_validation_build()`, retain the six deterministic items but
   assign the six deterministic base IDs and six-affix sets from `d57c2e8`, set
   `required_level = 95` and `reinforcement = 15`, and call
   `items::validate_item()` before insertion.

This is test-fixture data only.  It does **not** change monster density, room
bounds, monster capacity, `required_kills`, all-target clearing, gameplay
golden values, the CTest timeout, or Stage B--G scope.  Do not increase the
12,000 iteration budget: doing so hides the mismatch and makes the test slower
without restoring the dense-room test contract.

## Focused regression selection after the repair

1. Rebuild only target `arpg_stage10_validation_fixture`.
2. Run the executable once directly (captures its diagnostic output).
3. Run only `ctest --test-dir out/build/windows-msvc-debug -R
   '^stage10\\.validation_fixture\\.real_abyss_transactions$' --output-on-failure`.

The CTest registration is exactly that selector with a 30-second timeout
(`tests/dungeon/CMakeLists.txt:109-125`).  No full CTest run is justified for
this isolated fixture repair.  Residual risk is limited to fixture-data
validity/stat balance; `validate_item()` prevents an invalid synthetic item,
and the focused executable plus exact CTest selector cover the failed exit-8
path and all subsequent reward/abandon/environment assertions in the same
fixture.

## Evidence level

**High.** The conclusion is jointly supported by the current source control
flow, current density and capacity contracts, the exact exit-8 branch, supplied
fresh runtime residual (560), and a non-ancestor historical commit containing
the narrowly corresponding dense-room fixture adaptation.  A successful
post-repair run remains required before declaring the fix complete.
