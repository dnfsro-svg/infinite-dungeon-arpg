# Task 9: 子项目 A 全量门禁、独立审查和真实窗口验收

## 状态

- **BLOCKED**
- Worktree: `E:/game/.worktrees/whole-game-refactor-2026-07`
- Validated revision: `ba1c93f4fab37108d2da079cb14ab1dc748aa741`
- Scope: only subproject A; no B-G implementation or file was introduced.
- No production source, expected image/hash, CTest timeout, or test assertion
  was changed.  No commit or push was made because the completion gate is red.

## Resource preflight and build

All builds/tests used `CMAKE_BUILD_PARALLEL_LEVEL=1` and `-j1`; no overlapping
build or CTest chain was started.

| Command/result | Exit | Time/evidence |
| --- | ---: | --- |
| Initial Debug build from a plain shell | 2 | 0.575 s; MSVC could not find `cstddef` (`C1083`). |
| Load `Launch-VsDevShell.ps1`, then `cmake --build --preset windows-msvc-debug -- -j1` | 0 | 146.837 s, 134 actions. |
| `ctest --preset windows-msvc-debug -j1`, then serial continuation `-I 9,124` after preset stop-on-failure | 8 | Combined 7,550.84 s. |
| `cmake --preset windows-msvc-release` | 0 | 8.394 s. |
| `cmake --build --preset windows-msvc-release -- -j1` | 0 | 588.247 s, 420 actions. |
| `ctest --preset windows-msvc-release -j1`, then serial continuation `-I 6,124` after preset stop-on-failure | 8 | Combined 7,260.12 s. |

The initial Debug failure was an environment error only; the successful build
used the same source revision.  After all test work, process readback found no
`cl`, `link`, `ninja`, `cmake`, `ctest`, or `arpg_*` residue.  The Release
CTest `LastTest.log` was subsequently replaced by a `ctest --show-only=json-v1`
inventory query; counts, names, durations, console output, and
`LastTestsFailed.log` had already been captured.  This report does not present
the overwritten log as retained evidence.

## Full CTest results

| Configuration | Total | Pass | Executed fail | Timeout | Access violation | Fixture not run | Non-pass | Wall time |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Debug | 124 | 99 | 19 | 2 | 2 | 6 | 25 | 7,550.84 s |
| Release | 124 | 98 | 20 | 2 | 2 | 6 | 26 | 7,260.12 s |

Debug executed failures were:

1. `stage16.loot_reinforcement.simulation` (60 s timeout)
2. `stage11.death_stress.determinism_zero_alloc` (`codec-17`)
3. `stage9.validation_fixture.real_v4_reward_reload` (access violation)
4. `stage10.validation_fixture.real_abyss_transactions` (access violation)
5. `stage9.evidence.no_injected_defeat_trace`
6. `stage10.validation_game.capture_after_present`
7. `stage10.formal_game.capture_after_present`
8. `stage11.death_fixture.transactions`
9. `stage11.death_formal.five_paths` (300 s timeout)
10. `persistence.save_commit_stack_guard`
11. `stage16.loot_reinforcement.real_raylib`
12. `stage12.material_formal`
13. `stage11b.settings_formal`
14. `stage11c.hud_formal`
15. `stage11d.loot_formal`
16. `stage11c.architecture.hud_boundaries_self_test`
17. `architecture.persistence_checkpoint_only`
18. `platform.input_latency_source`
19. `platform.host_input_source`

The six dependent not-run tests were Stage12 material validator/self-test,
Stage11B settings validator, Stage11C validator/self-test, and Stage11D loot
validator.  Release has the same set plus `dungeon.units`, whose single failed
case reports that neither event nor combat-relay overflow count was positive.

The 60-second Stage16 simulation executable continued running for more than
ten minutes when invoked directly and produced no assertion output.  Its test
source, principal dungeon dependency object hashes, and 60-second CMake
property are identical to the plan base, so this is recorded as a legacy/time-
budget concern, not silently fixed by raising the timeout.  The current gate
nevertheless remains red.

`architecture.persistence_checkpoint_only` is the exact forbidden
`room_progress_codec.cpp -> dungeon/health_potion_loot.hpp` include already
captured at the Task 1 review base.  `stage11b.settings_formal` is the formal
pause/resume failure already captured by Tasks 3/7A.  Task 7B also records the
Stage10/11 and Stage11D formal groups at its own base.  Those attributions are
kept separate from new/unattributed failures; they do not satisfy the full
CTest completion gate.

