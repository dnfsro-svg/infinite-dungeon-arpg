### Task 7: Live Audio, Fullscreen, and VSync Preview with Rollback

**Files:**
- Create: `src/platform/raylib/window_settings.hpp`
- Create: `src/platform/raylib/window_settings.cpp`
- Create: `tests/platform/window_settings_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
struct WindowSettingsBackend final {
    void* context{};
    bool (*set_volume)(void*, std::uint8_t){};
    bool (*set_window_mode)(void*, settings::WindowMode){};
    bool (*set_vsync)(void*, bool){};
    settings::WindowMode (*window_mode)(void*){};
    bool (*vsync)(void*){};
};
enum class LiveSettingsResult : std::uint8_t { applied, invalid, backend_failed, readback_failed };
[[nodiscard]] LiveSettingsResult apply_live_settings(
    const settings::SettingsData& current,
    const settings::SettingsData& desired,
    WindowSettingsBackend) noexcept;
[[nodiscard]] LiveSettingsResult rollback_live_settings(
    const settings::SettingsData& previewed,
    const settings::SettingsData& committed,
    WindowSettingsBackend) noexcept;
[[nodiscard]] WindowSettingsBackend raylib_window_settings_backend() noexcept;
```

- [ ] **Step 1: Write fake backend RED tests**

Cover volume conversion, ordered apply, each backend failure, mode/VSync readback mismatch, rollback, and no call for unchanged values.

Controller transaction contract:

- Validate both snapshots before backend calls. Apply changed fields in stable order `volume -> window mode -> VSync`; read back mode immediately after setting it and VSync immediately after setting it.
- Any setter failure or readback mismatch must best-effort restore already changed fields to `current` in reverse order; the returned error remains the original `backend_failed` or `readback_failed`. Unchanged fields receive no setter/readback calls.
- `rollback_live_settings(previewed, committed, backend)` uses the same transactional path in the opposite direction; it is not a blind unconditional setter sequence.
- Missing required callbacks are `backend_failed` only when their field must change/read back; fully unchanged valid snapshots return `applied` even with an empty backend.
- Add a pure helper for the pre-`InitWindow` flags derived from committed settings (VSync and fullscreen/windowed) so Task 8 can consume it; Task 7 must not modify host yet.
- Runtime windowed restoration is exactly 1280x720 centered on the current monitor. Desktop fullscreen uses raylib 6.0 borderless/fullscreen state APIs and verifies the resulting logical WindowMode. Volume conversion is exactly `percent / 100.0F` and only occurs through load/preview/rollback calls.

- [ ] **Step 2: Verify RED**

Expected: platform target fails on missing backend.

- [ ] **Step 3: Implement fake-testable orchestration and raylib adapter**

Initialize flags from loaded committed settings before `InitWindow`; runtime mode uses raylib 6.0 fullscreen/window APIs and restores 1280×720 centered window. Apply `SetMasterVolume(percent / 100.0F)` only on load/preview/rollback.

- [ ] **Step 4: Run platform tests and compile-time raylib version guard**

Run platform 1/1 and build `arpg_game`; expected pass with existing raylib 6.0 static assertions.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: preview and rollback live settings"
```

---
