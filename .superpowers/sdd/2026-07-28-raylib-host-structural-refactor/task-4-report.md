# Task 4 Report: Stage11C HUD Validation Extraction

## Scope

- BASE: `5f64d894046b391d2ae0d5a630b17b43cd132f06`.
- Added `host_validation_stage11c.hpp/.cpp`, registered the source in
  `arpg_raylib`, and moved the Stage11C state, physical driver, snapshot hash,
  completion predicate, and summary writer out of `raylib_host.cpp`.
- The host retains the original low-health fixture, CJK readiness assignment,
  debug visibility updates, renderer model/notices/layout/hash capture,
  presented-frame counter, and `EndDrawing()`-then-screenshot path.

## TDD evidence

- RED: architecture, evidence, and sequence guards each failed because the new
  Stage11C private header was absent.
- GREEN: all three guards pass after the production move. Evidence mutations
  copy and change the actual host, input, or Stage source; the shared
  splice-aware scanner isolates the Stage functions.

## Verification

- Serial MSVC targets `arpg_game`, `arpg_platform_tests`,
  `arpg_stage11c_hud_stress`, and `arpg_stage11c_hud_formal` linked.
- Required Step3 selector listed exactly 8 tests and passed 8/8: units 17.88s,
  stress 0.29s, architecture 0.08s/self 0.51s, sequence 37.97s/self 311.93s,
  evidence 2.70s/self 27.25s.
- `git diff --check` passed.

## Review round 2 hardening

- Production code changed only by adding equivalent braces to the two
  Stage11C early returns, preserving the original execution paths while making
  the source-level guard's single-final-return contract explicit.
- Evidence guard now rejects early top-level returns in the driver, hash,
  reached, and summary functions. Its host seams are located as unique real
  line comments in the code-state `run_raylib_host` runtime range; the guard
  verifies exact model/notices/layout/hash values and capture scope/order.
- Architecture and sequence guards now share a quote-aware, raw-line CMake
  source-registration scanner. It removes only real comments before list
  processing and accepts the unquoted exact source argument only inside a
  genuine `add_library(arpg_raylib ...)` or `target_sources(arpg_raylib ...)`
  command range.

## Review round 2 TDD and verification

- RED: the old evidence self-test accepted `stage_driver_early_return` in
  49.751s; it also accepted a marker-comment/string host decoy in 45.354s.
  The old architecture self-test accepted the CMake comment/quoted decoy in
  19.043s, and the sequence CMake-only self-test accepted it in 50.882s.
- GREEN before final closure: evidence direct/self passed in 3.011s/66.554s
  after early-return hardening; expanded host-seam direct/self passed in
  4.660s/126.717s. Architecture direct/self passed in 2.984s/19.490s, while
  sequence direct/CMake-only passed in 44.353s/54.528s.
- Final VS2022 x64 serial build of `arpg_game`, `arpg_platform_tests`,
  `arpg_stage11c_hud_stress`, and `arpg_stage11c_hud_formal` passed in
  196.053s. The exact eight-test Step3 selector passed serially in 969.00s:
  units 17.45s, stress 0.30s, architecture 2.76s/self 17.86s, sequence
  43.65s/self 756.37s, and evidence 5.05s/self 125.54s.
- Step4 formal was intentionally not rerun. The same 102-file staged-asset,
  BASE-proven baseline recorded above remains applicable.

## Review round 3 Phase A2 direct GREEN

- RED self-test command:
  `cmake -DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07 -DGUARD_TEST_ROOT=<unique-temp-root> -P tests/platform/stage11c_hud_evidence_guard_self_test.cmake`.
  The direct guard remained green (`exit 0`, 4.502s), while the self-test
  failed as intended (`exit 1`, 126.606s): it accepted
  `stage_driver_unreachable_return`.
- The all-depth return guard initially rejected the positive driver contract
  (`exit 1`, 4.317s). The driver was rewritten to a single equivalent final
  return and the summary's two early returns were rewritten as equivalent
  nested conditions. Reached retains its current nine-return inventory: the
  single pre-switch `return false;`, seven switch cases, and final fallback.
