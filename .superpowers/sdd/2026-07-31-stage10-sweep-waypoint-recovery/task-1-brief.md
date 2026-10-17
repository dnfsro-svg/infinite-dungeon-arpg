### Task 1: Make sweep arrival follow actual movement output

## Binding Global Constraints

- Work only in `E:\game\.worktrees\task9-gate-recovery` on branch
  `codex/task9-gate-recovery`, committed HEAD `f607677`.
- Preserve the existing fixture diff whose pre-card hash is
  `3af6eec94e088467368c72c250c44c3c4447340f`.
- Modify only `tests/dungeon/stage10_validation_fixture.cpp` and
  `docs/superpowers/plans/2026-07-31-stage10-sweep-waypoint-recovery.md`.
- Never edit, stage, or commit either superseded untracked Stage 10 plan.
- Keep configure/build/direct/CTest serial with
  `CMAKE_BUILD_PARALLEL_LEVEL=1` and `-j1`.
- Do not change production code, route/order/rows, movement thresholds,
  gameplay/equipment/density/bounds/goldens, budget, timeout, assertions, or
  Stage B-G behavior.
- Do not inject defeat/events, call defeat helpers, or write HP, defeat,
  target, phase, cooldown, or save state.
- Keep exactly one `session.tick(movement)` in the driver loop.
- Do not delete/reset/clean or run full CTest. Direct GREEN must precede CTest
  and commit.

**Files:**

- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Add/commit:
  `docs/superpowers/plans/2026-07-31-stage10-sweep-waypoint-recovery.md`
- Test: existing `stage10.validation_fixture.real_abyss_transactions`

- [ ] **Step 1: Preserve RED evidence and scope**

Use the recorded last run without rerunning it:

```text
native exit 8 after 12,000 ticks
119/600 defeated; 481 remaining
sweep_escape=1; sweep_ticks=3599; waypoint=0
player=(-76.7703,-60.7451); current waypoint x about -77.178
all control/cooldown states empty; all attack geometry false
```

Confirm committed HEAD, empty staging, the pre-card fixture diff hash, and that
only the fixture is tracked dirty.

- [ ] **Step 2: Replace the separate arrival box**

In `sweep_movement()`:

1. compute the current waypoint as today;
2. compute `MovementInput movement = movement_toward(player, target)`;
3. if `movement.x != 0 || movement.y != 0`, return it unchanged;
4. otherwise advance `state.waypoint` exactly once modulo the existing
   waypoint count, recompute the target, and return
   `movement_toward(player, target)`.

Remove only the separate `fabs(... <= 0.25F)` waypoint-arrival branch. Do not
loop, change `movement_toward()`, duplicate its thresholds, alter the route, or
touch lease/action logic.

- [ ] **Step 3: Static audit before execution**

```powershell
git diff --check
git diff -- tests/dungeon/stage10_validation_fixture.cpp
rg -n "relay_defeated\s*\(|events_\.try_push\s*\(" `
  tests/dungeon/stage10_validation_fixture.cpp
```

Audit added lines for direct state mutation, threshold/budget edits, and verify
the driver loop still contains exactly one `session.tick(movement)`.

- [ ] **Step 4: Configure/build serially, then direct once**

From VS2022 x64 Developer PowerShell:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage10_validation_fixture -- -j1
$process = Start-Process `
  'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe' `
  -Wait -PassThru -NoNewWindow
$process.ExitCode
```

Require configure/build exit 0 and native direct exit 0. If direct is nonzero,
do not run CTest or commit; preserve its failure diagnostics and allow at most
three evidence-driven fix rounds before a new root-cause/design card.

- [ ] **Step 5: Run only the registered selector after direct GREEN**

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
```

Require exactly 1/1 passed within the unchanged timeout.

- [ ] **Step 6: Scope audit and exact commit**

Require `git diff --check`, an empty pre-stage index, and exact scope. Stage
only the new plan and fixture:

```powershell
git add docs/superpowers/plans/2026-07-31-stage10-sweep-waypoint-recovery.md `
  tests/dungeon/stage10_validation_fixture.cpp
git commit -m "test: recover stage10 sweep waypoint"
```

## Self-Review

- Zero movement and waypoint arrival cannot disagree.
- A collision that blocks nonzero requested movement is still handled by the
  existing progress check rather than misclassified as arrival.
- The deterministic route and legal input path remain unchanged.
- No production behavior, fixture budget/timeout/assertion, or evidence rule is
  weakened.
