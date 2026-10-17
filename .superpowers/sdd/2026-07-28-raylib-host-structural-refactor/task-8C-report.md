# Task 8C: Raylib window lifetime guard report

Commit: `2358e54eb2ff75c45b6c84847aef3d6dd4665c96`

## Scope

Implemented `HostWindowLifetime` with an injectable counting backend and moved
only the window lifecycle transaction from `run_raylib_host`: config flags,
window creation, readiness, explicit close, and destructor fallback.  The Host
still owns post-ready directory/min-size/exit-key/foreground policy and calls
audio, combat-renderer, and pause-renderer shutdown before `window.close()`.

The three brief-external retargets are required, proven-equivalent test changes:

- `host_validation_sequence_guard_test.cmake`: its shutdown-order anchor now
  binds the real explicit `window.close()` call instead of removed direct
  `CloseWindow()`.
- `stage11d_loot_evidence_guard_test.cmake`: its unconditional Host summary
  tail receives the same one-token, real close-owner retarget.
- `stage12_environment_render_tests.cpp`: `platform.units` RED was
  `init_window != std::string::npos`; it verifies that background-only recovery
  rejection happens before the real window-creation boundary, now
  `window.initialize(config, committed_settings)`. No assertion was removed or
  reordered.

## TDD evidence

RED, after registering the new counting-backend test and before creating the
lifetime implementation:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
```

Expected RED observed: `host_window_lifetime_tests.cpp(3): fatal error C1083:
cannot open include file: 'host_window_lifetime.hpp'`.

GREEN build:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
```

Exit 0.  The final focused rerun reported `513 cases, 0 failures`; the targeted
`stage11d.loot_evidence_guard` reported `Stage11D loot formal evidence guard
passed`; final `host_validation_source_test.cmake` exit code was 0.

## Selector and baseline failures

The required selector was run serially.  Its initial full run covered 124
selected tests.  It reported known/unrelated failures in
`stage11b.settings_formal`, `stage11b.settings_evidence_validator`,
`architecture.persistence_checkpoint_only`, and
`platform.host_validation_sequence_self_test` (the latter fails at the existing
Stage11C `stage11c_production_snapshot_hash` diagnostic).  An initial Task 8C
source-guard canonical-contract failure was fixed; the final direct guard is
green.  The initial broad `stage11d.*` diagnostic run was controller-cancelled
as redundant after its active ctest tree had been confirmed.

## Self-review

- No real window opens in the new lifetime tests; all raylib calls are injected
  counters.
- `initialize` preserves `SetConfigFlags -> InitWindow -> IsWindowReady`;
  failed readiness never owns or closes a window; `close` is idempotent; the
  destructor covers early return and stack unwinding.
- Host catches retain their exact exit codes and log strings without duplicate
  close calls.  Success teardown ordering is preserved.
- `raylib_host.cpp` is 1,240 lines; the baseline explains why remaining loop
  coordination cannot be mechanically split.
- `git diff --check` passed.  The progress ledger and newline-only
  `raylib_host.hpp` were not staged or edited.

## Independent review repair

Review base: `2358e54eb2ff75c45b6c84847aef3d6dd4665c96`.

### RED witnesses

The predecessor source guard was exercised with two smallest synthetic inputs:

- A `#if 0` `HostWindowLifetime` declaration has one raw
  `class HostWindowLifetime final {` occurrence, while its unconditional
  lexical surface has zero.  The old raw-header count would therefore accept
  disabled interface evidence.
- A separate `task8c_foreign_window_owner()` with `CloseWindow();` leaves the
  old Host-only `CloseWindow(` count at zero and the legitimate backend
  `&CloseWindow` binding at one.  The predecessor guard did not scan that
  foreign production source.

The new guard preserves those witnesses and asserts that the disabled member
does not occur on the active surface and that the foreign owner is rejected.

### Repair

- Interface members are now counted on both the unconditional active surface
  and the lexer-sanitized surface after whitespace compaction; comments and
  `#if 0` text cannot provide interface evidence or hide duplicates.
- Every production `src/platform/raylib/**/*.cpp` is now scanned.  Exactly one
  lexer/active occurrence of `SetConfigFlags`, `InitWindow`, and `CloseWindow`
  is allowed in `host_window_lifetime.cpp`; all other production sources must
  contain zero.  `IsWindowReady` remains an allowed query API outside the
  lifetime owner.
- The Stage11C sequence guard now uses the non-fatal function-boundary probe.
  Missing lexical signatures retain the existing `Evidence validation function
  is missing` diagnostic; signatures that exist but are only declarations use
  the existing Stage11C `definition is missing` diagnostic.  No expected
  diagnostic was weakened or changed.

### GREEN evidence

```powershell
cmake -DSOURCE_ROOT='E:/game/.worktrees/whole-game-refactor-2026-07' -P `
  'E:/game/.worktrees/whole-game-refactor-2026-07/tests/platform/host_validation_source_test.cmake'
```

Exit 0 in 217.1 seconds.  Output included:
`Task 8C RED witnesses: raw disabled interface and foreign CloseWindow owner
pass predecessor-local checks` and `Host validation facade/runtime source
boundaries verified`.

```powershell
ctest --test-dir 'E:/game/.worktrees/whole-game-refactor-2026-07/out/build/windows-msvc-debug' `
  -R '^platform\.host_validation_sequence_self_test$' --output-on-failure -j1
```

Passed.  `LastTest.log` reports `platform.host_validation_sequence_self_test`
passed in 1256.95 seconds.  The command wrapper reached its 15-minute limit,
but its inherited CTest process remained active; parent/child command-line
inspection confirmed it was the single requested self-test, and the completed
CTest log recorded the pass.  No test timeout, expected diagnostic, or
mutation case was altered.

`git diff --check` also exited 0 after the repair.

Repair commit: `ba1c93f4fab37108d2da079cb14ab1dc748aa741`
(`test: harden raylib window lifetime guard`).  It intentionally excludes the
controller-owned `progress.md` and `raylib_host.hpp` worktree changes.

### Concerns

The source guard deliberately rejects disabled duplicate lifecycle symbols as
well as active ones, making ownership evidence fail closed.  The full sequence
self-test is expensive (about 21 minutes on this machine), so verification was
kept to the required single test and did not broaden into the 124-test selector.
