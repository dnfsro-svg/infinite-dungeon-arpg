# Review package: 3c7eb3b..38db8aa

## Commits
38db8aa fix: clamp and verify pause overlay rendering

## Files changed
 src/platform/raylib/pause_menu_renderer.cpp | 125 ++++++++++++++++-------
 src/platform/raylib/pause_menu_renderer.hpp |  32 +++++-
 src/platform/raylib/pause_menu_view.cpp     |  15 ++-
 tests/platform/pause_menu_view_tests.cpp    | 151 ++++++++++++++++++++++++++++
 tests/platform/platform_test_main.cpp       |   2 +-
 5 files changed, 281 insertions(+), 44 deletions(-)

## Diff
diff --git a/src/platform/raylib/pause_menu_renderer.cpp b/src/platform/raylib/pause_menu_renderer.cpp
index 9982ba0..be48863 100644
--- a/src/platform/raylib/pause_menu_renderer.cpp
+++ b/src/platform/raylib/pause_menu_renderer.cpp
@@ -17,68 +17,115 @@ void draw_centered_text(
     const int text_width = MeasureText(text, font_size);
     const int x = static_cast<int>(
         bounds.x + (bounds.width - static_cast<float>(text_width)) * 0.5F);
     const int y = static_cast<int>(
         bounds.y + (bounds.height - static_cast<float>(font_size)) * 0.5F);
     DrawText(text, x, y, font_size, color);
 }
 
 }  // namespace
 
