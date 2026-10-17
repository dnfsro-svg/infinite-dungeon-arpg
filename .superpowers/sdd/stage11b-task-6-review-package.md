# Review package: 248d502..3c7eb3b

## Commits
3c7eb3b feat: render bounded pause settings overlay

## Files changed
 src/platform/raylib/CMakeLists.txt          |   2 +
 src/platform/raylib/pause_menu_renderer.cpp |  84 +++++++++
 src/platform/raylib/pause_menu_renderer.hpp |   9 +
 src/platform/raylib/pause_menu_view.cpp     | 160 ++++++++++++++++
 src/platform/raylib/pause_menu_view.hpp     |  42 +++++
 tests/platform/CMakeLists.txt               |   1 +
 tests/platform/pause_menu_view_tests.cpp    | 280 ++++++++++++++++++++++++++++
 tests/platform/platform_test_main.cpp       |   4 +-
 8 files changed, 581 insertions(+), 1 deletion(-)

## Diff
diff --git a/src/platform/raylib/CMakeLists.txt b/src/platform/raylib/CMakeLists.txt
index 5a33051..81ce1e4 100644
--- a/src/platform/raylib/CMakeLists.txt
+++ b/src/platform/raylib/CMakeLists.txt
@@ -11,20 +11,22 @@ add_library(arpg_raylib STATIC
     dungeon_view_math.cpp
     dungeon_runtime.cpp
     host_input.cpp
     host_launch_options.cpp
     hud_renderer.cpp
     inventory_renderer.cpp
     inventory_view_math.cpp
     passive_tree_renderer.cpp
     passive_tree_view_math.cpp
     pause_menu_state.cpp
+    pause_menu_view.cpp
+    pause_menu_renderer.cpp
     raylib_input.cpp
     raylib_host.cpp
     room_renderer.cpp
     stable_key_raylib.cpp)
 
 target_include_directories(arpg_raylib
     PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
 target_compile_features(arpg_raylib PUBLIC cxx_std_17)
 target_link_libraries(arpg_raylib
     PUBLIC arpg_dungeon arpg_persistence raylib
diff --git a/src/platform/raylib/pause_menu_renderer.cpp b/src/platform/raylib/pause_menu_renderer.cpp
new file mode 100644
index 0000000..9982ba0
--- /dev/null
+++ b/src/platform/raylib/pause_menu_renderer.cpp
@@ -0,0 +1,84 @@
+#include "pause_menu_renderer.hpp"
+
+#include "pause_menu_view.hpp"
+
+#include <raylib.h>
+
+#include <cstddef>
+
+namespace arpg::platform {
+namespace {
+
+void draw_centered_text(
+    const char* text,
+    Rectangle bounds,
+    int font_size,
+    Color color) noexcept {
+    const int text_width = MeasureText(text, font_size);
+    const int x = static_cast<int>(
+        bounds.x + (bounds.width - static_cast<float>(text_width)) * 0.5F);
+    const int y = static_cast<int>(
+        bounds.y + (bounds.height - static_cast<float>(font_size)) * 0.5F);
+    DrawText(text, x, y, font_size, color);
+}
+
+}  // namespace
+
+void draw_pause_menu(const PauseMenuState& state) noexcept {
+    const PauseMenuView view = make_pause_menu_view(state);
+    if (view.row_count == 0U || view.title == nullptr) return;
+
+    const int screen_width = GetScreenWidth();
+    const int screen_height = GetScreenHeight();
+    const PauseMenuLayout layout = pause_menu_layout(
+        screen_width, screen_height);
+
+    DrawRectangle(0, 0, screen_width, screen_height,
+        Color{2, 4, 8, 190});
+    DrawRectangleRounded(layout.panel, 0.04F, 8,
+        Color{10, 14, 23, 248});
+    DrawRectangleRoundedLinesEx(layout.panel, 0.04F, 8, 2.0F,
+        Color{94, 159, 206, 255});
+
+    const Rectangle title_bounds{
+        layout.panel.x + 24.0F,
+        layout.panel.y + 14.0F,
+        layout.panel.width - 48.0F,
+        32.0F,
+    };
+    draw_centered_text(view.title, title_bounds, 28,
+        Color{191, 225, 255, 255});
+
+    for (std::size_t row = 0U; row < view.row_count; ++row) {
+        const Rectangle bounds = layout.rows[row];
+        if (row == view.selected_row) {
+            DrawRectangleRounded(bounds, 0.18F, 5,
+                Color{42, 91, 126, 235});
+            DrawRectangleRoundedLinesEx(bounds, 0.18F, 5, 1.0F,
+                Color{121, 197, 244, 255});
+        }
+        DrawText(view.rows[row].data(),
+            static_cast<int>(bounds.x + 10.0F),
+            static_cast<int>(bounds.y + 3.0F),
+            16,
+            row == view.selected_row
+                ? Color{244, 249, 255, 255}
+                : Color{207, 218, 231, 255});
+    }
+
+    DrawLine(
+        static_cast<int>(layout.footer.x),
+        static_cast<int>(layout.footer.y - 5.0F),
+        static_cast<int>(layout.footer.x + layout.footer.width),
+        static_cast<int>(layout.footer.y - 5.0F),
+        Color{75, 86, 104, 220});
+    const bool has_message = view.message != nullptr && view.message[0] != '\0';
+    draw_centered_text(
+        has_message ? view.message : "Arrow keys navigate | Enter select | Esc back",
+        layout.footer,
+        16,
+        has_message ? Color{255, 139, 139, 255}
+                    : Color{165, 178, 196, 255});
+}
+
+}  // namespace arpg::platform
diff --git a/src/platform/raylib/pause_menu_renderer.hpp b/src/platform/raylib/pause_menu_renderer.hpp
new file mode 100644
index 0000000..fcd77f8
--- /dev/null
+++ b/src/platform/raylib/pause_menu_renderer.hpp
@@ -0,0 +1,9 @@
+#pragma once
+
+#include "pause_menu_state.hpp"
+
+namespace arpg::platform {
+
+void draw_pause_menu(const PauseMenuState& state) noexcept;
+
+}  // namespace arpg::platform
diff --git a/src/platform/raylib/pause_menu_view.cpp b/src/platform/raylib/pause_menu_view.cpp
new file mode 100644
index 0000000..5ba1c84
--- /dev/null
+++ b/src/platform/raylib/pause_menu_view.cpp
@@ -0,0 +1,160 @@
+#include "pause_menu_view.hpp"
+
+#include "stable_key_raylib.hpp"
+
+#include <cstdio>
+
+namespace arpg::platform {
+namespace {
+
+constexpr float kPanelWidth = 760.0F;
+constexpr float kPanelHeight = 544.0F;
+constexpr float kRowInsetX = 24.0F;
+constexpr float kFirstRowY = 60.0F;
+constexpr float kRowHeight = 22.0F;
+constexpr float kRowStride = 27.0F;
+constexpr float kFooterY = 496.0F;
+constexpr float kFooterHeight = 28.0F;
+
+template <typename... Arguments>
+void write_row(
+    PauseMenuView& view,
+    std::size_t row,
+    const char* format,
+    Arguments... arguments) noexcept {
+    if (row >= view.rows.size()) return;
+    static_cast<void>(std::snprintf(
+        view.rows[row].data(), view.rows[row].size(),
+        format, arguments...));
+    view.rows[row].back() = '\0';
+}
+
+[[nodiscard]] const char* window_mode_label(
+    settings::WindowMode mode) noexcept {
+    switch (mode) {
+        case settings::WindowMode::windowed:
+            return "Windowed";
+        case settings::WindowMode::fullscreen:
+            return "Fullscreen";
+    }
+    return "Unknown";
+}
+
+[[nodiscard]] const char* enabled_label(bool enabled) noexcept {
+    return enabled ? "On" : "Off";
+}
+
+void build_settings_rows(
+    PauseMenuView& view,
+    const PauseMenuState& state) noexcept {
+    write_row(view, 0U, "Master SFX: %u%% (saved %u%%)",
+        static_cast<unsigned>(state.draft.master_sfx_percent),
+        static_cast<unsigned>(state.committed.master_sfx_percent));
+    write_row(view, 1U, "Window Mode: %s (saved %s)",
+        window_mode_label(state.draft.window_mode),
+        window_mode_label(state.committed.window_mode));
+    write_row(view, 2U, "VSync: %s (saved %s)",
+        enabled_label(state.draft.vsync_enabled),
+        enabled_label(state.committed.vsync_enabled));
+
+    for (std::size_t index = 0U;
+         index < static_cast<std::size_t>(settings::SettingAction::count);
+         ++index) {
+        const auto action = static_cast<settings::SettingAction>(index);
+        write_row(view, index + 3U, "%s: %s (saved %s)",
+            settings::action_label(action),
+            stable_key_label(settings::binding_for(state.draft, action)),
+            stable_key_label(settings::binding_for(state.committed, action)));
+    }
+    write_row(view, 13U, "%s", "Reset Defaults");
+    write_row(view, 14U, "%s", "Apply");
+    write_row(view, 15U, "%s", "Cancel");
+}
+
+}  // namespace
+
+PauseMenuLayout pause_menu_layout(int width, int height) noexcept {
+    const float screen_width = static_cast<float>(width);
+    const float screen_height = static_cast<float>(height);
+    PauseMenuLayout layout{};
+    layout.panel = {
+        (screen_width - kPanelWidth) * 0.5F,
+        (screen_height - kPanelHeight) * 0.5F,
+        kPanelWidth,
+        kPanelHeight,
+    };
+    for (std::size_t row = 0U; row < kPauseMenuRowCapacity; ++row) {
+        layout.rows[row] = {
+            layout.panel.x + kRowInsetX,
+            layout.panel.y + kFirstRowY
+                + static_cast<float>(row) * kRowStride,
+            layout.panel.width - kRowInsetX * 2.0F,
+            kRowHeight,
+        };
+    }
+    layout.footer = {
+        layout.panel.x + kRowInsetX,
+        layout.panel.y + kFooterY,
+        layout.panel.width - kRowInsetX * 2.0F,
+        kFooterHeight,
+    };
+    return layout;
+}
+
+PauseMenuView make_pause_menu_view(const PauseMenuState& state) noexcept {
+    PauseMenuView view{};
+    view.message = state.message;
+    switch (state.screen) {
+        case PauseScreen::closed:
+            return view;
+        case PauseScreen::root:
+            view.title = "PAUSED";
+            view.row_count = 3U;
+            view.selected_row = state.selected_row;
+            write_row(view, 0U, "%s", "Continue");
+            write_row(view, 1U, "%s", "Settings");
+            write_row(view, 2U, "%s", "Quit Game");
+            return view;
+        case PauseScreen::settings:
+            view.title = "SETTINGS";
+            view.row_count = kPauseMenuRowCapacity;
+            view.selected_row = state.selected_row;
+            build_settings_rows(view, state);
+            return view;
+        case PauseScreen::capture_binding: {
+            view.title = "BIND KEY";
+            view.row_count = 1U;
+            view.selected_row = 0U;
+            const settings::SettingAction action = state.capture_action.value_or(
+                settings::SettingAction::count);
+            write_row(view, 0U, "Press a key for %s (current %s)",
+                settings::action_label(action),
+                stable_key_label(settings::binding_for(state.draft, action)));
+            return view;
+        }
+        case PauseScreen::quit_confirm:
+            view.title = "QUIT GAME?";
+            view.row_count = 2U;
+            view.selected_row = state.selected_row;
+            write_row(view, 0U, "%s", "Quit Game");
+            write_row(view, 1U, "%s", "Back");
+            return view;
+    }
+    return view;
+}
+
+std::optional<std::size_t> hit_test_pause_row(
+    const PauseMenuLayout& layout,
+    Vector2 point) noexcept {
+    for (std::size_t row = 0U; row < kPauseMenuRowCapacity; ++row) {
+        const Rectangle bounds = layout.rows[row];
+        if (point.x >= bounds.x && point.x < bounds.x + bounds.width
+                && point.y >= bounds.y
+                && point.y < bounds.y + bounds.height) {
+            return row;
+        }
+    }
+    return std::nullopt;
+}
+
+}  // namespace arpg::platform
diff --git a/src/platform/raylib/pause_menu_view.hpp b/src/platform/raylib/pause_menu_view.hpp
new file mode 100644
index 0000000..e58818c
--- /dev/null
+++ b/src/platform/raylib/pause_menu_view.hpp
@@ -0,0 +1,42 @@
+#pragma once
+
+#include "pause_menu_state.hpp"
+
+#include <raylib.h>
+
+#include <array>
+#include <cstddef>
+#include <optional>
+
+namespace arpg::platform {
+
+inline constexpr std::size_t kPauseMenuRowCapacity = 16U;
+inline constexpr std::size_t kPauseMenuRowTextCapacity = 64U;
+
+struct PauseMenuLayout final {
+    Rectangle panel{};
+    Rectangle rows[kPauseMenuRowCapacity]{};
+    Rectangle footer{};
+};
+
+struct PauseMenuView final {
+    std::array<std::array<char, kPauseMenuRowTextCapacity>,
+        kPauseMenuRowCapacity> rows{};
+    std::size_t row_count{};
+    std::size_t selected_row{};
+    const char* title{};
+    const char* message{};
+};
+
+[[nodiscard]] PauseMenuLayout pause_menu_layout(
+    int width,
+    int height) noexcept;
+
+[[nodiscard]] PauseMenuView make_pause_menu_view(
+    const PauseMenuState& state) noexcept;
+
+[[nodiscard]] std::optional<std::size_t> hit_test_pause_row(
+    const PauseMenuLayout& layout,
+    Vector2 point) noexcept;
+
+}  // namespace arpg::platform
diff --git a/tests/platform/CMakeLists.txt b/tests/platform/CMakeLists.txt
index bd90b91..295af4f 100644
--- a/tests/platform/CMakeLists.txt
+++ b/tests/platform/CMakeLists.txt
@@ -4,20 +4,21 @@ if(TARGET arpg_raylib)
         platform_test_main.cpp
         host_input_tests.cpp
         combat_feedback_tests.cpp
         combat_view_math_tests.cpp
         monster_view_tests.cpp
         dungeon_view_math_tests.cpp
         passive_tree_view_tests.cpp
         host_launch_options_tests.cpp
         dungeon_runtime_tests.cpp
         pause_menu_state_tests.cpp
+        pause_menu_view_tests.cpp
         death_input_gate_tests.cpp
         death_overlay_view_tests.cpp)
     target_sources(arpg_platform_tests PRIVATE inventory_view_math_tests.cpp)
     target_compile_features(arpg_platform_tests PRIVATE cxx_std_17)
     target_include_directories(arpg_platform_tests
         PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
     target_link_libraries(arpg_platform_tests PRIVATE arpg_raylib)
     if(MSVC)
         target_compile_options(arpg_platform_tests PRIVATE /utf-8)
     endif()
diff --git a/tests/platform/pause_menu_view_tests.cpp b/tests/platform/pause_menu_view_tests.cpp
new file mode 100644
index 0000000..82835ba
--- /dev/null
+++ b/tests/platform/pause_menu_view_tests.cpp
@@ -0,0 +1,280 @@
+#include "allocation_probe.hpp"
+#include "test_framework.hpp"
+
+#include "pause_menu_view.hpp"
+
+#include <array>
+#include <cstddef>
+#include <cstring>
+
+namespace {
+
+using namespace arpg;
+
+bool inside(Rectangle inner, Rectangle outer) noexcept {
+    return inner.x >= outer.x && inner.y >= outer.y
+        && inner.x + inner.width <= outer.x + outer.width
+        && inner.y + inner.height <= outer.y + outer.height;
+}
+
+bool separated(Rectangle first, Rectangle second) noexcept {
+    return first.x + first.width <= second.x
+        || second.x + second.width <= first.x
+        || first.y + first.height <= second.y
+        || second.y + second.height <= first.y;
+}
+
+platform::PauseMenuState settings_state() noexcept {
+    platform::PauseMenuState state{};
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 12U;
+    state.committed = settings::default_settings();
+    state.draft = state.committed;
+    state.draft.master_sfx_percent = 55U;
+    state.draft.window_mode = settings::WindowMode::fullscreen;
+    state.draft.vsync_enabled = false;
+    state.draft.bindings = {
+        settings::StableKey::digit_0,
+        settings::StableKey::digit_1,
+        settings::StableKey::digit_2,
+        settings::StableKey::digit_3,
+        settings::StableKey::digit_4,
+        settings::StableKey::digit_5,
+        settings::StableKey::digit_6,
+        settings::StableKey::digit_7,
+        settings::StableKey::digit_8,
+        settings::StableKey::digit_9,
+    };
+    state.message = "Apply failed; retry";
+    return state;
+}
+
+test::Failure layouts_are_bounded_centered_and_fixed() noexcept {
+    constexpr std::array<std::array<int, 2>, 3> kSizes{{
+        {{1024, 576}}, {{1280, 720}}, {{1920, 1080}},
+    }};
+    float panel_width = 0.0F;
+    float panel_height = 0.0F;
+    float row_height = 0.0F;
+    for (const auto& size : kSizes) {
+        const Rectangle screen{
+            0.0F, 0.0F,
+            static_cast<float>(size[0]), static_cast<float>(size[1])};
+        const platform::PauseMenuLayout layout =
+            platform::pause_menu_layout(size[0], size[1]);
+        ARPG_REQUIRE(inside(layout.panel, screen));
+        ARPG_REQUIRE(layout.panel.width <= 760.0F);
+        ARPG_REQUIRE(layout.panel.x >= 32.0F);
+        ARPG_REQUIRE(layout.panel.x + layout.panel.width
+            <= screen.width - 32.0F);
+        ARPG_REQUIRE(layout.panel.y >= 16.0F);
+        ARPG_REQUIRE(layout.panel.y + layout.panel.height
+            <= screen.height - 16.0F);
+        ARPG_REQUIRE(test::near(layout.panel.x * 2.0F + layout.panel.width,
+            screen.width));
+        ARPG_REQUIRE(test::near(layout.panel.y * 2.0F + layout.panel.height,
+            screen.height));
+        ARPG_REQUIRE(inside(layout.footer, layout.panel));
+        for (std::size_t row = 0U; row < 16U; ++row) {
+            ARPG_REQUIRE(inside(layout.rows[row], layout.panel));
+            ARPG_REQUIRE(separated(layout.rows[row], layout.footer));
+            if (row != 0U) {
+                ARPG_REQUIRE(separated(layout.rows[row - 1U], layout.rows[row]));
+                ARPG_REQUIRE(layout.rows[row - 1U].y
+                    + layout.rows[row - 1U].height < layout.rows[row].y);
+            }
+        }
+        if (panel_width == 0.0F) {
+            panel_width = layout.panel.width;
+            panel_height = layout.panel.height;
+            row_height = layout.rows[0].height;
+        } else {
+            ARPG_REQUIRE(test::near(layout.panel.width, panel_width));
+            ARPG_REQUIRE(test::near(layout.panel.height, panel_height));
+            ARPG_REQUIRE(test::near(layout.rows[0].height, row_height));
+        }
+    }
+    return {};
+}
+
+test::Failure hit_test_uses_half_open_rows_only() noexcept {
+    const platform::PauseMenuLayout layout =
+        platform::pause_menu_layout(1280, 720);
+    for (std::size_t row = 0U; row < 16U; ++row) {
+        const Rectangle bounds = layout.rows[row];
+        const auto center = platform::hit_test_pause_row(layout,
+            {bounds.x + bounds.width * 0.5F,
+             bounds.y + bounds.height * 0.5F});
+        ARPG_REQUIRE(center.has_value());
+        ARPG_REQUIRE(*center == row);
+        const auto top_left = platform::hit_test_pause_row(
+            layout, {bounds.x, bounds.y});
+        ARPG_REQUIRE(top_left.has_value());
+        ARPG_REQUIRE(*top_left == row);
+        ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
+            {bounds.x + bounds.width,
+             bounds.y + bounds.height * 0.5F}).has_value());
+        ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
+            {bounds.x + bounds.width * 0.5F,
+             bounds.y + bounds.height}).has_value());
+    }
+    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
+        {layout.panel.x + 2.0F, layout.panel.y + 2.0F}).has_value());
+    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
+        {layout.rows[0].x,
+         layout.rows[0].y + layout.rows[0].height + 1.0F}).has_value());
+    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
+        {layout.footer.x + 2.0F, layout.footer.y + 2.0F}).has_value());
+    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
+        {layout.panel.x - 1.0F, layout.panel.y}).has_value());
+    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
+        {layout.panel.x + layout.panel.width,
+         layout.panel.y + layout.panel.height}).has_value());
+    return {};
+}
+
+test::Failure root_and_quit_views_have_complete_content() noexcept {
+    platform::PauseMenuState state{};
+    state.committed = settings::default_settings();
+    state.draft = state.committed;
+    state.screen = platform::PauseScreen::root;
+    state.selected_row = 1U;
+    state.message = "Settings recovered";
+    const auto root = platform::make_pause_menu_view(state);
+    ARPG_REQUIRE(std::strcmp(root.title, "PAUSED") == 0);
+    ARPG_REQUIRE(root.row_count == 3U);
+    ARPG_REQUIRE(root.selected_row == 1U);
+    ARPG_REQUIRE(std::strcmp(root.rows[0].data(), "Continue") == 0);
+    ARPG_REQUIRE(std::strcmp(root.rows[1].data(), "Settings") == 0);
+    ARPG_REQUIRE(std::strcmp(root.rows[2].data(), "Quit Game") == 0);
+    ARPG_REQUIRE(root.message == state.message);
+
+    state.screen = platform::PauseScreen::quit_confirm;
+    state.selected_row = 0U;
+    const auto quit = platform::make_pause_menu_view(state);
+    ARPG_REQUIRE(std::strcmp(quit.title, "QUIT GAME?") == 0);
+    ARPG_REQUIRE(quit.row_count == 2U);
+    ARPG_REQUIRE(quit.selected_row == 0U);
+    ARPG_REQUIRE(std::strcmp(quit.rows[0].data(), "Quit Game") == 0);
+    ARPG_REQUIRE(std::strcmp(quit.rows[1].data(), "Back") == 0);
+    return {};
+}
+
+test::Failure settings_view_shows_all_draft_and_committed_values() noexcept {
+    const platform::PauseMenuState state = settings_state();
+    const auto view = platform::make_pause_menu_view(state);
+    ARPG_REQUIRE(std::strcmp(view.title, "SETTINGS") == 0);
+    ARPG_REQUIRE(view.row_count == 16U);
+    ARPG_REQUIRE(view.selected_row == 12U);
+    ARPG_REQUIRE(view.message == state.message);
+    constexpr const char* kRows[] = {
+        "Master SFX: 55% (saved 100%)",
+        "Window Mode: Fullscreen (saved Windowed)",
+        "VSync: Off (saved On)",
+        "Move Up: 0 (saved W)",
+        "Move Down: 1 (saved S)",
+        "Move Left: 2 (saved A)",
+        "Move Right: 3 (saved D)",
+        "Light Attack: 4 (saved J)",
+        "Jump: 5 (saved K)",
+        "Launcher: 6 (saved L)",
+        "Interact: 7 (saved E)",
+        "Inventory: 8 (saved I)",
+        "Passive Tree: 9 (saved P)",
+        "Reset Defaults",
+        "Apply",
+        "Cancel",
+    };
+    for (std::size_t row = 0U; row < std::size(kRows); ++row) {
+        ARPG_REQUIRE(std::strcmp(view.rows[row].data(), kRows[row]) == 0);
+    }
+    return {};
+}
+
+test::Failure capture_and_closed_views_are_explicit() noexcept {
+    platform::PauseMenuState state = settings_state();
+    state.screen = platform::PauseScreen::capture_binding;
+    state.capture_action = settings::SettingAction::light_attack;
+    const auto capture = platform::make_pause_menu_view(state);
+    ARPG_REQUIRE(std::strcmp(capture.title, "BIND KEY") == 0);
+    ARPG_REQUIRE(capture.row_count == 1U);
+    ARPG_REQUIRE(capture.selected_row == 0U);
+    ARPG_REQUIRE(std::strcmp(capture.rows[0].data(),
+        "Press a key for Light Attack (current 4)") == 0);
+
+    state.capture_action.reset();
+    const auto missing = platform::make_pause_menu_view(state);
+    ARPG_REQUIRE(std::strcmp(missing.rows[0].data(),
+        "Press a key for Unknown (current Unknown)") == 0);
+
+    state.screen = platform::PauseScreen::closed;
+    const auto closed = platform::make_pause_menu_view(state);
+    ARPG_REQUIRE(closed.title == nullptr);
+    ARPG_REQUIRE(closed.row_count == 0U);
+    return {};
+}
+
+test::Failure row_buffers_terminate_and_generation_is_deterministic() noexcept {
+    const platform::PauseMenuState state = settings_state();
+    const auto first = platform::make_pause_menu_view(state);
+    const auto second = platform::make_pause_menu_view(state);
+    for (std::size_t row = 0U; row < first.rows.size(); ++row) {
+        ARPG_REQUIRE(first.rows[row][63U] == '\0');
+        ARPG_REQUIRE(std::memcmp(first.rows[row].data(),
+            second.rows[row].data(), first.rows[row].size()) == 0);
+    }
+    return {};
+}
+
+test::Failure all_view_layout_and_hit_paths_allocate_nothing() noexcept {
+    std::array<platform::PauseMenuState, 5U> states{};
+    states[0] = settings_state();
+    states[0].screen = platform::PauseScreen::closed;
+    states[1] = settings_state();
+    states[1].screen = platform::PauseScreen::root;
+    states[2] = settings_state();
+    states[3] = settings_state();
+    states[3].screen = platform::PauseScreen::capture_binding;
+    states[3].capture_action = settings::SettingAction::passive_tree;
+    states[4] = settings_state();
+    states[4].screen = platform::PauseScreen::quit_confirm;
+
+    const std::uint64_t before = test::allocation_count();
+    for (std::size_t iteration = 0U; iteration < 1000U; ++iteration) {
+        for (const auto& state : states) {
+            const auto view = platform::make_pause_menu_view(state);
+            ARPG_REQUIRE(view.row_count <= 16U);
+        }
+        constexpr std::array<std::array<int, 2>, 3> kSizes{{
+            {{1024, 576}}, {{1280, 720}}, {{1920, 1080}},
+        }};
+        for (const auto& size : kSizes) {
+            const auto layout = platform::pause_menu_layout(size[0], size[1]);
+            const auto hit = platform::hit_test_pause_row(layout,
+                {layout.rows[iteration % 16U].x,
+                 layout.rows[iteration % 16U].y});
+            ARPG_REQUIRE(hit.has_value());
+            ARPG_REQUIRE(*hit == iteration % 16U);
+            ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
+                {layout.footer.x, layout.footer.y}).has_value());
+        }
+    }
+    ARPG_REQUIRE(test::allocation_count() == before);
+    return {};
+}
+
+constexpr test::TestCase kCases[] = {
+    {"layouts are bounded and centered", &layouts_are_bounded_centered_and_fixed},
+    {"hit test is half open", &hit_test_uses_half_open_rows_only},
+    {"root and quit content", &root_and_quit_views_have_complete_content},
+    {"settings draft and committed content", &settings_view_shows_all_draft_and_committed_values},
+    {"capture and closed content", &capture_and_closed_views_are_explicit},
+    {"row buffers terminate deterministically", &row_buffers_terminate_and_generation_is_deterministic},
+    {"view layout hit have zero allocations", &all_view_layout_and_hit_paths_allocate_nothing},
+};
+
+}  // namespace
+
+arpg::test::TestSuite pause_menu_view_suite() noexcept {
+    return arpg::test::make_suite("pause_menu_view", kCases);
+}
diff --git a/tests/platform/platform_test_main.cpp b/tests/platform/platform_test_main.cpp
index 35c1a1e..31be26d 100644
--- a/tests/platform/platform_test_main.cpp
+++ b/tests/platform/platform_test_main.cpp
@@ -10,20 +10,21 @@ arpg::test::TestSuite monster_view_suite() noexcept;
 arpg::test::TestSuite combat_feedback_suite() noexcept;
 arpg::test::TestSuite host_input_suite() noexcept;
 arpg::test::TestSuite dungeon_view_math_suite() noexcept;
 arpg::test::TestSuite passive_tree_view_suite() noexcept;
 arpg::test::TestSuite host_launch_options_suite() noexcept;
 arpg::test::TestSuite dungeon_runtime_suite() noexcept;
 arpg::test::TestSuite inventory_view_math_suite() noexcept;
 arpg::test::TestSuite death_input_gate_suite() noexcept;
 arpg::test::TestSuite death_overlay_view_suite() noexcept;
 arpg::test::TestSuite pause_menu_state_suite() noexcept;
+arpg::test::TestSuite pause_menu_view_suite() noexcept;
 
 int main() {
 #if defined(_WIN32) && defined(_DEBUG)
     _set_error_mode(_OUT_TO_STDERR);
     _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
     _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
     _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
     _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
     _set_abort_behavior(_WRITE_ABORT_MSG,
         _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
@@ -34,14 +35,15 @@ int main() {
         combat_feedback_suite(),
         host_input_suite(),
         dungeon_view_math_suite(),
         passive_tree_view_suite(),
         host_launch_options_suite(),
         dungeon_runtime_suite(),
         inventory_view_math_suite(),
         death_input_gate_suite(),
         death_overlay_view_suite(),
         pause_menu_state_suite(),
+        pause_menu_view_suite(),
     };
 
-    return arpg::test::run_suites(suites, 143, "stage 11b task 5 pause state");
+    return arpg::test::run_suites(suites, 150, "stage 11b task 6 pause view");
 }
