# Task 7B implementation report

## Scope

- Worktree: `E:/game/.worktrees/whole-game-refactor-2026-07`
- BASE: `e98e042`
- Prerequisite guard commit: `d2792fc`
- Structural refactor only. Gameplay, input priority, death continuation,
  fixed-step timing, snapshots, cleanup, event draining, target short-circuit,
  rendering, persistence, RNG, and audio behavior remain unchanged.
- Only the ten corrected Task 7B files are eligible for the implementation
  commit. The tracked progress ledger remains unstaged.

## TDD and implementation

- Added final RED owner guards before production changes. The central source,
  central sequence, Stage10, Stage11, and Stage11D guards each rejected the
  missing Task 7B facade owner for their intended diagnostic.
- Added six `HostValidationRuntime` operations for death eligibility,
  fixed-step movement selection, tick observation, death-result observation,
  Stage11D post-tick observation, and fixed-step validation target completion.
- Moved only validation decisions and observers. The Host remains the sole
  owner of the death request, fixed tick, snapshot refresh, cleanup,
  `drain_events`, and loop break.
- Preserved movement priority exactly: Stage11, otherwise Stage10, otherwise
  production movement. A zero Stage11 result never falls through.
- Preserved death order exactly: eligibility, real request, snapshot refresh
  for every non-rejected result, then observer. Faulted remains observable.
- Preserved fixed-step order exactly: previous snapshot, movement selection,
  fixed tick, fixed-tick observation, snapshot, existing snapshot observer,
  Stage11D post-tick observation, cleanup, event drain, validation target.

## Guard hardening and independent review

- Central source and sequence mutations run the same shared validators as the
  production files; legacy fixture paths cannot bypass Task 7A/7B ownership.
- Active and lexical inventories fold C++ phase-2 line splices and reject
  comment, string, raw-string, inactive, lambda, dead, cross-scope, duplicate,
  wrong-argument, discarded-result, and reordered-call decoys.
- Stage10 binds the unique real `arpg::platform::run_raylib_host` body and the
  correct runtime namespace. Central runtime validation binds all six APIs to
  the exact `arpg::platform` namespace and rejects duplicate/prefixed namespace
  variants. Stage11D does the same for the post-tick owner and uses a unique
  drain-to-target anchor for the after-drain mutation.
- Final independent Round 3 review: Critical 0, Important 0, Minor 1; approved
  for commit. The Minor is a prefix token in the independent Stage11 guard;
  the central exact namespace validator rejects that same production-source
  bypass, so it is not a Task 7B correctness blocker.

## Build and focused verification

- Serial MSVC build of `arpg_game`, `arpg_dungeon_tests`, and
  `arpg_platform_tests`: PASS in 219 s. The only warning is the pre-existing
  `%zu`/`int` format warning in `dungeon_abyss_stress_tests.cpp`.
- `platform.host_validation_source`: PASS, 41.73 s.
- `platform.host_validation_sequence`: PASS, 371.68 s in the final selector.
- `stage10.evidence.no_private_injection`: PASS, 6.74 s after final hardening.
- `stage10.evidence.mutation_self_test`: PASS, 41.12 s.
- `stage11.death_evidence.production_paths`: PASS, 17.27 s.
- `stage11.death_evidence.mutation_self_test`: PASS, 18.63 s.
- Stage11B evidence guard plus self-test: PASS, 2/2 in 22.93 s.
- Stage11D evidence guard: PASS, 12.47 s.
- Stage11D 55-bad / 2-harmless mutation self-test: PASS, 710.84 s.
- `git diff --check`: PASS, with only line-ending conversion warnings.

## Prescribed 38-test selector

- The exact selector ran all 38 tests serially. The outer command wrapper hit
  its 3604 s wait limit, but the original CTest PID continued without a second
  launch and completed naturally. `LastTest.log` and
  `LastTestsFailed.log`, both written at completion, prove the first and last
  selected tests and the complete 38-test inventory.
- Result: 28 passed and 10 failed/fixture-skipped. Every Task 7B ownership,
  architecture, source, sequence, and adversarial-mutation gate passed,
  including `platform.units` (497 cases), both central guards, the 1479.25 s
  sequence self-test, all Stage10/11 evidence guards and self-tests, both
  Stage11B guards, and every Stage11D guard/self-test.
- The ten non-gate results reproduce already-recorded BASE failures:
  - Task 2's six Stage10/11 failures: death stress `codec-17`, silent Stage10
    transaction fixture exit, missing Stage10 validation capture, Stage10
    formal exit 10, silent Stage11 transaction fixture exit, and the Stage11
    five-path formal 300 s timeout.
  - Task 3/7A's Stage11B formal pause/resume failure and its dependent evidence
    validator not run.
  - Task 5B's Stage11D formal selector failure and its dependent evidence
    validator not run.
- No Task 7B production change touches the Stage10/11 dungeon fixture/stress
  sources. The Stage11B and Stage11D formal failures were independently
  recorded on their earlier exact BASE revisions.

## Resource discipline

- Every build/test launch was preceded by a duplicate process check and used
  `-j1`.
- The long final selector kept one CTest chain. Parent/child CMake processes
  were verified as the current mutation fixture, not duplicate tasks.
- The wrapper timeout did not cause a relaunch; the original PID was monitored
  to natural completion. No residual `cmake`, `ctest`, `ninja`, `cl`, or
  `link` process remains.

## Result

- `DONE_WITH_CONCERNS`: Task 7B implementation, build, every affected gate,
  adversarial self-test, exact behavior review, and independent review passed.
  The only concerns are the explicitly pre-recorded BASE formal/fixture/stress
  failures listed above.