+PauseMenuRenderPlan make_pause_menu_render_plan(
+    const PauseMenuView& view) noexcept {
+    PauseMenuRenderPlan plan{};
+    if (view.row_count == 0U || view.title == nullptr) return plan;
+
+    const auto push = [&plan](
+        PauseMenuRenderOpKind kind,
+        std::size_t row_index = 0U,
+        bool selected = false) noexcept {
+        if (plan.op_count >= plan.ops.size()) return;
+        plan.ops[plan.op_count++] = {kind, row_index, selected};
+    };
+
+    push(PauseMenuRenderOpKind::dim);
+    push(PauseMenuRenderOpKind::panel);
+    push(PauseMenuRenderOpKind::title);
+    const std::size_t row_count = view.row_count < kPauseMenuRowCapacity
+        ? view.row_count
+        : kPauseMenuRowCapacity;
+    for (std::size_t row = 0U; row < row_count; ++row) {
+        push(PauseMenuRenderOpKind::row, row, row == view.selected_row);
+    }
+    plan.has_message = view.message != nullptr && view.message[0] != '\0';
+    if (plan.has_message) push(PauseMenuRenderOpKind::message);
+    push(PauseMenuRenderOpKind::footer);
+    return plan;
+}
+
 void draw_pause_menu(const PauseMenuState& state) noexcept {
     const PauseMenuView view = make_pause_menu_view(state);
-    if (view.row_count == 0U || view.title == nullptr) return;
+    const PauseMenuRenderPlan plan = make_pause_menu_render_plan(view);
+    if (plan.op_count == 0U) return;
 
     const int screen_width = GetScreenWidth();
     const int screen_height = GetScreenHeight();
     const PauseMenuLayout layout = pause_menu_layout(
         screen_width, screen_height);
 
-    DrawRectangle(0, 0, screen_width, screen_height,
-        Color{2, 4, 8, 190});
-    DrawRectangleRounded(layout.panel, 0.04F, 8,
-        Color{10, 14, 23, 248});
-    DrawRectangleRoundedLinesEx(layout.panel, 0.04F, 8, 2.0F,
-        Color{94, 159, 206, 255});
-
     const Rectangle title_bounds{
         layout.panel.x + 24.0F,
         layout.panel.y + 14.0F,
         layout.panel.width - 48.0F,
         32.0F,
     };
-    draw_centered_text(view.title, title_bounds, 28,
-        Color{191, 225, 255, 255});
-
-    for (std::size_t row = 0U; row < view.row_count; ++row) {
-        const Rectangle bounds = layout.rows[row];
-        if (row == view.selected_row) {
-            DrawRectangleRounded(bounds, 0.18F, 5,
-                Color{42, 91, 126, 235});
-            DrawRectangleRoundedLinesEx(bounds, 0.18F, 5, 1.0F,
-                Color{121, 197, 244, 255});
+    for (std::size_t index = 0U; index < plan.op_count; ++index) {
+        const PauseMenuRenderOp op = plan.ops[index];
+        switch (op.kind) {
+            case PauseMenuRenderOpKind::dim:
+                DrawRectangle(0, 0, screen_width, screen_height,
+                    Color{2, 4, 8, 190});
+                break;
+            case PauseMenuRenderOpKind::panel:
+                DrawRectangleRounded(layout.panel, 0.04F, 8,
+                    Color{10, 14, 23, 248});
+                DrawRectangleRoundedLinesEx(
+                    layout.panel, 0.04F, 8, 2.0F,
+                    Color{94, 159, 206, 255});
+                break;
+            case PauseMenuRenderOpKind::title:
+                draw_centered_text(view.title, title_bounds, 28,
+                    Color{191, 225, 255, 255});
+                break;
+            case PauseMenuRenderOpKind::row: {
+                const Rectangle bounds = layout.rows[op.row_index];
+                if (op.selected) {
+                    DrawRectangleRounded(bounds, 0.18F, 5,
+                        Color{42, 91, 126, 235});
+                    DrawRectangleRoundedLinesEx(
+                        bounds, 0.18F, 5, 1.0F,
+                        Color{121, 197, 244, 255});
+                }
+                DrawText(view.rows[op.row_index].data(),
+                    static_cast<int>(bounds.x + 10.0F),
+                    static_cast<int>(bounds.y + 3.0F),
+                    16,
+                    op.selected
+                        ? Color{244, 249, 255, 255}
+                        : Color{207, 218, 231, 255});
+                break;
+            }
+            case PauseMenuRenderOpKind::message:
+                draw_centered_text(view.message, layout.footer, 16,
+                    Color{255, 139, 139, 255});
+                break;
+            case PauseMenuRenderOpKind::footer:
+                DrawLine(
+                    static_cast<int>(layout.footer.x),
+                    static_cast<int>(layout.footer.y - 5.0F),
+                    static_cast<int>(layout.footer.x
+                        + layout.footer.width),
+                    static_cast<int>(layout.footer.y - 5.0F),
+                    Color{75, 86, 104, 220});
+                if (!plan.has_message) {
+                    draw_centered_text(
+                        "Arrow keys navigate | Enter select | Esc back",
+                        layout.footer,
+                        16,
+                        Color{165, 178, 196, 255});
+                }
+                break;
         }
-        DrawText(view.rows[row].data(),
-            static_cast<int>(bounds.x + 10.0F),
-            static_cast<int>(bounds.y + 3.0F),
-            16,
-            row == view.selected_row
-                ? Color{244, 249, 255, 255}
-                : Color{207, 218, 231, 255});
     }
-
-    DrawLine(
-        static_cast<int>(layout.footer.x),
-        static_cast<int>(layout.footer.y - 5.0F),
-        static_cast<int>(layout.footer.x + layout.footer.width),
-        static_cast<int>(layout.footer.y - 5.0F),
-        Color{75, 86, 104, 220});
-    const bool has_message = view.message != nullptr && view.message[0] != '\0';
-    draw_centered_text(
-        has_message ? view.message : "Arrow keys navigate | Enter select | Esc back",
-        layout.footer,
-        16,
-        has_message ? Color{255, 139, 139, 255}
-                    : Color{165, 178, 196, 255});
 }
 
 }  // namespace arpg::platform
diff --git a/src/platform/raylib/pause_menu_renderer.hpp b/src/platform/raylib/pause_menu_renderer.hpp
index fcd77f8..cb6457d 100644
--- a/src/platform/raylib/pause_menu_renderer.hpp
+++ b/src/platform/raylib/pause_menu_renderer.hpp
@@ -1,9 +1,39 @@
 #pragma once
 
-#include "pause_menu_state.hpp"
+#include "pause_menu_view.hpp"
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
 
 namespace arpg::platform {
 
+inline constexpr std::size_t kPauseMenuRenderOpCapacity = 21U;
+
+enum class PauseMenuRenderOpKind : std::uint8_t {
+    dim,
+    panel,
+    title,
+    row,
+    message,
+    footer,
+};
+
+struct PauseMenuRenderOp final {
+    PauseMenuRenderOpKind kind{};
+    std::size_t row_index{};
+    bool selected{};
+};
+
+struct PauseMenuRenderPlan final {
+    std::array<PauseMenuRenderOp, kPauseMenuRenderOpCapacity> ops{};
+    std::size_t op_count{};
+    bool has_message{};
+};
+
+[[nodiscard]] PauseMenuRenderPlan make_pause_menu_render_plan(
+    const PauseMenuView& view) noexcept;
+
 void draw_pause_menu(const PauseMenuState& state) noexcept;
 
 }  // namespace arpg::platform
diff --git a/src/platform/raylib/pause_menu_view.cpp b/src/platform/raylib/pause_menu_view.cpp
index 5ba1c84..9413606 100644
--- a/src/platform/raylib/pause_menu_view.cpp
+++ b/src/platform/raylib/pause_menu_view.cpp
@@ -37,20 +37,26 @@ void write_row(
         case settings::WindowMode::fullscreen:
             return "Fullscreen";
     }
     return "Unknown";
 }
 
 [[nodiscard]] const char* enabled_label(bool enabled) noexcept {
     return enabled ? "On" : "Off";
 }
 