## Formal evidence comparison

The fresh Debug run passed `platform.host_validation_source` (217 s),
`platform.host_validation_sequence` (89 s), and the Stage11B/11C/11D evidence
source guards (16/27/20 s).  This proves the current source still binds the
facade, input/snapshot ordering, post-present capture and ordered summary
writers.  Artifact-level comparison is not green:

| Stage | Expected contract | Fresh result |
| --- | --- | --- |
| 10 | Seven named 1280x720 formal captures and complete state evidence | Formal directory empty; formal capture failed. |
| 11 | Five named 1280x720 death/continue captures plus path summary | Only `01-normal-death.png` exists (1280x720); formal timed out. |
| 11B | `pause/settings/rebound/swap/restart/single-slot/corrupt` evidence and passing aggregate | Five PNGs are valid 1280x720, but `settings.png` and `swap.png` are absent; aggregate ends `result=fail`; validator not run. |
| 11C | Six named 1280x720 PNGs and valid ordered per-scene hashes | Only `combat`, `low-health`, `debug` exist; three scene hashes/valid bits are zero; aggregate ends `result=fail`; validators not run. |
| 11D | Six named 1280x720 PNGs, summaries and manifest | Evidence directory empty; formal failed; validator not run. |
| 17 | Production real-window evidence, state fields and validator | Four Stage17 tests passed. Seven named 1280x720 PNGs and V3 ordered state fields were accepted in Debug and Release. |

Stage17's checked-in validation document still lists an older five-file,
23-field contract while the current passing validator emits seven files and a
larger V3 state.  That is documentation drift to reconcile separately; no
golden/hash expectation was updated in Task 9.

## Real raylib window matrix

Launch command used the Release executable, seed `0x8`, and isolated directory
`out/task9-real-window-save-20260730-1355` for save, settings, and screenshots.
Exactly one game window existed.  Windows Graphics Capture returned a white
OpenGL client surface; visual decisions therefore use the game's own F12
post-present PNG (1280x720), not the white control screenshot.

| Check | Result | Evidence |
| --- | --- | --- |
| Movement | INCONCLUSIVE | `D` injected; camera lock and dense monster pile prevented independent position proof. |
| Light attack | INCONCLUSIVE | `J` injected and a hit/action flash was visible, but actor overlap prevents attribution. |
| Jump | INCONCLUSIVE | `K` injected; player died before a distinct jump frame was captured. |
| Launch + 0.5 s float | INCONCLUSIVE | Strict `L -> refresh -> 500 ms -> F12`; hit flash visible, actor height not distinguishable. |
| Active skills 1/2 | INCONCLUSIVE | `Numpad1` and `Numpad2` injected; death/overlap prevented independently attributable skill frames. |
| Inventory | PASS | Complete Equipment/Inventory/Item Detail/material-bag screen visible; current inventory was empty. |
| Drop pickup | INCONCLUSIVE | No item reached the inventory and no pickup receipt was captured. |
| Pause/settings | PASS | Pause, Settings, saved values, all bindings, Apply/Cancel, and return path visible. |
| Doors | PARTIAL | Lightning/Chaos/Water door artwork visible; interaction was blocked by the uncleared 400-monster room. |
| Next-floor hole | INCONCLUSIVE | No cleared-room interaction or floor transition was reached. |
| Death continue | PASS | Chinese death recap visible; `E` returned to active combat. |
| HUD/font | PASS | Chinese HUD, English control hints, skills, room/element data and overlays rendered clearly. |
| Restart persistence | PASS | After a menu-confirmed exit, the same isolated directory restarted directly into the prior death recap; `run_a.sav`/`run_b.sav` were present. |
| Clean exit | PASS | Both launches exited via the in-game confirmation page; original/restart PIDs and all `arpg_game` processes read back as absent. |

While the Pause menu was visibly active, a two-second process sample consumed
3.047 CPU seconds (~153.9% of one logical core), with 671.29 MiB working set,
611.23 MiB private bytes, 601 handles, 23 threads, and `Responding=True`.
This fails the requested no-idle-high-usage observation.

