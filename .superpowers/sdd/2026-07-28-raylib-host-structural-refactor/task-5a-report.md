# Task 5A report: Stage11D validation runtime extraction

Status: COMPLETE

Base: `3aea1ee8a3dd1b79f44de0bdf565334f09599477`.

Commit: `6566026`.

Scope: move only Stage11D state, selectors/navigation, physical-input
injection, and its fixed-step input runtime into
`host_validation_stage11d.hpp` and
`host_validation_stage11d_runtime.cpp`. Presented/capture/report and the
host-loop sample-map-pause-submit/abyss-claim behavior remain in
`raylib_host.cpp` for Task 5B.

## TDD record

Pre-card guard baseline command:

```powershell
cmake -DSOURCE_ROOT=<worktree> \
  -P tests/platform/stage11d_loot_evidence_guard_test.cmake
```

Result at untouched BASE: exit 1. The legacy guard removed the marked state
definition but then rejected the legitimate `HostValidationStates` member
`Stage11DLootValidationState stage11d{}` as "host validation code outside its
isolated seam". This is recorded as a pre-existing guard false positive, not
as extraction RED and not as a production defect.

Relocation RED and final GREEN commands/results will be appended as they are
observed.

Legacy baseline after correcting only the false-positive alias exclusion:

- direct guard: PASS;
- legacy self-test: PASS, `bad_mutations=22`, 87.4 seconds.

Task 5A relocation RED command:

```powershell
cmake -DSOURCE_ROOT=<worktree> \
  -DGUARD_TEST_ROOT=<build>/stage11d-task5a-red \
  -P tests/platform/stage11d_loot_evidence_guard_self_test.cmake
```

Result: exit 1 in 11.6 seconds for the intended reason:

```text
Stage11D loot evidence guard accepted mutation: runtime driver in uncalled lambda
```

This proves the legacy raw-token guard accepted the complete Stage11D physical
driver when its real body was moved into a non-executed nested scope.

Initial GREEN guard performance correction:

- full-host sanitizer attempt: timed out at 124 seconds (exit 124);
- first bounded version: 53.42 seconds, then 47.18 seconds while exposing
  expected crop/scope mistakes;
- final bounded, single-sanitize direct guard: PASS in 11.65 seconds.

The final guard sanitizes the Stage11D runtime once and scans bounded host input
and fixed-step regions. Forbidden host definitions use a raw-miss fast path:
only a raw signature hit invokes the code-state token walker. The normal
baseline therefore does not sanitize or lexically walk the 160 KiB host for
these checks, while harmless comment/string hits are distinguished from code.

The relocated self-test fixtures were statically audited before the final
run. Thirty-one underlying mutation anchors/markers across the new header,
runtime, retained host, formal harness, renderer, and validator each matched
exactly once; the eight host-insertion mutations share one separately verified
unique presented-semantics anchor. Host fixture text is normalized to LF before
multi-line replacement. Two obsolete fixtures discovered during extraction
were corrected to target the new ownership boundary:

- retained-host bypass mutations now attach to `presented_semantics`;
- the ground snapshot write mutation now targets the sanitized whole runtime,
  rather than only the physical-driver function.

Final self-test result:

```text
Stage11D loot evidence guard self-test passed: bad_mutations=39; harmless_decoys=2
```

Elapsed time: 483.1 seconds. A preceding targeted run passed five new negative
mutations plus two harmless decoys in 193.4 seconds. Reviewer follow-up coverage
now proves:

- state and all eight runtime definitions have unique signatures at their
  required absolute brace depths, independently of marker depth;
- header state and runtime selector payloads cannot move under extra namespaces
  while leaving their seam markers in place;
- the real pause call remains at host input depth zero and the fixed-step
  activation call remains at fixed-step depth one;
- uncalled/nested pause and activation payloads are rejected;
- real moved definitions in host code are rejected by both guards, while the
  same signature in a comment and string is accepted by both guards.

## Implementation record

- Added `host_validation_stage11d.hpp` containing the fixed-capacity
  `Stage11DLootValidationState` and the narrow Stage11D runtime interface.
- Added `host_validation_stage11d_runtime.cpp` containing the moved selectors,
  safe navigation, physical key injection, fixed-step activation selector, and
  post-snapshot abyss-claim observation.
- `raylib_host.cpp` retains evidence semantics, presentation, capture, reached
  state, and summary/report behavior. The abyss observer call remains after
  `fixed_tick`, the fresh session snapshot, and Stage17 observation.
- Registered the new runtime source once in the raylib CMake target.
- Extended the sequence guard to enforce header/runtime ownership and absence
  of moved definitions from the host.

## Verification

Serial build:

```powershell
cmake --build --preset windows-msvc-debug \
  --target arpg_game arpg_platform_tests -- -j1
```

Result: PASS in 205.9 seconds. Both targets linked. Output contained only the
existing C4267/C4127 warnings and Ninja's recoverable `premature end of file;
recovering` notice.

Final direct guards after cleanup of the unused guard helper:

- Stage11D loot evidence guard: PASS in 12.7 seconds;
- host-validation sequence guard: PASS in 46.4 seconds.

`git diff --check`: PASS. The final exact-seven parent/reviewer gate also passed
all seven selected tests.

## Exact-seven timeout audit

The parent/reviewer exact-seven gate completed with 5/7 passing. Its only two
failures were timeout terminations, not assertion diagnostics:

- `stage11d.architecture.loot_boundaries_self_test`: 300.04 seconds, legacy
  `TIMEOUT 300`;
- `stage11d.loot_evidence_guard_self_test`: 300.03 seconds, legacy
  `TIMEOUT 300`.

The same evidence self-test has a direct PASS at 483.1 seconds. Both suites are
mutation-heavy serial guard tests, so the 300-second metadata budget no longer
represents their verified runtime after the Task 5A scope checks were expanded.
The minimal metadata correction sets the architecture self-test to 600 seconds
and the evidence self-test to 900 seconds. No assertion, mutation, test
selection, or production source was changed for this timeout correction.
Regenerated CTest JSON first confirmed those values, then the final serial
exact-seven gate validated them in practice:

- `platform.units`: PASS, 17.48 seconds;
- `stage11d.architecture.loot_boundaries`: PASS, 121.63 seconds;
- `stage11d.architecture.loot_boundaries_self_test`: PASS, 305.58 seconds;
- `platform.host_validation_sequence`: PASS, 51.87 seconds;
- `platform.host_validation_sequence_self_test`: PASS, 847.73 seconds;
- `stage11d.loot_evidence_guard`: PASS, 12.94 seconds;
- `stage11d.loot_evidence_guard_self_test`: PASS, 445.55 seconds.

Final result: 7/7 PASS in 1802.84 seconds. The two formerly truncated suites
completed inside their corrected 600/900-second budgets, proving that the first
gate failures were stale timeout metadata rather than assertion failures.

## Resource record

- Worktree and branch were clean at BASE before edits.
- Build/test commands are serial (`-j1`) and are launched only after checking
  for residual CMake, CTest, Ninja, compiler, and linker processes associated
  with this worktree.
- No residual CMake, CTest, Ninja, compiler, or linker process remained after
  the final exact-seven gate.
- Literal `$guardRoot`, `$outputRoot`, and `$sequenceRoot` test directories
  were verified as untracked generated output and removed with exact-path
  cleanup; the final worktree contains none of them.
