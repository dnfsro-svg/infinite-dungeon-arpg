# Task 9 fix round 5/5 — close remaining lifecycle and renderer guard gaps

## Context

- Plan: `docs/superpowers/plans/2026-07-28-raylib-host-structural-refactor.md`
- Task brief: `.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task-9-brief.md`
- Shared report: `.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task-9-report.md`
- Round 4 brief: `.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task-9-round4-brief.md`
- Round 4 review package: `.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/review-9efa31e..23f94e8.diff`
- Worktree: `E:/game/.worktrees/whole-game-refactor-2026-07`
- Branch: `codex/whole-game-refactor-2026-07`
- Fix base/HEAD at dispatch: `23f94e8a8c4184ce105addf24f3ae417edb1606a`
- This is the final allowed fix round (5/5). Make a bounded, fail-closed repair. Do not broaden into a C/C++ preprocessor interpreter.

## Global constraints

- Work only in the isolated worktree. Never modify `E:/game`.
- Do not change gameplay, golden values, timeouts, Stage interfaces, or any B-G scope.
- Do not delete files, reset Git, or overwrite unrelated dirty work.
- Builds and long tests are serial: `CMAKE_BUILD_PARALLEL_LEVEL=1`, `-j1`.
- Before rerunning a timed-out command, inspect and stop only its exact orphan PID; never leave duplicate CMake/CTest chains.
- Preserve the deliberately unstaged ledger and Task 9 baseline. Never stage line-ending-only dirt in `raylib_host.hpp` or Stage15 evidence, nor generated/PYC files.
- Use `apply_patch` for edits. Follow RED -> GREEN -> refactor. Record every command, real exit code, duration, and diagnostic in the shared report.
- Do not rerun full Debug/Release CTest or real-window validation in this fix round; their known non-A blockers remain separate.

## Independently confirmed Important findings

### 1. Exact CMake exemption allowlist is not fail-closed

`tests/platform/host_validation_source_test.cmake` currently proves only that the canonical block occurs once. An extra `ARPG_ALLOW_RAYLIB_*` source definition can be appended elsewhere while the canonical block remains intact.

Required repair:

- Keep the canonical block check.
- Additionally prove the entire `src/platform/raylib/CMakeLists.txt` contains exactly the approved exemption identifiers and counts, with no unknown `ARPG_ALLOW_RAYLIB_*` token and no extra occurrence:
  - `SET_CONFIG_FLAGS`: 1
  - `INIT_WINDOW`: 1
  - `IS_WINDOW_READY`: 2
  - `CLOSE_WINDOW`: 1
  - `WINDOW_SHOULD_CLOSE`: 1
- Add a RED/GREEN mutation that appends an exemption for an unrelated source and is rejected for an allowlist-specific reason.

### 2. Lifecycle escape scanning is too narrow

The three source gates glob only `src/platform/raylib`. A header under another `src` subtree can run after `/FI`, `#undef` a lifecycle poison, and be included by a raylib source.

Required repair:

- For lifecycle ownership/escape checks, scan all project production code-like files under `SOURCE_ROOT/src` with extensions `.cpp`, `.hpp`, `.h`, `.inc`, `.inl`, `.ipp`, not only the raylib directory.
- Preserve exact sanctioned roles: the policy header and owner seam are the only lifecycle directive exceptions; registered API owner/use roles remain exact; all other project sources have role `none`.
- Do not scan build/output/vendor/test trees.
- Add a RED/GREEN fixture representing `src/core/task9_escape.hpp` with `#undef InitWindow`, and prove the real project-file selection plus normal sanitized production path rejects it for the intended lifecycle-ownership reason.
- Keep the three gates consistent (`host_validation_source`, `host_input_source`, `input_latency_source`) without duplicating a divergent scanner.

### 3. Parameterized renderer token-paste macro bypass

The current macro check collapses literal `##` but cannot infer arguments. This bypass remains possible before `FixedStepRunner` without changing the reviewed identifier budgets:

```cpp
#define TASK9_EARLY(a,b,c,d) a ## b.reset(new c ## d{})
TASK9_EARLY(renderer, _storage, Combat, Renderer);
```

Required repair:

- Fail closed on token-paste operators (`##` or `%:%:`) in project macro definitions on the raylib code-like surface. Current production has no such macro, so do not implement argument expansion or a general preprocessor.
- Retain the existing explicit forbidden renderer-name/replacement checks for ordinary macros.
- Add include-based RED/GREEN mutations for both `##` and `%:%:` parameterized forms and require the renderer-macro-specific rejection reason.
- Do not weaken direct-depth/order/identifier-budget guards.

### 4. `push_macro`/`pop_macro` self-test bypasses the production sanitizer

The selective test feeds raw text, but production feeds `arpg_sanitize_cpp_source`; quoted lifecycle names are blanked before the current regex, making the test falsely green.

Required repair:

- Detect `#pragma push_macro(...)` and `#pragma pop_macro(...)` on the same sanitized/phase-2-normalized path used by production.
- A bounded fail-closed rule may reject every non-policy `push_macro`/`pop_macro` directive under production `src`; no current production use exists. This avoids relying on the erased quoted argument.
- Update the selective tests so they call the real sanitizer/normalization path before the ownership validator.
- Add both push and pop RED/GREEN cases and prove intended diagnostics.
- Preserve harmless comment/string/preprocessor-brace behavior already covered by existing tests.

## TDD and verification

1. Before implementation, add/run four selective mutations and record RED with intended reasons:
   - extra CMake allow exemption;
   - cross-directory lifecycle `#undef` fixture through the production selection/sanitizer path;
   - parameterized renderer macro include (`##`, then `%:%:`);
   - sanitized `push_macro` and `pop_macro` fixtures.
2. Implement only the bounded repairs above, then rerun all selective mutations GREEN.
3. Run `git diff --check` and a scope audit against `23f94e8`.
4. Reconfigure the existing MSVC Debug preset once so lifecycle compile probes still pass; do not rebuild unchanged game binaries unless configuration proves it necessary.
5. Run serially, with fresh evidence after the final edit:
   - full `host_validation_source_test.cmake`;
   - full `host_input_source_test.cmake`;
   - full `input_latency_source_test.cmake`;
   - full `host_validation_sequence_guard_self_test.cmake` once because the shared renderer guard changes (timeout at least 1800 seconds).
6. If any final edit touches shared namespace/Stage11C helpers, also run the full Stage11C architecture self-test; otherwise do not rerun it.
7. No duplicate heavy command and no inferred success: record real exit code and elapsed time.

## Commit and report contract

- Append a Round 5 section to `task-9-report.md` with review findings, RED/GREEN commands, exit codes, timings, production effect, and remaining concerns.
- Self-review the full uncommitted diff against `23f94e8`.
- Stage only files owned by this fix. Never stage `progress.md`, baseline, Stage15 evidence, `raylib_host.hpp` line-ending dirt, or generated/PYC files.
- Commit only after all required covering tests pass. Suggested message: `test: close raylib lifecycle guard escapes`.
- Return: `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`; commit hash; one-line test summary; concerns.
