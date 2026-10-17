# Task 7B Fixed-Step and Death-Path Encapsulation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan task by
> task, with an independent review gate before each commit.

**Goal:** Move the Host validation death-continue decision, fixed-step input,
tick observation, Stage11D post-tick observation, and Stage10/11 reached
decision behind `HostValidationRuntime` without changing gameplay or evidence
behavior.

**Architecture:** Keep the real gameplay transaction and loop authority in
`raylib_host.cpp`. The opaque PImpl facade owns validation-only decisions and
state mutations. Preserve the temporary Task 7A `HostValidationStateAccess`
for Task 7C presentation/capture/report consumers.

**Tech Stack:** C++17, raylib 6.0, CMake/CTest, MSVC 19.44, Windows SDK
10.0.26100.0.

## Global Constraints

- Work only in `E:/game/.worktrees/whole-game-refactor-2026-07` on branch
  `codex/whole-game-refactor-2026-07`.
- Task BASE is commit `e98e042`.
- This is a structural refactor only. Preserve gameplay, input, death,
  persistence, fixed-step, snapshot, event, inventory, rendering, RNG, and
  audio behavior exactly.
- Before each build/test, prove there is no second `cmake`, `ctest`, `ninja`,
  `cl`, or `link` chain. Build and test serially with `-j1`.
- Do not modify or stage the tracked progress ledger, ignored task reports, or
  unrelated untracked `.pyc`.
- Do not edit Stage headers or Stage implementation sources. State algorithms
  remain in their current owners; the facade delegates to them.

## Corrected file scope

The master plan's five-file list cannot pass its own mandatory selector. The
following ten-file scope is the minimum correction because Stage10/11/11D
evidence guards still bind the direct Host ownership that Task 7B removes, and
the Stage11D guard is already stale on BASE after Task 7A.

**Production:**

- Modify `src/platform/raylib/host_validation.hpp`
- Modify `src/platform/raylib/host_validation_runtime.cpp`
- Modify `src/platform/raylib/raylib_host.cpp`

**Central guards:**

- Modify `tests/platform/host_validation_source_test.cmake`
- Modify `tests/platform/host_validation_sequence_guard_test.cmake`

**Owner evidence guards:**

- Modify `tests/dungeon/stage10_evidence_guard_test.cmake`
- Modify `tests/dungeon/stage11_death_evidence_guard_test.cmake`
- Modify `tests/dungeon/stage11_death_evidence_guard_self_test.cmake`
- Modify `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Modify `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`

No CMake registration change is required. Derive
`host_validation_runtime.cpp` from the existing Host header/source directory
inside owner guards, while retaining their existing override contracts.

## Interfaces

Add these exact facade methods:

```cpp
[[nodiscard]] bool should_continue_death(
    const dungeon::DungeonSnapshot&) const noexcept;
combat::MovementInput fixed_step_movement(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    combat::MovementInput production_input) noexcept;
void observe_fixed_tick() noexcept;
void observe_death_continue_result(
    dungeon::RequestResult) noexcept;
void observe_post_fixed_tick(
    const dungeon::DungeonSnapshot&,
    const items::ItemOwnershipState*) noexcept;
[[nodiscard]] bool fixed_step_target_reached(
    const dungeon::DungeonSnapshot&) const noexcept;
```

Add only the required raylib-free forward declarations:

```cpp
namespace arpg::combat { struct MovementInput; }
namespace arpg::dungeon {
class DungeonSession;
enum class RequestResult : std::uint8_t;
}
namespace arpg::items { struct ItemOwnershipState; }
```

## Explicit boundary decisions

- Keep the Host's existing `if (!step_death)` gate. Call
  `fixed_step_movement()` only inside it; do not duplicate or move the death
  check into the facade.
- `fixed_step_movement()` is a scenario-selection chain, not a value fallback:
  Stage11 enabled returns only Stage11 output; otherwise Stage10 enabled
  returns only Stage10 output; otherwise return `production_input`. A zero
  Stage11 output must not fall through.
- Keep the existing Stage17 `observe_snapshot(current)` call separate.
  `observe_post_fixed_tick()` owns only the Stage11D abyss-claim observation
  against the same post-tick snapshot and item ownership.
- Task 7B removes the only Host write/increment of
  `stage11b_validation_state.fixed_ticks`. Four presentation-only reads remain
  in Host for Task 7C; do not add a temporary getter or move presentation code.
- Keep Stage10/11 presentation reached checks and Stage11D `abyss_claimed`
  presentation reads in Host until Task 7C. Guards must scope their
  prohibitions to the death segment and fixed-step loop, not the entire file.
- Keep the seven-accessor `HostValidationStateAccess` shim unchanged.

---

### Task 7B.0: Repair the Stage11D guard prerequisite

**Files:**

- Modify `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Modify `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`

