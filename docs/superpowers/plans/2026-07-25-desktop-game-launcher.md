# Desktop Game Launcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build, verify, package, and place on the Windows desktop a low-resource native launcher for Infinite Dungeon.

**Architecture:** A pure C++ `arpg_launcher_core` owns installation inspection, launch-request construction, save-path resolution, and layout geometry. A separate Win32 GUI executable uses Direct2D/DirectWrite for rendering and thin Windows API adapters for process/folder operations. A PowerShell deployment script installs one verified Release tree beneath LocalAppData and creates the desktop shortcut transactionally.

**Tech Stack:** C++17, MSVC 19.44, Windows SDK 10.0.26100.0, Win32, Direct2D, DirectWrite, CMake/Ninja, the existing custom C++ test framework, PowerShell, Python/Pillow for the source-controlled icon.

## Global Constraints

- Work only in `E:\game\.worktrees\desktop-game-launcher` on branch `codex/desktop-game-launcher`; do not modify the dirty `E:\game` checkout.
- The launcher targets Windows x64, C++17, MSVC 19.44, and Windows SDK 10.0.26100.0.
- The launcher must not link raylib or .NET and must not open a console window.
- The launcher window is 920×560 logical pixels, non-resizable, Per-Monitor-V2 DPI-aware, and rendered with Direct2D/DirectWrite.
- Chinese UI text uses `Microsoft YaHei UI`, opaque solid colors, measured DirectWrite layouts, and no texture text or blur scaling.
- The launcher only validates files, starts the game, opens the save directory, and exits; no accounts, networking, updater, ads, announcements, telemetry, or gameplay changes.
- The installed game starts without validation arguments and retains `%LOCALAPPDATA%\InfiniteDungeon\save` as the existing default save location.
- Required deployed items are `arpg_game.exe` plus `assets/fonts`, `assets/player`, `assets/skills`, `assets/stage12`, `assets/stage14/audio`, and `assets/stage15/audio`.
- The desktop shortcut resolves the real Desktop known folder and targets `%LOCALAPPDATA%\InfiniteDungeon\app\无限地下城启动器.exe`; it never targets a worktree or build directory.
- Follow red-green-refactor for every behavior change; generated icon pixels are exempt from unit-first development but require an asset validation test.
- Do not delete existing user files, reset Git, or overwrite unrelated changes. Deployment may replace its own app files only after validation and must never touch `%LOCALAPPDATA%\InfiniteDungeon\save`.

## File Map

- `src/launcher/launcher_core.hpp/.cpp`: installation model, required paths, save path, user-visible status, and launch request.
- `src/launcher/launcher_layout.hpp/.cpp`: fixed-DIP rectangles and non-overlap helpers.
- `src/launcher/launcher_platform_win32.hpp/.cpp`: current EXE path, `CreateProcessW`, save-folder creation, `ShellExecuteW`, and Windows error text.
- `src/launcher/launcher_renderer.hpp/.cpp`: Direct2D/DirectWrite resources and pure-color drawing.
- `src/launcher/launcher_main.cpp`: `wWinMain`, message loop, hit testing, command dispatch, and state updates.
- `src/launcher/arpg_launcher.manifest`: PerMonitorV2 and long-path declarations.
- `src/launcher/arpg_launcher.rc`: executable icon resource.
- `assets/launcher/infinite_dungeon.ico`: multi-resolution launcher icon derived from the approved Stage 12 UI material atlas.
- `tools/build_launcher_icon.py`: deterministic icon generator.
- `tests/launcher/*`: C++ unit tests, manifest/binary smoke checks, icon validation, and deployment self-test.
- `scripts/DeployLauncher.ps1`: validated LocalAppData installation and desktop shortcut creation.
- `docs/validation/evidence/desktop-launcher/*`: final screenshots and dependency/process evidence.

---

### Task 1: Launcher installation model and launch request

