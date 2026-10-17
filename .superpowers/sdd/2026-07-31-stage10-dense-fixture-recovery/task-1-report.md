# Stage 10 Dense Fixture Recovery — Task 1 Report

## Status

Implementation matches the Task 1 brief's deterministic fixture-data change, but
the required GREEN validation does not pass on the mandated baseline. No commit
was created because the independently testable repair is still red.

## RED evidence

The brief-recorded post-stack-fix baseline evidence at `ab9f888` is preserved:

```text
stage10.validation_fixture.real_abyss_transactions: 0/1
direct exit: 8 (0x00000008)
diagnostic: clear failed phase=1 abyss=1 rule=8 lifecycle_pending= room=1 hp=507 targets=560
```

## Change made

- `tests/dungeon/stage10_validation_fixture.cpp`
  - Added `items/item_catalog.hpp`.
  - Added the brief-specified `base_ids` and six-by-six `affixes` tables.
  - Made the generated item mutable and set, before insertion, exactly the
    specified base ID, cleared/copied affixes, six-affix count, level 95,
    reinforcement 15, and `validate_item()` guard.
- `.superpowers/sdd/2026-07-31-stage10-dense-fixture-recovery/task-1-brief.md`
  - Unchanged.
- `docs/superpowers/plans/2026-07-31-stage10-dense-fixture-recovery.md`
  - Marked Steps 1 and 2 complete locally (ignored by Git).

## Commands and results

1. Initial configuration in the non-developer shell:

   ```powershell
   $env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
   cmake --preset windows-msvc-debug
   ```

   Exit `1`, elapsed `74 ms`; CMake reported `Stage 0 requires Windows SDK
   10.0.26100.0; detected ''`.

2. Configuration through VS2022 BuildTools x64 `VsDevCmd.bat` with
   `CMAKE_BUILD_PARALLEL_LEVEL=1`:

   ```powershell
   cmake --preset windows-msvc-debug
   ```

   Exit `0`, elapsed `8174 ms`; configured and generated successfully.

3. Serial target build:

   ```powershell
   cmake --build --preset windows-msvc-debug --target arpg_stage10_validation_fixture -- -j1
   ```

   Exit `0`, elapsed `4042 ms`; built and linked the single fixture target.

4. Direct GREEN check:

   ```powershell
   & 'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe'
   ```

   Exit `8`, elapsed `1424 ms`:

   ```text
   clear failed phase=1 abyss=1 rule=8 lifecycle_pending= room=1 hp=729 targets=577
   ```

5. Exact serial CTest selector:

   ```powershell
   ctest --test-dir out/build/windows-msvc-debug -R '^stage10\.validation_fixture\.real_abyss_transactions$' --output-on-failure -j1
   ```

   Exit `8`, elapsed `1468 ms`; exactly `1/1` test ran and failed in `1.34 s`
   with the same clear diagnostic (the CTest rendering showed the empty
   `lifecycle_pending` field on a separate line).

6. Scope audit:

   ```powershell
   git diff --check
   git diff -- tests/dungeon/stage10_validation_fixture.cpp
   git status --short
   ```

   Exit `0`; no whitespace error. The tracked working-tree change is only
   `tests/dungeon/stage10_validation_fixture.cpp`. Git emitted only the normal
   LF-to-CRLF working-copy notice while showing the diff.

## Concerns / blocker

The fixture data was applied exactly as specified and accepted by
`validate_item()`, but the real production combat loop still reaches its
unchanged 12,000-tick cap. Health improved from the recorded RED value `507` to
`729`, confirming the equipment is active, yet remaining targets are `577`.
The brief forbids changing any other fixture behavior, production rule, budget,
or golden value, so there is no authorized additional code change to make.

No full CTest suite was run. No files were deleted, reset, or changed outside
the specified fixture/plan/report locations. No historical hunk was imported.

## Fix round 1 — dense streamed-room driver

### Change made

- Kept the existing deterministic equipment diff unchanged.
- Added only the brief-required fixture includes for production active-skill
  geometry, room spatial grid, and active-skill IDs.
- Added a fixture-local five-row boustrophedon sweep at row centers `2, 6, 10,
  14, 18`, derived from production room bounds and cell dimensions. It is used
  only when the resident snapshot has no living target while total remaining
  targets are nonzero.