- GREEN command:
  `cmake -DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07 -P tests/platform/stage11c_hud_evidence_guard_test.cmake`.
  It passed (`exit 0`, 4.550s). The expanded self-test was intentionally not
  run in Phase A2.

## Review round 3 Phase A3 self-test

- Command:
  `ctest --test-dir out/build/windows-msvc-debug -R '^stage11c\\.hud_evidence_guard_self_test$' --output-on-failure -j1`.
  It passed (`exit 0`, test 152.88s; wall-clock 152.951s).
- The self-test verified each new mutation against its target rejection
  summary: `stage_driver_unreachable_return` → `physical driver return
  inventory`; `stage_hash_unreachable_return` → `production hash return
  inventory`; `stage_reached_unreachable_return` → `reached predicate return
  inventory`; `stage_summary_unreachable_return` → `summary return
  inventory`; and `stage_hash_direct_overwrite` → `production hash direct
  overwrite`.

## Review round 3 Phase B1 seam-scope RED

- Added self-test mutations that preserve the unique BEGIN/END markers and
  unmodified seam contents while moving the complete observation seam or
  complete presented-capture seam into an uncalled `[&] { ... };` lambda; a
  third mutation moves the complete reached seam into `if (false) { ... }`.
- Quick mutation command:
  `cmake -DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07 -DOUTPUT_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07/out/build/windows-msvc-debug/phase-b1-red-mutation -P out/build/windows-msvc-debug/stage11c-phase-b1-red.cmake`.
  The existing guard incorrectly accepted
  `host_observation_seam_in_lambda` (`exit 0`, 4.834s), establishing RED.

## Review round 3 Phase B2 seam-scope GREEN

- The runtime crop is now sanitized into a length-preserving code-state
  surface; the guard asserts equal raw/sanitized lengths before mapping raw
  marker positions. Observation, reached, and presented-capture BEGIN/END
  markers each have the fixed absolute runtime brace depth 3, and each pair
  must have matching begin/end depth inside the verified `run_raylib_host`
  crop.
- GREEN verification was serial: direct guard `exit 0`, 23.903s; complete
  observation-lambda mutation rejected by `observation seam scope`, `exit 0`,
  18.114s; complete reached-`if(false)` mutation rejected by `reached seam
  scope`, `exit 0`, 23.413s; complete presented-capture-lambda mutation
  rejected by `presented capture seam scope`, `exit 0`, 24.292s. The mutation
  driver treats expected guard rejection as success.

## Review round 3 Phase B3 streaming-depth performance correction

- Removed the full runtime-crop sanitized copy. The existing streaming marker
  lexer now treats `{` and `}` as code-state delimiters and carries brace depth
  to each real line-comment marker; braces in strings, character literals, and
  comments are not counted. Seam extraction consumes the returned
  `position|depth` entries directly.
- Serial GREEN verification: direct guard `exit 0`, 6.317s; observation lambda
  rejected by `observation seam scope`, `exit 0`, 0.957s; reached `if(false)`
  rejected by `reached seam scope`, `exit 0`, 1.460s; presented-capture lambda
  rejected by `presented capture seam scope`, `exit 0`, 1.815s. Direct is now
  below the 10s performance limit without relaxing the absolute depth checks.

## Review round 3 Phase C1 bracket-form RED

- Architecture self-test now has CMake override mutations that replace the
  real source entry with a `#[=[ ... ]=]` bracket comment containing an
  independent source-name line and a non-closing `]]`, or with a `[==[ ... ]==]`
  bracket argument containing the source name, `#`, `;`, parentheses, and a
  pseudo target. Both expect the exact-registration error.
- Sequence self-test has matching bracket-comment and bracket-argument CMake
  mutations before `SEQUENCE_CMAKE_ONLY` returns, after the existing line
  comment/quoted mutation.
