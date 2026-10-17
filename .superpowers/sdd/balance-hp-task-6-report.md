# Task 6 Report — Atomic Health Potion Auto-Use

Status: `DONE_WITH_CONCERNS`

Commit: `37a0848` (`feat: atomically auto-consume health potions`)

## Scope

Implemented only Task 6 in the seven allowed source/test files. No codec,
`MaterialId`, inventory, UI, asset, monster balance, room balance, player
balance, or golden-file changes were made.

The runtime now requests eligible potions without distance checks, selects
ascending spawns, applies no HP/ground/receipt side effects before exact
commit, persists secondary claim bits, and folds final-kill claims into the
existing clear transaction. Clear batches stop strictly above 75%, consume at
most four, and abyss clear removes the rule before healing from post-clear max
HP.

## TDD evidence

### Phase A

RED:

```text
11 cases, 4 failures
[FAIL] threshold/distance: pending.has_value()
[FAIL] stable scan/recheck: session.pending_save().has_value()
[FAIL] exact receipt: pending.has_value()
[FAIL] focused count: actual 11 != expected 7
```

GREEN: `11 cases, 0 failures`.

### Phase B

RED:

```text
14 cases, 1 failures
[FAIL] cache mismatch: assert_unchanged_fault(wrong_kind, 500, 1U)
```

GREEN: `14 cases, 0 failures`.

### Phase C

RED:

```text
18 cases, 3 failures
[FAIL] clear save missing health_potion_claim
[FAIL] low-health clear missing pending save
[FAIL] test seam assumed sacrificed_max < 100
```

The third failure was a test-only fixed-max assumption. The assertion was
corrected to derive actual base max HP dynamically without weakening post-clear
ordering. GREEN: `18 cases, 0 failures`.

## Final verification

Build:

```text
cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1
ninja: no work to do.
```

Focused: `18 cases, 0 failures`.

Legacy equipment stress after fixture adaptation: `2 cases, 0 failures`.

Dungeon full:

```text
305 cases, 1 failures
```

The sole failure is the explicitly allowed, already registered abyss-stress
golden mismatch. It was not changed:

```text
expected: 0xe102b17e6423351b
actual:   0x0d55b41ebb0b1128
```

All other 304 cases passed. An earlier full run exposed six non-golden fixture
regressions (one equipment stress and five abyss transaction paths). The
allowed test-support seam now services exact single-potion commits for
synthetic non-final kills while leaving final-wave potions for the real clear
transaction; all six regressions disappeared on the next full run.

## Committed files

- `src/dungeon/dungeon_session.hpp`
- `src/dungeon/dungeon_session.cpp`
- `src/dungeon/dungeon_transition.cpp`
- `src/dungeon/dungeon_snapshot.cpp`
- `tests/dungeon/dungeon_health_potion_tests.cpp`
- `tests/dungeon/dungeon_test_support.hpp`
- `tests/dungeon/dungeon_test_main.cpp`

## Concern

Only the intentionally unchanged abyss-stress golden hash remains stale. No
other known Task 6 concern remains.

## Review hardening patch

Status: `DONE_WITH_CONCERNS`

Patch commit: `a83531f9ccbe3caf62a0de36e2ee08cf4af1b777`
(`fix: harden atomic health potion clear transactions`)

The review patch stayed inside four of the original seven allowed files. It
did not advance Task 7, modify the shared report, or alter a golden file.

### I1 — health-claim abyss-clear rollback

RED first failed because `not_committed` faulted instead of returning to
combat. After separating the protected disposition, the next fixed-frame
assertion remained RED because previously materialized abyss rewards could not
be reconstructed idempotently. A full run also caught an over-broad change to
the no-potion abyss-clear protection contract. The final implementation makes
only abyss clears carrying a health claim retryable, preserves every other
protected abyss kind and the legacy no-potion clear behavior, validates exact
existing abyss reward materials, and rebuilds the same pending state and claim
on the next fixed frame.

GREEN assertions cover unchanged HP, all ground potion/material fields,
stable state and claim bits, full receipt fields, closed exits, combat phase,
and deterministic retry state/claim.

### I2 — exact whole-state single-potion cache

RED accepted a pending state whose `current_room.depth` was polluted and then
published it. The first whole-state expected-copy implementation exposed an
existing zero-allocation regression. GREEN reuses the pre-reserved validation
scratch, copies stable state without allocation, applies exactly the expected
generation and one secondary claim bit, and checks the entire run state with
`same_run_state`.

### I3 — clear receipt kind

