# Task 5B report: Stage11D presented semantics and reporting extraction

Status: COMPLETE

Base: `a9e34cd2c378`.

Commit: `18fb820`.

Scope: move only Stage11D semantic recording, target-visible evaluation,
scenario naming, and summary/report implementation into
`host_validation_stage11d_report.cpp`. Keep the real `GroundLootView`, HUD
notice sampling, reached/capture decisions, presentation/capture order, and
captured-state mutation in `raylib_host.cpp`.

## TDD record

Named RED mutation:

```text
host_semantic_uncalled_lambda
```

The mutation moves the only `stage11d_record_semantics` call in the presented
host seam into an uncalled lambda while leaving its raw token present. Command:

```powershell
cmake -DSOURCE_ROOT=<worktree> \
  -DGUARD_TEST_ROOT=<temp>/stage11d-task5b-red \
  -DTASK5B_RED_ONLY=ON \
  -P tests/platform/stage11d_renderer_integration_guard_self_test.cmake
```

Result: expected RED, exit 1 in 0.3 seconds:

```text
Stage11D guard accepted bad host_semantic_uncalled_lambda host mutation
```

This proves the pre-Task5B renderer guard did not bind semantic recording to
the executable `run_raylib_host` presented-frame scope.

## Resource record

- Worktree was clean at `a9e34cd2c378` before Task 5B edits.
- No CMake, CTest, Ninja, compiler, or linker process was active before RED.
- All build/test work for this card remains serial.

## Implementation

- Moved the complete `evidence_semantics` seam into
  `src/platform/raylib/host_validation_stage11d_report.cpp`.
- Kept the production `GroundLootView`, both fresh HUD notice samples,
  presented target/record calls, reached/capture decisions, frame presentation,
  capture success, captured-state update, and post-loop summary call in
  `raylib_host.cpp` without changing their bindings or order.
- Added only the three public report declarations to
  `host_validation_stage11d.hpp` and registered the new source exactly once in
  `arpg_raylib`.
- Strengthened the Stage11D renderer, evidence, and host-sequence guards so
  report ownership, real `run_raylib_host`/main-loop scope, presentation and
  capture lifecycle, all 43 summary keys, and post-loop summary order are
  code-aware and fail closed.

## Verification

Serial commands and results:

```text
stage11d_renderer_integration_guard_test.cmake  exit 0  34.7s
stage11d_loot_evidence_guard_test.cmake         exit 0  16.6s
host_validation_sequence_guard_test.cmake       exit 0  59.5s
Task5B evidence named mutations (4/4)            exit 0  77.4s
renderer uncalled-lambda mutation                exit 0  30.5s
renderer semantic-after-capture mutation        exit 0  25.9s
renderer report-source mutations (4/4)          exit 0   8.9s
arpg_raylib serial build                         exit 0  93.6s
git diff --check                                exit 0
```

The first bare-shell build attempt stopped before compilation because the
ordinary PowerShell session had no `WindowsSDKVersion`. Re-running through the
repository `Configure.ps1` entry loaded MSVC 19.44 / Windows SDK 10.0.26100.0;
the new report object, host object, and `arpg_raylib.lib` then built cleanly.

Per the controller instruction, the implementer did not run the complete long
selector/formal chain; that remains for parent integration validation.

Final boundary audit: exactly 9 brief-listed tracked/untracked source/test files
changed, 0 unexpected files, 1 CMake registration for the new report source,
and 0 residual CMake/CTest/Ninja/compiler/linker processes. The host is now
2602 lines; the extracted report source is 332 lines.

## Review follow-up

Production review found one P3 fidelity issue: the extracted
`stage11d_target_visible` definition had lost the base definition's
`[[nodiscard]]` attribute. The attribute was restored at report-source line 72;
no body, binding, test, or other production code changed. `git diff --check`
remained clean, and the standard MSVC 19.44 / Windows SDK 10.0.26100.0 serial
`arpg_raylib` build passed again (exit 0, 96.7 seconds), compiling the report
object and linking `arpg_raylib.lib`.

Review then found a P2 guard-only gap: the renderer guard proved all 43
summary keys and their order, but did not bind four important field RHS values,
the result formula, or the two evaluator producers. Production remained
byte-for-byte equivalent to the base behavior.

TDD follow-up:

```text
RED: first producer constant mutation accepted       exit 1   3.1s
GREEN: 7/7 targeted binding mutations rejected       exit 0  17.7s
Final targeted self after shared-test refactor        exit 0  18.3s
Fresh renderer direct baseline                        exit 0  33.8s
```

The guard now checks normalized, code-aware complete bindings for:

- evaluator `monster_affix_danger` and `monster_ai_phase` producers;
- three-element `monster_affix_danger`, casted three-element
  `monster_ai_phase`, and three-element `defeat_player_hp` output values;
- `target_ordinal=state.target_ordinal`;
- the exact captured/rare-only-abyss/abyss-claimed result ternary, including
  pass and fail literals.

Seven named constant-RHS/result mutations preserve markers and key order but
must fail for their specific producer/output binding reason. They run both in
the targeted mode and the ordinary renderer guard self-test. No production,
evidence, sequence, or CMake file changed in this P2 follow-up.

## Parent integration validation

The parent serial build passed in 217 seconds.

The exact five-test headless gate passed 5/5 in 126.64 seconds:

| Test | Result | Time |
|---|---:|---:|
| `platform.units` | PASS | 17.43s |
| `stage11d.loot_root_safety` | PASS | 0.08s |
| `platform.host_validation_sequence` | PASS | 59.59s |
| `stage11d.renderer_integration_guard` | PASS | 33.28s |
| `stage11d.loot_evidence_guard` | PASS | 15.89s |

The Stage11D formal chain was executed. `stage11d.loot_formal` failed in
0.03 seconds because the current formal executable returns exit 4 for
`--select-only`; its dependent evidence validator therefore did not run. A
detached rebuild of the exact base `a9e34cd` reproduced the same formal exit 4
for `--select-only`. This card changes neither the formal source nor the
core/dungeon/items/combat selector dependencies. The formal result is therefore
BASE-proven red and deferred rather than repaired inside this pure structural
card.

Independent review is now final with no open P1, P2, or P3 findings. The P3
`[[nodiscard]]` fidelity finding and the P2 producer/summary binding findings
are closed by the follow-ups recorded above. Task 5B was committed as
`18fb820`.
