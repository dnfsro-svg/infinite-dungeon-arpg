# Task 7C implementation report

> Evidence status: **COMPLETE**. Exact 28-file commit: `60611de`
> (`refactor: complete host validation boundary`).

## Scope and frozen BASE

- Worktree: `E:/game/.worktrees/whole-game-refactor-2026-07`
- Branch: `codex/whole-game-refactor-2026-07`
- Task BASE: `adacc409c16a5ed6e936b614651d588a19c6e0d1`
- Scope: the corrected 28 tracked files in `task-7c-brief.md`: six production
  files, six central/input/registration guards, fifteen Stage-owner guards,
  and `docs/validation/raylib-host-refactor-baseline.md`.
- The tracked progress ledger is deliberately outside the future commit and
  remains unstaged. This report and the task card are ignored evidence files.
- Structural refactor only: no gameplay, fixed-step, input, death/pause,
  renderer, ground-loot, capture precedence, persistence, RNG, audio, or exit
  rule is intentionally changed.

Task 7C reuses Task 7B's exact 38-test BASE record and adds a separately
captured 14-test BASE selector after serially relinking its three executable
targets at `adacc40`.

| BASE component | Passed | Failed or dependency-skipped | Real time |
| --- | ---: | ---: | ---: |
| Task 7B exact selector | 28 | 10 | recorded in `task-7b-report.md` |
| Task 7C relinked addendum | 9 | 5 | 410.17 s |
| Combined 52-name BASE manifest | 37 | 15 | compared per test, not by totals only |

The relinked addendum's five non-passes were
`stage11c.hud_formal`, its two dependent evidence tests, and the two stale
Stage11C evidence guard/self-test failures. The latter failed because the old
guard could not bind the actual Stage11C runtime alias; this was known guard
drift and is not represented as a clean-project production failure. The
earlier unrelinked diagnostic run is not BASE evidence.

The final comparison framework is fixed as follows:

1. Extract the exact 52 selected names from the completed CTest record and
   require the final name set to match it exactly.
2. Compare every final result to its same-name BASE result. All architecture,
   source, sequence, and mutation gates must be green.
3. Classify any remaining non-gate result only against its already recorded
   Stage10/11, Stage11B, Stage11C, or Stage11D BASE category and root cause;
   any new name or new failure is a regression.
4. Require complete final stdout/stderr, `LastTest.log`, and
   `LastTestsFailed.log`; a total count alone is insufficient.