- RED commands ran serially. Architecture:
  `ctest --test-dir out/build/windows-msvc-debug -R '^stage11c\\.architecture\\.hud_boundaries_self_test$' --output-on-failure -j1`;
  `exit 8`, 22.515s, because old parsing accepted
  `stage_cmake_bracket_comment_decoy`. Sequence CMake-only:
  `cmake -DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07 -DGUARD_TEST_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07/out/build/windows-msvc-debug/sequence-cmake-only-round3 -DSEQUENCE_CMAKE_ONLY=ON -P tests/platform/host_validation_sequence_guard_self_test.cmake`;
  `exit 1`, 107.750s, because it accepted
  `sequence_cmake_bracket_comment_decoy`. Each stopped at the first erroneous
  acceptance, before the already-registered bracket-argument mutation.

## Review round 3 Phase C2 bracket-form GREEN

- The shared source-registration scanner now maintains cross-line lexical
  state for normal code, escaped double quotes, ordinary line comments,
  bracket comments, and bracket arguments. It recognizes `#[(=*)[` and
  `[(=*)[` with any number of equals and masks each non-code region until its
  exact `]${equals}]` close; newlines remain available for independent-line
  matching while hidden `#`, `;`, parentheses, source names, and pseudo targets
  cannot affect registration or parenthesis depth.
- Serial GREEN commands/results: architecture direct
  `ctest --test-dir out/build/windows-msvc-debug -R '^stage11c\\.architecture\\.hud_boundaries$' --output-on-failure -j1`,
  `exit 0`, 3.017s; architecture self with the bracket mutations, `exit 0`,
  25.422s; sequence direct
  `ctest --test-dir out/build/windows-msvc-debug -R '^platform\\.host_validation_sequence$' --output-on-failure -j1`,
  `exit 0`, 55.792s; and sequence CMake-only with an absolute
  `GUARD_TEST_ROOT`, `exit 0`, 138.807s.
- Both bracket-comment and bracket-argument mutations are now rejected for
  the required exact-registration cause: architecture reports
  `arpg_raylib does not register host_validation_stage11c.cpp exactly once`;
  sequence reports `arpg_raylib does not register host_validation_stage11c.cpp`.

## Review round 3 Phase D1b hash and marker-scan hardening

- Hash core now requires the unique, ordered FNV offset-basis, xor, and prime
  statements. The xor and prime must occur at the immediate body depth of the
  exact `mix` lambda, before its matching close; direct overwrite and return
  inventory checks remain in force.
- The runtime crop now performs one streaming code-state pass for all six
  Stage11C BEGIN/END markers. It records `name|kind|position|depth`, preserves
  splice handling, and ignores marker/brace text in comments or literals;
  extraction then enforces raw/code uniqueness, fixed depth 3, begin=end depth,
  and seam order from that single table.
- Direct evidence command passed (`exit 0`, 4.975s). Fast mutations passed by
  obtaining the required guard rejection: observation/reached/presented-capture
  relocation by their respective seam-scope causes in 1.002s/1.003s/1.047s;
  FNV offset/xor/prime drift by `production hash core` in
  5.235s/5.568s/4.920s. The mutation driver treats expected rejection as
  success; full evidence self-test was intentionally not run in this phase.

## Review round 3 Phase D2 final evidence validation

- Serial CTest evidence validation passed: direct
  `ctest --test-dir out/build/windows-msvc-debug -R '^stage11c\\.hud_evidence_guard$' --output-on-failure -j1`
  exited 0 in 4.462s (CTest 4.41s); complete self-test
  `ctest --test-dir out/build/windows-msvc-debug -R '^stage11c\\.hud_evidence_guard_self_test$' --output-on-failure -j1`
  exited 0 in 174.855s (CTest 174.82s).
- The complete self-test therefore confirmed the return-inventory, FNV-core,
  direct-hash-overwrite, and static-compilable seam-relocation mutations all
  fail through their designated rejection summaries.

## Review round 3 Phase D3a executable hash-core scope

- Added a static-compilable relocation mutation: the complete FNV algorithm is
  retained inside an uncalled `[[maybe_unused]]` lambda without its inner final
  return, while the outer function returns a replacement `hash{1U}`.
- Hash-core checks now require initialization and mix-lambda open at absolute
  depth 1, xor/prime at depth 2, the ordered top-level session/progression mix
  chain at depth 1, and final `return hash;` at depth 1.
