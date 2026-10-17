# Stage 10 Continuous Melee Chain Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan task by
> task.

**Goal:** Eliminate repeated five-unit ranged-stance retreats between dense-room
targets by preserving melee engagement intent across deterministic target
handoffs.

**Architecture:** Preserve the inherited equipment, event lease,
close-for-light transition, corrected sweep, and action priority. Promote melee
intent from per-lock state to fixture driver state. Once the existing ranged
decision enters close mode, every later resident acquisition stays close and
prefers a target already in the real light lane before falling back to nearest
distance.

**Design:**
`docs/superpowers/specs/2026-07-31-stage10-continuous-melee-chain-design.md`

## Global Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery` on
  `codex/task9-gate-recovery`, committed HEAD `8b99ef7`.
- Preserve the inherited uncommitted Stage 10 fixture diff whose pre-card hash
  is `8e80991684cab8297d072a5b23e0504f36d51f78`.
- Modify only `tests/dungeon/stage10_validation_fixture.cpp` and this plan.
- Never edit/stage/commit any superseded untracked Stage 10 plan.
- Keep all configure/build/run/test activity serial with
  `CMAKE_BUILD_PARALLEL_LEVEL=1` and `-j1`.
- Do not add combo buffering in this card.
- Do not change production source, equipment, route/thresholds, gameplay,
  density, bounds, goldens, 12,000-tick budget, timeout, assertions, or Stage
  B-G behavior.
- Never inject defeat/events, call defeat helpers, or write HP,
  defeat/remaining/phase/cooldown/input/save state.
- Keep exactly one `session.tick(movement)` per driver-loop iteration.
- Do not delete/reset/clean or touch another worktree. Do not run full CTest.
- Preserve RED without rerunning it before the change. Direct GREEN must precede
  CTest and commit.

---

### Task 1: Preserve close engagement across target handoffs

**Files:**

- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Add/commit:
  `docs/superpowers/plans/2026-07-31-stage10-continuous-melee-chain-recovery.md`
- Test: existing `stage10.validation_fixture.real_abyss_transactions`

- [ ] **Step 1: Preserve RED and scope**

Use the latest recorded run without rerunning it:

```text
native exit 8; 12,000 ticks; 144/600 defeated
Storm/Draw/light = 7/35/51
lock acquisitions/absent releases = 75/68
sweep ticks = 1,623; sweep escape false
player HP 768/768; barrier 51
```

Confirm HEAD, empty staging, inherited fixture diff hash, and only the fixture
as tracked dirty.

- [ ] **Step 2: Add persistent melee intent and counters**

Add `bool melee_chain{}` to `DenseDriverState`, plus failure-only counters for:

- melee-chain entry;
- melee handoffs after an absent lock;
- in-light-lane melee acquisitions;
- nearest-resident melee acquisitions;
- ranged acquisitions after melee-chain entry;
- cumulative initial squared distance for melee acquisitions.

Set `melee_chain` at the same completed public action decision that currently
sets `close_for_light`. Do not clear it in lock acquire/release, action
completion, hurt/hit-stop, cooldown, or sweep transitions. Reset only through
construction of a new driver state.

- [ ] **Step 3: Deterministically select the next melee target**

Extend the existing resident selection helper with a melee-chain mode. Honor
the one-attempt stalled ordinal first. When melee mode is active:

1. select among active living residents satisfying existing
   `in_attack_lane()`;
2. otherwise select among all active living residents;
3. within either tier choose lower squared XY distance, then lower ordinal for
   equal distance.

When acquired in melee mode, preserve the ordinal but set per-lock
`close_for_light=true` immediately and navigate with existing
`movement_toward(player.position, target.position)`. Do not rebuild/walk a
`kDrawSlashRange` stance. Before melee-chain entry, preserve existing ranged
acquisition exactly.

When an active melee lock disappears, increment the handoff counter and acquire
the next resident under the same melee mode. If none exists, use the existing
sweep; the next discovered resident remains melee. Retain the existing
close-movement no-progress release/exclusion/sweep behavior.

If direct evidence shows that a close-pursuit stall repeatedly reacquires a
blocked nearest target after only one successful sweep tick, add
`recover_until_light_lane`: a close stall enters it; it continues the existing
deterministic sweep regardless of ordinary XY progress; it exits only when an
active living resident satisfies the existing `in_attack_lane()`, which is
then acquired through the same tier-1 distance/ordinal order. Do not add a
timeout, disable collision checks, or change ordinary no-resident sweep.

- [ ] **Step 4: Extend diagnostics and perform static audit**

Print the new mode/counters only on failure. Run:

```powershell
git diff --check
git diff -- tests/dungeon/stage10_validation_fixture.cpp
rg -n "relay_defeated\s*\(|events_\.try_push\s*\(" `
  tests/dungeon/stage10_validation_fixture.cpp
```

Audit added lines for forbidden writes, combo buffering, budget/threshold edits,
and exactly one driver-loop `session.tick(movement)`.

- [ ] **Step 5: Serial build and single direct gate**

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

Require configure/build/direct exit 0. On nonzero direct, do not run CTest or
commit; use failure diagnostics for at most three evidence-driven rounds before
writing a new root-cause/design card.

- [ ] **Step 6: Registered selector only after direct GREEN**

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
```

Require exactly 1/1 passed within the unchanged timeout.

- [ ] **Step 7: Scope audit and exact commit**

Require `git diff --check`, empty pre-stage index, and exact scope. Stage only
the new plan and fixture:

```powershell
git add docs/superpowers/plans/2026-07-31-stage10-continuous-melee-chain-recovery.md `
  tests/dungeon/stage10_validation_fixture.cpp
git commit -m "test: preserve stage10 melee target chain"
```

## Self-Review

- Ranged behavior is unchanged until the existing close transition.
- Once melee begins, ordinary target handoffs cannot force a ranged retreat.
- Empty resident regions and blocked close movement still use deterministic
  sweep/exclusion recovery.
- No combo input or production/evidence/budget change is hidden in this card.