+[[nodiscard]] std::size_t clamp_selected_row(
+    std::size_t selected_row,
+    std::size_t row_count) noexcept {
+    return selected_row < row_count ? selected_row : row_count - 1U;
+}
+
 void build_settings_rows(
     PauseMenuView& view,
     const PauseMenuState& state) noexcept {
     write_row(view, 0U, "Master SFX: %u%% (saved %u%%)",
         static_cast<unsigned>(state.draft.master_sfx_percent),
         static_cast<unsigned>(state.committed.master_sfx_percent));
     write_row(view, 1U, "Window Mode: %s (saved %s)",
         window_mode_label(state.draft.window_mode),
         window_mode_label(state.committed.window_mode));
     write_row(view, 2U, "VSync: %s (saved %s)",
@@ -103,46 +109,49 @@ PauseMenuLayout pause_menu_layout(int width, int height) noexcept {
 
 PauseMenuView make_pause_menu_view(const PauseMenuState& state) noexcept {
     PauseMenuView view{};
     view.message = state.message;
     switch (state.screen) {
         case PauseScreen::closed:
             return view;
         case PauseScreen::root:
             view.title = "PAUSED";
             view.row_count = 3U;
-            view.selected_row = state.selected_row;
+            view.selected_row = clamp_selected_row(
+                state.selected_row, view.row_count);
             write_row(view, 0U, "%s", "Continue");
             write_row(view, 1U, "%s", "Settings");
             write_row(view, 2U, "%s", "Quit Game");
             return view;
         case PauseScreen::settings:
             view.title = "SETTINGS";
             view.row_count = kPauseMenuRowCapacity;
-            view.selected_row = state.selected_row;
+            view.selected_row = clamp_selected_row(
+                state.selected_row, view.row_count);
             build_settings_rows(view, state);
             return view;
         case PauseScreen::capture_binding: {
             view.title = "BIND KEY";
             view.row_count = 1U;
             view.selected_row = 0U;
             const settings::SettingAction action = state.capture_action.value_or(
                 settings::SettingAction::count);
             write_row(view, 0U, "Press a key for %s (current %s)",
                 settings::action_label(action),
                 stable_key_label(settings::binding_for(state.draft, action)));
             return view;
         }
         case PauseScreen::quit_confirm:
             view.title = "QUIT GAME?";
             view.row_count = 2U;
-            view.selected_row = state.selected_row;
+            view.selected_row = clamp_selected_row(
+                state.selected_row, view.row_count);
             write_row(view, 0U, "%s", "Quit Game");
             write_row(view, 1U, "%s", "Back");
             return view;
     }
     return view;
 }
 
 std::optional<std::size_t> hit_test_pause_row(
     const PauseMenuLayout& layout,
     Vector2 point) noexcept {
diff --git a/tests/platform/pause_menu_view_tests.cpp b/tests/platform/pause_menu_view_tests.cpp
index 82835ba..e63dfc0 100644
--- a/tests/platform/pause_menu_view_tests.cpp
+++ b/tests/platform/pause_menu_view_tests.cpp
@@ -1,18 +1,20 @@
 #include "allocation_probe.hpp"
 #include "test_framework.hpp"
 
+#include "pause_menu_renderer.hpp"
 #include "pause_menu_view.hpp"
 
 #include <array>
 #include <cstddef>
 #include <cstring>
+#include <limits>
 
 namespace {
 
 using namespace arpg;
 
 bool inside(Rectangle inner, Rectangle outer) noexcept {
     return inner.x >= outer.x && inner.y >= outer.y
         && inner.x + inner.width <= outer.x + outer.width
         && inner.y + inner.height <= outer.y + outer.height;
 }
@@ -68,42 +70,91 @@ test::Failure layouts_are_bounded_centered_and_fixed() noexcept {
         ARPG_REQUIRE(layout.panel.x + layout.panel.width
             <= screen.width - 32.0F);
         ARPG_REQUIRE(layout.panel.y >= 16.0F);
         ARPG_REQUIRE(layout.panel.y + layout.panel.height
             <= screen.height - 16.0F);
         ARPG_REQUIRE(test::near(layout.panel.x * 2.0F + layout.panel.width,
             screen.width));
         ARPG_REQUIRE(test::near(layout.panel.y * 2.0F + layout.panel.height,
             screen.height));
         ARPG_REQUIRE(inside(layout.footer, layout.panel));
+        ARPG_REQUIRE(test::near(
+            layout.footer.x - layout.panel.x, 24.0F));
+        ARPG_REQUIRE(test::near(
+            layout.footer.y - layout.panel.y, 496.0F));
+        ARPG_REQUIRE(test::near(layout.footer.width, 712.0F));
+        ARPG_REQUIRE(test::near(layout.footer.height, 28.0F));
         for (std::size_t row = 0U; row < 16U; ++row) {
             ARPG_REQUIRE(inside(layout.rows[row], layout.panel));
             ARPG_REQUIRE(separated(layout.rows[row], layout.footer));
+            ARPG_REQUIRE(test::near(
+                layout.rows[row].x - layout.panel.x, 24.0F));
+            ARPG_REQUIRE(test::near(
+                layout.rows[row].y - layout.panel.y,
+                60.0F + static_cast<float>(row) * 27.0F));
+            ARPG_REQUIRE(test::near(layout.rows[row].width, 712.0F));
+            ARPG_REQUIRE(test::near(layout.rows[row].height, 22.0F));
             if (row != 0U) {
                 ARPG_REQUIRE(separated(layout.rows[row - 1U], layout.rows[row]));
                 ARPG_REQUIRE(layout.rows[row - 1U].y
                     + layout.rows[row - 1U].height < layout.rows[row].y);
             }
         }
         if (panel_width == 0.0F) {
             panel_width = layout.panel.width;
             panel_height = layout.panel.height;
             row_height = layout.rows[0].height;
         } else {
             ARPG_REQUIRE(test::near(layout.panel.width, panel_width));
             ARPG_REQUIRE(test::near(layout.panel.height, panel_height));
             ARPG_REQUIRE(test::near(layout.rows[0].height, row_height));
         }
     }
     return {};
 }
 
+test::Failure layout_has_exact_1024_golden_geometry() noexcept {
+    const platform::PauseMenuLayout layout =
+        platform::pause_menu_layout(1024, 576);
+    ARPG_REQUIRE(test::near(layout.panel.x, 132.0F));
+    ARPG_REQUIRE(test::near(layout.panel.y, 16.0F));
+    ARPG_REQUIRE(test::near(layout.panel.width, 760.0F));
+    ARPG_REQUIRE(test::near(layout.panel.height, 544.0F));
+
+    ARPG_REQUIRE(test::near(layout.rows[0].x, 156.0F));
+    ARPG_REQUIRE(test::near(layout.rows[0].y, 76.0F));
+    ARPG_REQUIRE(test::near(layout.rows[0].width, 712.0F));
+    ARPG_REQUIRE(test::near(layout.rows[0].height, 22.0F));
+    ARPG_REQUIRE(test::near(
+        layout.rows[0].y - layout.panel.y, 60.0F));
+
+    ARPG_REQUIRE(test::near(layout.rows[15].x, 156.0F));
+    ARPG_REQUIRE(test::near(layout.rows[15].y, 481.0F));
+    ARPG_REQUIRE(test::near(layout.rows[15].width, 712.0F));
+    ARPG_REQUIRE(test::near(layout.rows[15].height, 22.0F));
+    for (std::size_t row = 1U; row < 16U; ++row) {
+        ARPG_REQUIRE(test::near(
+            layout.rows[row].y - layout.rows[row - 1U].y, 27.0F));
+        ARPG_REQUIRE(test::near(
+            layout.rows[row].y
+                - (layout.rows[row - 1U].y
+                    + layout.rows[row - 1U].height),
+            5.0F));
+    }
+
+    ARPG_REQUIRE(test::near(layout.footer.x, 156.0F));
+    ARPG_REQUIRE(test::near(layout.footer.y, 512.0F));
+    ARPG_REQUIRE(test::near(layout.footer.width, 712.0F));
+    ARPG_REQUIRE(test::near(layout.footer.height, 28.0F));
+    return {};
+}
+
 test::Failure hit_test_uses_half_open_rows_only() noexcept {
     const platform::PauseMenuLayout layout =
         platform::pause_menu_layout(1280, 720);
     for (std::size_t row = 0U; row < 16U; ++row) {
         const Rectangle bounds = layout.rows[row];
         const auto center = platform::hit_test_pause_row(layout,
             {bounds.x + bounds.width * 0.5F,
              bounds.y + bounds.height * 0.5F});
         ARPG_REQUIRE(center.has_value());
         ARPG_REQUIRE(*center == row);
@@ -207,50 +258,146 @@ test::Failure capture_and_closed_views_are_explicit() noexcept {
     ARPG_REQUIRE(std::strcmp(missing.rows[0].data(),
         "Press a key for Unknown (current Unknown)") == 0);
 
     state.screen = platform::PauseScreen::closed;
     const auto closed = platform::make_pause_menu_view(state);
     ARPG_REQUIRE(closed.title == nullptr);
     ARPG_REQUIRE(closed.row_count == 0U);
     return {};
 }
 
+test::Failure selected_rows_clamp_to_each_visible_view() noexcept {
+    platform::PauseMenuState state = settings_state();
+    state.selected_row = (std::numeric_limits<std::size_t>::max)();
+
+    state.screen = platform::PauseScreen::root;
+    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 2U);
+
+    state.screen = platform::PauseScreen::settings;
+    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 15U);
+
+    state.screen = platform::PauseScreen::quit_confirm;
+    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 1U);
+
+    state.screen = platform::PauseScreen::capture_binding;
+    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 0U);
+
+    state.screen = platform::PauseScreen::closed;
+    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 0U);
+    return {};
+}
+
+test::Failure render_plans_have_exact_screen_orders() noexcept {
+    platform::PauseMenuState state = settings_state();
+    state.message = nullptr;
+    constexpr platform::PauseScreen kScreens[] = {
+        platform::PauseScreen::root,
+        platform::PauseScreen::settings,
+        platform::PauseScreen::capture_binding,
+        platform::PauseScreen::quit_confirm,
+    };
+    for (const platform::PauseScreen screen : kScreens) {
+        state.screen = screen;
+        state.selected_row = (std::numeric_limits<std::size_t>::max)();
+        state.capture_action = settings::SettingAction::light_attack;
+        const platform::PauseMenuView view =
+            platform::make_pause_menu_view(state);
+        const platform::PauseMenuRenderPlan plan =
+            platform::make_pause_menu_render_plan(view);
+        ARPG_REQUIRE(plan.op_count == view.row_count + 4U);
+        ARPG_REQUIRE(plan.ops[0].kind
+            == platform::PauseMenuRenderOpKind::dim);
+        ARPG_REQUIRE(plan.ops[1].kind
+            == platform::PauseMenuRenderOpKind::panel);
+        ARPG_REQUIRE(plan.ops[2].kind
+            == platform::PauseMenuRenderOpKind::title);
+        std::size_t selected_count = 0U;
+        for (std::size_t row = 0U; row < view.row_count; ++row) {
+            const auto& op = plan.ops[row + 3U];
+            ARPG_REQUIRE(op.kind == platform::PauseMenuRenderOpKind::row);
+            ARPG_REQUIRE(op.row_index == row);
+            ARPG_REQUIRE(op.selected == (row == view.selected_row));
+            if (op.selected) ++selected_count;
+        }
+        ARPG_REQUIRE(selected_count == 1U);
+        ARPG_REQUIRE(plan.ops[plan.op_count - 1U].kind
+            == platform::PauseMenuRenderOpKind::footer);
+        ARPG_REQUIRE(!plan.has_message);
+    }
+
+    state.screen = platform::PauseScreen::closed;
+    const auto closed = platform::make_pause_menu_render_plan(
+        platform::make_pause_menu_view(state));
+    ARPG_REQUIRE(closed.op_count == 0U);
+    ARPG_REQUIRE(!closed.has_message);
+    return {};
+}
+
+test::Failure render_plan_inserts_optional_message_before_footer() noexcept {
+    platform::PauseMenuState state = settings_state();
+    const platform::PauseMenuView view =
+        platform::make_pause_menu_view(state);
+    const platform::PauseMenuRenderPlan message_plan =
+        platform::make_pause_menu_render_plan(view);
+    ARPG_REQUIRE(message_plan.has_message);
+    ARPG_REQUIRE(message_plan.op_count == view.row_count + 5U);
+    ARPG_REQUIRE(message_plan.ops[message_plan.op_count - 2U].kind
+        == platform::PauseMenuRenderOpKind::message);
+    ARPG_REQUIRE(message_plan.ops[message_plan.op_count - 1U].kind
+        == platform::PauseMenuRenderOpKind::footer);
+
+    state.message = "";
+    const platform::PauseMenuView empty_view =
+        platform::make_pause_menu_view(state);
+    const platform::PauseMenuRenderPlan empty_plan =
+        platform::make_pause_menu_render_plan(empty_view);
+    ARPG_REQUIRE(!empty_plan.has_message);
+    ARPG_REQUIRE(empty_plan.op_count == empty_view.row_count + 4U);
+    ARPG_REQUIRE(empty_plan.ops[empty_plan.op_count - 1U].kind
+        == platform::PauseMenuRenderOpKind::footer);
+    return {};
+}
+
 test::Failure row_buffers_terminate_and_generation_is_deterministic() noexcept {
     const platform::PauseMenuState state = settings_state();
     const auto first = platform::make_pause_menu_view(state);
     const auto second = platform::make_pause_menu_view(state);
     for (std::size_t row = 0U; row < first.rows.size(); ++row) {
         ARPG_REQUIRE(first.rows[row][63U] == '\0');
         ARPG_REQUIRE(std::memcmp(first.rows[row].data(),
             second.rows[row].data(), first.rows[row].size()) == 0);
     }
     return {};
 }
 
 test::Failure all_view_layout_and_hit_paths_allocate_nothing() noexcept {
     std::array<platform::PauseMenuState, 5U> states{};
     states[0] = settings_state();
     states[0].screen = platform::PauseScreen::closed;
     states[1] = settings_state();
     states[1].screen = platform::PauseScreen::root;
+    states[1].message = nullptr;
     states[2] = settings_state();
     states[3] = settings_state();
     states[3].screen = platform::PauseScreen::capture_binding;
     states[3].capture_action = settings::SettingAction::passive_tree;
+    states[3].message = "";
     states[4] = settings_state();
     states[4].screen = platform::PauseScreen::quit_confirm;
 
     const std::uint64_t before = test::allocation_count();
     for (std::size_t iteration = 0U; iteration < 1000U; ++iteration) {
         for (const auto& state : states) {
             const auto view = platform::make_pause_menu_view(state);
             ARPG_REQUIRE(view.row_count <= 16U);
+            const auto plan = platform::make_pause_menu_render_plan(view);
+            ARPG_REQUIRE(plan.op_count <= 21U);
         }
         constexpr std::array<std::array<int, 2>, 3> kSizes{{
             {{1024, 576}}, {{1280, 720}}, {{1920, 1080}},
         }};
         for (const auto& size : kSizes) {
             const auto layout = platform::pause_menu_layout(size[0], size[1]);
             const auto hit = platform::hit_test_pause_row(layout,
                 {layout.rows[iteration % 16U].x,
                  layout.rows[iteration % 16U].y});
             ARPG_REQUIRE(hit.has_value());
@@ -258,23 +405,27 @@ test::Failure all_view_layout_and_hit_paths_allocate_nothing() noexcept {
             ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
                 {layout.footer.x, layout.footer.y}).has_value());
         }
     }
     ARPG_REQUIRE(test::allocation_count() == before);
     return {};
 }
 
 constexpr test::TestCase kCases[] = {
     {"layouts are bounded and centered", &layouts_are_bounded_centered_and_fixed},
+    {"layout has exact 1024 golden geometry", &layout_has_exact_1024_golden_geometry},
     {"hit test is half open", &hit_test_uses_half_open_rows_only},
     {"root and quit content", &root_and_quit_views_have_complete_content},
     {"settings draft and committed content", &settings_view_shows_all_draft_and_committed_values},
     {"capture and closed content", &capture_and_closed_views_are_explicit},
+    {"selected rows clamp to visible views", &selected_rows_clamp_to_each_visible_view},
+    {"render plans have exact screen orders", &render_plans_have_exact_screen_orders},
+    {"render plan inserts optional message", &render_plan_inserts_optional_message_before_footer},
     {"row buffers terminate deterministically", &row_buffers_terminate_and_generation_is_deterministic},
     {"view layout hit have zero allocations", &all_view_layout_and_hit_paths_allocate_nothing},
 };
 
 }  // namespace
 
 arpg::test::TestSuite pause_menu_view_suite() noexcept {
     return arpg::test::make_suite("pause_menu_view", kCases);
 }
diff --git a/tests/platform/platform_test_main.cpp b/tests/platform/platform_test_main.cpp
index 31be26d..8b767ae 100644
--- a/tests/platform/platform_test_main.cpp
+++ b/tests/platform/platform_test_main.cpp
@@ -38,12 +38,12 @@ int main() {
         passive_tree_view_suite(),
         host_launch_options_suite(),
         dungeon_runtime_suite(),
         inventory_view_math_suite(),
         death_input_gate_suite(),
         death_overlay_view_suite(),
         pause_menu_state_suite(),
         pause_menu_view_suite(),
     };
 
-    return arpg::test::run_suites(suites, 150, "stage 11b task 6 pause view");
+    return arpg::test::run_suites(suites, 154, "stage 11b task 6 pause view");
 }