RED accepted explicit wrong kinds for both health-claim `room_clear` and
`abyss_clear`, publishing clear effects. GREEN requires every present result
kind on a health claim to equal the pending kind before publication. Missing
optional kinds remain compatible. Both normal and abyss paths assert mismatch
faults with unchanged stable state, HP, potion ground state, and receipt.

### I4 — oversized health claim

RED terminated under the MSVC STL assertion `array subscript out of range`
when material validation traversed `count=5`. GREEN validates `count` in
`1..kPendingHealthPotionClaimCapacity` before material-cache traversal. The
test also exercises the real commit path and verifies a safe mismatch fault
with no HP, ground, stable-state, or receipt side effects.

### Review verification

- Focused: `18 cases, 0 failures`.
- Equipment stress: `2 cases, 0 failures`.
- Full dungeon: `305 cases, 1 failures`; all other 304 cases passed.
- Sole failure remained the registered golden mismatch:
  expected `0xe102b17e6423351b`, actual `0x0d55b41ebb0b1128`.
- `git diff --check`: clean.

## Second review hardening — pre-combat abyss-clear retry

Status: `DONE_WITH_CONCERNS`

Patch commit: `78da4bf` (`fix: rebuild abyss potion clear before combat tick`)

The remaining I1 review case used a real `CombatWorld` thunderstorm hazard.
Its telegraph advances from 2 to 1 on the final-kill tick, so the old retry
path was genuinely reachable and damaged the player on the next tick before
rebuilding the clear transaction.

RED:

```text
18 cases, 1 failure
[FAIL] abyss_after_retry.combat->player.hp
    == abyss_before_failure.combat->player.hp
```

GREEN adds an explicit session retry flag only for a `not_committed`
`abyss_clear` carrying a health-potion claim. While set, combat actions and
active skills are rejected. The next session tick consumes the flag and
rebuilds the clear pending state before any `combat_->tick()` call, then
returns without advancing combat. Fault, construction, reset, transient
clear, and exact successful receipt paths clear the flag so it cannot become
stale. The legacy protected behavior for abyss clears without potions remains
unchanged.

The regression asserts unchanged HP and unchanged hazard telegraph, active,
lifetime, and latch state, plus rejected combat input and deterministic clear
claim reconstruction.

### Second review verification

- Focused: `18 cases, 0 failures`.
- Equipment stress: `2 cases, 0 failures`.
- Full dungeon: `305 cases, 1 failures`; all other 304 cases passed.
- Sole failure remained the registered golden mismatch:
  expected `0xe102b17e6423351b`, actual `0x0d55b41ebb0b1128`.
- `git diff --check`: clean.

## Third review hardening — public retry gate

Status: `DONE_WITH_CONCERNS`

Patch commit: `6ffa3cf1108fcd1ff2501221d29c3f32d30a3a59`
(`fix: gate abyss potion clear retry mutations`)

The remaining Important review finding was reachable after a
`not_committed` health-claim `abyss_clear`: phase had returned to `combat`,
pending was empty, and only combat actions and active casts were blocked.
Public pickup, equipment, crafting, skill-loadout, reset, and stray save-result
entry points could therefore create a competing pending transaction, clear the
retry flag, switch to `abyss_fail`, or enter fault before the pre-combat retry.

RED:

```text
19 cases, 2 failures
[FAIL] abyss clear health retry gates public mutations:
    retry_is_intact(nearby)
[FAIL] focused count: actual 19 != expected 18
```

The business failure proves `request_nearby_pickups()` created a competing
health-potion pending save during the retry window. The case-count failure was
the expected test-suite increment.

GREEN adds one narrow retry-gate predicate and checks it before every public
gameplay request or save-creating path, `reset_current_room()`, and both public
pending-save result entry points. The session tick remains the sole consumer of
the flag and still rebuilds the identical health-claim `abyss_clear` before any
combat tick. Read-only snapshot/state access and event observation are
unchanged. The regression uses fresh retry sessions to exercise nearby potion
pickup, valid equip, valid transmute craft, valid skill removal, abyss reset,
and both stray resolve paths. After every call it asserts combat phase, no
pending save, no fault, and an intact retry flag, then proves the next tick
rebuilds the original clear claim.

### Third review verification

- Build: `arpg_dungeon_tests` completed successfully.
- Focused: `19 cases, 0 failures`.
- Equipment stress: `2 cases, 0 failures`.
- Full dungeon: `306 cases, 1 failure`; all other 305 cases passed.
- Sole failure remained the registered golden mismatch:
  expected `0xe102b17e6423351b`, actual `0x0d55b41ebb0b1128`.
- `git diff --check` and staged diff check: clean.
- Only six of the seven allowed Task 6 files changed; tracked
  `.superpowers/sdd/task-6-report.md` was untouched.