- Added legal production API selection for resident targets: Storm slot 1,
  then Draw slot 0 if Storm was not accepted, then the existing light fallback.
  No defeat injection, monster/counter mutation, new damage/range/cooldown
  constant, tick-budget change, timeout change, or production edit was made.

### Commands and results

1. Serial target build in VS2022 x64 developer environment:

   ```powershell
   cmake --build --preset windows-msvc-debug --target arpg_stage10_validation_fixture -- -j1
   ```

   Exit `0`, elapsed `4183 ms`; rebuilt and linked only
   `arpg_stage10_validation_fixture`.

2. Required one direct execution (no prior re-run in this fix round):

   ```powershell
   & 'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe'
   ```

   Exit `8`, elapsed `538 ms`; the executable emitted no fixture diagnostic.
   Therefore the exact CTest selector was intentionally not run, per the
   revised brief's `direct exit 0` gate.

3. Scope audit:

   ```powershell
   git diff --check
   git diff -- tests/dungeon/stage10_validation_fixture.cpp
   git status --short
   ```

   Exit `0`; no whitespace error. The only tracked change is
   `tests/dungeon/stage10_validation_fixture.cpp`. Git printed only its normal
   LF-to-CRLF working-copy notice while displaying the diff.

### Self-review and blocker

The fixture now has the requested legal sweep and action priority, and still
calls `session.tick(movement)` exactly once per loop iteration. The direct
exit-8 result has no `drive_clear()` diagnostic. In this function, that exact
combination can only arise from the leading `if (!service_pending(session,
store)) return false;` path; timeout and fault/no-combat branches print a
diagnostic. This is a new save-service terminal condition, not a demonstrated
room-clear terminal state. The task's one-direct-run and no-CTest-before-GREEN
rules prohibit another runtime probe, and its scope forbids unrelated
persistence changes. No commit was created.

## Fix round 2 — pending-save root-cause probe

### Diagnostic method

Before `SaveStore::commit`, the temporary fixture-only log copied pending kind
and expected generation. It then recorded commit state/error/verified
generation and, after `resolve_pending_save`, phase/fault/pending kind/commit
generation. The instrumentation was removed immediately after the one required
direct run because the evidence identifies a non-receipt root cause.

### Commands and results

1. Serial diagnostic target build:

   ```powershell
   cmake --build --preset windows-msvc-debug --target arpg_stage10_validation_fixture -- -j1
   ```

   Exit `0`, elapsed `4017 ms`; only the fixture target rebuilt and linked.

2. One direct diagnostic execution:

   ```powershell
   & 'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe'
   ```

   Exit `8`, elapsed `536 ms`, with this exact evidence:

   ```text
   save pending_kind=10 expected_generation=3 commit_state=0 commit_error=0 commit_generation=3 resolved_phase=0 resolved_fault=0 resolved_pending_kind=-1
   save pending_kind=18 expected_generation=4 commit_state=0 commit_error=0 commit_generation=4 resolved_phase=1 resolved_fault=0 resolved_pending_kind=-1
   save pending_kind=16 expected_generation=5 commit_state=1 commit_error=12 commit_generation=1 resolved_phase=8 resolved_fault=12 resolved_pending_kind=16
   ```

3. After removing the temporary diagnostics, scope audit:

   ```powershell
   git diff --check
   git status --short
   ```

   Exit `0`; no whitespace error, and the only tracked change remains
   `tests/dungeon/stage10_validation_fixture.cpp` (apart from Git's standard
   LF-to-CRLF working-copy notice).

### Conclusion and self-review

`pending_kind=16` is `death_retreat`, not a missing pending-kind receipt. Its
SaveStore result is `commit_state=1` (`not_committed`) with `commit_error=12`
(`invalid_checkpoint`), verified generation `1` instead of expected generation
`5`; resolution enters phase `8` with fault `12` and retains the pending kind.
The first two commits succeed. The revised instruction permits passing the
fourth receipt field only if missing kind is demonstrated, which this probe
falsifies. Therefore no receipt-field change was made, no CTest was run, no
production/persistence change was attempted, and no commit was created.

## Fix round 3 — death timing and throughput probe

### Diagnostic method

