### Task 5: Pure Pause and Settings State Machine

**Files:**
- Create: `src/platform/raylib/pause_menu_state.hpp`
- Create: `src/platform/raylib/pause_menu_state.cpp`
- Create: `tests/platform/pause_menu_state_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
enum class PauseScreen : std::uint8_t { closed, root, settings, capture_binding, quit_confirm };
enum class PauseCommand : std::uint8_t { none, preview, apply, rollback, resume, quit };
struct PauseContext final {
    bool death_active{}, recovery_active{}, inventory_open{}, passive_open{},
        pending_save{}, window_focused{true};
};
struct PauseInput final {
    bool escape{}, enter{}, up{}, down{}, left{}, right{}, activate{}, focus_lost{};
    std::optional<settings::StableKey> captured_key{};
};
struct PauseMenuState final {
    PauseScreen screen{PauseScreen::closed};
    std::size_t selected_row{};
    std::optional<settings::SettingAction> capture_action{};
    settings::SettingsData committed{};
    settings::SettingsData draft{};
    const char* message{};
};
[[nodiscard]] PauseCommand update_pause_menu(
    PauseMenuState&, const PauseContext&, const PauseInput&) noexcept;
```

- [ ] **Step 1: Write every transition as RED**

Cover overlay priority, pending-save rejection, open/resume, settings/back, capture/cancel/focus loss, valid capture, occupied-key swap, defaults draft, Apply command, save-error state, quit confirmation, and same-frame Esc+attack gate.

Controller row/transition contract (keeps Task 6 layout deterministic):

- `root` rows are `0 Continue`, `1 Settings`, `2 Quit`; navigation wraps. Enter/activate are equivalent. Opening Settings copies `committed` to `draft` and clears any stale message.
- `settings` rows are `0 Volume`, `1 Window Mode`, `2 VSync`, `3..12` the ten `SettingAction` values in enum order, `13 Reset Defaults`, `14 Apply`, `15 Cancel`; navigation wraps.
- Volume left/right changes by 5 and clamps to 0..100. Window/VSync left/right or activate toggles. These three changes return `preview` only when the draft actually changes.
- Binding-row activate enters `capture_binding`. Valid captured keys use `assign_or_swap`, return to settings, and do not preview live input. Reserved/invalid keys keep capture active with a static error message. `Esc`, `focus_lost`, or `!context.window_focused` cancel capture without changing the draft.
- Reset Defaults replaces the draft with defaults but preserves its current revision and returns `preview` because live window/audio fields may change. It does not publish settings.
- Apply validates draft, refuses `revision == UINT64_MAX`, sets `draft.revision = committed.revision + 1`, and returns `apply` without changing committed or closing settings. A host-supplied static error message may remain while retrying; Task 8 will publish or retain draft after I/O.
- Cancel or Settings `Esc` restores `draft = committed`, clears message, returns to root, and returns `rollback`.
- `quit_confirm` has rows `0 Quit`, `1 Back`, defaults to Back, wraps, and only row 0 activate/Enter returns `quit`; Esc or Back returns root.
- When closed, recovery/death/inventory/passive/pending-save contexts consume pause-opening Esc in their higher-priority owner and this machine stays closed. A normal Esc opens root and returns no gameplay command; Task 8 will use the opened screen to drop same-frame gameplay.
- Use static string literals only for `message`; no allocation and no external ownership.

- [ ] **Step 2: Verify RED**

Expected: platform target fails on missing state machine.

- [ ] **Step 3: Implement table-like deterministic transitions**

The state machine only mutates `PauseMenuState` and returns commands; it must not call raylib, SettingsStore, Combat, Dungeon or filesystem APIs.

- [ ] **Step 4: Run platform and death gate tests**

Run `ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|stage11.death_evidence.mutation_self_test)$" --output-on-failure`.

Expected: 2/2 pass and death gate behavior remains authoritative.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: model pause and settings navigation"
```

---