**Produces:** A green evidence guard for the Task 7A BASE ownership before any
Task 7B production change.

- [ ] **Step 1: Confirm the precise BASE RED**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug \
  -R '^stage11d\.loot_evidence_guard$' --output-on-failure -j1
```

Expected: FAIL at the stale Host `runtime_input` seam/direct
`inject_stage11d_physical_edges` ownership requirement.

- [ ] **Step 2: Retarget only Task 7A input ownership**

Read `host_validation_runtime.cpp` as a distinct source. Require the Host's
single `validation_runtime->inject_physical_edges(...)` binding and require
the exact Stage11D leg inside `HostValidationRuntime::inject_physical_edges()`:

```cpp
const PhysicalKeySnapshot stage11d_physical_keys =
    host_validation::inject_stage11d_physical_edges(
        stage11c_physical_keys, *impl_->config, input_settings,
        dungeon_snapshot, impl_->states.stage11d);
```

Remove only the obsolete Host `runtime_input` marker dependency. Preserve all
Stage11D physical-driver, fixed-step activation, evidence, render, capture,
summary, and negative-mutation checks.

- [ ] **Step 3: Move the matching mutation fixture**

The self-test must mutate the real facade runtime Stage11D input leg and still
reach the original Stage11D binding diagnostic. Reject comment/string/raw,
inactive, lambda/cross-scope, and wrong-argument decoys.

- [ ] **Step 4: Verify and independently review the prerequisite**

Run direct guard and self-test serially, then `git diff --check`. An independent
reviewer must confirm that this is only a Task 7A ownership realignment.

- [ ] **Step 5: Commit the prerequisite**

```powershell
git add tests/platform/stage11d_loot_evidence_guard_test.cmake \
  tests/platform/stage11d_loot_evidence_guard_self_test.cmake
git commit -m "test: realign stage11d validation ownership"
```

---

### Task 7B.1: Write the final owner guards and confirm RED

**Files:**

- Modify `tests/platform/host_validation_source_test.cmake`
- Modify `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify `tests/dungeon/stage10_evidence_guard_test.cmake`
- Modify `tests/dungeon/stage11_death_evidence_guard_test.cmake`
- Modify `tests/dungeon/stage11_death_evidence_guard_self_test.cmake`
- Modify `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Modify `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`

**Produces:** Source guards that bind the six new facade methods and reject the
old direct Host ownership.

- [ ] **Step 1: Extend the facade/runtime exact contracts**

Use Task 7A's active-plus-lexical unique definition validator. Require all six
declarations and definitions, exact arguments/qualifiers, and exact ownership:

```cpp
bool HostValidationRuntime::should_continue_death(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    const bool pending = snapshot.death.has_value()
        && snapshot.death->can_continue && !snapshot.death->saving;
    const auto scenario = impl_->config->stage11_validation;
    const bool validation_continue =
        scenario == Stage11ValidationScenario::deep_continue
        || scenario == Stage11ValidationScenario::floor_one_continue;
    return pending && validation_continue
        && !impl_->states.stage11.continue_requested;
}
```

`observe_death_continue_result()` must accept every result other than
`rejected`, including `faulted`, and only for the two continue scenarios.
It must never call `request_death_continue()`.

`observe_fixed_tick()` contains exactly
`++impl_->states.stage11b.fixed_ticks;`.

`observe_post_fixed_tick()` only null-checks ownership and delegates to
`observe_stage11d_abyss_claim(states.stage11d, snapshot, *ownership)`; it must
not snapshot, submit a pickup/claim, or call Stage17 observation.

`fixed_step_target_reached()` must be the short-circuit expression
`stage10_validation_reached(...) || stage11_validation_reached(...)` in that
order.

- [ ] **Step 2: Bind Host data flow and reachability**

Within the real `run_raylib_host()` death segment require:

```text
host_death_input_gate
-> should_continue_death(current) sets only death_gate.continue_death
-> if (death_gate.continue_death)
-> the unique real runtime.request_death_continue()
-> accepted/faulted snapshot refresh
-> observe_death_continue_result(death_continue_result)
```

Within the direct fixed-step loop require:

```text
previous = current
-> if (!step_death) fixed_step_movement(*session, current, movement) RHS
-> runtime.fixed_tick(...)
-> observe_fixed_tick()
-> session->snapshot(current)
-> observe_snapshot(current)
-> observe_post_fixed_tick(current, &session->item_state())
-> death/passive/inventory cleanup
-> drain_events(...)
-> if (fixed_step_target_reached(current)) break
```

Forbid direct old input/reached calls, the direct Stage11 continue write, the
direct Stage11B increment, and the direct Stage11D claim call only in their
affected executable segments. Presentation reads remain allowed until 7C.

- [ ] **Step 3: Add adversarial mutations**

Each must fail the shared validator for its intended reason:

- correct call in comment/string/raw/inactive/lambda/dead/cross-scope;
- discarded `fixed_step_movement()` result;
- Stage11 zero-output fallback to Stage10/production;
- duplicate, missing, or pre-`fixed_tick` `observe_fixed_tick()`;
- `observe_post_fixed_tick(previous, ...)`, null/wrong ownership, after-drain
  placement, or a facade-side request/extra snapshot;
- discarded target result followed by unconditional `break`;
- Host-side direct Stage state write hidden beside a facade call;
- `observe_death_continue_result()` before the real request/snapshot or with a
  fabricated result;
- `result == accepted` in place of `result != rejected`.

- [ ] **Step 4: Retarget Stage owner evidence guards**

Stage10 and Stage11 guards must read the facade runtime owner for input/reached
delegation while keeping Stage algorithms in `host_validation_stage10_11.cpp`.
Stage11 Host evidence still requires exactly one real
`runtime.request_death_continue()` under the real death gate. Update its
self-test mutations to the new `should_continue_death()` and observer chain.

Stage11D evidence must move only abyss observer ownership from Host into
`observe_post_fixed_tick()` and keep Stage11D runtime algorithm checks. Update
its self-test to mutate the real new owner.

- [ ] **Step 5: Confirm precise RED**

Run the two central direct guards. Expected first failure: missing facade API
or missing Host facade call, not a CMake parse error and not an unrelated old
owner diagnostic.

---

### Task 7B.2: Implement death and fixed-step facade ownership

**Files:**

- Modify `src/platform/raylib/host_validation.hpp`
- Modify `src/platform/raylib/host_validation_runtime.cpp`
- Modify `src/platform/raylib/raylib_host.cpp`

- [ ] **Step 1: Add forward declarations and six methods**

Use the exact interfaces above. Do not include raylib or Stage headers in the
public facade.

- [ ] **Step 2: Replace only the death validation decision/state write**

The Host remains the sole owner of `runtime.request_death_continue()`, the
snapshot refresh, and clean-exit behavior. Call the observer after that refresh
block with the real/default result.

- [ ] **Step 3: Replace fixed-step validation selection and observers**

Keep `if (!step_death)` in Host and assign the facade return directly to
`step_movement`. Keep the real `runtime.fixed_tick()` and post-tick snapshot in
Host. Insert the two observers at the exact order above and replace the two
reached/break blocks with one facade condition.

- [ ] **Step 4: Compare against BASE**

Normalize and compare the death segment and fixed-step loop against `e98e042`.
Every removed statement must map one-for-one to a facade statement; no
production authority, snapshot, cleanup, drain, or break statement may move.

---

### Task 7B.3: Build, verify, review, and commit

- [ ] **Step 1: Build serially**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
cmake --build --preset windows-msvc-debug \
  --target arpg_game arpg_dungeon_tests arpg_platform_tests -- -j1
```

