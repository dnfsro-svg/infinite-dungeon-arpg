# Task 6B implementation report

## Scope

- Worktree: `E:/game/.worktrees/whole-game-refactor-2026-07`
- BASE: `33d86816d9f5e31e6fbb5d73f9091d2b07135d8a`
- No files outside the seven task files and this required report were changed by Task 6B. The existing `progress.md` modification was preserved. The prohibited untracked `.pyc` was not modified or staged.

## TDD

- RED: after adding the three guard prerequisites, ran `ctest --test-dir out/build/windows-msvc-debug -R '^platform\\.host_validation_sequence$' --output-on-failure -j1`.
- Expected failure: `Stage17 report source is required: .../host_validation_stage17_report.cpp` (1/1 failed; 47.32 s).
- GREEN: the same guard passed after the report extraction (`platform.host_validation_sequence`, 127.01 s). The first direct GREEN invocation was tool-time-limited while CTest continued; `LastTest.log` recorded the completed PASS.

## Implementation

- Added `host_validation_stage17_report.cpp`, registered once beside `host_validation_stage17_runtime.cpp`.
- Moved the nine BASE definitions: five private helpers and four public capture/completion/summary APIs.
- Kept `raylib_host.cpp` as capture lifecycle owner and retained the Stage17 request-before-competing-paths, successful-capture completion mark, clean-shutdown assignment, and 11B -> 11C -> 11D -> 17 summary order.
- Added the four API declarations to the existing Stage17 header without raylib exposure.
- Extended all three specified source guards; the sequence guard validates report ownership, public declaration/definition uniqueness, private helper scope, literal ownership, lifecycle surface, and source registration.

## BASE comparison

- Compared the nine moved BASE definitions against `33d8681`: 9/9 definitions are present once in the report source, 0 definitions remain in `raylib_host.cpp`, and 0 definitions are in the Stage17 runtime source.
- Normalization used by the guard: shared lexer translation-phase-2 logical lines, inactive-preprocessor masking, comment/string sanitization for definition evidence, and brace-depth checks. Literal ownership uses the unmodified source text because string sanitization intentionally removes basename/summary tokens.

## Verification

- Pre-build process check: no active `cmake`, `ctest`, `ninja`, `cl`, or `link` process.
- Build command attempted serially with `CMAKE_BUILD_PARALLEL_LEVEL=1` and `-j1`, but CMake regeneration stopped before compilation: `Stage 0 requires Windows SDK 10.0.26100.0; detected ''`.
- Therefore the requested full build and filtered CTest suite could not be completed in this environment. `git diff --check` remains pending after the blocked build.

## Concerns

- Resolved by the coordinator: SDK 10.0.26100.0 was installed; the ordinary
  PowerShell session lacked the repository-required developer environment.
  `scripts/Configure.ps1` entered the fixed MSVC 19.44 / SDK 26100 environment.

## Test repair round 1

- Root cause: `HOST_OVERRIDE` negative fixtures deliberately omit the production `try` nesting. The unconditional production loop-depth check rejected the split fixed-step fixture before its intended fixed-step-priority assertion.
- Fix: retain depth-two enforcement for the real host only; allow `HOST_OVERRIDE` fixtures to reach their targeted negative-control checks. Report ownership checks remain source-level checks and continue to run for every invocation.
- Tightened the Task 6B CMake proof by parameterizing and reusing `assert_stage17_top_level_registration` for both `host_validation_stage17_report.cpp` and `host_validation_stage17_runtime.cpp`.
- Cleaned the host aliases: removed no-longer-used Stage17 state aliases and normalized the new alias indentation.
- Verification command: `ctest --test-dir out/build/windows-msvc-debug -R '^(platform\\.host_validation_sequence|platform\\.host_validation_sequence_self_test)$' --output-on-failure -j1`.
- Result: 2/2 passed. `platform.host_validation_sequence` 125.58 s; `platform.host_validation_sequence_self_test` 1210.63 s.
- Independent BASE equivalence evidence supplied by the coordinator: all 9/9 moved functions are identical after whitespace folding; `present_frame_and_maybe_capture` and complete `run_raylib_host` are equivalent to BASE.

## Independent review repair round 1 findings

- Lifecycle proof must bind the real executable capture/request/success/mark and post-loop summary sequence, including capture helper ownership.
- Header declarations require direct `arpg::platform::host_validation` scope binding; report private helpers require binding to either of its two anonymous namespace ranges.
- String-token ownership must inspect only active code/string literal surfaces, excluding comments and inactive preprocessor regions.

## Independent review repair round 1 implementation