**Files:**
- Create: `src/launcher/CMakeLists.txt`
- Create: `src/launcher/launcher_core.hpp`
- Create: `src/launcher/launcher_core.cpp`
- Create: `tests/launcher/CMakeLists.txt`
- Create: `tests/launcher/launcher_test_main.cpp`
- Create: `tests/launcher/launcher_core_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: C++17 `std::filesystem`; no game or raylib targets.
- Produces:
  - `enum class InstallationState : std::uint8_t { ready, invalid_launcher_path, missing_game_executable, missing_asset_directory };`
  - `struct InstallationStatus { InstallationState state; std::filesystem::path application_directory; std::filesystem::path game_executable; std::filesystem::path missing_path; bool ready() const noexcept; };`
  - `InstallationStatus inspect_installation(const std::filesystem::path&) noexcept;`
  - `struct LaunchRequest { std::filesystem::path application; std::filesystem::path working_directory; std::wstring command_line; };`
  - `std::optional<LaunchRequest> make_launch_request(const InstallationStatus&) noexcept;`
  - `std::optional<std::filesystem::path> default_save_directory(std::wstring_view) noexcept;`
  - `std::wstring installation_message(const InstallationStatus&);`

- [ ] **Step 1: Write the failing core tests and declare the wished-for API**

Create `launcher_core.hpp` with only the exact declarations above and an empty `launcher_core.cpp`. Add six cases to the existing custom test framework:

```cpp
ready_installation_is_accepted();
missing_game_is_reported();
missing_asset_directory_is_reported();
ready_installation_builds_argument_free_request();
long_chinese_game_path_is_preserved_verbatim();
local_app_data_builds_existing_default_save_path();
empty_local_app_data_is_rejected();
```

The ready fixture creates `arpg_game.exe` as a regular file and each exact required directory under a temporary path containing both spaces and Chinese characters. Assert that the launch request has:

```cpp
ARPG_REQUIRE(request->application == status.game_executable);
ARPG_REQUIRE(request->working_directory == status.application_directory);
ARPG_REQUIRE(request->command_line == L"\"" + status.game_executable.native() + L"\"");
ARPG_REQUIRE(request->command_line.find(L"--") == std::wstring::npos);
```

The long-path case builds a ready `InstallationStatus` with a synthetic native path longer than 260 characters and verifies that `make_launch_request()` preserves every wide character without narrowing or truncation.

Create `launcher_test_main.cpp` with `run_suites(suites, 7, "desktop launcher core")`. Add `src/launcher` after the root graphics block and `tests/launcher` inside the root `BUILD_TESTING` block. The temporary `arpg_launcher_core` target compiles the empty translation unit so the tests reach an unresolved-symbol RED failure rather than a CMake source-not-found error.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
./scripts/Build.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.units$' --output-on-failure
```

Expected: the launcher test target fails to link because the declared core functions have no definitions. Record that linker failure in the task report.

- [ ] **Step 3: Implement the minimal core behavior**

Define the exact required paths as a single constexpr array:

```cpp
inline constexpr std::array<std::wstring_view, 6U> kRequiredAssetDirectories{{
    L"assets/fonts",
    L"assets/player",
    L"assets/skills",
    L"assets/stage12",
    L"assets/stage14/audio",
    L"assets/stage15/audio",
}};
```

`inspect_installation()` must derive the application directory from the launcher executable's parent, use non-throwing `std::filesystem` overloads with `std::error_code`, require `arpg_game.exe` to be a regular file, and return the first missing asset directory. `make_launch_request()` returns `nullopt` unless status is ready and quotes only argv[0]. `default_save_directory()` returns `<local-app-data>/InfiniteDungeon/save` without creating it. `installation_message()` returns exactly:

```text
游戏已就绪
启动器路径无效
缺少 arpg_game.exe
缺少资源：<relative path>
```

Change `arpg_launcher_core` to a real static library, expose `src/launcher` as a public include directory, enable project warnings and `/utf-8`, and add `launcher.units` with labels `headless;launcher`. Set `MSVC_RUNTIME_LIBRARY` to `MultiThreaded$<$<CONFIG:Debug>:Debug>` on both `arpg_launcher_core` and `arpg_launcher_tests` so the standalone launcher path does not acquire a dynamic MSVC runtime through its core library.

- [ ] **Step 4: Run focused and core-only tests to verify GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher_tests
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.units$' --output-on-failure
./scripts/Test.ps1 -Preset windows-msvc-core-debug
```

Expected: seven launcher cases pass and the existing core-only suite remains green.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt src/launcher tests/launcher
git commit -m "feat: add launcher installation core"
```

---

### Task 2: Deterministic layout and launcher icon

