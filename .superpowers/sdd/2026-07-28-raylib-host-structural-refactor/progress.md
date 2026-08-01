# SDD ledger — plan: docs/superpowers/plans/2026-07-28-raylib-host-structural-refactor.md

Branch: codex/whole-game-refactor-2026-07
Plan commit: 05c3834
Plan correction: 3b3ac0a (fixed-step production-input guard anchor)
Merge baseline: f132f276e5ac8339acb9573146a260193961eeb6
Task 0: pending
Task 0: review findings (round 1 required) — Stage11C model/hash scan narrowed; alias RHS not proven; fixed-step branch structure not proven
Task 0: fix round 1/5 (3 addressed, 0 open — complete-surface model/hash scan; exact alias RHS; fixed-step branch structure; commits 6c173ee..0197327)
Task 0: complete (commits 3b3ac0a..0197327, review clean)
Task 1: review findings (round 1 required) — Stage11C input-source forbidden scan narrowed; final Head not rebuilt/full-selector rerun
Task 1: fix round 1/5 (2 addressed, 0 open — complete input forbidden scan; final Head build/selector evidence; commits dfbef4d..cdecc8b)
Task 1: deferred baseline — architecture.persistence_checkpoint_only fails on unchanged room_progress_codec.cpp -> dungeon/health_potion_loot.hpp dependency
Task 1: deferred build noise — Ninja reported recoverable premature end of file warning; both targets linked, exit 0
Task 1: complete (commits 0197327..cdecc8b, review clean)
Task 2: review findings (round 1 required) — focused CTest selector matched no stage10/stage11 tests and capture-order mutations failed for the wrong prerequisite; shared Stage10/11 source token scans allowed cross-function substitution
Task 2: fix round 1/5 (2 addressed, 0 open — corrected selector/mutation fixtures; per-function Session-route guards; scoped re-review required; FIX_BASE 67b1574)
Task 2: scoped re-review findings (round 2 required) — Stage10 capture mutations still fail additional non-order gates; brace parser counts comment braces and can again span into Stage11
Task 2: fix round 2/5 (2 addressed, 0 open — single-target capture mutations; shared comment/string-safe function isolation with adversarial mutations; scoped re-review required; FIX_BASE 0ef2305)
Task 2: scoped re-review finding (round 3 required) — C++ backslash-newline splicing can continue or form line comments, letting commented route decoys satisfy the scanner
Task 2: fix round 3/5 (2 addressed, 0 open — splice-aware comment scanning and Stage10/11 continuation-decoy mutations; scoped re-review required; FIX_BASE 894f3e6)
Task 2: deferred full-selector evidence — 22 selected; the last full execution was 16 pass/6 non-guard failures, with no claim that those six failures are baseline-proven
Task 2: complete (commits cdecc8b..7defd1c, review clean after 3 fix rounds)
Task 3: Stage11B state/injection/completion/hash/summary extracted into
host_validation_stage11b.hpp/.cpp. Formal 11B evidence remains red at the
same pause-resume predicate: BASE comparable runs observed 1 -> 4 and 1 -> 5,
while HEAD observed 1 -> 5. Record this as same failing predicate with
variable observed tick count; dependent validator not run, and fingerprints
were noncausal after the short circuit.
Task 3: review findings (round 1 required) — formal Step 4 failed and report reversed the short-circuit cause; Stage/source and host/sequence guards permit comment or cross-scope decoys; Stage11B private header missing from dependency scan; unused host include remains
Task 3: fix round 1/5 (all review findings addressed) — corrected formal
causality with comparable BASE evidence; Stage function isolation and
splice-safe decoys; host loop/function brace-depth scope checks including
lambda/out-of-loop full-call decoys; Stage11B header dependency mutation;
removed unused include. Serial build passed; focused selector lists exactly
six and passed 6/6 (581.18s). Fix commit: 94ac373.
Task 3: scoped re-review findings (round 2 required) — raw run_raylib_host pre-crop loses earlier comment/string lexical state; splice mutation retains only the short call rather than the guard's full sampled-keys declaration
Task 3: fix round 2/5 (all review findings addressed) — code-state lexer proves
the raw crop candidate before function isolation; one-pass cropped sanitizer;
block and valid escaped-multiline string pre-open decoys; complete-declaration
splice decoy; LF/CRLF lexical equivalence checks and splice block-close fix in
shared scanner. Task2 Stage10/11 scanner tests 4/4; serial build passed;
focused exact-six selector 6/6 (416.28s). No formal Step4 rerun. Fix commit:
7e2297b.
Task 3: complete (commits 22e73e4..9f0758c, review clean after 2 fix rounds; formal Step4 remains a BASE-proven variable-tick failure at the same predicate)
Task 4: implementation complete pending independent review; Step3 focused selector passed 8/8. Step4 formal has a same-asset, BASE-proven baseline: BASE/HEAD staged bin assets each had 102 files with relative-path plus SHA256 DIFF_COUNT=0; combat/low-health/debug passed and cleared/abyss-warning/level-up failed on both, with identical zero snapshot and image hashes for the three failures. Low-health image hashes remain non-deterministic between runs but validator valid=1 on both sides.
Task 4: review findings (round 1 required) — Stage11C evidence guard does not constrain injection/hash/reached/summary algorithms; host capture guard lacks comment/string-safe unique model/notices/layout/hash assignments and presented-frame/capture ordering; architecture/sequence boundary checks accept raw-text comment/string/forward-declaration/cross-function/CMake-comment decoys. FIX_BASE afa9637.
Task 4: fix round 1/5 (all findings addressed) — Stage11C Stage-source
function semantics and host capture ordering are guarded; architecture and
sequence guards use splice-aware small-file isolation with comment/string,
forward, cross-function/lambda, LF/CRLF splice, and CMake-comment/quoted-decoy
self-tests. VS2022 serial four-target build passed in 195.59s; exact Step3
selector passed 8/8 in 844.80s. Step4 formal intentionally not rerun: same
102-file staged asset, BASE-proven formal baseline remains applicable. Fix
commit: 2486d32 (`test: harden stage11c validation guards`).
Task 4: scoped re-review findings (round 2 required) — function-token guards
still accept early-return plus unreachable original code; raw seam discovery is
not unique or bound to the real run_raylib_host lexical/function scope; capture
ordering, brace depth, and full layout RHS remain underconstrained; sequence
keeps a raw CMake source check and architecture accepts a semicolon-split
`# decoy;host_validation_stage11c.cpp` registration. FIX_BASE 2486d32.
Task 4: fix round 2/5 (all findings addressed) — production changes are only
equivalent braces around two early returns; evidence guard rejects early
top-level returns, binds code-state runtime seams, and enforces exact RHS plus
capture scope/order; architecture and sequence share a quote-aware raw-line
CMake source-registration parser. VS2022 serial four-target build passed in
196.05s; exact Step3 selector passed 8/8 in 969.00s. Step4 formal intentionally
not rerun: the same 102-file staged asset and BASE-proven formal baseline remain
applicable. Fix commit: `test: close stage11c guard bypasses`.
Task 4: scoped re-review findings (round 3 required) — core guards still count
returns only at top-level function depth, so nested unconditional returns and a
late direct hash overwrite can leave required tokens unreachable while passing;
runtime seam checks preserve relative depth but do not bind marker pairs to the
absolute executable depth of `run_raylib_host`; the shared CMake scanner does
not persist bracket-comment state or distinguish bracket-quoted arguments, so
disabled or quoted source text can be counted as a real registration. FIX_BASE
4127669. Production behavior and the round-2 build/exact-eight evidence remain
accepted; only these validation-guard gaps are open.
Task 4: fix round 3/5 (all findings addressed) — Stage11C driver and summary
use behavior-equivalent single-exit/nested control flow; guards now enforce
all-depth return inventories, the executable FNV core and ordered mix chain,
and one-pass absolute runtime seam depths with compile-valid relocation
mutations. The shared CMake scanner preserves cross-line quote/comment state
and rejects arbitrary-equals bracket comments and bracket arguments. VS2022
x64 serial four-target build passed in 194.54s; the exact eight-test selector
passed 8/8 in 1118.22s. Step4 formal was intentionally not rerun because the
unchanged 102-file staged-asset BASE comparison still proves the known formal
failures are pre-existing. Fix commit: `test: seal stage11c guard control flow`.
Task 4: complete — independent final review of `4127669..94acada` found no
P1/P2/P3 findings and approved the card. Cross-audit confirmed HEAD 94acada,
a clean worktree, all four rebuilt executables, the exact 8/8 CTest log, and no
residual build/test processes. Task 5A is now the only active card.
Task 5A: complete — Stage11D state, selectors, physical-input runtime, safe
movement, fixed-step activation, and abyss-claim observation were extracted to
`host_validation_stage11d.hpp/.cpp` without changing host ordering or report
semantics. Independent round-2 review and final snapshot review found no
P1/P2/P3 findings. Serial build passed; the final exact-seven selector passed
7/7 in 1802.84s after correcting only the stale CTest self-test timeout metadata
to 600/900 seconds. Code commit: 6566026.
Task 5B: complete — Stage11D semantic recording, visibility evaluation, and
summary/report implementation moved to `host_validation_stage11d_report.cpp`;
the real host presented/reached/capture/captured/summary chain remained byte-
equivalent. Independent review closed one fidelity P3 and one guard P2, then
approved with no open P1/P2/P3. Three-target serial build passed in 217s and
the exact headless selector passed 5/5 in 126.64s. Formal evidence remains
deferred: both HEAD and a clean detached BASE `a9e34cd` build return exit 4 from
`--select-only` before entering the host, with formal source and selector
dependencies unchanged. Code commit: 18fb820.
Task 6A: active — extract Stage17 active-skill validation runtime.