- Added real-host-only capture lifecycle assertions for `stage17_capture_path`, requested capture binding, competing-path priority, capture success, gated completion marking, exact shutdown assignment, 11B -> 11C -> 11D -> 17 summary order, and `present_frame_and_maybe_capture` raylib ownership.
- Header declarations now require direct `arpg::platform::host_validation` depth; private helpers now bind to the appropriate first/second anonymous namespace range.
- Replaced raw literal searches with named-literal preservation before the shared lexer and inactive-conditional masker; basenames bind to `stage17_capture_name`, while result/shutdown fields bind to the summary function.
- Direct sequence was iterated after two guard defects: the second anonymous namespace required relative bounds, and a non-Stage17 ready-state predicate was not globally unique. The final direct sequence pass completed in 200.72 s.

## Coordinator final verification

- The prescribed filtered selector ran 9 tests on the production extraction:
  8 passed; only `platform.host_validation_sequence_self_test` failed because
  the new guard precondition masked its intended negative assertion. All other
  selected tests passed: `platform.units`, four `stage17.*` tests,
  `platform.input_latency_source`, `platform.host_input_source`, and the direct
  `platform.host_validation_sequence` guard.
- After repair round 1, the only affected pair was rerun on the final guard and
  passed 2/2 (125.58 s and 1210.63 s). This combines with the unchanged 7/7
  results above; unrelated tests were not repeated.
- Final serial build, after the alias-only Host cleanup, used the repository
  toolchain entry and completed 4/4 Ninja steps for `arpg_game` and
  `arpg_platform_tests`.
- Final `git diff --check` exited 0; only Git's existing CRLF conversion
  warnings were printed.

## Independent review repair round 1 final verification

- The first prescribed four-test combination passed `platform.input_latency_source`
  (107.15 s), `platform.host_input_source` (118.92 s), and
  `platform.host_validation_sequence` (205.46 s). The mutation self-test had
  no assertion output, but reached its 1500 s CTest timeout (1500.05 s).
- Reduced only redundant guard work: active literal evidence is needed solely
  in the report source's capture-name and summary functions, so the guard no
  longer re-lexes the host/runtime/header for those literals; the retained
  report path also no longer sanitizes before calling a masker that sanitizes
  itself. No timeout, CMake registration, or mutation fixture was changed.
- Re-ran only the failed `platform.host_validation_sequence_self_test` case:
  PASS in 1305.65 s, below the existing 1500 s timeout.
- Final `git diff --check` exited 0; Git printed only existing CRLF conversion
  warnings. No staging or commit was performed.

## Independent review repair round 2 findings

- The first lifecycle proof was limited to production execution and used a
  cross-loop regular expression, so it did not prove direct reachability,
  source-wide raylib API ownership, or mutation resistance.
- Namespace containment accepted token matches without proving the header's
  nested namespace relationship or report anonymous namespace ownership.
- Active literal binding was correct for the report target functions, but the
  performance repair had removed active wrong-owner rejection for the other
  three Stage17 surfaces.

## Independent review repair round 2 implementation and verification

- Replaced the production lifecycle regex with complete active-host validation:
  it extracts `run_raylib_host`, its real loop, and the presentation helper;
  proves direct binding depth, all capture competition ordering, no return
  between request and completion, gated marking, post-loop shutdown and four
  summaries, and source-wide single ownership of the three raylib APIs.
- Direct production execution now runs in-memory adversarial lifecycle checks
  for deleted completion, request-after-competition ordering, early return,
  and uncalled lambda/cross-scope evidence. Legacy `HOST_OVERRIDE` fixtures
  keep their focused checks; extra cross-file literal scans run only on the
  direct production path.
- Header scope now proves `host_validation` is directly contained by the
  extracted `arpg::platform` range. Both report anonymous ranges must be
  directly contained by the extracted Stage17 host-validation namespace.
- Restored comment/preprocessor-aware active literal wrong-owner rejection for
  host/runtime/header, while each required report literal must occur exactly
  once in its owning target function.
- `platform.host_validation_sequence`: PASS, 280.37 s.
- `platform.host_validation_sequence_self_test`: PASS, 1440.89 s (under the
  unchanged 1500 s CTest timeout). No timeout, CMake registration, production
  source, or self-test fixture was modified.

## Independent review repair round 3 and final approval

- Replaced construction-only lifecycle mutations with a shared non-fatal
  validator used by both production and every negative source variant.
- The eight capture-competition tokens are unique, ordered, and bound to their
  required brace depths. The early-return window now extends from the Stage17
  capture request through the Stage17 summary call.
- Verified precise rejection codes for deleted completion marking
  (`capture_mark_missing`), request-after-competition and uncalled-lambda
  evidence (`capture_competition_scope`), request-to-summary early return
  (`capture_early_return`), and cross-function completion evidence
  (`capture_mark_missing`).
- Hoisted only repeated inactive-conditional preprocessing. Production and all
  five negative variants still enter the same validator; no mutation or
  timeout was removed or relaxed.
- Final direct guard: PASS, 312.38 s.
- Final self-test: PASS, 1423.22 s, below the unchanged 1500 s timeout.
- Final `git diff --check`: PASS, with only existing CRLF conversion warnings.
- Independent round-3 review marked all three prior findings ADDRESSED, found
  no new Critical/Important finding, and approved Task 6B.
