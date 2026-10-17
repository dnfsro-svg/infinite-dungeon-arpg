# Task 1 report: launcher installation core

## RED

Created the declared API, empty translation unit, and seven real filesystem-based
tests. `./scripts/Build.ps1 -Preset windows-msvc-debug` reached the launcher
link step and failed as intended with `LNK2019`/`LNK1120`: the declared
`InstallationStatus::ready`, `inspect_installation`, `make_launch_request`,
`default_save_directory`, and `installation_message` functions had no
definitions. The same whole-project build also reported an unrelated existing
link failure for `arpg_stage11b_settings_formal`.

The required focused CTest command then reported `launcher.units` as Not Run
because the expected executable was absent after that intentional link failure.

## GREEN and refactor

Implemented the smallest filesystem-only installation model. It checks the
launcher parent directory, requires `arpg_game.exe` as a regular file, scans the
six specified asset directories in order, builds a quoted argv[0]-only request,
and derives the save path without creating it. The asset message uses a generic
relative path so its contract remains `assets/stage12` on Windows.

Added the standalone static library/test targets, project warnings, `/utf-8`,
and the requested static MSVC runtime to both launcher targets. Compile command
inspection confirmed both launcher compilation commands use `/MTd`, `/W4`,
`/permissive-`, and `/utf-8` in Debug.

## Verification

- `cmake --build --preset windows-msvc-debug --target arpg_launcher_tests`:
  passed (after entering the repository's MSVC configure environment).
- `ctest --test-dir out/build/windows-msvc-debug -R '^launcher\\.units$' --output-on-failure`:
  passed, 1/1 test; launcher binary reports `7 cases, 0 failures`.
- `./scripts/Test.ps1 -Preset windows-msvc-core-debug`: one retained run logged
  core, abyss, skills, combat, dungeon, the first Stage 16 suites, and Stage 11C
  passing. `dungeon.units` completed 287 cases with 0 failures in 150.69
  seconds. The run then remained in the Stage 11D self-test beyond the focused
  verification window and was stopped to avoid consuming the shared runner; its
  remaining 39 tests were not completed. A later accidental duplicate invocation
  was stopped first; no third invocation was started.
- `git diff --check`: passed.