**Files:**
- Create: `src/launcher/launcher_layout.hpp`
- Create: `src/launcher/launcher_layout.cpp`
- Create: `tools/build_launcher_icon.py`
- Create: `assets/launcher/infinite_dungeon.ico`
- Create: `tests/launcher/launcher_layout_tests.cpp`
- Create: `tests/launcher/launcher_icon_tests.py`
- Modify: `src/launcher/CMakeLists.txt`
- Modify: `tests/launcher/CMakeLists.txt`
- Modify: `tests/launcher/launcher_test_main.cpp`

**Interfaces:**
- Consumes: the fixed 920×560 logical-pixel design and `assets/stage12/ui_material.png`.
- Produces:
  - `struct RectF { float left; float top; float right; float bottom; };`
  - `struct LauncherLayout { RectF title; RectF subtitle; RectF status; RectF start; RectF verify; RectF save; RectF exit; RectF path; };`
  - `LauncherLayout make_launcher_layout(float width, float height) noexcept;`
  - `bool overlaps(RectF, RectF) noexcept;`
  - `struct PixelSize { int width; int height; };`
  - `PixelSize launcher_pixel_size(unsigned int dpi) noexcept;`
  - a multi-resolution `infinite_dungeon.ico` containing 16, 32, 48, 64, 128, and 256 pixel frames.

- [ ] **Step 1: Write failing layout tests**

Add five C++ cases and raise the suite count from 7 to 12:

```cpp
design_size_keeps_every_rect_inside_client_area();
buttons_never_overlap_each_other();
status_and_path_do_not_overlap_actions();
undersized_client_area_returns_empty_layout();
dpi_scaling_preserves_the_fixed_logical_size();
```

Use 920×560 for the first three cases. The undersized case passes 799×449 and requires all rectangles to be zero. The DPI case requires exact pixel sizes `920×560`, `1150×700`, `1380×840`, and `1840×1120` for 96, 120, 144, and 192 DPI. Add only declarations to `launcher_layout.hpp` and an empty translation unit so RED is an unresolved-symbol failure.

- [ ] **Step 2: Run the focused C++ test and verify RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher_tests
```

Expected: unresolved `make_launcher_layout`/`overlaps` symbols.

- [ ] **Step 3: Implement the fixed-DIP layout**

Return these exact rectangles for a 920×560 client area:

```cpp
title    = {56, 42, 864, 98};
subtitle = {58, 102, 862, 132};
status   = {58, 174, 862, 226};
start    = {58, 260, 862, 342};
verify   = {58, 370, 308, 426};
save     = {335, 370, 585, 426};
exit     = {612, 370, 862, 426};
path     = {58, 474, 862, 522};
```

For larger client areas, center the 920×560 design surface and offset all rectangles; for either dimension below 800×450, return zero rectangles. `overlaps()` treats touching edges as non-overlap.

`launcher_pixel_size()` computes each dimension as `(logical * dpi + 95) / 96`, rejects DPI 0 with `{0,0}`, and is the only size conversion used by the Win32 window.

- [ ] **Step 4: Run layout tests and verify GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher_tests
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.units$' --output-on-failure
```

Expected: 12 cases, 0 failures.

- [ ] **Step 5: Write the failing icon asset test**

