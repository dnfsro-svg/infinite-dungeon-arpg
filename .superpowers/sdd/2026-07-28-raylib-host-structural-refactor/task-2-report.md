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

None for Task 2. The known unrelated
`architecture.persistence_checkpoint_only` baseline failure was not run or
modified.
