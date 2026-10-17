# Task 9 fix round 4/5 — compile-time lifecycle ownership

## Context

- Plan: `docs/superpowers/plans/2026-07-28-raylib-host-structural-refactor.md`
- Task brief: `.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task-9-brief.md`
- Shared report: `.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task-9-report.md`
- Worktree: `E:/game/.worktrees/whole-game-refactor-2026-07`
- Branch: `codex/whole-game-refactor-2026-07`
- Fix base/HEAD at dispatch: `9efa31ec36a6225ff92756575db8e6a411c0473c`
- This is fix round 4/5. A prior implementer attempted textual CMake macro interpretation and left an uncommitted WIP. Replace that approach with the bounded design below; do not reset or checkout files.

## Global constraints

- Work only in the isolated worktree. Never modify `E:/game`.
- Do not change gameplay, golden values, timeouts, Stage interfaces, or any B-G scope.
- Do not delete files, reset Git, or overwrite unrelated dirty work.
- Builds and long tests are serial: `CMAKE_BUILD_PARALLEL_LEVEL=1`, `-j1`.
- Before rerunning a timed-out command, inspect and stop only its exact orphan PID; never leave duplicate CMake/CTest chains.
- Preserve the deliberately unstaged ledger and Task 9 baseline. Never stage line-ending-only dirt in `raylib_host.hpp` or Stage15 evidence, nor generated/PYC files.
- Use `apply_patch` for edits. Follow RED -> GREEN -> refactor. Record every command, real exit code, duration, and diagnostic in the shared report.

## Open review findings

1. The current WIP token-paste interpreter misses cross-file `.inc` aliases, one-argument forwarding, and definition/`#undef` timing; it also creates harmless-macro false positives. Do not grow it into a C preprocessor.
2. Renderer ordering can be bypassed by an early `reset(new CombatRenderer{})` plus an unreachable exact anchor, or by an included macro that performs the early construction.
3. Namespace matching must accept `namespace arpg { namespace platform { ... } }`, but the scope walker must ignore phase-2-spliced preprocessor directive bodies and reject `inline namespace`/wrong/detail scopes.
4. Wrapper forwarding can be changed by include macros. The owner needs the exact double-`#undef` seam around `<raylib.h>`, and registered production code needs a compile-time ownership policy.

## Required design

### A. Compile-time lifecycle poison

Add `src/platform/raylib/raylib_lifecycle_poison.hpp`.

- It includes `<raylib.h>` before defining poison macros.
- It defines a dedicated `Blocked` value whose call operator and address-taking are deleted, plus an `active` sentinel.
- Unless the matching allow macro is present, define these API identifiers as object-like macros mapped to the blocked value:
  - `SetConfigFlags`
  - `InitWindow`
  - `IsWindowReady`
  - `CloseWindow`
  - `WindowShouldClose`
- Exact allow macros:
  - `ARPG_ALLOW_RAYLIB_SET_CONFIG_FLAGS`
  - `ARPG_ALLOW_RAYLIB_INIT_WINDOW`
  - `ARPG_ALLOW_RAYLIB_IS_WINDOW_READY`
  - `ARPG_ALLOW_RAYLIB_CLOSE_WINDOW`
  - `ARPG_ALLOW_RAYLIB_WINDOW_SHOULD_CLOSE`
- Object-like poisoning must reject direct calls, parenthesized calls, taking the address, aliases, and identifiers produced by token-paste rescanning at compile time.

In `src/platform/raylib/CMakeLists.txt`:

- Force-include the policy header for the whole `arpg_raylib` target:
  - MSVC: `/FI<absolute-policy-header>`
  - GCC/Clang: a correct two-argument `-include <absolute-policy-header>` compile option.
- Set exact source-level `COMPILE_DEFINITIONS` exemptions:
  - `host_window_lifetime.cpp`: allow SetConfigFlags, InitWindow, IsWindowReady, CloseWindow.
  - `combat_renderer.cpp`, `death_overlay_renderer.cpp`, `hud_renderer.cpp`, `pause_menu_renderer.cpp`, `window_settings.cpp`: allow only IsWindowReady.
  - `raylib_host.cpp`: allow only WindowShouldClose.
  - Every other present or future target source receives no exemption and is poisoned for all five APIs.