Create `launcher_icon_tests.py` using Pillow. It opens `assets/launcher/infinite_dungeon.ico`, requires the exact frame set `{(16,16),(32,32),(48,48),(64,64),(128,128),(256,256)}`, requires RGBA alpha extrema `(0,255)`, and rejects a fully black or fully transparent 256×256 frame. Register it as `launcher.icon_asset` with labels `headless;launcher;assets` and working directory `${PROJECT_SOURCE_DIR}`.

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.icon_asset$' --output-on-failure
```

Expected: FAIL because the icon does not exist.

- [ ] **Step 6: Generate the deterministic icon and verify GREEN**

`build_launcher_icon.py` must crop cell 9 (`x=128, y=128, width=128, height=128`) from `assets/stage12/ui_material.png`, preserve alpha, center it on a transparent square, resize with Lanczos, and write the exact six ICO sizes. Run the generator once and commit both the generator and generated ICO.

Run:

```powershell
python tools/build_launcher_icon.py
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.icon_asset$' --output-on-failure
```

Expected: icon test passes.

- [ ] **Step 7: Commit**

```powershell
git add src/launcher tests/launcher tools/build_launcher_icon.py assets/launcher/infinite_dungeon.ico
git commit -m "feat: add launcher layout and icon"
```

---

### Task 3: Native Win32 Direct2D launcher window

**Files:**
- Create: `src/launcher/launcher_platform_win32.hpp`
- Create: `src/launcher/launcher_platform_win32.cpp`
- Create: `src/launcher/launcher_renderer.hpp`
- Create: `src/launcher/launcher_renderer.cpp`
- Create: `src/launcher/launcher_main.cpp`
- Create: `src/launcher/arpg_launcher.manifest`
- Create: `src/launcher/arpg_launcher.rc`
- Create: `tests/launcher/launcher_manifest_test.cmake`
- Create: `tests/launcher/launcher_window_smoke_test.ps1`
- Modify: `src/launcher/CMakeLists.txt`
- Modify: `tests/launcher/CMakeLists.txt`

**Interfaces:**
- Consumes: `InstallationStatus`, `LaunchRequest`, `LauncherLayout`, and `assets/launcher/infinite_dungeon.ico`.
- Produces:
  - `std::optional<std::filesystem::path> current_executable_path() noexcept;`
  - `struct PlatformResult { bool ok; unsigned long error_code; std::wstring message; };`
  - `PlatformResult launch_game(const LaunchRequest&) noexcept;`
  - `PlatformResult open_save_directory(const std::filesystem::path&) noexcept;`
  - executable `无限地下城启动器.exe` with window title `无限地下城启动器`.

- [ ] **Step 1: Add failing manifest and binary smoke tests**

`launcher_manifest_test.cmake` reads `src/launcher/arpg_launcher.manifest` and requires `PerMonitorV2`, `longPathAware`, and `true`. `launcher_window_smoke_test.ps1` receives `-Launcher`, copies the EXE into an empty temporary directory, starts it, waits at most five seconds for `MainWindowHandle != 0`, requires `MainWindowTitle -eq '无限地下城启动器'`, parses the PE optional header and requires subsystem value `2` (`Windows GUI`), then closes only that process.

Register:

```cmake
add_test(NAME launcher.manifest
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/launcher_manifest_test.cmake")
add_test(NAME launcher.window_smoke
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_CURRENT_SOURCE_DIR}/launcher_window_smoke_test.ps1"
        -Launcher "$<TARGET_FILE:arpg_launcher>")
```

Run configure/build. Expected RED: manifest or launcher target is absent.

- [ ] **Step 2: Implement the Win32 platform adapter**

`current_executable_path()` uses a growing `GetModuleFileNameW` buffer and rejects truncation. `launch_game()` copies `request.command_line` into mutable contiguous storage and calls:

```cpp
CreateProcessW(
    request.application.c_str(), command_line.data(), nullptr, nullptr,
    FALSE, 0, nullptr, request.working_directory.c_str(), &startup, &process);
```

Close both returned handles immediately. `open_save_directory()` first calls `std::filesystem::create_directories(path, error)` and then `ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL)`. Format failures with `FormatMessageW`; never throw across `wWinMain`.

- [ ] **Step 3: Implement the Direct2D/DirectWrite renderer**

Create one `ID2D1Factory`, one `IDWriteFactory`, an HWND render target recreated after device loss, and text formats using exactly:

```cpp
L"Microsoft YaHei UI";  // title 42 DIP, subtitle 18, status 24, buttons 22, path 15
```

Draw a dark navy-to-steel background, opaque silver borders, cyan focus/hover accents, pure white primary text, pure green ready status, and pure red error status. Use `IDWriteTextLayout` within each `LauncherLayout` rectangle; set `DWRITE_WORD_WRAPPING_NO_WRAP` and ellipsis trimming for the path. No text opacity below 1.0 and no bitmap text.

The subtitle text is exactly `版本 0.1.0 · 单人无限地下城`. A non-ready Start button is visibly gray and has no hover/pressed accent. When the pointer rests inside the path rectangle, draw an opaque tooltip card over `{58, 430, 862, 522}` and render the complete wide-character path with wrapping; the card covers the underlying content instead of drawing text through it.

- [ ] **Step 4: Implement window behavior**

`wWinMain` initializes COM, registers a non-resizable window class, sizes 920×560 DIPs with `AdjustWindowRectExForDpi`, and handles `WM_DPICHANGED` using the suggested rectangle. The controller performs these exact commands:

```text
开始游戏     -> re-inspect -> CreateProcessW -> PostQuitMessage(0) only on success
检查游戏文件 -> re-inspect -> update status -> invalidate
打开存档目录 -> resolve LOCALAPPDATA -> create/open -> show any failure in status
退出         -> DestroyWindow
Enter        -> 开始游戏
Escape       -> 退出
```

Mouse hover/press state changes only button decoration. When installation status is not ready, hit testing and Enter must not invoke Start.

Track a separate `show_full_path` state from the path rectangle hover and invalidate only when it changes, so the full-path tooltip required by the design is available without a second window or narrow-character conversion.

- [ ] **Step 5: Wire the executable, manifest, and icon**

When `TARGET arpg_game` exists, add:

```cmake
add_executable(arpg_launcher WIN32
    launcher_main.cpp launcher_platform_win32.cpp launcher_renderer.cpp
    arpg_launcher.manifest arpg_launcher.rc)
