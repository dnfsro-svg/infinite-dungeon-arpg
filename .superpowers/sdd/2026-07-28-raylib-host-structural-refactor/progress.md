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
Task 6A: complete — Stage17 shared state plus six runtime observers/injector
and private helpers were moved verbatim into the raylib-free header and new
runtime source. Production comparison passed 20/20 moved blocks, 9/9 retained
capture/report blocks, and the complete run_raylib_host body. Serial builds
passed; the exact five-test selector passed 5/5, every later affected guard was
rerun directly, and final independent review approved with no open P1/P2/P3.
Code commit: 33d8681.
Task 6B: complete — Stage17 capture/report helpers moved into
`host_validation_stage17_report.cpp`; production capture lifecycle and summary
order remained Host-owned. Final direct guard passed in 312.38s, self-test
passed in 1423.22s under the unchanged 1500s timeout, and independent round-3
review approved with no open Critical/Important findings. Code commit: 35cda88.
Task 7A: complete — opaque HostValidationRuntime now owns the six validation
states and encapsulates physical-input injection plus migrated Stage17 event,
snapshot, inventory, and submitted-action observation. Serial build passed;
all Task 7A gates passed in the final 16-test selector, while the known BASE
`stage11b.settings_formal` failure remained and its dependent validator was
not run. Independent round-3 review approved with Critical 0 / Important 0.
Code commit: e98e042.
Task 7B: complete — fixed-step and death-path encapsulation.
Task 7B.0: complete — Stage11D evidence ownership was realigned to the Task 7A
facade without production changes. The final guard passed in 15.05s and the
47-bad / 2-harmless mutation self-test passed in 694.42s under the unchanged
900s timeout. Independent round-3 review approved with Critical 0 / Important
0 / Minor 0. Code commit: d2792fc.
Task 7B.1: complete — final death/fixed-step owner guards proved RED before
production migration. The opaque facade now owns death eligibility, movement
selection, fixed-tick/death/post-tick observation, and validation target
completion without moving Host gameplay authority or changing call order.
Three-target serial build passed; all affected source/sequence/Stage10/11/11B/
11D guards and adversarial self-tests passed. The exact 38-test selector ran to
completion with every Task 7B gate green; its ten failures/skips match the
pre-recorded Task 2, Task 3/7A, and Task 5B BASE failures. Independent Round 3
review approved with Critical 0 / Important 0 / Minor 1 (non-blocking and
covered by the central exact namespace guard). Code commit: adacc40.
Task 7C: active — presentation/capture/summary final boundary. Three parallel
read-only audits found the master card incomplete: death-input cache and
pre-fixed pause-transition facade seams were missing; Stage11D needs the real
same-frame HUD notices plus explicit dimensions; the temporary state-access
shim, several mutation self-tests, the sequence-self timeout, and the 52-test
selector were outside the stated scope. The corrected 28-file execution card
is frozen in ignored `task-7c-brief.md` at BASE adacc40. A single 14-test BASE
addendum process is running before any production edit; together with Task 7B's
38-test record it forms the Task 7C 52-test comparison set. Production code is
still unchanged.
Task 7C planning gate: complete — corrected 28-file card independently reviewed
with production Critical 0 / Important 0, guard Critical 0 / Important 0 /
Minor 0, and gate Critical 0 / Important 0. The card preserves the distinct
GroundLoot/overlay phases, exact capture override and same-frame exit behavior,
52/30/14 selectors, bounded background test processes, and 25 named mutation
families mapped to their real owners.
Task 7C BASE addendum: complete at adacc40 after serially relinking Stage11C
stress/formal and Stage17 formal. The exact 14-test addendum reproduced 9 pass,
Stage11C formal failure, two dependent fixture skips, and the two pre-existing
stale Stage11C evidence-guard failures in 410.17s; a prior unrelinked diagnostic
run had the identical name/result set and is not used as BASE evidence.
Task 7C RED phase: active — no production code changed. Stage10 evidence RED at
the missing presentation-decision definition; Stage11 death evidence RED at the
missing runtime presentation owner; Stage11B architecture/evidence RED at the
missing pre-fixed pause-transition facade. The four central/input guards are
also RED at the intended missing seams: exact CaptureOwner API, public cached
Stage11D input snapshot access, removal of HostValidationStateAccess, and the
new death-input sequence. Stage11C architecture/evidence are RED at the missing
observe_hud and prepare_hud_snapshot runtime owners. Stage11D guard retargeting
then completed its RED proof at the intended missing explicit screen-dimension
seams in both renderer-integration and loot-evidence guards. All direct Task 7C
guards are now RED for expected missing production owners; production migration
is active while mutation self-test files finish in their disjoint scope.
Task 7C production migration: implemented in the six approved files. The
opaque runtime now owns death-input cache access, pre-tick pause observation,
HUD preparation/evidence, active-skill and ground-loot observation, presented
completion/capture decisions, capture-result promotion, and ordered summaries.
Host retains the real renderer, Stage12/manual capture arbitration, EndDrawing,
capture I/O, and clean-shutdown authority. `arpg_game` configured and built
serially with MSVC in 29.7s.
Task 7C direct GREEN: central facade/source passed in 106.05s after its retained
dead-code/format adversaries were hardened; central sequence passed in 92.19s;
host input passed in 107.03s; input latency passed in 70.48s; Stage11C
architecture/evidence passed in 70.97s/48.34s; Stage11D renderer/loot passed in
71.58s/31.13s. Migration exposed and fixed only stale guard ownership,
formatting, CMake semicolon-counting, and removed-seam expectations. Stage10,
Stage11, and Stage11B hardened direct guards are still being reconciled before
mutation self-tests.
Task 7C final verification: complete; commit pending. The focused selector
passed 30/30. The exact 52-name regression run reproduced only BASE functional
failure categories; its transient #71/#73 timeouts and intentional #104 RED
were closed by current-state cold reruns: #71 passed in 249.54s, #73 passed in
275.55s, and #104 passed in 1290.53s. M24/M25 now have 22 targeted adversarial
cases (101 total mapped Mxx cases); targeted 22/22 passed. Two final bounded
read-only reviews approved with Critical 0 / Important 0 / Minor 0. Exact
28-file staging and commit remain; this ledger must stay unstaged.
Task 7C: complete — exact 28-file commit `60611de` (`refactor: complete
host validation boundary`). The progress ledger remains the only tracked
unstaged file in the isolated worktree.
Task 8A: complete — fixed-step frame gating moved verbatim into the dedicated
`host_frame_gate.cpp`; public declaration and the two Host calls are unchanged.
RED first failed only at the missing owner source. A stale input-latency guard
made the real scope five files and was retargeted to the new owner. VS2022/SDK
26100 serial build linked `arpg_game` and `arpg_platform_tests`; the planned
selector passed 3/3 in 456.3s, the retargeted input-latency guard passed in
66.7s, and the final streamlined source guard passed in 220.8s. Its peak
Task8A addition was reduced from about 480 lines to 183 while retaining full
owner-TU, direct-call, all-src ownership/macro, phase-2/token-paste, and real
CMake registration boundaries. Two final reviews approved with Critical 0 /
Important 0; one fail-closed CMake formatting risk remains non-blocking. Code
commit: `1c33dea` (`refactor: extract host frame gate`).
Task 8B: complete — settings notice, renderer-filter selection, preview/apply/
rollback/quit settlement, and persistence publication moved into the value-type
`HostSettingsRuntime`; the four compatibility functions are thin wrappers and
Host retains the original update/notice/revision/settle/hint order. The final
serial MSVC build linked the game, platform tests, and Stage11B formal target;
platform units passed 510/510 in 17.21s, Host source/input/sequence guards passed
in 334.03s/308.30s/169.95s, Stage11B and Stage11D base guards passed, and the
new Task8B targeted owner suites passed 7-bad/1-harmless in 226.57s and
17-bad/1-harmless in 743.00s. The pre-existing Stage11B formal fixture failure
remains unchanged and was not reclassified as a Task 8B regression. Final
production and guard reviews approved with Critical 0 / Important 0 / Minor 0.
Code commit: `89928b7` (`refactor: extract host settings runtime`).
Task 8C: minor (deferred): `HostWindowBackend` permits null or throwing
function pointers even though `initialize()` and `close()` are `noexcept`;
production supplies a complete non-throwing raylib backend, so document or
enforce that precondition in a later API-hardening pass.
Task 8C: fix round 1/5 (3 addressed, 0 open — active interface surface,
unique raylib window owner, and full sequence self-test evidence; commits
2358e54..ba1c93f).
Task 8C: complete (commits 89928b7..ba1c93f, review clean).
Task 9 guard hardening: complete — exception cleanup and exact-owner guards
landed in `50d501c` and `9efa31e`. Fix round 4 added target-wide compile-time
raylib lifecycle poisoning, real compiler probes, the exact owner seam, and
renderer/namespace hardening in `23f94e8`. Its review found four remaining
guard escapes; final fix round 5 closed extra CMake exemptions, cross-directory
lifecycle directives, parameterized `##`/`%:%:` renderer macros, and sanitized
push/pop pragmas in `4f50dec`. All required selective tests, MSVC configure,
and four final full guards passed; final independent review approved with
Critical 0 / Important 0. The guard-only commits are frozen and no B-G or
gameplay behavior was changed.
Task 9: BLOCKED — the recorded full gate remains Debug 99/124 and Release
98/124, formal evidence is incomplete for five stages, several real-window
actions remain inconclusive, and the paused game used about 1.54 logical CPU
cores. The two deferred backend/renderer-lifetime P2 concerns also remain.
Rounds 4-5 intentionally did not rerun or weaken those gates. Do not push or
declare subproject A complete from the guard-only approval; gate recovery must
be a separately scoped prerequisite task from the preserved Task 9 baseline.
Task 9 gate recovery: integrated — `codex/task9-gate-recovery` fast-forwarded
from `4f50dec` to `e416340` after independent overlap audit. The recovery
commits restore the real Stage9 population fixture and the public-only Stage10
dense combat/reward transaction fixture; their isolated branch gates passed,
while post-integration focused revalidation and the original full Task 9 gate
remain pending. Existing Task 9 evidence/metric dirt was hash-verified unchanged.
Task 9 deferred P2 audit: closed as stale-report findings — commit `50d501c`
already enforces complete `noexcept` window callbacks with null rejection and
keeps renderer resource shutdown ahead of window destruction in both exception
paths. Existing unit/source guards cover both contracts; no production change
is required. The final Task 9 report must replace its obsolete open-P2 wording.
Task 9 Stage9 evidence recovery: implementation/review complete — the 1000-room
trace no longer synthesizes `relay_defeated`; it defeats each sealed-plan
ordinal through the approved real `CombatWorld::defeat_monster` seam and the
production session relay. The helper now accepts legal zero-affix monsters while
retaining all blueprint/runtime/count/bit/event/fault checks. Debug serial build
was clean; Stage9 fixture and evidence guard passed 2/2, and the direct 1000-room
stress passed 2 cases/0 failures in 26.171 s. Stage10 remained on its approved
75 s worst-case budget; its fresh successful transaction took 20.01 s. Final
specification and code-quality reviews both passed with zero open findings.
Task 9 save-slot stack recovery: implementation/review complete — all three
automatic `SaveCheckpointSlot` values in the exit-unlock tests now use checked
`std::nothrow` heap ownership without changing revisions, capture/restore
operations, or assertions. The exact stack guard changed from RED to 1/1 GREEN,
the Debug dungeon target built serially, and independent specification/code
quality reviews passed with zero findings. Full dungeon runtime regression is
deferred to the grouped Task 9 gate after the remaining persistence repair.
