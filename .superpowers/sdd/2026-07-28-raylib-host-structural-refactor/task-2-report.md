# Task 2 Report: Stage10/11 Host Validation Extraction

## Revisions

- BASE SHA: `cdecc8bf686f7a3c7c61ff29cbe9f577d974c02b`
- FINAL SHA: recorded in the final task handoff after this report is amended
  into the same commit. (A commit cannot embed its own content-addressed SHA
  without changing that SHA.)

## Changed Files

- `src/platform/raylib/host_validation_stage10_11.hpp` (new): named
  `host_validation` state types and host-facing declarations.
- `src/platform/raylib/host_validation_stage10_11.cpp` (new): Stage10/11
  routes, completion predicates, and the environment-visual helper.
- `src/platform/raylib/raylib_host.cpp`: includes the new boundary and keeps
  only qualified state/function use at the original loop sites.
- `src/platform/raylib/CMakeLists.txt`: registers the new implementation.
- `tests/dungeon/stage10_evidence_guard_test.cmake` and
  `tests/dungeon/stage11_death_evidence_guard_test.cmake`: split algorithm
  checks onto the new source while retaining host call and capture checks.
- `tests/platform/host_validation_sequence_guard_test.cmake`: requires the
  new boundary, its state/function ownership, CMake registration, and the
  unchanged qualified fixed-step priority.
- `tests/platform/stage11c_hud_evidence_guard_test.cmake`: retargets the
  moved Stage10 definition endpoint to the next stable host endpoint without
  narrowing the guarded driver range.
- `tests/platform/stage12_environment_render_tests.cpp`: follows the moved
  Stage10 implementation while retaining the same production skill-route
  assertions.

## RED Evidence

Before production code existed, the updated guards failed as intended:

- Stage10 direct guard command failed with
  `Stage 10 validation route target is missing: ...host_validation_stage10_11.cpp`.
- Stage11 direct guard command failed with
  `Stage 11 validation route target is missing: ...host_validation_stage10_11.cpp`.
- Sequence direct guard command failed with
  `Host validation boundary target is missing: ...host_validation_stage10_11.hpp`.

These failures prove the guards require the new ownership boundary rather
than accepting the old in-host implementation.

## GREEN Evidence

- Direct Stage10 guard: passed after the new route/state sources were added.
- Direct Stage11 guard: passed (`Stage 11 production evidence guard passed`).
- Direct Stage11C evidence guard: passed after its end anchor was migrated.
- Direct `host_validation_sequence_guard_self_test.cmake`: passed.
- Required build command (MSVC DevCmd, `CMAKE_BUILD_PARALLEL_LEVEL=1`,
  `-j1`) produced all three requested targets. The final Ninja log records
  `bin/arpg_game.exe`, `bin/arpg_dungeon_tests.exe`, and
  `bin/arpg_platform_tests.exe`; their latest timestamps are 08:36:19–20.
- Required focused CTest command completed. The latest `LastTest.log` records
  `dungeon.units` 343 cases/0 failures, `platform.units` 497 cases/0 failures,
  and `platform.host_validation_sequence` passed. The runner's 64-second
  observation window timed out while the existing CTest process continued;
  the completed final log has no `Test Failed.` entry. `LastTestsFailed.log`
  contains only the prior 08:31 pre-fix `platform.units` residue and was not
  used as this run's result.
- `git diff --check`: passed.

## Resource and Build-Chain Checks

- Before building, no `cmake`, `ninja`, `cl`, `link`, or requested target
  process was active; 8,903 MiB physical memory was free.
- Builds used one observed serial chain at a time (`cmake -> ninja -j 1 -> cl`).
  Tool waits expired during the long serial build, so the existing child chain
  was monitored to completion rather than restarted.

## Self-Review

- `Stage10ValidationState` and `Stage11ValidationState` are defined once in
  the named `arpg::platform::host_validation` namespace; the scenario enums
  remain in `raylib_host.hpp`.
- New code reuses Task 1 navigation helpers and directly includes
  `dungeon/dungeon_session.hpp`; it has no raylib, renderer, persistence, or
  test header in its public header.
- The host preserves the original locations of `validation_continue`,
  `continue_requested`, presented-frame counters, `stage10_validation_captured`,
  and Stage11 -> Stage10 -> production movement priority; only the ownership
  qualifiers changed.
- No change touches `persistence/room_progress_codec.cpp` or the known
  `architecture.persistence_checkpoint_only` baseline issue.
- No P0/P1/P2 issue found in the changed surface.

## Remaining Issues

No open Task 2 guard issue. The known unrelated
`architecture.persistence_checkpoint_only` baseline failure was not modified;
the corrected full-selector runtime/formal failures are recorded below.

## Fix Round 1 Evidence