- Do not use PUBLIC definitions or leak exemptions to formal/test targets.

Keep the exact double-`#undef` seam in `host_window_lifetime.cpp`: one group after project headers and before `<raylib.h>`, one group immediately after `<raylib.h>`, with no intervening local include/define before the four wrappers.

### B. Real compile probes, not source-text macro simulation

Use the existing `arpg_verify_direct_input_poison` pattern in `tests/platform/CMakeLists.txt` as the model. Add lifecycle poison compile probes using real C++ compilation and `STATIC_LIBRARY` try-compile behavior.

Required RED/GREEN cases:

- active sentinel/control compiles;
- direct call and address-taking are rejected by `Blocked`;
- `CAT(Init, Window)`, `CAT3(Init, Window,)`, object alias plus forwarding, and an include-based alias are rejected;
- harmless `PreInit ## WindowSuffix`, `InitSuffix ## Window`, and `alpha ## beta` compile;
- the exact owner allow set compiles its four calls;
- an IsWindowReady-only allow probe compiles IsWindowReady but still rejects InitWindow;
- failure probes must prove the lifecycle poison caused the failure, not an unrelated syntax/include error.

Replace/remove the Round 4 WIP's broad token-paste interpreter and fake-source expectations that duplicate the compiler. Retain only small static checks needed to prove the force-include contract, exact allowlist, forbidden non-policy `#define`/`#undef`/push_macro/pop_macro escape hatches, direct identifier ownership/counts, and the owner wrapper seam. Scan project code-like extensions `.cpp`, `.hpp`, `.h`, `.inc`, `.inl`, `.ipp` for escape directives. The policy header and exact owner seam are the only sanctioned directive roles.

### C. Renderer and namespace guards

- On the extracted direct `run_raylib_host` surface, require the owner declaration at its expected direct depth and the FixedStepRunner -> make_unique<CombatRenderer> -> renderer reference anchors at the same try-body depth and in canonical order.
- Preserve the reviewed exact identifier budgets (`CombatRenderer == 3`, `renderer_storage == 7`) if they remain valid against production.
- Reject common early construction paths before FixedStepRunner and reject project macro definitions in code-like raylib files whose replacement contains/forms `CombatRenderer`, `renderer_storage`, or `make_unique<CombatRenderer>`. Include the reviewed `.inc` `TASK9_EARLY()` mutation.
- Namespace stack comparison flattens `::` components so compact and layered namespaces are equivalent.
- Mask complete phase-2-spliced preprocessor directive lines before brace/scope walking.
- Reject inline namespace, anonymous/detail/non-namespace extra scopes when the exact named path is required.

## TDD and verification

First verify the missing force-include/CMake contract and compile-probe cases fail for the intended reason before production/CMake policy implementation. Preserve already recorded RED evidence for the seven earlier mutations, but new poison behavior needs fresh RED evidence.

Run serially, with credible timeouts and PID cleanup:

1. lifecycle poison configure/compile probes and new selective mutations;
2. renderer selective sequence mutation and layered/wrong namespace selective tests;
3. `git diff --check` and a scope audit;
4. under a loaded MSVC developer environment, build `arpg_game` and `arpg_platform_tests` with `-j1`;
5. run `arpg_platform_tests.exe` (all cases);
6. full `host_validation_source_test.cmake`;
7. full `host_input_source_test.cmake`;
8. full `input_latency_source_test.cmake`;
9. full Stage11C architecture self-test if its shared namespace code changed;
10. full host validation sequence self-test once, because its renderer guard changed (budget at least 1800 seconds).

Do not run Debug/Release full CTest in this fix round. Task 9's known non-A full-gate blockers remain separate.

## Commit and report contract

- Append a Round 4 section to `task-9-report.md` with RED/GREEN commands, exit codes, timings, production effect, and remaining concerns.
- Self-review the full uncommitted diff against `9efa31e`.
- Stage only files owned by this fix. Never stage progress.md, baseline, Stage15 evidence, `raylib_host.hpp` line-ending dirt, or generated files.
- Commit only after all required covering tests pass. Suggested message: `test: enforce raylib lifecycle ownership at compile time`.
- Return: `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`; commit hash; one-line test summary; concerns.
