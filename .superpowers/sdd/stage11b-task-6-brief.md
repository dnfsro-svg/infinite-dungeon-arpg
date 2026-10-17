### Task 6: Bounded Pause/Settings View and Renderer

**Files:**
- Create: `src/platform/raylib/pause_menu_view.hpp`
- Create: `src/platform/raylib/pause_menu_view.cpp`
- Create: `src/platform/raylib/pause_menu_renderer.hpp`
- Create: `src/platform/raylib/pause_menu_renderer.cpp`
- Create: `tests/platform/pause_menu_view_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
struct PauseMenuLayout final { Rectangle panel; Rectangle rows[16]; Rectangle footer; };
struct PauseMenuView final {
    std::array<std::array<char, 64>, 16> rows{};
    std::size_t row_count{};
    std::size_t selected_row{};
    const char* title{};
    const char* message{};
};
[[nodiscard]] PauseMenuLayout pause_menu_layout(int width, int height) noexcept;
[[nodiscard]] PauseMenuView make_pause_menu_view(const PauseMenuState&) noexcept;
[[nodiscard]] std::optional<std::size_t> hit_test_pause_row(
    const PauseMenuLayout&, Vector2) noexcept;
void draw_pause_menu(const PauseMenuState&) noexcept;
```

- [ ] **Step 1: Write layout/content RED tests**

Assert all panels and actionable rows fit at 1024×576, 1280×720 and 1920×1080; root/settings/capture/quit text is complete; all ten bindings and committed/draft values render; 63-character termination holds; hit testing rejects gaps/outside.

Controller content/layout contract:

- Titles are stable English UI literals: `PAUSED`, `SETTINGS`, `BIND KEY`, `QUIT GAME?`.
- Root rows are `Continue`, `Settings`, `Quit Game`; quit rows are `Quit Game`, `Back`; capture view has one informational row naming the captured action and current draft key.
- Settings uses the Task5 16-row order. The first three rows show draft Master SFX, Window Mode, and VSync plus the saved/committed value. Rows 3..12 show `action_label`, draft `stable_key_label`, and committed key so unsaved swaps are visible. Rows 13..15 are `Reset Defaults`, `Apply`, `Cancel`.
- Every generated row is null-terminated within 64 bytes; truncation must stay deterministic and no view/layout/hit function may allocate.
- Use a centered panel no wider than 760 px, at least 32 px outer horizontal margin, 16 px vertical margin, a fixed header/footer region, and 16 non-overlapping row rectangles with explicit gaps. At 1024x576 every row and footer must fit; larger resolutions remain centered without scaling text geometry beyond the bounded panel.
- `hit_test_pause_row` returns only when the point is inside a row rectangle using half-open right/bottom edges; panel padding, row gaps, footer and outside return null.
- The renderer is stateless and thin: one final full-screen dim layer, panel, title, only `view.row_count` rows, selected highlight, message/footer. It must not call state transitions, stores, Combat, Dungeon, or filesystem APIs.

- [ ] **Step 2: Verify RED**

Expected: platform target fails because view functions are absent.

- [ ] **Step 3: Implement pure view then thin renderer**

Use fixed arrays and `std::snprintf`; renderer draws a final full-screen dim layer, centered panel, selection highlight and footer after world/HUD. Do not store gameplay state in renderer.

- [ ] **Step 4: Run platform tests and link game**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^platform.units$" --output-on-failure
```

Expected: platform 1/1 and game link pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render bounded pause settings overlay"
```

---
