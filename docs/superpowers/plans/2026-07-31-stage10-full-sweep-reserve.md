# Stage 10 Full-Sweep Reserve Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan task by
> task.

**Goal:** Add exactly one room-scale sweep cycle to the fixed tail reserve so
the already-live Stage 10 driver can discover the final nonresident target.

**Architecture:** Preserve all inherited behavior and the 96-tick population
slope. Change only the test helper base from 12,000 to 24,000; keep the
registered 75-second timeout unchanged.

**Design:**
`docs/superpowers/specs/2026-07-31-stage10-full-sweep-reserve-design.md`

## Global Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery` on
  `codex/task9-gate-recovery`, committed HEAD `898755d`.
- Preserve inherited fixture+CMake diff hash
  `811392e3c7d0e5857f68feb43fbba45e44cbfc97`.
- Modify only the existing base constant in
  `tests/dungeon/stage10_validation_fixture.cpp` and this plan.
- Do not edit CMake; timeout remains exactly 75 seconds.
- Never edit, stage, or commit the four superseded untracked Stage 10 plans.
- Do not change the 96N slope, driver, waypoint, action code, production code,
  gameplay, population, rewards, saves, assertions, or evidence semantics.
- Build/test serially; no full CTest and no game launch.
- Use the recorded 69,600-tick RED without rerunning unchanged code.

---

### Task 1: Add one fixed sweep cycle and close Stage 10

**Files:**

- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Add/commit: `docs/superpowers/plans/2026-07-31-stage10-full-sweep-reserve.md`
- Test: `stage10.validation_fixture.real_abyss_transactions`
- Guard: `stage10.evidence.no_private_injection`

- [ ] **Step 1: Preserve scope and RED**

Confirm HEAD, empty staging, inherited hash, and:

```text
exit 8; 69,600 ticks; 599/600 defeated; one remaining; 29.638s
lock none; sweep_escape true; waypoint 4; sweep_ticks 35,172
```

- [ ] **Step 2: Change exactly one constant**

In `clear_tick_budget()`, change `kBaseBudget` from `12000U` to `24000U`.
Leave `kTicksPerMonster=96U`, capacity validation, `uint64_t` arithmetic,
narrowing, diagnostics, callers, and every other line unchanged.

- [ ] **Step 3: Static audit**

Require `git diff --check`, exact one-line behavioral delta for this card, no
CMake/driver/injection change, and one driver-loop `session.tick(movement)`.

- [ ] **Step 4: Serial target build and one direct gate**

Use the known VS2022 x64/SDK environment. Build only
`arpg_stage10_validation_fixture`, then run it once.

Require exit 0. If nonzero, stop without CTest/staging/commit; do not add more
budget. Preserve the complete terminal state for residency/waypoint diagnosis.

- [ ] **Step 5: Exact tests after direct GREEN**

Run serially:

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.evidence\.no_private_injection$' `
  --output-on-failure -j1
```

Require exactly 1/1 passed for each and transaction runtime below 75 seconds.

- [ ] **Step 6: Controller review and exact commit**

Do not stage/commit in the implementation subtask. After independent review,
the controller stages only the fixture, inherited CMake timeout, and these four
accepted recovery plans:

- population-scaled fixture budget;
- close-facing liveness;
- sparse-tail budget rebaseline;
- full-sweep reserve.

Commit message:

```text
test: restore stage10 dense abyss transaction fixture
```

## Self-Review

- Final formula exactly `24000 + 96N`; timeout remains 75.
- This card has one behavioral source-line change.
- Full clear and every existing transaction/evidence assertion remain intact.
- No production or unrelated test changes.