- [ ] **Step 2: Run focused ownership guards first**

Run the two central guards plus Stage10 evidence/self-test, Stage11 death
evidence/self-test, Stage11B guard/self-test, and Stage11D evidence/self-test
serially. Fix only Task 7B failures; do not relax a diagnostic.

- [ ] **Step 3: Run the prescribed selector**

```powershell
ctest --test-dir out/build/windows-msvc-debug \
  -R '^(platform\.units|platform\.host_validation_(source|sequence)|stage10\.|stage11\.|stage11b\.|stage11d\.)' \
  --output-on-failure -j1
git diff --check
```

The selector currently lists 38 tests. Record known BASE failures separately;
do not claim them fixed and do not weaken Task 7B gates because of them.

- [ ] **Step 4: Independent review**

Require Critical 0 and Important 0 for the latest complete diff. Review must
check exact behavior mapping, scope containment, all owner evidence guards,
and adversarial mutation reachability.

- [ ] **Step 5: Write report and commit exact files**

Write the ignored report to
`.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task-7b-report.md`.
Stage only the ten Task 7B files listed above, excluding progress and `.pyc`:

```powershell
git add src/platform/raylib/host_validation.hpp \
  src/platform/raylib/host_validation_runtime.cpp \
  src/platform/raylib/raylib_host.cpp \
  tests/platform/host_validation_source_test.cmake \
  tests/platform/host_validation_sequence_guard_test.cmake \
  tests/platform/stage11d_loot_evidence_guard_test.cmake \
  tests/platform/stage11d_loot_evidence_guard_self_test.cmake \
  tests/dungeon/stage10_evidence_guard_test.cmake \
  tests/dungeon/stage11_death_evidence_guard_test.cmake \
  tests/dungeon/stage11_death_evidence_guard_self_test.cmake
git commit -m "refactor: encapsulate host validation fixed step"
```

Return `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED` with exact
build/test/review evidence.