- FIX_BASE: `67b1574a75bf578b0e463e7910adb13cda68b444`.
- The original selector used `stage10\.` and `stage11\.` inside an anchored
  expression, so it matched neither Stage prefix. Both the formal plan and
  Task 2 brief now use `stage10\..*|stage11\..*`; `ctest -N` confirms 22
  matching tests instead of the prior 3.
- Stage10/11 guards accept `STAGE_SOURCE`/`STAGE_HEADER` overrides for mutation
  testing while preserving the production-path defaults. Each guard now uses a
  character-by-character brace-depth scan to isolate complete input and reached
  function blocks. Route tokens are checked only in their owning input block;
  completion tokens are checked only in their owning reached block.
- Stage10 added `stage10.evidence.mutation_self_test`. Both capture-order
  fixtures first satisfy the host call surface. The first models a pre-present
  capture directly; the second models a pre-present Load/Export plus a legal
  post-`EndDrawing()` dummy helper, and is rejected by aggregate capture/order
  checks. Both self-test expected rejection categories pass.
- Stage11's existing mutation self-test now has a valid capture-order fixture
  and passes with the expected capture-order rejection.
- Both Stage10 and Stage11 self-tests remove, one at a time, the owning
  `session.request_descent(true)` and
  `session.queue_action(combat::Action::light)` from a `STAGE_SOURCE` mutation.
  Each is rejected by the matching Stage input-block check, proving no
  cross-function substitution.
- Direct Stage10 and Stage11 production guards passed after hardening; direct
  Stage10 and Stage11 mutation self-tests also passed.
- The corrected full selector ran 22 tests. 16 passed, including all Stage10/11
  evidence guards, both mutation self-tests, Stage11 architecture boundaries,
  `dungeon.units` (343 cases/0 failures), `platform.units` (497 cases/0
  failures), and `platform.host_validation_sequence`. Six non-guard failures
  were recorded without being masked: `stage11.death_stress.determinism_zero_alloc`
  failed deterministic restart trace at `codec-17`; `stage10.validation_fixture.real_abyss_transactions`
  stopped after 2.85 seconds without test output; the Stage10 validation game
  did not produce its capture PNG; the Stage10 formal game exited with code 10;
  `stage11.death_fixture.transactions` stopped after 21.80 seconds without
  test output; and `stage11.death_formal.five_paths` timed out at its 300-second
  CTest limit. No gameplay or persistence code was changed to address these
  failures. A read-only `FIX_BASE` comparison confirms this fix did not change
  the production host extraction files or any `src/dungeon`/`src/persistence`
  path; that proves only that this guard/test fix left those production paths
  untouched, not that the six failures predate `FIX_BASE`.
- FINAL handoff SHA is reported outside this self-contained commit after amend;
  embedding the commit's own content-addressed SHA in this report would change
  that SHA.

## Fix Round 2 Evidence

- FIX_BASE: `0ef23055995e8e77338a684dbfea7949f6311944`.
- Stage10 capture mutation self-test now derives both negative cases from the
  real `VALID_HOST_SOURCE`, after asserting each textual anchor occurs exactly
  once and the replacement occurred. One mutation only reverses the helper's
  `EndDrawing()`/`LoadImageFromScreen()` order; the other leaves the formal
  helper, gates, and helper mentions unchanged while adding one pre-present
  Load/Export/Unload sequence. Guard diagnostics separately identify helper
  order versus duplicate capture cardinality. Both existing static Stage10
  negative fixtures now also retain the exact `captured_stage10_target &&
  capture_succeeded` result gate, completion gate, required host calls, and at
  least three helper mentions; each preserves only its intended capture error.
- `evidence_source_scan.cmake` supplies the shared scan implementation for both
  Stage10/11 guards and both mutation self-tests. It replaces comments,
  ordinary escaped strings, and character literals with equal-length spaces
  while preserving newlines, then finds braces and route tokens only in the
  sanitized code view. Raw route mutations use the validated source bounds, so
  replacement remains confined to the owning function.
- Both Stage10 and Stage11 self-tests now add `// {`, a block-comment brace, an
  escaped-string brace, and a character-literal brace inside the owning input
  function while deleting its own descent or queue route. All four adversarial
  mutations reject with the stage-specific missing-route diagnostic; they
  cannot consume the other stage's input function.
- Direct Stage10 and Stage11 production guard invocations passed. Both mutation
  self-tests passed. The requested literal quick-selector expression matched
  only `platform.host_validation_sequence` because its two prefix alternatives
  omitted `.*`; that one test passed. The corrected prefix expression then ran
  all intended quick tests: 11/11 passed, including both production guards,
  all Stage10/11 negative evidence cases, both mutation self-tests, and the
  platform sequence guard. The corrected full-selector `ctest -N` still lists
  22 tests. `git diff --check` passed.
- This round did not rerun the full 22-test execution; the Round 1 recorded
  result remains 16 passed and 6 failed as described above. No production,
  gameplay, dungeon, or persistence source was changed in this round.
