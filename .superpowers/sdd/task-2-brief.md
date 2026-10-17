### Task 2: Pause Settings Draft Semantics

**Files:**
- Modify: `src/platform/raylib/pause_menu_state.cpp`
- Modify: `src/platform/raylib/pause_menu_view.hpp`
- Modify: `src/platform/raylib/pause_menu_view.cpp`
- Modify: `tests/platform/pause_menu_state_tests.cpp`
- Modify: `tests/platform/pause_menu_view_tests.cpp`

**Interfaces:**
- Settings row 3 becomes `Loot Filter`; binding rows move to 4-13.
- Reset, Apply, Cancel move to rows 14, 15, 16.
- Produces: `kPauseMenuRowCapacity == 17U`.

- [ ] **Step 1: Write failing state/view tests**

Require navigation to wrap 17 rows. On row 3, left cycles `show_all -> rare_only`, right/Enter cycles forward, and the command is `preview`. Require reset to restore `show_all`, cancel to restore committed mode, and view text to include both draft and saved filter labels.

```cpp
state.screen = platform::PauseScreen::settings;
state.selected_row = 3U;
ARPG_REQUIRE(update(state, input_for(InputKind::right)) ==
    platform::PauseCommand::preview);
ARPG_REQUIRE(state.draft.loot_filter_mode ==
    settings::LootFilterMode::magic_or_better);
```

- [ ] **Step 2: Build platform tests and verify RED**

Run `cmake --build out/build/windows-msvc-debug --target arpg_platform_tests`; expected failures for the old 16-row layout.

- [ ] **Step 3: Implement row mapping and text**

Add `update_loot_filter`, update row constants, binding offsets, reset/apply/cancel row numbers, layout capacity, and `build_settings_rows`. Do not publish settings inside state logic; keep the existing preview/apply/rollback command boundary.

- [ ] **Step 4: Run focused tests and verify GREEN**

Run:

```powershell
$env:ARPG_TEST_FILTER='platform.pause'; .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: pause state, view, render-plan, host-gate, and zero-allocation tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/pause_menu_* tests/platform/pause_menu_*
git commit -m "feat: add loot filter setting control"
```

