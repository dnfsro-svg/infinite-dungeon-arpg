# Task 0 report: Raylib host acceptance baseline

## Status

Completed test-only Task 0 on review base
`3b3ac0a58d1037505a36d3cccd791a241e250d8d`. No `src/` production file or
gameplay behavior was changed.

## RED evidence

Known pre-task Stage11C drift:

```powershell
cmake "-DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07" -P tests/platform/stage11c_hud_evidence_guard_test.cmake
```

```text
CMake Error at tests/platform/stage11c_hud_evidence_guard_test.cmake:91 (message):
  Stage11C evidence guard cannot bind complete host capture surface
```

The old guard looked for the deleted independent-state text, rather than the
actual `Stage11CHudValidationState&` runtime alias. The baseline document
records this as pre-existing drift.

New sequence-guard mutation RED:

```powershell
cmake "-DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07" "-DHOST_OVERRIDE=E:/game/.worktrees/whole-game-refactor-2026-07/out/task0-sequence-mutation.cpp" -P tests/platform/host_validation_sequence_guard_test.cmake
```

```text
Host validation sequence guard rejected input injection chain order
```

The fixture puts Stage11D injection before Stage11C injection, proving the
new guard rejects a real ordering mutation.

## GREEN evidence

The following configure command succeeded after loading the installed MSVC
BuildTools developer environment:

```powershell
cmake --preset windows-msvc-debug
```

```text
-- Configuring done
-- Generating done
-- Build files have been written to: E:/game/.worktrees/whole-game-refactor-2026-07/out/build/windows-msvc-debug
```

The required serial CTest selection passed:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.host_validation_sequence|stage11c\.hud_evidence_guard|stage11c\.hud_evidence_guard_self_test)$' --output-on-failure -j1
```

```text
1/3 Test #93: platform.host_validation_sequence .......   Passed
2/3 Test #96: stage11c.hud_evidence_guard .............   Passed
3/3 Test #97: stage11c.hud_evidence_guard_self_test ...   Passed
100% tests passed, 0 tests failed out of 3
```

`git diff --check` exited 0.

## Files

- Added `docs/validation/raylib-host-refactor-baseline.md`.
- Added `tests/platform/host_validation_sequence_guard_test.cmake`.
- Updated `tests/platform/CMakeLists.txt` to register
  `platform.host_validation_sequence`.
- Updated `tests/platform/stage11c_hud_evidence_guard_test.cmake` to bind the
  actual Stage11C alias and independently inspect driver, runtime, capture,
  and summary regions without removing model/hash/screenshot assertions.
- Updated `tests/platform/stage11c_hud_evidence_guard_self_test.cmake` with
  the independent-value alias mutation.

## Self-review

- The sequence guard removes line and block comments before checking each
  token exactly once in the host-loop-plus-summary range.
- It preserves the corrected fixed-step priority chain:
  `stage11_validation_input` → `stage10_validation_input` →
  `step_movement = movement;`.
- The Stage11C self-test still rejects direct `HudViewModel` writes, fake
  snapshot hashes, logical-input bypasses, fake `result=pass`, pre-present
  screenshots, fake font readiness, and the new independent state value.
- `src/` was not changed.

## Concern

The first direct configure invocation lacked the MSVC environment; rerunning
through the installed `VsDevCmd.bat` succeeded. Two early build invocations
hit the tool timeout and left overlapping child trees; the later tree was
stopped, then the remaining tree was also stopped at the lead agent's request
because Task 0 acceptance requires configure plus the three CTests, not a full
build. No overlapping build output is used as GREEN evidence, and the final
process check found no task-owned `cmake`/`ninja`/`cl`/`link` processes.

---

## Fix round 1: review findings

### Covering tests

- `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`
  - `host_runtime_model_and_hash_overwrite` appends both a model overwrite and
    fake hash in the runtime segment.
  - `host_decoy_stage11c_alias` keeps the alias declaration but replaces its
    right-hand side with `stage11c_decoy`.
- `tests/platform/host_validation_sequence_guard_self_test.cmake`
  - `split-fixed-step-conditions` keeps all three tokens but uses separate
    inner `if` branches instead of the required if/else-if/else priority.

### RED evidence before the fixes

```powershell
cmake "-DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07" "-DGUARD_TEST_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07/out/task0-fix-round1-self" -P tests/platform/stage11c_hud_evidence_guard_self_test.cmake
```

```text
Stage11C evidence guard accepted named host mutation:
  host_runtime_model_and_hash_overwrite
```

After restoring complete host-surface counting, the next TDD cycle produced:

```text
Stage11C evidence guard accepted named host mutation:
  host_decoy_stage11c_alias
```

The old sequence guard also incorrectly accepted the split structure:

```powershell
cmake "-DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07" "-DHOST_OVERRIDE=E:/game/.worktrees/whole-game-refactor-2026-07/out/task0-fix-round1-sequence-structure.cpp" -P tests/platform/host_validation_sequence_guard_test.cmake
```

```text
RED: guard incorrectly accepted split fixed-step conditions
```

### Minimal fixes

- Model and hash assignment counts, member-overwrite detection, and the real
  production-hash assertion again scan the complete concatenated driver,
  runtime, capture, and summary host surface.
- The Stage11C alias must exactly bind
  `validation_states->stage11c`; its declaration prefix is also unique, so a
  same-name preceding decoy cannot satisfy the anchor.
- The sequence guard now extracts the branch between `if (!step_death)` and
  `runtime.fixed_tick`, requires its normalized if/else-if/else structure, and
  checks each movement token exactly once inside that branch.

### GREEN evidence

```powershell
cmake --preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.host_validation_sequence(_self_test)?|stage11c\.hud_evidence_guard(_self_test)?)$' --output-on-failure -j1
git diff --check
```

```text
1/4 Test #93: platform.host_validation_sequence ............. Passed
2/4 Test #94: platform.host_validation_sequence_self_test ... Passed
3/4 Test #97: stage11c.hud_evidence_guard ................... Passed
4/4 Test #98: stage11c.hud_evidence_guard_self_test ......... Passed
100% tests passed, 0 tests failed out of 4
```

No full build was started in this repair round.