## Metrics

- Tracked raylib production surface: 25,136 -> 26,033 physical lines (+897).
- `raylib_host.cpp`: 4,135 -> 1,240 lines (-2,895).
- Largest current file: `raylib_host.cpp` 1,240 lines; next is
  `inventory_renderer.cpp` 1,222 lines.
- Added-file line counts are recorded in
  `docs/validation/raylib-host-refactor-baseline.md`.
- Current Host has no private Stage validation-state type and no direct
  `inject_stage*`, `observe_stage*`, or Stage summary writer; the only
  validation facade is `HostValidationRuntime`.

## Review and completion decision

Two independent read-only reviews completed.  After deduplication they report
P0=0, P1=3, P2=2:

- P1: `platform.input_latency_source` and `platform.host_input_source` still
  require `SetConfigFlags`/`InitWindow` in `raylib_host.cpp` after Task 8C moved
  the transaction to `host_window_lifetime.cpp`.  The guards need a cross-file
  contract and mutation coverage; weakening their expected text is not enough.
- P1: the `ba1c93f` shared evidence scanner diagnostic change makes
  `stage11c.architecture.hud_boundaries_self_test` fail for the wrong reason.
  It needs a nonfatal bounds probe followed by the owner-specific diagnostic.
- P1: both full CTest configurations and the formal evidence chains are red;
  recorded BASE attribution is not an exception to Task 9's all-green gate.
- P2: `HostWindowBackend` permits null or throwing callbacks while the lifetime
  methods are `noexcept` and directly invoke them; this previously deferred
  precondition remains open.
- P2: `make_unique` can throw after renderer/material resources initialize,
  while explicit renderer shutdown exists only on the success tail and the
  relevant wrappers do not provide destructor cleanup before the window
  context closes.

The scope reviewer found no B-G or square-room pollution: production changes
remain under `src/platform/raylib`, and every new production `.cpp` is listed
by that directory's CMake target.  Scope cleanliness does not offset the
findings above.

Task 9 cannot satisfy the completion gate because both full suites are red,
formal evidence is incomplete for five stages, several real-window interactions
remain inconclusive, and paused CPU is high.  Therefore documentation is left
as an unstaged working-tree change and there is deliberately no commit, push,
or remote-hash claim.

## Fix round 4/5: compile-time lifecycle ownership

### RED evidence

All commands ran in `E:/game/.worktrees/whole-game-refactor-2026-07` with no
overlapping build, CMake, CTest, or `arpg_*` chain.

| Command/probe | Exit | Time | Intended diagnostic |
| --- | ---: | ---: | --- |
| `cmake -DSOURCE_ROOT=... -P tests/platform/host_validation_source_test.cmake` after adding the CMake contract test | 1 | 0.129 s | Lifecycle force-include contract expected 1, found 0. |
| MSVC DevShell `cmake --preset windows-msvc-debug` after adding real lifecycle compile probes | 1 | 4.084 s | Active sentinel could not include the missing `raylib_lifecycle_poison.hpp`; the existing direct-input controls still behaved normally. |
| First post-policy source guard | 1 | 161.284 s | The superseded CMake token-paste interpreter falsely classified the policy header itself as a lifecycle owner violation. |
| `inline_owner_namespace` selective mutation | 1 | 87.874 s | The old namespace walker accepted `inline namespace platform`. |
| `phase2_directive_scope_decoy` selective mutation | 1 | 87.873 s | Braces in a phase-2-spliced macro body were treated as real scopes. |
| `.inc` `TASK9_EARLY()` renderer macro selective mutation | 1 | 77.483 s | The sequence guard accepted the included early-renderer macro. |

One intermediate GREEN attempt is recorded rather than hidden: the first
custom `try_compile` invocation linked an executable and failed on missing
`main` (exit 1, 2.444 s).  Setting `CMAKE_TRY_COMPILE_TARGET_TYPE` in the
calling scope produced the required static-library probes.  A first directive
masking attempt also ran after token whitespace had already removed logical
line boundaries; `phase2_directive_scope_decoy` remained red (exit 1,
88.936 s).  Moving masking before normalization fixed the root cause.

### Production and guard effect

