# Stage 11-C Task 1 Report

Status: DONE

## RED evidence

Command:

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_platform_tests'
```

Expected failure excerpt:

```text
tests\platform\hud_view_model_tests.cpp(3): fatal error C1083: 无法打开包括文件: “hud_view_model.hpp”: No such file or directory
```

The initial non-elevated build could not write the build lock and did not have
the SDK environment. The command above was rerun in the installed MSVC Build
Tools x64 environment and produced the required API-missing RED failure.

## Changed files and behavior

- `src/platform/raylib/hud_view_model.hpp`: fixed-capacity, raylib-free HUD
  projection interface and models.
- `src/platform/raylib/hud_view_model.cpp`: read-only snapshot projection with
  resource clamping, saturated ratios, default progression thresholds, fixed
  status-tag priority, Chinese target text, and NUL-terminated fixed text.
- `tests/platform/hud_view_model_tests.cpp`: nine real projection tests for
  normal/missing combat, non-mutating clamping, level 100, pending XP, all
  status tags, max biases, Chinese target text, and truncation.
- `src/platform/raylib/CMakeLists.txt`: links the production projection into
  `arpg_raylib`.
- `tests/platform/CMakeLists.txt`: adds the HUD test source.
- `tests/platform/platform_test_main.cpp`: registers `hud_view_model_suite()`
  and updates the exact platform count from 180 to 189.

## GREEN evidence

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game'
```

Result: passed; `arpg_platform_tests` and `arpg_game` linked successfully.

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 && ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.module_boundary)$" --output-on-failure'
```

Result: 2/2 tests passed (`platform.units`, `platform.module_boundary`).

```powershell
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Result: `189 cases, 0 failures`.

```powershell
git -c safe.directory=E:/game/.worktrees/stage11c-complete-hud diff --check
```

Result: passed with no whitespace errors before commit.

## Commit

`67e6ca0d53fa024c407a24f6a8d3e697820b05a6` — `feat: project production hud view model`

## Self-review

- Scope is limited to Task 1; no renderer, controller, item filter, quality,
  macOS, or Stage 11-D work was added.
- Projection has no Session/Store calls, RNG, physical input sampling, fixed
  ticks, dynamic containers, raylib drawing, or `TextFormat` use.
- The committed files are exactly the six Task 1 source/test/build files.

Concerns: none.

## Follow-up: bounded non-NUL hint buffers

Status: DONE

Review found that `ControlHints::primary` and `ControlHints::secondary` are
fixed arrays and do not promise NUL termination. The original `%s` formatting
could read past either array.

### RED evidence

The regression test fills both 160-byte buffers without a NUL byte and
requires bounded 32-byte projections of both values. Before the fix it failed:

```text
[FAIL] hud_view_model.non-terminated hint buffers:
std::strstr(output.room.secondary.bytes.data(),
"SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS") != nullptr
189 cases, 1 failures
```

### Fix and GREEN evidence

`hud_view_model.cpp` now formats the two hint buffers with `%.32s` precision,
which bounds each read before the 96-byte output buffer is considered. No
dynamic allocation was added. The existing truncation/NUL regression remains
covered separately, so the exact platform count is now 190.

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.module_boundary)$" --output-on-failure
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
git diff --check
```

Result: build passed; CTest 2/2 passed; direct test result
`190 cases, 0 failures`; `git diff --check` passed.

Follow-up commit:
`ec4ce4869c10548d86109dd5d66292c61f8984e8` — `fix: bound hud hint formatting`

Concerns: none.
