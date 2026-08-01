# Stage 10 Close-Facing Liveness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan task by
> task.

**Goal:** Remove the deterministic zero-input close-combat fixed point exposed
by the first population-scaled Stage 10 direct run.

**Architecture:** Preserve every inherited fixture and timeout change. Add one
per-lock public-input facing correction after the existing action decision;
fall back to the existing exclusion/sweep recovery if that single correction
does not make the same target attackable.

**Design:**
`docs/superpowers/specs/2026-07-31-stage10-close-facing-liveness-design.md`

## Global Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery` on
  `codex/task9-gate-recovery`, committed HEAD `feadac9`.
- Preserve the inherited uncommitted fixture+CMake binary diff hash
  `0e0846cdaf04a0eccb540b6396376a08aec0556c`.
- Modify only `tests/dungeon/stage10_validation_fixture.cpp` and this plan.
- Do not edit the inherited `tests/dungeon/CMakeLists.txt` change in this card.
- Never edit, stage, or commit superseded untracked Stage 10 plans.
- Do not change budget formula, timeout, production code, gameplay, population,
  thresholds, action priority, rewards, saves, or evidence semantics.
- Keep exactly one `session.tick(movement)` per driver iteration.
- Use only public movement/action APIs; no private mutation or defeat injection.
- Build and test serially. Do not run full CTest or launch the game.
- Use the recorded 48,914-tick RED without rerunning unchanged code.

---

### Task 1: Correct one facing-only close deadlock per lock

**Files:**

- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Add/commit:
  `docs/superpowers/plans/2026-07-31-stage10-close-facing-liveness.md`
- Test: `stage10.validation_fixture.real_abyss_transactions`
- Guard: `stage10.evidence.no_private_injection`

- [ ] **Step 1: Preserve scope and RED**

Confirm HEAD, empty staging area, inherited diff hash, and recorded terminal
signature:

```text
exit 8; budget 48,914; 486/600 defeated
player=(-32.9425,81.0109), facing left
target=(-32.4962,81.2404), close=true, geometry_light=false
attack/skill/input empty; sweep=false
```

- [ ] **Step 2: Add per-lock correction state and diagnostic**

Add `close_facing_turn_attempted` and `close_facing_corrections` to the fixture
driver state. Reset the per-lock flag in both acquire and release paths; print
both fields only in failure diagnostics.

- [ ] **Step 3: Detect a facing-only deadband candidate**

While an existing close lock is active, after normal
`movement_toward(player,target)` returns zero, recognize a candidate only when:

- player movement is controllable;
- `abs(dx) <= 1.70`, `abs(dy) <= 0.55`, and `abs(dx) > 0.20`;
- the player faces away from `sign(dx)`;
- the target is not in the existing light lane.

Do not send the input yet; retain a candidate flag/direction through the
existing Storm→Draw→light action decision.

- [ ] **Step 4: Correct once or enter existing recovery**

Only when `action.ready && !action.action_requested`:

- first candidate on this lock: set `movement.x=sign(dx)`, mark attempted,
  increment correction count, and do not arm ordinary close movement progress;
- later candidate on the same lock: release it with existing failure accounting,
  exclude the ordinal once, set `recover_until_light_lane=true` and
  `sweep_escape=true`, then use the existing deterministic sweep movement.

Do not queue an action in the correction branch. A disappeared target must
continue through the existing absent handoff.

- [ ] **Step 5: Static audit**

Require `git diff --check`, only the fixture newly changed by this card, no
forbidden injection symbols, no budget/timeout edits, and one driver-loop
`session.tick(movement)`.

- [ ] **Step 6: Serial build and one direct gate**

Use the known VS2022 x64 environment and SDK override, build only
`arpg_stage10_validation_fixture`, then run the executable once.

If exit 0, proceed. If exit 8, stop without CTest/commit and report whether the
old facing fixed-point signature is absent plus the complete new counters. Do
not change budget or timeout in this card.

- [ ] **Step 7: Exact tests after direct GREEN only**

Run serially:

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.evidence\.no_private_injection$' `
  --output-on-failure -j1
```

Require 1/1 passed for each.

- [ ] **Step 8: Scope audit and exact commit**

After GREEN and review, stage only the current accepted fixture, inherited
CMake timeout, this plan, and the preceding population-budget plan. Commit as:

```text
test: restore stage10 dense abyss transaction fixture
```

## Self-Review

- The correction is a real public movement input, never a state write.
- It cannot preempt a queued skill or attack and cannot oscillate per lock.
- A failed one-frame correction closes into existing exclusion/sweep recovery.
- No production or gameplay rule changes.