The temporary fixture-only probe counted loop tick, initial/defeated/remaining
targets, consecutive and cumulative no-resident ticks, accepted Storm/Draw
casts, and queued light inputs. On a `death_retreat` pending save it printed
those counters together with the current player resources/position, health
potion ground and receipt state, and available combat-death fields. It did not
change input selection, combat, persistence, targets, or timing. The probe was
removed after the one direct execution.

### Commands and results

1. Serial diagnostic build:

   ```powershell
   cmake --build --preset windows-msvc-debug --target arpg_stage10_validation_fixture -- -j1
   ```

   Exit `0`, elapsed `3943 ms`; only the fixture target rebuilt and linked.

2. One direct diagnostic execution:

   ```powershell
   & 'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe'
   ```

   Exit `8`, elapsed `578 ms`:

   ```text
   death_retreat driver_tick=10712 initial=600 defeated=3 remaining=597 hp=0 max_hp=768 barrier=0 x=0.364091 y=-78.0981 ground_health_potions=0 potion_receipt_valid=1 potion_receipt_consumed=1 potion_receipt_restored=192 pending_potion=0 no_resident_consecutive=0 no_resident_cumulative=0 storm_accepted=1 draw_accepted=0 light_queued=6 death_tick=10710 death_source=1 death_raw=12 death_barrier_loss=0 death_health_loss=8 death_final=8
   ```

3. After removing the temporary probe:

   ```powershell
   git diff --check
   git status --short
   ```

   Exit `0`; no whitespace error, with only
   `tests/dungeon/stage10_validation_fixture.cpp` tracked as modified.

### Conclusion and next smallest repair direction

This is decisive throughput failure, not evidence for a survivability or health
potion repair: death occurs at tick `10710` with only `3/600` targets defeated;
the available potion was already consumed and restored `192` HP, and no
no-resident ticks occurred. The next minimal investigation/fix should therefore
target the route/action driver—specifically why it produces only one accepted
Storm, zero accepted Draw, and six queued lights before death—without changing
player stats, potion behavior, production code, density, the tick budget, or
timeouts. No CTest was run and no commit was created.

## Fix round 4 — locked target and interior ranged stance

### Change made

- Added an optional locked monster ordinal to the existing dense-driver state.
  A living active target remains selected until it dies or leaves the active
  snapshot; only then does the driver choose the nearest resident monster.
- Replaced target-center movement with the brief-specified interior stance:
  facing is derived from the target's side of the room, X is exactly one
  production Draw Slash range inward, and Y is clamped by the production Draw
  Slash terminal half-width. At the stance, one horizontal movement tick is
  used when necessary to establish the intended facing.
- Preserved the existing Storm -> Draw -> light production-API priority, sweep,
  equipment, 12,000-tick budget, one-tick-per-loop behavior, and all assertions.

### Commands and results

1. VS2022 x64 configure with `CMAKE_BUILD_PARALLEL_LEVEL=1`:

   ```powershell
   cmake --preset windows-msvc-debug
   ```

   Exit `0`, elapsed `8.4 s`; configuration and generation succeeded. An
   immediately preceding operator attempt set the environment variable with a
   trailing space and was rejected by CMake; the corrected quoted `set` command
   above is the configuration result used for the build.