set_target_properties(arpg_launcher PROPERTIES
    OUTPUT_NAME "无限地下城启动器"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(arpg_launcher PRIVATE
    arpg_launcher_core d2d1 dwrite shell32 ole32 user32 gdi32)
add_dependencies(arpg_launcher arpg_game)
```

Enable `/utf-8`, project warnings, and `UNICODE;_UNICODE;WIN32_LEAN_AND_MEAN;NOMINMAX`. The resource file assigns ID 101 to `../../assets/launcher/infinite_dungeon.ico`.

- [ ] **Step 6: Build and run focused UI tests**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.(manifest|window_smoke|units|icon_asset)$' --output-on-failure
```

Expected: the launcher opens briefly from an empty directory, reports the missing game instead of exiting, has a GUI subsystem, and all launcher tests pass.

- [ ] **Step 7: Commit**

```powershell
git add src/launcher tests/launcher
git commit -m "feat: add native desktop launcher"
```

---

### Task 4: Validated LocalAppData deployment and desktop shortcut

**Files:**
- Create: `scripts/DeployLauncher.ps1`
- Create: `tests/launcher/deploy_launcher_self_test.ps1`
- Modify: `tests/launcher/CMakeLists.txt`

**Interfaces:**
- Consumes: one Release `bin` directory containing `无限地下城启动器.exe`, `arpg_game.exe`, and all six required asset directories.
- Produces: `%LOCALAPPDATA%\InfiniteDungeon\app` and a known-folder Desktop shortcut `无限地下城.lnk`.

- [ ] **Step 1: Write the failing deployment self-test**

The self-test creates a temporary fake CMake Release tree with `bin` and a sibling `CMakeCache.txt` containing `CMAKE_BUILD_TYPE:STRING=Release`. It creates dummy launcher/game files plus all six asset directories, calls:

```powershell
& $DeployScript `
  -GameBuildDirectory $fakeBin `
  -InstallRoot $installRoot `
  -DesktopDirectory $desktopRoot
```

Then assert:

- every required file/directory exists under `$installRoot`;
- `$desktopRoot\无限地下城.lnk` exists;
- COM readback gives `TargetPath = $installRoot\无限地下城启动器.exe` and `WorkingDirectory = $installRoot`;
- the shortcut icon points to the installed launcher;
- a second fake input missing `assets/skills` exits nonzero and leaves a pre-existing sentinel in the installed app unchanged;
- no path named `save` is created, moved, or removed.

Register `launcher.deploy_self_test` with labels `headless;launcher;deployment`. Run it and expect RED because `DeployLauncher.ps1` is absent.

- [ ] **Step 2: Implement strict input validation**

Use this parameter contract:

```powershell
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameBuildDirectory,
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'InfiniteDungeon\app'),
    [string]$DesktopDirectory = [Environment]::GetFolderPath('Desktop')
)
```

Resolve all three paths. Before writing anything, require the two EXEs, all six asset directories, and a sibling `CMakeCache.txt` containing exactly `CMAKE_BUILD_TYPE:STRING=Release`. Never search another build directory or worktree.

- [ ] **Step 3: Implement staged deployment and shortcut creation**

Create a unique sibling staging directory, copy launcher/game/assets into it, and re-run the same validation against staging. If an app directory exists, rename it to unique `app.previous.<timestamp>.<pid>`; rename staging to the final app name. If publishing fails, restore the previous app name. Create/overwrite the shortcut only after the final app validates:

```powershell
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut((Join-Path $DesktopDirectory '无限地下城.lnk'))
$shortcut.TargetPath = Join-Path $InstallRoot '无限地下城启动器.exe'
$shortcut.WorkingDirectory = $InstallRoot
$shortcut.IconLocation = "$($shortcut.TargetPath),0"
$shortcut.Description = '启动无限地下城'
$shortcut.Save()
```

Do not enumerate, rename, copy, or remove `%LOCALAPPDATA%\InfiniteDungeon\save`.

- [ ] **Step 4: Run the deployment self-test and launcher suite**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.' --output-on-failure
```