- Direct evidence command exited 0 in 4.890s. The targeted relocation command
  exited 0 in 4.775s because the guard rejected it by the exact
  `production hash core` cause; the driver treats that expected rejection as
  success. Full self-test was intentionally not run in this targeted phase.

## Review round 3 Phase D3b final evidence rerun

- Final serial evidence CTest passed: direct exited 0 in 5.308s (CTest 5.26s)
  and full self-test exited 0 in 172.650s (CTest 172.61s). The full self-test
  includes `stage_hash_core_in_uncalled_lambda`, confirming its designated
  `production hash core` rejection along with all prior evidence mutations.

## Review round 3 Phase D4 VS2022 x64 serial build

- Command (VS2022 Build Tools x64 environment):
  `cmake --build out/build/windows-msvc-debug --target arpg_game arpg_platform_tests arpg_stage11c_hud_stress arpg_stage11c_hud_formal -- -j1`.
  It exited 0 in 194.543s. The sampled peak main compiler working set was
  `cl.exe` 1,970,655,232 bytes (1,879.36 MiB).
- The build linked all four requested targets. Existing MSVC warnings remained
  in unrelated platform tests (C4267/C4127), and Ninja emitted its non-fatal
  `premature end of file; recovering` warning; neither caused a build failure.

## Review round 3 Phase D5 exact-eight selector

- The selector
  `^(platform\\.units|platform\\.host_validation_sequence|stage11c\\.architecture\\.|stage11c\\.hud_evidence_guard|stage11c\\.hud_stress)`
  listed exactly eight tests (#34/#58/#72/#73/#94/#95/#98/#99), then serial
  `ctest --output-on-failure -j1` exited 0 in 1,118.265s (CTest real
  1,118.22s). Per-test times: units 17.00s; HUD stress 0.30s; architecture
  2.86s/self 24.58s; sequence 54.82s/self 841.21s; evidence 4.84s/self
  172.58s. All 8/8 passed; no formal test was selected or run.

## Remaining formal evidence

- Step4 listed exactly two tests. `stage11c.hud_formal` failed in 160.32s:
  combat, low-health, and debug passed; cleared, abyss-warning, and level-up
  failed. The dependent validator was skipped.
- A detached BASE worktree was staged to the same 102 runtime assets as HEAD;
  a relative-path plus SHA-256 comparison returned `DIFF_COUNT=0`. Its formal
  run exited 1 in about 101.98s with the same per-scenario result: combat and
  low-health passed; cleared, abyss-warning, and level-up failed; debug
  passed. The three failing scenarios have the same zero snapshot hash and
  image hash on BASE and HEAD, so this is a BASE-proven formal baseline rather
  than a refactor regression. Low-health image hashing remains non-deterministic
  between runs, but its validator result is valid (`valid=1`) on both sides.

## Review round 1 hardening

- Evidence guard now isolates the four Stage11C functions from the private
  Stage source and locks their driver, hash, reached, and summary semantics.
  Host capture assertions use comment/string-safe seams for unique model,
  notices, layout, hash, presented-frame, and screenshot ordering checks.
- Architecture guard uses the same splice-aware small-file scanner for the
  Stage header/source. It rejects comment/string/forward-declaration decoys;
  its CMake check accepts only an exact, uncommented source entry inside
  `arpg_raylib`.
- Sequence guard retains host ordering/scope assertions while validating the
  four Stage11C definitions through the Stage source extractor. Its self-test
  rejects header/source comment, string, forward, cross-function, lambda, and
  LF/CRLF spliced-comment decoys.

## Review round 1 verification

- VS2022 x64 serial build of `arpg_game`, `arpg_platform_tests`,
  `arpg_stage11c_hud_stress`, and `arpg_stage11c_hud_formal`: passed in
  195.59s.
- Step3 `ctest -N` selector listed exactly eight tests; serial execution
  passed 8/8 in 844.80s: `platform.units` 17.75s, HUD stress 0.30s,
  architecture 2.81s/self 18.96s, sequence 56.59s/self 690.22s, and evidence
  2.79s/self 55.37s.
- Step4 formal was intentionally not rerun. The same 102-file staged-asset
  BASE comparison above remains the applicable BASE-proven evidence.
- `git diff --check` passed.
