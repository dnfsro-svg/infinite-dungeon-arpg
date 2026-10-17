# Task 7A implementation report

## Scope

- Worktree: `E:/game/.worktrees/whole-game-refactor-2026-07`
- BASE: `35cda8816fd3615f312e902671dfd3678c981625`
- Structural refactor only: gameplay, bindings, injection/rearm behavior,
  death/pause behavior, fixed-step order, event order, snapshots, inventory,
  rendering, persistence, RNG, and audio remain unchanged.
- The existing tracked `progress.md` edit and the unrelated untracked test
  `.pyc` were preserved and excluded from Task 7A staging.

## TDD and implementation

- Registered `platform.host_validation_source`; its initial RED was the
  required missing `host_validation.hpp` diagnostic.
- Added the raylib-free opaque `HostValidationRuntime` facade, internal
  six-state aggregate, temporary seven-accessor transition shim, and runtime
  implementation.
- `create()` owns exactly two nothrow allocations before `InitWindow()` and
  maps failure to the existing save-initialization exit.
- Input composition remains `11B -> 11C -> 11D -> cached snapshot -> Stage17
  suspend -> 17`.
- The final Stage17 snapshot remains the input for gameplay rearm and
  `map_host_frame_input`; the cached Stage11D snapshot remains the input for
  death gating and pause mouse/focus/command handling.
- Combat, snapshot, inventory, and submitted-action observation moved behind
  the facade without moving Stage17 draw/capture/report ownership.
- `drain_events()` now receives one runtime pointer while preserving observer,
  renderer, feedback, and audio order at all six call sites.
- The Stage11B frame-28/frame-29 rebound counting moved behind the submitted
  action observer with Stage17 observation first.

## BASE comparison

- The five Stage state headers are unchanged from BASE; Task 7A moved only
  state-instance ownership.
- Manual production diff review found no behavior drift in allocation order,
  readiness assignment, state aliases, input composition, cached/final
  snapshot consumers, event ordering, snapshot cadence, inventory placement,
  or submitted-action counting.
- Independent final review separately confirmed the production behavior above.

## Guard hardening and independent review

- Round 1: Critical 0, Important 5. Closed exact facade/state/Impl ownership,
  observer reachability/arguments, cached/final snapshot binding, shim header
  escape, and raw/inactive/cross-scope input-latency bypasses.
- Round 2: Critical 0, Important 4. Closed inactive-first definition borrowing,
  exact seven-accessor API ownership, lambda/dead-scope snapshot decoys, and
  discarded 11B/11C/11D injector results.
- Round 3 follow-ups closed discarded pause-input results and exact-bound all
  seven `HostValidationStateAccess` implementation bodies to their matching
  `Impl` fields.
- Final independent review: Critical 0, Important 0, new Minor 0; Task 7A is
  approved for submission.

## Focused verification

- Repository toolchain configure plus serial build of `arpg_game` and
  `arpg_platform_tests`: PASS (5/5 build steps in the final build).
- `platform.input_latency_source`: PASS, 71.73 s after final RHS binding.
- `platform.host_validation_source`: PASS, 16.57 s after final StateAccess
  implementation binding.
- `stage11b.settings_evidence_guard`: PASS, 1.17 s.
- `stage11b.settings_evidence_guard_self_test`: PASS, 18.62 s.
- Earlier unchanged final surfaces passed on this Task 7A source state:
  `platform.host_input_source` 125.18 s and
  `platform.host_validation_sequence` 363.57 s.
- Same-machine BASE comparison showed no source-guard slowdown:
  `host_input_source` was 126.20 s on BASE; BASE input-latency and sequence
  exceeded 70 s and 300 s respectively while Task 7A passed within 71.73 s
  and 363.57 s.
- Final prescribed selector selected 16 tests and ran serially for 2073.66 s:
  14 passed, one known BASE failure, and its dependent validator was not run.
  All Task 7A gates passed: `platform.units` 17.84 s,
  `platform.input_latency_source` 67.68 s,
  `platform.host_input_source` 126.99 s,
  `platform.host_validation_source` 16.03 s,
  `platform.host_validation_sequence` 361.29 s, and
  `platform.host_validation_sequence_self_test` 1409.64 s. All selected
  Stage17, Stage11B architecture/guard/self-test/stress tests also passed.
- The only failing test was the pre-recorded BASE failure
  `stage11b.settings_formal` (14.42 s); its dependent
  `stage11b.settings_evidence_validator` was consequently not run. This is the
  same formal pause/resume evidence failure documented under Task 3 and is
  unrelated to Task 7A production or guard files.
- `git diff --check`: PASS before the final selector, with only existing CRLF
  conversion warnings.

## Resource discipline

- Every build/test launch was preceded by a duplicate-process check.
- A superseded selector run was terminated immediately after review found
  additional guard gaps; process-tree readback confirmed zero residual
  `cmake`, `ctest`, `ninja`, `cl`, or `link` processes before continuing.
- The final selector ran as one serial CTest chain and left no residual build
  or test processes.

## Result

- `DONE_WITH_CONCERNS`: Task 7A implementation, build, all affected gates, and
  final independent review passed. The only concern is the explicitly
  baseline-recorded Stage11B formal failure described above.
