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

