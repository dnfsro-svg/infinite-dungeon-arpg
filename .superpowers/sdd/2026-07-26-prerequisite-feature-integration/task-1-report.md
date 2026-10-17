# Task 1 report — desktop launcher merge without material regressions

## Status

Completed. Commit: `373f85961d5eb0c612d4b12157c16baffe3b8f45`
(`merge: integrate native desktop launcher`).  This is a two-parent merge of
`2612c2e` and `205f708` (`codex/desktop-game-launcher`).  No Stage 19 branch
was merged.

## Implementation

- Performed `git merge --no-ff --no-commit codex/desktop-game-launcher`.
- The only unmerged path was the expected
  `tests/platform/platform_test_main.cpp`.
- Resolved its aggregate suite count to exactly:

  ```cpp
  return arpg::test::run_suites(suites, 477,
      "HUD and UI material slice contract");
  ```

  This preserves 443 common-base cases, 30 current-main cases, and 4 launcher
  cases.
- The automatic source merge adds the native `arpg_launcher`,
  `arpg_launcher_tests`, launcher deployment/icon/test support, and host launch
  option/death-resume plumbing.
- No new production fix was written; this was a merge plus the prescribed test
  aggregate conflict resolution.

## Commands and results

| Command | Result |
| --- | --- |
| `git status --short --branch --untracked-files=all` | Clean `codex/integration-large-square-rooms` baseline. |
| `git rev-parse HEAD` (before merge) | `2612c2eb7dcee438d9b99261ff8056efcc90eb94`. |
| `git diff --name-status d710e01..HEAD` | Exactly the three approved `docs/superpowers/plans/2026-07-26-*` files. |
| `ctest --preset windows-msvc-core-debug -R '^core\.units$' --output-on-failure` | Passed, 1/1. |
| `git merge --no-ff --no-commit codex/desktop-game-launcher` | Expected content conflict only in `tests/platform/platform_test_main.cpp`. |
| `git diff --name-only --diff-filter=U` | Exactly `tests/platform/platform_test_main.cpp`. |
| `rg -n '^(<<<<<<<|=======|>>>>>>>)' src tests CMakeLists.txt` | No matches after resolution. |
| `git diff HEAD -- src/platform/raylib/raylib_host.cpp` | Only launcher option/death-resume plumbing (plus formatting). |
| `git diff HEAD -- src/platform/raylib/combat_renderer.cpp src/platform/raylib/room_renderer.cpp` | Empty. |
| `git diff HEAD -- assets/skills assets/stage12` | Empty. |
| `. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh`; `cmake --build --preset windows-msvc-debug --target arpg_launcher_tests -- -j1`; launcher/process-guard `ctest` regex | Passed, 7/7. |
| `. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh`; `cmake --build --preset windows-msvc-debug --target arpg_launcher arpg_platform_tests -- -j1`; `ctest` for `platform.units` and `launcher.window_smoke` | Passed, 2/2. |
| `out/build/windows-msvc-debug/bin/arpg_platform_tests.exe` | `477 cases, 0 failures`. |
| `git diff --check --cached` | Passed. |
| `git commit -m 'merge: integrate native desktop launcher'` | Created `373f859`. |

Every build command used `CMAKE_BUILD_PARALLEL_LEVEL=1` and Ninja `-j1`; no
second build was started while another build was live.

## TDD evidence

N/A. No production fix or new behavior was authored in this task. The only
manual change was the specified three-way conflict aggregate (`477`), with the
existing merged launcher and platform test suites serving as verification.

## Changed files

The merge changes these tracked paths:

```text
.superpowers/sdd/2026-07-25-desktop-game-launcher/task-5-render-fix-report.md
CMakeLists.txt
assets/launcher/infinite_dungeon.ico
docs/superpowers/plans/2026-07-25-desktop-game-launcher.md
docs/superpowers/specs/2026-07-25-desktop-game-launcher-design.md
scripts/DeployLauncher.ps1
src/app/main.cpp
src/launcher/CMakeLists.txt
src/launcher/arpg_launcher.manifest
src/launcher/arpg_launcher.rc
src/launcher/launcher_core.cpp
src/launcher/launcher_core.hpp
src/launcher/launcher_layout.cpp
src/launcher/launcher_layout.hpp
src/launcher/launcher_main.cpp
src/launcher/launcher_platform_win32.cpp
src/launcher/launcher_platform_win32.hpp
src/launcher/launcher_renderer.cpp
src/launcher/launcher_renderer.hpp
src/platform/raylib/dungeon_runtime.cpp
src/platform/raylib/dungeon_runtime.hpp
src/platform/raylib/raylib_host.cpp
src/platform/raylib/raylib_host.hpp
tests/launcher/CMakeLists.txt
tests/launcher/deploy_launcher_self_test.ps1
tests/launcher/launcher_core_tests.cpp
tests/launcher/launcher_icon_tests.py
tests/launcher/launcher_layout_tests.cpp
tests/launcher/launcher_manifest_test.cmake
tests/launcher/launcher_platform_win32_tests.cpp
tests/launcher/launcher_test_main.cpp
tests/launcher/launcher_window_smoke_test.ps1
tests/platform/CMakeLists.txt
tests/platform/audio_process_guard_common.cmake
tests/platform/audio_process_guard_common_self_test.cmake
tests/platform/dungeon_runtime_tests.cpp
tests/platform/host_launch_options_tests.cpp
tests/platform/platform_test_main.cpp
tests/platform/stage14_audio_integration_guard.cmake
tests/platform/stage15_audio_integration_guard.cmake
tools/build_launcher_icon.py
```

## Self-review

- Verified the expected conflict set was exactly one file before resolving it.
- Verified `CombatRenderer::synchronize_residency(...)`, active-skill draw
  runtime validation, current material assets, native high-resolution room
  backgrounds, pure-color Chinese font code, and Stage 12/17 evidence paths
  were not removed or changed by the merge: renderer and `assets/skills` /
  `assets/stage12` diffs are empty; the rebuilt `platform.units` suite reports
  477 passing cases.
- The 7 launcher/process-guard tests include the narrow Win32 process-call
  exception validation. The launcher window smoke test passed with its bounded
  cleanup. Launcher unit tests only exercise save-path construction; no test
  writes the production `%LOCALAPPDATA%/InfiniteDungeon/save` location.
- No conflict markers remain and the staged merge passed whitespace checking.

## Concerns / scope note

The source branch tracks three historical supporting records not listed in the
Task 1 file summary: `docs/superpowers/plans/2026-07-25-desktop-game-launcher.md`,
`docs/superpowers/specs/2026-07-25-desktop-game-launcher-design.md`, and
`.superpowers/sdd/2026-07-25-desktop-game-launcher/task-5-render-fix-report.md`.
They originated in the launcher branch history (the first two in `c652721` and
`9450339`; the report begins in `fed1d3b` and is updated by later launcher
commits). They were retained intentionally as tracked merge history, not
silently excluded or manually edited. This is a scope-review point for any
later history-cleanup task; no deletion was authorized here.

One initial build invocation in a fresh tool-shell failed before compilation
because that new PowerShell process lacked MSVC `INCLUDE` and therefore could
not find `stdarg.h`. Re-running the prescribed Configure and build commands in
the same PowerShell process initialized `INCLUDE`, completed the full build,
and all required tests passed. This was an execution-environment boundary, not
a source change.
