# Task 1 report: shared validation input and navigation primitives

Status: DONE_WITH_CONCERNS

Base reviewed: `0197327e2938a766d74490b63ba9978603ae78b9`.

Task commit: `dfbef4d refactor: extract host validation input and navigation`.

## TDD record

RED command:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.host_validation_sequence|stage11c\.architecture\.|stage11c\.hud_evidence_guard)' --output-on-failure -j1
```

Expected RED result: 0/6 passed. The new guards rejected the deliberately
absent boundary files, for example:

```text
Host validation boundary target is missing:
E:/game/.worktrees/whole-game-refactor-2026-07/src/platform/raylib/host_validation_input.hpp
```

GREEN build command (MSVC `VsDevCmd.bat`, one chain):

```powershell
cmake --build --preset windows-msvc-debug --target arpg_raylib arpg_platform_tests -- -j1
```

Result: exit 0; both `arpg_raylib` and `arpg_platform_tests` linked. MSVC
reported pre-existing warnings in unrelated platform tests; Ninja reported
`warning: premature end of file; recovering` after the successful link.

GREEN focused command after the final Stage17 equivalence substitution:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.host_validation_sequence|stage11c\.architecture\.|stage11c\.hud_evidence_guard)' --output-on-failure -j1
```

Result: 6/6 passed.

## Required-suite concern, kept separate from Task 1 GREEN

The brief's broader CTest selector was run once before the final Stage17
equivalence-only substitution. It passed 24/25 tests. The sole failure is
outside this card and exists unchanged at the review base:

```text
90 - architecture.persistence_checkpoint_only ... Failed
CMake Error at tests/platform/persistence_boundary_test.cmake:35 (message):
  Persistence file includes a forbidden Dungeon header:
  src/persistence/room_progress_codec.cpp:
  #include "dungeon/health_potion_loot.hpp"
```

Evidence that Task 1 did not change the involved persistence source or guard:

```powershell
git diff 0197327e2938a766d74490b63ba9978603ae78b9 -- src/persistence/room_progress_codec.cpp tests/platform/persistence_boundary_test.cmake
```

Result: no output.

## Changed files

- `src/platform/raylib/host_validation_input.hpp/.cpp`: shared physical-key
  injection declarations and unchanged implementation.
- `src/platform/raylib/host_validation_navigation.hpp/.cpp`: shared stateless
  validation navigation declarations and unchanged implementation.
- `src/platform/raylib/raylib_host.cpp`: removed old local definitions and
  rewired Stage11B, 11C, 11D, and 17 to the shared primitives.
- `src/platform/raylib/CMakeLists.txt`: registers both new sources in
  `arpg_raylib`.
- five listed platform guard/self-test scripts: enforce the new ownership,
  preserve Stage11C scan coverage, and reject direct logical actions or a
  skipped stable binding from the shared input source.

## Self-review

- The moved navigation code preserves traversal order and every float
  threshold/constant verbatim.
- Input injection preserves physical-key mutation order; Stage17 movement now
  delegates to the same action/movement primitive with `pressed == false`.
- New headers have no direct `raylib`, renderer, persistence, or test include.
- `git diff --check` passed.

## Fix round 1: shared input evidence coverage

Fix commit: `cdecc8b test: cover shared validation input evidence`.

Covering test: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`.

RED command (before changing the guard):

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^stage11c\.hud_evidence_guard_self_test$' --output-on-failure -j1
```

Expected RED result: 0/1 passed. The old shared-input scan accepted both
mutations, proving that the helper had lost part of the original driver ban
surface:

```text
Stage11C evidence guard accepted named input mutation: input_test_access
Stage11C evidence guard accepted named input mutation: input_direct_viewmodel
```

GREEN command:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^stage11c\.hud_evidence_guard_self_test$' --output-on-failure -j1
```

Result: 1/1 passed. The self-test now rejects all four shared-input mutations:
`TestAccess`, `HudViewModel direct_model`, direct `session.queue_action`, and
skipping `settings::binding_for`.

Final build command (MSVC `VsDevCmd.bat`, one `-j1` chain):

```powershell
cmake --build --preset windows-msvc-debug --target arpg_raylib arpg_platform_tests -- -j1
```

Result: exit 0; both targets linked. The original build output ended with
`ninja: warning: premature end of file; recovering`; this is recorded as a
warning, not treated as pristine output.

Final required selector:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|architecture\.|stage11c\.architecture\.|stage11c\.hud_evidence_guard)' --output-on-failure -j1
```

Result: 24/25 passed. The 24 passing tests include all Task 1 direct guards:
`platform.host_validation_sequence`, its self-test, both Stage11C architecture
guards, and both Stage11C evidence guards. The only failure remains the
unchanged base `architecture.persistence_checkpoint_only` failure shown above.

The persistence non-overlap proof was rerun after this fix:

```powershell
git diff 0197327e2938a766d74490b63ba9978603ae78b9 -- src/persistence/room_progress_codec.cpp tests/platform/persistence_boundary_test.cmake
```

Result: no output.
