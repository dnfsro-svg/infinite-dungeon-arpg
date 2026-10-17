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

