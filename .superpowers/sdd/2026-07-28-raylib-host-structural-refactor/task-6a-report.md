# Task 6A report: Stage17 active-skill validation runtime extraction

Status: COMPLETE — independently approved

Base: `7ae2744`.

Scope: move only Stage17 state, runtime observation and physical-input
injection into `host_validation_stage17.hpp` and
`host_validation_stage17_runtime.cpp`; retain capture/report lifecycle and
all production host-loop ordering in `raylib_host.cpp`.

## Resource record

- Worktree was clean at `7ae2744` before Task 6A edits.
- No CMake, CTest, Ninja, compiler, or linker process was active.
- Free physical memory at kickoff was approximately 4.4 GB.
- All build and test work for this card must remain serial.

## TDD record

- Initial ownership RED: after migrating the source guard but before creating
  `host_validation_stage17.hpp`, the direct guard rejected the missing header
  (exit 1, approximately 38 seconds).
- The old semantic guard accepted the named `renderer-uncalled-lambda` decoy
  (RED, 54.2 seconds). The bounded direct-scope implementation subsequently
  rejected that decoy and the complete lambda/dead-branch/cross-scope family.
- Named mutations cover early injector return, custom lambda capture, removed
  skill-slot writes, inactive exact injector copies, ordinary/nested/spliced
  conditional regions, compact and hexadecimal zero expressions, whole
  inactive definitions, mutually exclusive branch fragments, namespace
  ownership, observer/drain dataflow, renderer ordering, and unreachable CMake
  registration.

## Implementation

- Moved the four shared Stage17 enums/state structures into the raylib-free
  `host_validation_stage17.hpp` boundary.
- Moved the six public runtime observers/injector, nine private helpers, and
  one private storm-target structure verbatim into
  `host_validation_stage17_runtime.cpp`.
- Kept all capture/report helpers, paths, completion mutation, screenshot
  lifecycle, `EndDrawing()`, summary writing, and host-loop call sites in
  `raylib_host.cpp` for Task 6B.
- Registered the new runtime source exactly once. The host fell from 2602 to
  1935 lines; the new header/runtime are 160/585 lines.
- Hardened the affected source guards so only unconditional executable code in
  the real function/namespace/CMake scope can provide evidence. Conditional
  code is folded with the shared phase-2 lexer and excluded rather than
  approximating the full C preprocessor.

## Verification

Serial evidence:

```text
arpg_raylib build                              PASS  39/39
arpg_game + arpg_platform_tests build          PASS  90/90
final Stage12 incremental rebuild              PASS   2/2   3.1s
exact five-test integration selector           PASS   5/5 360.05s
final platform.units                           PASS   1/1  17.62s
final platform.input_latency_source            PASS       113.3s
final platform.host_input_source               PASS       146.4s
final platform.host_validation_sequence        PASS       122.52s
BASE exact moved-definition comparison         PASS   20/20
retained capture/report comparison             PASS    9/9
complete run_raylib_host comparison             PASS
git diff --check                               PASS
```

The exact five-test selector passed together before the last guard-only
hardening. Each subsequently affected test was then rerun directly and passed;
production code did not change during those follow-ups. Ninja's truncated
dependency log was repaired with `ninja -t recompact`; a second recompact had
no warning and `ninja -n arpg_platform_tests` reported no work. No build/test
process remained afterward.

The unrelated `architecture.persistence_checkpoint_only` and Stage11D formal
selector failures remain outside this card and were not modified.

## Independent review

Final independent review: **Approved**, with no open P1/P2/P3 finding.

The review independently confirmed 20/20 moved blocks, 9/9 retained blocks,
and the entire `run_raylib_host()` against BASE `7ae2744`. Follow-up rounds
closed concrete decoys involving early returns, inactive preprocessor copies,
phase-2 splices, split comment openers, unknown conditional chains, mutually
exclusive branch reassembly, observer/drain arguments and order, namespace
ownership, and unreachable CMake registration.

The test-generated `tests/platform/__pycache__/*.pyc` file is intentionally
untracked and excluded from staging under the no-delete rule. Exactly the
eight brief-listed source/test files form the Task 6A commit.

Code commit: `33d8681` (`refactor: extract stage17 validation runtime`).
