### Task 1: Replace the permanent navigation lock with an event lease

## Binding Global Constraints

- Work only in `E:\game\.worktrees\task9-gate-recovery` on branch
  `codex/task9-gate-recovery`, starting from committed HEAD `2eec9c2` plus the
  preserved uncommitted round-5 Stage 10 fixture diff.
- Modify only `tests/dungeon/stage10_validation_fixture.cpp` and
  `docs/superpowers/plans/2026-07-31-stage10-event-lease-recovery.md`.
- Never stage or commit the superseded untracked
  `docs/superpowers/plans/2026-07-31-stage10-dense-fixture-recovery.md`.
- Keep configure, build, direct execution, and CTest serial with
  `CMAKE_BUILD_PARALLEL_LEVEL=1` and `-j1`.
- Do not change production source, gameplay/equipment rules, density, bounds,
  golden values, 12,000-tick budget, timeout, fixture assertions, or Stage B-G
  behavior.
- Do not inject defeat/events, call a test defeat helper, or write monster HP,
  defeat state, remaining targets, room phase, cooldowns, or save state.
- Use exactly one `session.tick(movement)` per driver-loop iteration.
- Do not delete files, reset/clean Git, touch another worktree, or run the full
  CTest suite.
- The round-5 RED is already established; do not rerun it before implementing
  the lease.
- Direct GREEN is mandatory before the exact registered CTest. Never commit a
  RED implementation.

**Files:**

- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Add/commit:
  `docs/superpowers/plans/2026-07-31-stage10-event-lease-recovery.md`
- Test: existing `stage10.validation_fixture.real_abyss_transactions`

**Interfaces:**

- Consumes only `CombatSnapshot`, public `DungeonSession` movement/action/skill
  requests, existing production geometry constants, and the preserved
  five-lane sweep.
- Produces deterministic fixture-only navigation transitions and failure-only
  lease diagnostics.
- Preserves the existing real clear, reward, save, abyss lifecycle,
  abandonment, reload, room-size, and environment assertions.

- [ ] **Step 1: Preserve the new-plan RED and protected scope**

Use the already recorded round-5 result; do not rerun it before changing the
lease:

```text
native exit: 8
driver tick: 12000
initial/defeated/remaining: 600 / 55 / 545
player: 768/768 HP, 92 barrier, fixed at (-10.152, -47.4393)
actions: Storm 5, Draw 11, light 11
```

The existing fixture diff hash is
`8bcc00efda5fc9fd4c6dd030800c3875e8853947`. Record it before editing. Confirm
the staging area is empty and the only tracked dirty path is the Stage 10
fixture.

- [ ] **Step 2: Add explicit lease state and stable selection**

Extend `DenseDriverState` with only:

```cpp
bool stance_reached{};
bool sweep_escape{};
bool pending_stance_progress_check{};
bool pending_sweep_progress_check{};
float previous_stance_distance_squared{};
Vec3 previous_player_position{};
std::optional<MonsterOrdinal> stalled_ordinal{};
std::uint32_t lock_acquired{};
std::uint32_t release_absent{};
std::uint32_t release_stale_geometry{};
std::uint32_t release_movement_stall{};
std::uint32_t sweep_ticks{};
```

Use the project's fully qualified ordinal type if the short alias is not in
scope. Reset stance/progress fields whenever a lock is released or acquired.
Do not add an elapsed-lock or no-damage timeout.

Replace nearest-resident selection with a helper that accepts one optional
excluded ordinal. Select the lowest squared player distance; when distances
compare equal, select the lower monster ordinal. Skip inactive/dead entries and
the excluded ordinal. Clear `stalled_ordinal` after exactly one acquisition
attempt.

Add small helpers for:

- squared XY distance;
- acquiring/freezing the existing interior stance and facing;
- releasing a lock with a diagnostic reason;
- recognizing whether a previous requested movement tick was controllable.

Controllable means the previous snapshot had a living player, no hurt, no hit
stop, no active basic attack, no active skill, and no action/skill was accepted
or queued in that loop iteration.

- [ ] **Step 3: Evaluate action geometry separately from readiness**

Factor the existing resident-wide geometry scans into a fixture-local value:

```cpp
struct AttackGeometry final {
    bool storm{};
    bool draw{};
    bool light{};
    [[nodiscard]] bool any() const noexcept;
};
```

Compute it from the current snapshot using only
`kStormCenterForward`, `kStormStrikeRadius`, `kDrawSlashRange`,
`kDrawSlashHalfWidthAtEnd`, and the existing light lane. Geometry must be
available even when an action is temporarily unavailable due to cooldown,
hurt, hit stop, or animation.