Expected: deployment self-test and all launcher tests pass.

- [ ] **Step 5: Commit**

```powershell
git add scripts/DeployLauncher.ps1 tests/launcher
git commit -m "feat: deploy launcher to desktop"
```

---

### Task 5: Full verification, real deployment, and visual acceptance

**Files:**
- Create: `docs/validation/evidence/desktop-launcher/launcher-ready.png`
- Create: `docs/validation/evidence/desktop-launcher/launcher-missing-assets.png`
- Create: `docs/validation/evidence/desktop-launcher/launcher-validation.txt`

**Interfaces:**
- Consumes: completed launcher branch and the verified Release `bin` directory.
- Produces: actual LocalAppData installation, desktop shortcut, visual/process evidence, and no persistent test game process.

- [ ] **Step 1: Run fresh Debug verification**

Run:

```powershell
./scripts/Test.ps1 -Preset windows-msvc-debug
```

Expected: configure/build succeeds and all Debug CTest entries, including every `launcher.*` test, pass.

- [ ] **Step 2: Run fresh Release verification**

Run:

```powershell
./scripts/Test.ps1 -Preset windows-msvc-release
```

Expected: configure/build succeeds and all Release CTest entries pass. Do not suppress any existing graphics/formal validation failure; diagnose and report it if present.

- [ ] **Step 3: Record native dependency evidence**

Run `dumpbin /dependents` on both Release executables and record the exact DLL lists in `launcher-validation.txt`. The launcher must not depend on raylib or .NET DLLs; any non-system game dependency must be present on the machine before deployment is called successful.

- [ ] **Step 4: Deploy the verified Release directory**

Run:

```powershell
./scripts/DeployLauncher.ps1 `
  -GameBuildDirectory (Resolve-Path 'out/build/windows-msvc-release/bin')
```

Read back the shortcut via `WScript.Shell`, resolve its target and working directory, and append both to `launcher-validation.txt`. Confirm no deployment path points into `.worktrees`.

- [ ] **Step 5: Validate ready and missing-resource UI states**

Open the desktop shortcut with Computer Use. Capture `launcher-ready.png` and inspect at original resolution: all text must be sharp, opaque, within measured bounds, and non-overlapping. Temporarily rename only the installed `assets/skills` directory, press `检查游戏文件`, capture `launcher-missing-assets.png`, verify the red missing-resource message and disabled Start button, then restore the directory before continuing.

- [ ] **Step 6: Validate the real launch flow**

From the restored ready state, click `开始游戏`. Verify:

- the launcher PID exits;
- a new `arpg_game.exe` PID has executable path under `%LOCALAPPDATA%\InfiniteDungeon\app`;
- the game window renders;
- `%LOCALAPPDATA%\InfiniteDungeon\save` remains the game's save directory;
- no new game-validation flags appear in the process command line.

Stop only the newly launched installed-game PID after evidence is captured; do not stop or alter any older paused game process.

- [ ] **Step 7: Verify DPI behavior**

Use Windows DPI APIs or isolated UI scaling checks at 100%, 125%, 150%, and 200%. At each scale verify the client layout remains 920×560 DIPs, the title and buttons remain inside the client area, and no text overlaps. Record results in `launcher-validation.txt`.

- [ ] **Step 8: Commit evidence and final verification record**

```powershell
git add docs/validation/evidence/desktop-launcher
git commit -m "test: validate desktop launcher deployment"
```

- [ ] **Step 9: Whole-branch review and finish**

Generate the full branch diff from `git merge-base main HEAD`, request a whole-branch review, fix any load-bearing findings once, re-run the covering tests, and then use `superpowers:finishing-a-development-branch`. Do not merge into dirty `E:\game` without an explicit safe integration decision.
