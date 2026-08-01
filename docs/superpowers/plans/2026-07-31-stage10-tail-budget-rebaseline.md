# Stage 10 Sparse-Tail Budget Rebaseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan task by
> task.

**Goal:** Give the now-live dense Stage 10 public driver enough finite time to
finish the legal sparse tail while keeping failure cost bounded.

**Architecture:** Change only the test helper's checked population formula and
its registered timeout. Preserve the accepted dense driver, facing correction,
single real clear, durable checkpoint fork, and every transaction assertion.

**Design:**
`docs/superpowers/specs/2026-07-31-stage10-tail-budget-rebaseline-design.md`

## Global Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery` on
  `codex/task9-gate-recovery`, committed HEAD `e6793c6`.
- Preserve inherited uncommitted fixture+CMake diff hash
  `a8accd8782bd98656efb493902a2aba5c65002b4`.
- Modify only the budget helper in
  `tests/dungeon/stage10_validation_fixture.cpp`, the single Stage 10 timeout
  in `tests/dungeon/CMakeLists.txt`, and this plan.
- Never edit, stage, or commit the four superseded untracked Stage 10 plans.
- Do not change driver/navigation/action code, production code, gameplay,
  population, thresholds, rewards, saves, or evidence semantics.
- Keep exactly one `session.tick(movement)` per driver iteration.
- Build/test serially; no full CTest and no game launch.
- Use the recorded 48,914-tick RED without rerunning unchanged code.

---

### Task 1: Apply the final finite budget and verify Stage 10

**Files:**

- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Add/commit:
  `docs/superpowers/plans/2026-07-31-stage10-tail-budget-rebaseline.md`
- Test: `stage10.validation_fixture.real_abyss_transactions`
- Guard: `stage10.evidence.no_private_injection`

- [ ] **Step 1: Preserve scope and RED**

Confirm HEAD, empty staging, inherited hash, and recorded live-tail RED:

```text
exit 8; 48,914 ticks; 562/600 defeated; 38 remaining; 19.4s
lock none; sweep_escape true; all 12 recoveries reacquired
```

- [ ] **Step 2: Replace only the budget formula**

Keep existing population/capacity/result validation and `uint64_t` arithmetic.
Replace the old scaled constants/calculation with:

```text
budget = 12000 + 96 * initial_monster_count
```

Keep the result as `uint32_t` after the existing checked narrowing. Preserve
all diagnostics and callers.

- [ ] **Step 3: Change only the transaction timeout**

Change `stage10.validation_fixture.real_abyss_transactions` from the inherited
60 seconds to 75 seconds. No other CMake line or test timeout may change.

- [ ] **Step 4: Static audit**

Require `git diff --check`, only the two allowed tracked files, no new driver
changes, no forbidden injection, and exactly one driver-loop
`session.tick(movement)`.

- [ ] **Step 5: Serial target build and one direct gate**

Use the known VS2022 x64/SDK environment, build only
`arpg_stage10_validation_fixture`, and run the executable exactly once.

Require exit 0. If nonzero, stop without CTest/staging/commit and preserve the
complete terminal diagnostics. Do not enlarge the formula in this card.

- [ ] **Step 6: Exact registered tests after direct GREEN**

Run serially:

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.evidence\.no_private_injection$' `
  --output-on-failure -j1
```

Require 1/1 passed for each and transaction runtime below 75 seconds.

- [ ] **Step 7: Controller review and exact commit**

Do not stage or commit in the implementation subtask. After independent spec
and code review, the controller stages only:

- `tests/dungeon/stage10_validation_fixture.cpp`;
- `tests/dungeon/CMakeLists.txt`;
- `docs/superpowers/plans/2026-07-31-stage10-population-scaled-fixture-budget.md`;
- `docs/superpowers/plans/2026-07-31-stage10-close-facing-liveness.md`;
- `docs/superpowers/plans/2026-07-31-stage10-tail-budget-rebaseline.md`.

Commit message:

```text
test: restore stage10 dense abyss transaction fixture
```

## Self-Review

- Formula exactly equals `12000 + 96N`; timeout exactly 75 seconds.
- No behavior change beyond the already approved inherited driver fixes.
- Full clear, rewards, claim/reload, abandon, and environment evidence remain.
- Production and unrelated tests remain untouched.
