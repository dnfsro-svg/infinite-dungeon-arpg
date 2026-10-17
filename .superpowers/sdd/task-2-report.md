# Stage 11-D Task 2 Report — Pause Settings Draft Semantics

## Status

DONE.

## Implementation

- Settings now contain 17 rows. `Loot Filter` is row 3, bindings occupy rows 4–13, and Reset/Apply/Cancel occupy rows 14/15/16.
- Row 3 cycles the draft filter with `PauseCommand::preview`: left is backward (`show_all` to `rare_only`); right and Enter/activate are forward.
- Reset restores the draft filter to `show_all`; Cancel rolls the draft back to the committed filter; Apply retains the existing draft/apply boundary and never publishes settings from state logic.
- Settings view renders both draft and saved labels: `Loot Filter: <draft> (saved <committed>)`.
- `kPauseMenuRowCapacity` is 17. To keep all 17 selectable rows above the fixed footer, row stride is 25 px (22 px rows, 3 px gaps).
- Required capacity correction: the 17-row view with an optional message needs 22 render operations (dim, panel, title, 17 rows, message, footer), so `kPauseMenuRenderOpCapacity` is 22 and is locked by a test.
- `platform_test_main.cpp` expected count is 242 because this task adds the loot-filter state case.

## TDD RED/GREEN Evidence

1. RED tests were written before production edits: 17-row wrap, row-3 filter cycling, shifted bindings/actions, reset/cancel semantics, 17-row view/layout, draft/saved labels, and 22-operation render-plan safety.
2. The initial sandbox build could not write the pre-existing Ninja/PDB files; rerunning from the VS 2022 x64 developer environment compiled the test executable.
3. RED execution:
   ```powershell
   $env:ARPG_TEST_FILTER='platform.pause'; .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
   ```
   Result: 242 cases, 17 failures. Failures were the expected old 16-row navigation, missing row-3 preview, old binding/action mappings, old layout capacity, missing text, and 21-operation capacity.
4. GREEN after the minimal implementation and test-baseline updates: the same command reports `242 cases, 0 failures`.

## Test Commands and Results

```powershell
cmd.exe /d /s /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build out/build/windows-msvc-debug --target arpg_platform_tests -- -j1'
$env:ARPG_TEST_FILTER='platform.pause'; .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
git diff --check
```

- Build: passed.
- Focused platform pause run: 242 cases, 0 failures; this executable includes pause state/view/render-plan, host-gate, and zero-allocation coverage.
- `git diff --check`: passed. Git emitted only pre-existing LF-to-CRLF normalization warnings.

## Modified Files

- `src/platform/raylib/pause_menu_state.cpp`
- `src/platform/raylib/pause_menu_view.hpp`
- `src/platform/raylib/pause_menu_view.cpp`
- `src/platform/raylib/pause_menu_renderer.hpp` (required render-plan capacity correction)
- `tests/platform/pause_menu_state_tests.cpp`
- `tests/platform/pause_menu_view_tests.cpp`
- `tests/platform/platform_test_main.cpp` (required test-count baseline update)

## Self-review and Concerns

- Verified all row mappings match the brief, draft-only state mutation is preserved, row 16 is laid out above the footer, and the optional-message plan exactly fills but does not exceed capacity 22.
- No dungeon integration, host auto-pickup, or ground rendering was changed.
- No remaining functional concern. The build must be invoked through the installed VS 2022 developer environment because the base shell lacks MSVC standard-library include paths.

## Commit

`570b7f6 feat: add loot filter setting control`