2. Serial single-target build:

   ```powershell
   cmake --build --preset windows-msvc-debug `
     --target arpg_stage10_validation_fixture -- -j1
   ```

   Exit `0`, elapsed `4.2 s`; the fixture compiled and linked.

3. The required sole direct execution:

   ```powershell
   & 'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe'
   ```

   Nonzero, elapsed `0.8 s`, with empty stdout/stderr. The execution wrapper
   normalizes every nonzero native exit to displayed exit `1` (confirmed
   separately with `cmd.exe /d /c exit 8`), so the fixture's original native
   exit code cannot be recovered from this run. Because direct was not GREEN,
   the exact CTest selector was not run.

### Exact observable terminal evidence and next root cause

There is no current-run fixture terminal diagnostic or action counter output,
so exact HP, position, remaining target count, and Storm/Draw/light counts are
not observable and are not inferred from round 3. The only exact current-run
terminal evidence is: nonzero exit after `0.8 s`, empty stdout, empty stderr,
and no matching Windows Application Error or Windows Error Reporting event in
the following ten-minute window. The fixture's explicit `main()` return codes
start at `2`, so the displayed normalized `1` cannot identify which fixture
gate failed.

Static review identifies one concrete next driver hypothesis: the required
stance-arrival tolerance is `0.25F`, but the mandated existing
`movement_toward()` stops horizontal input at `0.45F`. It can therefore settle
up to `0.45F` short of the exact-range stance, leaving Draw's computed forward
distance greater than `kDrawSlashRange` and never entering either the stance
facing branch or Draw geometry. The next smallest root-cause run should first
wrap the direct command so the native exit is preserved, then add fixture-only
terminal/action counters that include stance X error and Draw forward distance.
That requires a newly authorized direct run; this round's one-direct-run
constraint prevents collecting it now. No CTest was run and no commit was
created.

## Fix round 5 — fixed stance and resident-wide action geometry

### Change made

- Preserved the validated deterministic equipment and boustrophedon sweep from
  the prior rounds.
- Extended the navigation lock to store the legal stance and facing exactly
  once when the ordinal is acquired. The locked values no longer follow a
  moving monster each tick.
- Added a stance-only movement helper with the required `0.25F` dead zone on
  both axes; the sweep still uses the original `movement_toward()` helper.
- Decoupled navigation from action selection. Storm, Draw, and light now scan
  every active living resident using the production geometry constants and the
  common HP/hurt/hit-stop/basic-attack/active-skill/input-queue gate.
- Retained failure-only counters for accepted Storm/Draw casts and queued light
  actions, plus terminal resources and position. They do not change combat.

### Commands and results

1. VS2022 BuildTools x64 configure with
   `CMAKE_BUILD_PARALLEL_LEVEL=1`:

   ```powershell
   cmake --preset windows-msvc-debug
   ```

   Exit `0`, elapsed `6594 ms`; configuration and generation succeeded with
   `VSCMD_ARG_TGT_ARCH=x64`.

2. Serial single-target build:

   ```powershell
   cmake --build --preset windows-msvc-debug `
     --target arpg_stage10_validation_fixture -- -j1
   ```

   Exit `0`, elapsed `2754 ms`; only the fixture compiled and linked.

3. The required sole direct execution used `Start-Process -Wait -PassThru`
   with redirected stdout/stderr so the native process result was preserved:

   ```text
   native exit: 8 (0x00000008)
   elapsed: 2196 ms
   stdout: empty
   stderr:
   clear failed phase=1 abyss=1 rule=8 lifecycle_pending= room=1 hp=768 targets=545
   dense driver failed reason=tick_budget driver_tick=12000 session_tick=12000 phase=1 initial=600 defeated=55 remaining=545 hp=768 max_hp=768 barrier=92 x=-10.152 y=-47.4393 potion_restored=0 storm_accepted=5 draw_accepted=11 light_queued=11
   ```

   Because direct remained RED, the exact CTest selector was intentionally not
   run and no commit was created.

4. Scope audit after the run:

   ```powershell
   git diff --check
   git diff --name-only
   git status --short
   ```

   Exit `0`; no whitespace error. The only tracked changed file is
   `tests/dungeon/stage10_validation_fixture.cpp`. An added-line audit found no
   defeat helper/injection, direct HP or remaining-target mutation, budget or
   timeout edit, and `drive_clear()` still contains exactly one
   `session.tick(movement)` call.

### Exact remaining root cause

This round removed the four named implementation defects and changed the
terminal mode from the round-3 death at `3/600` to a full-health tick-budget
stall at `55/600`. The remaining limiter is now the fixed navigation-lock
lifetime itself: after reaching a target's one-time stance, the driver has no
legal navigation transition until that exact ordinal dies or leaves the active
snapshot. If that moving target leaves the fixed Draw cone while remaining
resident, resident-wide action scanning can attack only incidental monsters
that enter the parked player's current geometry; it cannot move toward them or
resume the sweep. The observed `27` accepted/queued actions over `12,000`
ticks, together with final full HP and a fixed-position stop, is the resulting
throughput stall. Draw's production cooldown is `240` ticks, so only `11`
accepted Draw casts is not a cooldown-cap explanation.

The next repair therefore needs an authorized navigation escape condition for
an alive-but-geometrically-stale lock (for example, bounded reacquisition or
sweep resumption when no resident is currently attackable). That conflicts
with the present binding rule to retain the ordinal until death or absence, so
another fixture change should not be guessed inside this round's scope.
