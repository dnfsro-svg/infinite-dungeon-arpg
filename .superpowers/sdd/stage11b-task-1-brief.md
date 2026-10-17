### Task 1: Stable Settings Types and Conflict-Free Bindings

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/platform/settings/CMakeLists.txt`
- Create: `src/platform/settings/settings_types.hpp`
- Create: `src/platform/settings/settings_types.cpp`
- Create: `tests/settings/CMakeLists.txt`
- Create: `tests/settings/settings_test_main.cpp`
- Create: `tests/settings/settings_types_tests.cpp`

**Interfaces:**
- Produces:

```cpp
namespace arpg::settings {
enum class SettingAction : std::uint8_t {
    move_up, move_down, move_left, move_right,
    light_attack, jump, launcher, interact, inventory, passive_tree, count
};
enum class StableKey : std::uint8_t {
    a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t,
    u, v, w, x, y, z, digit_0, digit_1, digit_2, digit_3, digit_4,
    digit_5, digit_6, digit_7, digit_8, digit_9, arrow_up, arrow_down,
    arrow_left, arrow_right, space, left_shift, right_shift,
    left_control, right_control, count
};
enum class WindowMode : std::uint8_t { windowed, fullscreen };
struct SettingsData final {
    std::uint8_t master_sfx_percent{100};
    WindowMode window_mode{WindowMode::windowed};
    bool vsync_enabled{true};
    std::array<StableKey, static_cast<std::size_t>(SettingAction::count)> bindings{};
    std::uint64_t revision{};
};
enum class SettingsValidationError : std::uint8_t {
    none, volume_range, volume_step, window_mode, key_range, duplicate_key
};
[[nodiscard]] SettingsData default_settings() noexcept;
[[nodiscard]] SettingsValidationError validate_settings(const SettingsData&) noexcept;
[[nodiscard]] StableKey binding_for(const SettingsData&, SettingAction) noexcept;
[[nodiscard]] bool assign_or_swap(SettingsData&, SettingAction, StableKey) noexcept;
[[nodiscard]] const char* action_label(SettingAction) noexcept;
}
```

- Default action order and keys are exactly `W/S/A/D/J/K/L/E/I/P`.

- [ ] **Step 1: Add the settings test target and write failing golden tests**

Add `add_subdirectory(src/platform/settings)` before raylib and `add_subdirectory(tests/settings)` under `BUILD_TESTING`. Tests must assert all ten defaults, volume/window/VSync/revision, every validation error, unoccupied assignment, occupied-key swap, and 1,000 swaps preserving uniqueness.

- [ ] **Step 2: Run the target and verify RED**

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
```

Expected: compilation fails because `settings_types.hpp` and `arpg_settings` do not exist.

- [ ] **Step 3: Implement the minimal stable schema and validation**

Use a fixed default array and an O(10²) duplicate scan; this runs only at load/apply and needs no dynamic allocation. `assign_or_swap` must find the owner of the new key and swap its old key into that owner.

- [ ] **Step 4: Run focused and boundary regression**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
ctest --test-dir out/build/windows-msvc-debug -R "^settings.units$" --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R "^(core.units|combat.units|platform.module_boundary)$" --output-on-failure
```

Expected: settings 1/1 and selected regression 3/3 pass.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt src/platform/settings tests/settings
git commit -m "feat: define stage 11b settings schema"
```

---