- Added `raylib_lifecycle_poison.hpp`.  It includes `<raylib.h>` first and then
  object-like-poisons the five lifecycle APIs with a dedicated `Blocked`
  value whose call, copy/alias, and address-taking paths are deleted.
- `arpg_raylib` privately force-includes the absolute policy header.  Exact
  per-source `COMPILE_DEFINITIONS` allow only the four owner calls, the five
  reviewed `IsWindowReady` consumers, and the Host's `WindowShouldClose` call.
  No definition or forced include reaches test/formal targets.
- The owner retains two exact four-identifier `#undef` groups around
  `<raylib.h>`, with no local include or define between the second group and
  the four wrappers.
- Removed the WIP CMake alias/token-paste interpreter and its fake compiler
  expectations.  Real MSVC static-library probes now cover direct and
  parenthesized calls, address-taking, CAT/CAT3 rescanning, object forwarding,
  an include alias, harmless pastes, the owner allow set, and the ready-only
  allow/reject split.  Negative probes first compile a no-poison control and
  require a `Blocked` diagnostic.
- Static guards retain only force-include/exact allowlist checks, direct
  identifier budgets, exact lifecycle directive roles, the owner seam, and a
  bounded renderer-macro replacement check.  Code-like `.cpp/.hpp/.h/.inc/.inl/.ipp`
  files are scanned; non-policy lifecycle `#define`/`#undef`/push/pop escapes
  and direct or token-pasted renderer ownership macros are rejected.
- Renderer ownership now requires the owner, FixedStepRunner, allocation, and
  renderer reference at their reviewed direct depths in canonical order, with
  the reviewed `CombatRenderer == 3` and `renderer_storage == 7` budgets.
  Layered namespaces are flattened, inline/wrong/detail/anonymous extra scopes
  are rejected, and complete logical preprocessor directive lines are masked
  before scope walking.
- The shared Stage11C scope consumer now masks directives before whitespace
  normalization.  This resolves the previously recorded wrong-reason P1 while
  preserving owner-specific diagnostics.

### GREEN verification

| Command/check | Exit | Time/result |
| --- | ---: | --- |
| MSVC lifecycle configure/compile probes | 0 | 6.461 s; final repeat 6.567 s. |
| Compile-command scope audit | 0 | 73/73 `arpg_raylib` sources force-included; 7 exact exempt sources; 0 non-target leaks. |
| Namespace selectives | 0 | inline 87.974 s; phase-2 directive 88.408 s; layered 87.758 s; wrong 89.772 s; detail 88.556 s. |
| Lifecycle directive and renderer macro selectives | 0 | escape roles 89.824 s; direct `.inc` macro 0.106 s; token-paste `.inc` macro 0.097 s. |
| Renderer early-reset/dead-anchor sequence selective | 0 | 29.330 s; rejected for the intended renderer boundary diagnostic. |
| MSVC `cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1` | 0 | 123.840 s; 76/76 actions. |
| `arpg_platform_tests.exe` | 0 | 13.922 s; 514 cases, 0 failures.  Existing asset fallback warnings remained nonfatal. |
| Final full `host_validation_source_test.cmake` | 0 | 235.597 s. |
| Final full `host_input_source_test.cmake` | 0 | 120.652 s. |
| Final full `input_latency_source_test.cmake` | 0 | 285.840 s. |
| Full Stage11C architecture self-test | 0 | 346.049 s; rejected 20 mutations and accepted 5 harmless variants. |
| Full host validation sequence self-test | 0 | 1,444.345 s; within the required 1,800 s budget. |
| `git diff --check` before final reporting | 0 | No whitespace error. |

### Round 4 review and remaining concerns

The uncommitted diff was reviewed against `9efa31e`.  Production impact is
limited to the raylib lifecycle policy, target-local compile policy, and the
previously reviewed owner seam; all other changes are source/architecture
guards and compile probes.  No gameplay, golden value, timeout, Stage
interface, or B-G source changed.

Round 4's lifecycle/renderer/namespace findings are covered and green.  The
original Task 9 full Debug/Release CTest and real-window/formal-evidence
blockers remain separate; this round deliberately did not rerun or weaken
those gates, as required by the round brief.  The two previously deferred P2
items concerning throwing/null backend callbacks and renderer cleanup on
allocation failure are also unchanged and outside this compile-policy fix.