The exact 52-name run completed in 7110.19 s with the same functional
failure/dependency categories as BASE. Two architecture self-tests (#71 and
#73) timed out only in the long, resource-contended chain, and #104 stopped on
the then-current intentional RED fixture. The final affected reruns passed:
#71 in 249.54 s, #73 in 275.55 s, and #104 in 1290.53 s. The exact name set
matched the frozen manifest; after these affected reruns there is no new
per-name regression relative to BASE.

Final 52-test comparison plus affected exact reruns: **ACCEPTED**.

## TDD RED and GREEN

All direct guards were made RED before the six production files changed.
Their failures were at the intended missing Task 7C seams:

- central/input: exact `CaptureOwner`/`PresentationDecision` facade,
  `death_input_snapshot`, removal of `HostValidationStateAccess`, and the new
  death-input/observer sequence;
- Stage10/11: facade-owned presentation completion/capture decisions;
- Stage11B: pre-fixed-tick pause-transition observation;
- Stage11C: facade-owned HUD preparation and real HUD observation;
- Stage11D: facade-owned ground-loot observation and explicit real screen
  dimensions.

Long mutation self-tests were not started until the production migration made
the direct owners GREEN. During GREEN reconciliation, guards were hardened for
active lexical scope, inactive preprocessor regions, strings/raw strings,
uncalled lambdas, dead branches, cross-scope decoys, duplicate calls, stale
anchors, equivalent formatting, and CMake semicolon/list behavior. These were
guard corrections, not gameplay changes.

## Production migration

The opaque `HostValidationRuntime` now owns:

- the cached death-input snapshot and pre-fixed pause transition;
- Stage11C presentation-copy preparation and authoritative HUD evidence;
- active-skill and shared ground-loot presentation observation;
- presented-frame completion and desired capture ownership;
- capture-result state promotion, including exact paused-capture assignment;
- the ordered Stage11B/11C/11D/17 summary chain.

`raylib_host.cpp` still owns the production input/simulation loop, renderer,
`BeginDrawing`/`EndDrawing`, Stage12/material/manual/periodic path arbitration,
capture I/O, screenshot override behavior, clean-exit authority, and resource
shutdown. Ground-loot observation remains after the Stage17 draw observer and
Stage12 status collection but before passive/inventory/pause overlays. The
presented-frame decision remains after those overlays. Stage17 capture-path
query count, generic/manual override failure semantics, and current-frame
generic capture success in the clean-exit predicate are preserved.

`host_validation_state.hpp` remains; only the temporary
`HostValidationStateAccess` shim was removed. Stage11D visibility now receives
the Host's explicit screen width and height rather than querying globals.

## T7C mutation-family inventory

The 25 named families currently map to **101 concrete Mxx case/self-test
pairs**. Repeated case labels in different self-tests count separately because
each guard is an independent acceptance boundary.

| ID | Concrete cases | Owning self-test | Count |
| --- | --- | --- | ---: |
| `T7C-M01` | `m01_dead_decoy_before_stage17`, `m01_lambda_decoy`, `m01_before_stage17` | Stage11D renderer integration | 3 |
| `T7C-M02` | `m02_dead_decoy_before_stage12`, `m02_lambda_decoy`, `m02_before_stage12_status` | Stage11D renderer integration | 3 |
| `T7C-M03` | dead-decoy-after-passive, semantic-after-capture, and after passive/inventory/pause overlay | Stage11D renderer integration | 5 |
| `T7C-M04` | rebuild a second view; declare/pass a second view | Stage11D renderer integration | 2 |
| `T7C-M05` | cached notices; previous-frame notices | Stage11D renderer integration | 2 |
| `T7C-M06` | constant dimensions; swapped dimensions; report-side global screen calls | Stage11D renderer integration | 3 |
| `T7C-M07` | after `GetFrameTime`, after frame gate, after fixed tick, plus comment/string/raw/inactive/lambda/dead/cross-scope early decoys | Stage11B settings evidence | 10 |
| `T7C-M08` | dead-decoy/get-call and real before-renderer relocation in architecture; dead-decoy/get-call in evidence | Stage11C architecture + evidence | 3 |
| `T7C-M09` | lambda-decoy/get-call and real after-`BeginDrawing` relocation in architecture; lambda-decoy/get-call in evidence | Stage11C architecture + evidence | 3 |
| `T7C-M10` | model dead-decoy, notices lambda-decoy, fabricated model, fabricated notices, corrupt layout, corrupt hash | Stage11C evidence | 6 |
| `T7C-M11` | `m11_before_renderer` | central sequence | 1 |
| `T7C-M12` | `m12_fabricated`; `m12_stale` | central sequence | 2 |
| `T7C-M13` | `m13_duplicate_query` | central sequence | 1 |
| `T7C-M14` | `m14_missing_query` | central sequence | 1 |
| `T7C-M15` | missing occupied-path guard; inverted guard; OR guard; generic block before Stage12 | Stage10 evidence | 4 |
| `T7C-M16` | `m16_manual_loses_stage17` | central sequence | 1 |
| `T7C-M17` | manual reset missing; reset inverted; manual override ignored | Stage10 evidence | 3 |
| `T7C-M18` | callback before `EndDrawing` | Stage10 evidence | 1 |
| `T7C-M19` | omit ordinary-frame capture callback | Stage11D renderer integration | 1 |
| `T7C-M20` | duplicate ordinary-frame capture callback | Stage11C evidence | 1 |
| `T7C-M21` | six callback owner/success substitutions, three producer constants, and seven comment/string/raw/inactive/lambda/dead/cross-scope decoys | Stage10 evidence | 16 |
| `T7C-M22` | plain OR latch; parenthesized OR latch; split expression | Stage11B settings evidence | 3 |
| `T7C-M23` | omit selected current-frame generic success | Stage10 evidence | 1 |
| `T7C-M24` | parenthesized process exit; balanced exit-flag alias; ordinary exit-flag alias; clean-exit lambda; premature Stage11D exit; unrelated capture-driven exit; pre-capture exit; explicit clean-exit operator call; counter poison; direct exit request; loop break; cross-function decision decoy; pre-decision counter alias; capture-driven frame exit; controlled double increment; validation macro rewrite; facade completion overwrite | Stage11D loot evidence | 17 |
| `T7C-M25` | reorder summaries; move summaries post-shutdown; duplicate Stage11C summary; conditional Runtime summary; conditional Host summary; alternate Host summary invocation; alternate Runtime summary invocation; omit Stage11D summary | central sequence + Stage11C evidence + Stage11D loot evidence | 8 |
| **Total** |  |  | **101** |

This total intentionally excludes additional non-Mxx hardening cases, such as
the three pause-CJK readiness cases, four Stage11D ground semantic-gate/scope
cases, pending-capture publication/gate cases, and harmless equivalent/rename
variants. Consequently a whole self-test's headline mutation count can be
larger than its contribution to the 101 mapped Mxx pairs.

The central sequence contribution remains exactly eight cases: M11, both M12
variants, M13, M14, M16, and the two central M25 variants. Stage10's script
asserts 25 mapped cases (`M15=4, M17=3, M18=1, M21=16, M23=1`). Stage11B's
script asserts 16 Task 7C cases (`M07=10, M22=3`, plus three auxiliary
pause-CJK cases).

## Completed build and test evidence

- Serial MSVC `arpg_game` configure/build: PASS in 29.7 s.
- The complete fourteen-target Task 7C build set was later built serially:
  PASS. No parallel build/test chain was launched.
- Direct GREEN guards: central source, central sequence, host input, input
  latency, Stage10, Stage11, Stage11B architecture/evidence, Stage11C
  architecture/evidence, and Stage11D renderer/loot all passed.
- Recorded direct timings include central source 106.05 s, central sequence
  92.19 s, host input 107.03 s, input latency 70.48 s, Stage11C
  architecture/evidence 70.97/48.34 s, and Stage11D renderer/loot
  71.58/31.13 s.
- Stage10 mutation self-test: PASS in 155.94 s with the asserted 25 mapped
  Task 7C cases.
- Stage11B settings evidence self-test: PASS in 548.7 s with 16 Task 7C cases
  (`M07=10`, `M22=3`, pause-CJK=3).
- Stage11C HUD evidence self-test: PASS in 414.4 s after stale-anchor and
  executable-scope hardening.
- Stage11D renderer self-test: an earlier current-branch run passed in 521.4 s.
  Later guard hardening increased the script's declared Task 7C inventory from
  17/5 to 22 mutation/6 equivalent variants; the focused selector is the
  required proof for this final script state.
- Stage11D loot Task 7C targeted self-test: PASS, 22/22 M24/M25 cases. The
  final full #104 self-test also passed in 1290.53 s under the unchanged 1500 s
  timeout with all 88 generated fixture files reaching completion.

Timeout metadata is bounded rather than open-ended: the sequence self-test is
2400 s after its recorded 1479.25 s BASE run, Stage11C evidence is 600 s after
the Task 7C inventory measured 414.4 s, and Stage11D evidence is 1500 s after
the prior 710.84 s and current 1294.5 s measurements.

Focused 30-test selector: **PASS**, 30/30 in 5294.75 s. The later M24/M25
hardening changed only #104's two guard scripts; its final 22-case targeted run
and complete #104 rerun above are the exact current-state replacement evidence.

## Independent review

- Review A, production behavior: Critical 0, Important 0, Minor 0; approved.
  It covered death input, pause timing, real HUD/GroundLoot data, capture owner
  and override behavior, same-frame exit, cleanup, and summary order.
- Review B, guards and adversarial scope: Critical 0, Important 0, Minor 0;
  approved after the reported guard fixes. It covered legacy fixtures and
  comment/string/raw/inactive/lambda/dead/cross-file/position bypasses.
- Final snapshot re-review A: Critical 0, Important 0, Minor 0; **approved**.
- Final snapshot re-review B: Critical 0, Important 0, Minor 0; **approved**.
  Both explicitly reviewed balanced aliases, parenthesized process exits,
  conditional/alternate Host and Runtime summaries, and the bounded fast path.

## Size and legacy-symbol metrics

Physical counts include blank lines; nonblank counts exclude whitespace-only
lines.

| Surface | BASE physical | Current physical | BASE nonblank | Current nonblank | Delta physical/nonblank |
| --- | ---: | ---: | ---: | ---: | ---: |
| Six Task 7C production files | 2,317 | 2,360 | 2,210 | 2,245 | +43 / +35 |
| `run_raylib_host` | 1,039 | 898 | 1,032 | 891 | -141 / -141 |
| Public facade + runtime implementation | 277 | 512 | 238 | 462 | +235 / +224 |

The facade/runtime row is `host_validation.hpp` plus
`host_validation_runtime.cpp`. The six-file row additionally includes the
state header, Stage11D header/report, and Host.

| Legacy surface in `raylib_host.cpp` | BASE occurrences | Current occurrences |
| --- | ---: | ---: |
| `HostValidationStateAccess` | 7 | 0 |
| Six private Stage state type names, combined | 8 | 0 |
| direct `inject_stage*` calls | 0 | 0 |
| direct `observe_stage*` calls | 1 | 0 |
| direct Stage validation-summary writers | 4 | 0 |
| validation headers included | 9 | 1 (`host_validation.hpp`) |

The current Host also has no direct Stage presentation/capture/report owner;
those boundaries are enforced by the reviewed source and sequence guards.

## Resource discipline and remaining gates

- Before builds and long tests, `cmake|ctest|ninja|cl|link` processes were
  inspected to avoid a duplicate chain. All builds and CTest selectors use
  `-j1`.
- Long selectors use one hidden process with redirected stdout/stderr and a
  recorded PID. A wrapper timeout or a live child mutation fixture is monitored
  rather than relaunched.
- Final #104, #71, and #73 checks each ran as one serial chain. No duplicate
  build/test process remained after completion.
- No controller-tree file under `E:/game` was edited.

Final `git diff --check`, exact 28-file staging, cached review, and commit all
passed.

Current result: **COMPLETE at `60611de`**.
