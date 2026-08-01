# Stage 10 Population-Scaled Fixture Budget Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan task by
> task.

**Goal:** Restore the real Stage 10 abyss transaction fixture after hundredfold
room scaling without changing gameplay or weakening full-clear evidence.

**Architecture:** Keep one complete public-API combat clear. Size its finite
driver budget from immutable initial room population, then fork the durable
post-clear checkpoint into independent claim and abandon transaction branches.

**Design:**
`docs/superpowers/specs/2026-07-31-stage10-population-scaled-fixture-budget-design.md`

## Global Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery` on
  `codex/task9-gate-recovery`, committed HEAD `db642ae`.
- Preserve the inherited uncommitted Stage 10 fixture diff whose pre-card hash
  is `0f6fd993948be99d1be604e066ec2bb3cebcdcdd`.
- Modify only `tests/dungeon/stage10_validation_fixture.cpp`,
  `tests/dungeon/CMakeLists.txt`, and this plan.
- Never edit, stage, or commit the four superseded untracked Stage 10 plans.
- Do not modify production source, gameplay rules, monster population,
  equipment, damage, fixed-step timing, exits, rewards, save semantics, or
  evidence thresholds.
- Do not inject defeats/events or call private/test-only combat mutation APIs.
- Keep exactly one `session.tick(movement)` per `drive_clear()` iteration.
- Use serial configure/build/test only. Do not run full CTest or launch the game.
- Preserve the recorded RED; direct GREEN must precede CTest and commit.

---

### Task 1: Scale the finite clear budget and reuse the durable clear

**Files:**

- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Add/commit:
  `docs/superpowers/plans/2026-07-31-stage10-population-scaled-fixture-budget.md`
- Test: `stage10.validation_fixture.real_abyss_transactions`
- Guard: `stage10.evidence.no_private_injection`

- [ ] **Step 1: Preserve RED and scope**

Do not rerun the unchanged failure. Record the latest direct result:

```text
native exit 8; 12,000 ticks; 184/600 defeated; wall 3.874s
Storm/Draw/light = 7/33/79; player HP = 713
fixture diff hash = 0f6fd993948be99d1be604e066ec2bb3cebcdcdd
```

Confirm committed HEAD, empty staging area, exact inherited diff hash, and no
tracked dirty file except the fixture.

- [ ] **Step 2: Add checked population-scaled budget**

Include the shared gameplay limits header. Add a small test-only helper using
explicit `std::uint64_t` arithmetic:

```text
scaled = ceil(initial_monster_count * 60000 / 736)
budget = max(12000, scaled)
```

Reject population 0, population above `arpg::limits::kRoomMonsterCapacity`, or
any result not representable as `std::uint32_t`. `drive_clear()` must snapshot
the real initial population after servicing pending saves, compute the budget
once, then iterate to that finite bound. Failure diagnostics print the actual
budget and initial population.

- [ ] **Step 3: Fork claim and abandon from one durable clear**

Immediately after the first successful `drive_clear()` and before
`wait_for_rewards()`, call `store.load()` and validate the durable clear:

- ready load state;
- lifecycle `cleared`;
- matching room seed and life-sacrifice rule;
- checkpoint generation equal to `clear_generation`;
- valid nonzero reward total and zero generated/claimed/abandoned masks.

Keep the original session's reward materialization, claim, and reload path.
Seed `abandon_store` with the captured cleared checkpoint instead of
`entry->target`; construct `abandon_session` from the commit's verified state.
Remove the second `drive_clear()` and its unused HP/generation variables.
Preserve all existing abandon warning, arming, neutral-release, resolution,
inventory, ground-item, and final output assertions.

- [ ] **Step 4: Set a finite registered timeout**

Change only the registered Stage 10 transaction fixture timeout from 30 to 60
seconds. Do not change any other test timeout or selector.

- [ ] **Step 5: Static scope and injection audit**

Run:

```powershell
git diff --check
git diff -- tests/dungeon/stage10_validation_fixture.cpp tests/dungeon/CMakeLists.txt
rg -n "relay_defeated\s*\(|events_\.try_push\s*\(|defeat_monster\s*\(" `
  tests/dungeon/stage10_validation_fixture.cpp
```

Require no forbidden injection, no production change, and exactly one
`session.tick(movement)` inside the driver loop.

- [ ] **Step 6: Serial target build and direct GREEN gate**

From the configured VS2022 x64 environment:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage10_validation_fixture --parallel 1
$process = Start-Process `
  'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe' `
  -Wait -PassThru -NoNewWindow
$process.ExitCode
```

Require configure 0, build 0, and direct exit 0. If direct execution fails, do
not run CTest or commit. Preserve diagnostics and return for root-cause review;
do not silently enlarge the formula or timeout.

- [ ] **Step 7: Exact registered tests**

Only after direct GREEN:

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.evidence\.no_private_injection$' `
  --output-on-failure -j1
```

Require exactly 1/1 passed for each selector and the transaction fixture below
60 seconds.

- [ ] **Step 8: Scope audit and exact commit**

Require `git diff --check`, empty pre-stage index, and exact allowed scope.
Stage only the plan, fixture, and CMake file, then commit:

```powershell
git add docs/superpowers/plans/2026-07-31-stage10-population-scaled-fixture-budget.md `
  tests/dungeon/stage10_validation_fixture.cpp tests/dungeon/CMakeLists.txt
git commit -m "test: scale stage10 fixture to room population"
```

## Self-Review

- One complete 600/valid-population combat clear still uses public production
  actions, movement, skills, fixed ticks, saves, and receipts.
- Claim and abandon both descend from that exact durable clear and still use
  production reward/save APIs.
- The budget is finite, checked, population-derived, and diagnostic-visible.
- No gameplay, production runtime, evidence threshold, or unrelated test is
  changed.
