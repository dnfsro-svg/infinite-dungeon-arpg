### Task 4: Stable-Key Adapter and One-Sample Logical Input

**Files:**
- Create: `src/platform/raylib/stable_key_raylib.hpp`
- Create: `src/platform/raylib/stable_key_raylib.cpp`
- Create: `src/platform/raylib/host_input.hpp`
- Create: `src/platform/raylib/host_input.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/platform/raylib/raylib_input.hpp`
- Modify: `src/platform/raylib/raylib_input.cpp`
- Delete: `src/platform/raylib/combat_key_bindings.hpp`
- Replace: `tests/platform/combat_key_bindings_tests.cpp` with `tests/platform/host_input_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
[[nodiscard]] int raylib_key(settings::StableKey) noexcept;
[[nodiscard]] std::optional<settings::StableKey> stable_key_from_raylib(int) noexcept;
[[nodiscard]] const char* stable_key_label(settings::StableKey) noexcept;

struct PhysicalKeySnapshot final {
    std::array<bool, static_cast<std::size_t>(settings::StableKey::count)> down{};
    std::array<bool, static_cast<std::size_t>(settings::StableKey::count)> pressed{};
    bool escape{}, enter{}, f1{}, f12{}, v{}, mouse_left{}, focus_lost{};
    Vector2 mouse_position{};
};
struct HostFrameInput final {
    FrameKeyState keys{};
    combat::MovementInput movement{};
    std::array<bool, 3> combat_actions{};
    Vector2 mouse_position{};
};
[[nodiscard]] PhysicalKeySnapshot sample_physical_keys() noexcept;
[[nodiscard]] HostFrameInput map_host_frame_input(
    const settings::SettingsData&, const PhysicalKeySnapshot&) noexcept;
void submit_frame_actions(dungeon::DungeonSession&, const HostFrameInput&) noexcept;
```

- [ ] **Step 1: Write exhaustive adapter and mapping RED tests**

Test all 45 stable keys round-trip, unique raylib codes and labels; default and swapped movement/action/interact/inventory/passive mapping; pressed versus down; global keys independent of binding; one physical key cannot generate two logical actions after validation.

Controller clarification:

- `V` remains present in the 45-key adapter so the global screenshot edge is derived from `snapshot.pressed[StableKey::v]`; do not poll `KEY_V` a second time.
- Preserve the legacy `R` room-reset and `N` recovery-new-run commands, but derive them from the same stable snapshot. If a valid user binding owns `R`, that bound gameplay action wins and the legacy reset command must be false, so one physical key never emits two gameplay commands.
- Add test seams or source guards sufficient to prove each stable key is sampled exactly once for `pressed` and once for `down`, and that the pure mapper performs no device calls or heap allocation.

- [ ] **Step 2: Verify RED**

Run platform target; expected missing adapter/input symbols.

- [ ] **Step 3: Implement adapter and pure mapper**

`sample_physical_keys` loops each supported stable key once and calls both pressed/down once. `map_host_frame_input` performs no raylib calls and no allocation. Replace the private structures/functions at `raylib_host.cpp:36-82` with this module.

The host replacement in this task uses `default_settings()` as its temporary committed binding authority. Task 8 will replace that temporary authority with the loaded/applied settings instance. After replacement, `raylib_host.cpp` must not directly poll gameplay keys (`W/A/S/D/J/K/L/E/I/P/R/N`) or `V`; all are consumed from the single snapshot/mapped frame input. Do not implement pause/settings state or persistence integration here.

- [ ] **Step 4: Run latency and platform regression**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.input_latency_source)$" --output-on-failure
rg -n "KEY_[WASDJKLEIP]" src/platform/raylib/raylib_host.cpp src/platform/raylib/hud_renderer.cpp
```

Expected: platform tests pass; no gameplay-key hardcoding remains in host/HUD.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: map configurable keys to frame input"
```

---