Keep the current action readiness gate. When ready, use exactly:

1. request slot 1 if `geometry.storm`;
2. if not accepted, request slot 0 if `geometry.draw`;
3. if neither skill was accepted, queue light if `geometry.light`.

Return/record whether any action was accepted or queued so the same iteration
cannot be misclassified as a failed movement-progress tick.

- [ ] **Step 4: Implement the event-driven lease transitions**

At the beginning of each combat iteration:

1. Resolve pending progress checks from the previous snapshot.
2. If a previously controllable stance movement did not reduce squared stance
   distance by at least `1e-4F`, release the lock, set its ordinal as the
   one-attempt exclusion, increment `release_movement_stall`, and enter sweep
   escape.
3. Sweep escape persists until one controllable requested sweep tick changes
   player XY position; then acquisition may resume.
4. If the locked monster is inactive, dead, or absent from the active snapshot,
   release immediately and increment `release_absent`.

When not escaping and no lock exists, acquire deterministically and freeze the
existing interior stance/facing. If the only resident is the one-attempt
excluded ordinal, clear the exclusion and sweep rather than immediately
rebuilding the same unreachable stance.

While approaching, use `movement_toward_stance()` and arm the next progress
check only for nonzero movement on a controllable, no-action iteration. At the
stance, retain the one-tick facing correction. Set `stance_reached` only when
both axes are within `0.25F` and the current facing matches.

After stance arrival, if `AttackGeometry::any()` is false, release immediately,
increment `release_stale_geometry`, and perform one deterministic acquisition
attempt in the same iteration. It may select the same ordinal at its current
position, but must freeze a new stance. If no candidate is available, sweep.

Cooldown, hurt, hit stop, active attack, active skill, and rejected action
requests never release a geometrically useful lease. Always call
`session.tick(movement)` exactly once after the movement/action decision.

- [ ] **Step 5: Extend failure-only diagnostics and audit before execution**

Append the new counts and current lease state to
`print_dense_driver_failure()`: lock ordinal or none, frozen stance/facing,
stance reached, current stance distance squared, acquisitions, release counts
by reason, sweep ticks/waypoint, and action counts.

Before the first new direct run:

```powershell
git diff --check
git diff -- tests/dungeon/stage10_validation_fixture.cpp
rg -n "relay_defeated\s*\(|events_\.try_push\s*\(" `
  tests/dungeon/stage10_validation_fixture.cpp
```

Also inspect added lines for writes to `hp`, `remaining_targets`, defeated
counts/bits, room phase, cooldowns, or the 12,000 budget. Existing read-only
diagnostics such as `monster.hp > 0` are allowed.

- [ ] **Step 6: Configure and build only the fixture serially**

From a VS2022 x64 Developer PowerShell:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage10_validation_fixture -- -j1
```

Both commands must exit 0. Do not build unrelated targets.

- [ ] **Step 7: Require direct GREEN before registered CTest**

Run the native executable exactly once with its true exit code preserved:

```powershell
$process = Start-Process `
  'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe' `
  -Wait -PassThru -NoNewWindow
$process.ExitCode
```

Require exit 0. If nonzero, do not run CTest and do not commit. Use the
failure-only lease diagnostics for a minimal fix round; stop after five failed
rounds and write a fresh root-cause/design card rather than weakening any
constraint.

Only after direct GREEN, run:

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
```

Require exactly 1/1 passed within the unchanged timeout.

- [ ] **Step 8: Final scope audit and exact commit**

```powershell
git diff --check
git diff --name-status
git status --short
```

Only the Stage 10 fixture and this plan may be staged. The superseded old plan
must remain untracked and unstaged. Commit only after GREEN:

```powershell
git add docs/superpowers/plans/2026-07-31-stage10-event-lease-recovery.md `
  tests/dungeon/stage10_validation_fixture.cpp
git commit -m "test: recover stage10 dense driver lease"
```

## Self-Review

- The lock has observable event/state exits and no arbitrary lifetime.
- A living resident outside all attack geometries cannot park the driver.
- Cooldown and combat animation cannot cause false lock expiry.
- One unreachable frozen stance forces real sweep movement before reacquisition.
- Selection and equal-distance behavior are deterministic.
- The fixture still earns every clear/abyss/save result through production
  combat and lifecycle code.
- No production behavior, time budget, timeout, assertion, or Stage B-G rule is
  changed.
