#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "pause_menu_renderer.hpp"
#include "pause_menu_view.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>

namespace {

using namespace arpg;

bool inside(Rectangle inner, Rectangle outer) noexcept {
    return inner.x >= outer.x && inner.y >= outer.y
        && inner.x + inner.width <= outer.x + outer.width
        && inner.y + inner.height <= outer.y + outer.height;
}

bool separated(Rectangle first, Rectangle second) noexcept {
    return first.x + first.width <= second.x
        || second.x + second.width <= first.x
        || first.y + first.height <= second.y
        || second.y + second.height <= first.y;
}

platform::PauseMenuState settings_state() noexcept {
    platform::PauseMenuState state{};
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 12U;
    state.committed = settings::default_settings();
    state.draft = state.committed;
    state.draft.master_sfx_percent = 55U;
    state.draft.sfx_percent = 60U;
    state.draft.music_percent = 35U;
    state.draft.ambience_percent = 25U;
    state.draft.ui_percent = 70U;
    state.draft.window_mode = settings::WindowMode::fullscreen;
    state.draft.vsync_enabled = false;
    state.draft.loot_filter_mode = settings::LootFilterMode::magic_or_better;
    state.draft.bindings = {
        settings::StableKey::digit_0,
        settings::StableKey::digit_1,
        settings::StableKey::digit_2,
        settings::StableKey::digit_3,
        settings::StableKey::digit_4,
        settings::StableKey::digit_5,
        settings::StableKey::digit_6,
        settings::StableKey::digit_7,
        settings::StableKey::digit_8,
        settings::StableKey::digit_9,
    };
    state.message = "Apply failed; retry";
    return state;
}

test::Failure layouts_are_bounded_centered_and_fixed() noexcept {
    constexpr std::array<std::array<int, 2>, 3> kSizes{{
        {{1024, 576}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto& size : kSizes) {
        const Rectangle screen{
            0.0F, 0.0F,
            static_cast<float>(size[0]), static_cast<float>(size[1])};
        const platform::PauseMenuLayout layout =
            platform::pause_menu_layout(size[0], size[1]);
        ARPG_REQUIRE(inside(layout.panel, screen));
        ARPG_REQUIRE(layout.panel.width <= 1140.0F);
        ARPG_REQUIRE(layout.panel.x >= 32.0F);
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
        ARPG_REQUIRE(inside(layout.title, layout.panel));
        ARPG_REQUIRE(separated(layout.title, layout.rows[0]));
        const float scale = layout.panel.width / 760.0F;
        ARPG_REQUIRE(test::near(
            layout.panel.height, 620.0F * scale, 0.01));
        ARPG_REQUIRE(test::near(
            layout.footer.x - layout.panel.x, 24.0F * scale, 0.01));
        ARPG_REQUIRE(test::near(
            layout.footer.y - layout.panel.y, 580.0F * scale, 0.01));
        ARPG_REQUIRE(test::near(
            layout.footer.width, 712.0F * scale, 0.01));
        ARPG_REQUIRE(test::near(
            layout.footer.height, 28.0F * scale, 0.01));
        for (std::size_t row = 0U; row < 21U; ++row) {
            ARPG_REQUIRE(inside(layout.rows[row], layout.panel));
            ARPG_REQUIRE(separated(layout.rows[row], layout.footer));
            ARPG_REQUIRE(test::near(
                layout.rows[row].x - layout.panel.x, 24.0F * scale, 0.01));
            ARPG_REQUIRE(test::near(
                layout.rows[row].y - layout.panel.y,
                (62.0F + static_cast<float>(row) * 24.0F) * scale,
                0.01));
            ARPG_REQUIRE(test::near(
                layout.rows[row].width, 712.0F * scale, 0.01));
            ARPG_REQUIRE(test::near(
                layout.rows[row].height, 23.0F * scale, 0.01));
            if (row != 0U) {
                ARPG_REQUIRE(separated(layout.rows[row - 1U], layout.rows[row]));
                ARPG_REQUIRE(layout.rows[row - 1U].y
                    + layout.rows[row - 1U].height < layout.rows[row].y);
            }
        }
    }
    return {};
}

test::Failure full_hd_layout_scales_panel_and_rows_by_one_and_a_half() noexcept {
    const platform::PauseMenuLayout layout =
        platform::pause_menu_layout(1920, 1080);
    ARPG_REQUIRE(test::near(layout.panel.width, 1140.0F, 0.01));
    ARPG_REQUIRE(test::near(layout.panel.height, 930.0F, 0.01));
    ARPG_REQUIRE(test::near(layout.rows[0].height, 34.5F, 0.01));
    ARPG_REQUIRE(test::near(layout.rows[1].y - layout.rows[0].y,
        36.0F, 0.01));
    return {};
}

test::Failure layout_has_exact_1024_golden_geometry() noexcept {
    const platform::PauseMenuLayout layout =
        platform::pause_menu_layout(1024, 576);
    constexpr float kScale = 544.0F / 620.0F;
    ARPG_REQUIRE(test::near(layout.panel.x,
        (1024.0F - 760.0F * kScale) * 0.5F, 0.01));
    ARPG_REQUIRE(test::near(layout.panel.y, 16.0F, 0.01));
    ARPG_REQUIRE(test::near(layout.panel.width, 760.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.panel.height, 620.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.title.x,
        layout.panel.x + 24.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.title.y,
        layout.panel.y + 20.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.title.width, 712.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.title.height, 30.0F * kScale, 0.01));

    ARPG_REQUIRE(test::near(layout.rows[0].x,
        layout.panel.x + 24.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.rows[0].y,
        layout.panel.y + 62.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.rows[0].width, 712.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.rows[0].height, 23.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(
        layout.rows[0].y - layout.panel.y, 62.0F * kScale, 0.01));

    ARPG_REQUIRE(test::near(layout.rows[20].x,
        layout.panel.x + 24.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.rows[20].y,
        layout.panel.y + (62.0F + 20.0F * 24.0F) * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.rows[20].width, 712.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.rows[20].height, 23.0F * kScale, 0.01));
    for (std::size_t row = 1U; row < 21U; ++row) {
        ARPG_REQUIRE(test::near(
            layout.rows[row].y - layout.rows[row - 1U].y,
            24.0F * kScale, 0.01));
        ARPG_REQUIRE(test::near(
            layout.rows[row].y
                - (layout.rows[row - 1U].y
                    + layout.rows[row - 1U].height),
            1.0F * kScale, 0.01));
    }

    ARPG_REQUIRE(test::near(layout.footer.x,
        layout.panel.x + 24.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.footer.y,
        layout.panel.y + 580.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.footer.width, 712.0F * kScale, 0.01));
    ARPG_REQUIRE(test::near(layout.footer.height, 28.0F * kScale, 0.01));
    return {};
}

test::Failure hit_test_uses_half_open_rows_only() noexcept {
    const platform::PauseMenuLayout layout =
        platform::pause_menu_layout(1280, 720);
    for (std::size_t row = 0U; row < 21U; ++row) {
        const Rectangle bounds = layout.rows[row];
        const auto center = platform::hit_test_pause_row(layout,
            {bounds.x + bounds.width * 0.5F,
             bounds.y + bounds.height * 0.5F});
        ARPG_REQUIRE(center.has_value());
        ARPG_REQUIRE(*center == row);
        const auto top_left = platform::hit_test_pause_row(
            layout, {bounds.x, bounds.y});
        ARPG_REQUIRE(top_left.has_value());
        ARPG_REQUIRE(*top_left == row);
        ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
            {bounds.x + bounds.width,
             bounds.y + bounds.height * 0.5F}).has_value());
        ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
            {bounds.x + bounds.width * 0.5F,
             bounds.y + bounds.height}).has_value());
    }
    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
        {layout.panel.x + 2.0F, layout.panel.y + 2.0F}).has_value());
    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
        {layout.rows[0].x,
         layout.rows[0].y + layout.rows[0].height + 0.5F}).has_value());
    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
        {layout.footer.x + 2.0F, layout.footer.y + 2.0F}).has_value());
    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
        {layout.panel.x - 1.0F, layout.panel.y}).has_value());
    ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
        {layout.panel.x + layout.panel.width,
         layout.panel.y + layout.panel.height}).has_value());
    return {};
}

test::Failure root_and_quit_views_have_complete_content() noexcept {
    platform::PauseMenuState state{};
    state.committed = settings::default_settings();
    state.draft = state.committed;
    state.screen = platform::PauseScreen::root;
    state.selected_row = 1U;
    state.message = "Settings recovered";
    const auto root = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(root.title, "PAUSED") == 0);
    ARPG_REQUIRE(root.row_count == 3U);
    ARPG_REQUIRE(root.selected_row == 1U);
    ARPG_REQUIRE(std::strcmp(root.rows[0].data(), "Continue") == 0);
    ARPG_REQUIRE(std::strcmp(root.rows[1].data(), "Settings") == 0);
    ARPG_REQUIRE(std::strcmp(root.rows[2].data(), "Quit Game") == 0);
    ARPG_REQUIRE(root.message == state.message);

    state.screen = platform::PauseScreen::quit_confirm;
    state.selected_row = 0U;
    const auto quit = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(quit.title, "QUIT GAME?") == 0);
    ARPG_REQUIRE(quit.row_count == 2U);
    ARPG_REQUIRE(quit.selected_row == 0U);
    ARPG_REQUIRE(std::strcmp(quit.rows[0].data(), "Quit Game") == 0);
    ARPG_REQUIRE(std::strcmp(quit.rows[1].data(), "Back") == 0);
    return {};
}

test::Failure settings_view_shows_all_draft_and_committed_values() noexcept {
    const platform::PauseMenuState state = settings_state();
    const auto view = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(view.title, "SETTINGS") == 0);
    ARPG_REQUIRE(view.row_count == 21U);
    ARPG_REQUIRE(view.selected_row == 12U);
    ARPG_REQUIRE(view.message == state.message);
    constexpr const char* kRows[] = {
        "Master: 55% (saved 100%)",
        "SFX: 60% (saved 100%)",
        "Music: 35% (saved 45%)",
        "Ambience: 25% (saved 35%)",
        "UI: 70% (saved 80%)",
        "Window Mode: Fullscreen (saved Windowed)",
        "VSync: Off (saved On)",
        "Loot Filter: Magic or Better (saved Show All)",
        "Move Up: 0 (saved W)",
        "Move Down: 1 (saved S)",
        "Move Left: 2 (saved A)",
        "Move Right: 3 (saved D)",
        "Light Attack: 4 (saved J)",
        "Jump: 5 (saved K)",
        "Launcher: 6 (saved L)",
        "Interact: 7 (saved E)",
        "Inventory: 8 (saved I)",
        "Passive Tree: 9 (saved P)",
        "Reset Defaults",
        "Apply",
        "Cancel",
    };
    for (std::size_t row = 0U; row < std::size(kRows); ++row) {
        ARPG_REQUIRE(std::strcmp(view.rows[row].data(), kRows[row]) == 0);
    }
    return {};
}

test::Failure capture_and_closed_views_are_explicit() noexcept {
    platform::PauseMenuState state = settings_state();
    state.screen = platform::PauseScreen::capture_binding;
    state.capture_action = settings::SettingAction::light_attack;
    const auto capture = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(capture.title, "BIND KEY") == 0);
    ARPG_REQUIRE(capture.row_count == 1U);
    ARPG_REQUIRE(capture.selected_row == 0U);
    ARPG_REQUIRE(std::strcmp(capture.rows[0].data(),
        "Press a key for Light Attack (current 4)") == 0);

    state.capture_action.reset();
    const auto missing = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(missing.rows[0].data(),
        "Press a key for Unknown (current Unknown)") == 0);

    state.screen = platform::PauseScreen::closed;
    const auto closed = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(closed.title == nullptr);
    ARPG_REQUIRE(closed.row_count == 0U);
    return {};
}

test::Failure selected_rows_clamp_to_each_visible_view() noexcept {
    platform::PauseMenuState state = settings_state();
    state.selected_row = (std::numeric_limits<std::size_t>::max)();

    state.screen = platform::PauseScreen::root;
    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 2U);

    state.screen = platform::PauseScreen::settings;
    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 20U);

    state.screen = platform::PauseScreen::quit_confirm;
    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 1U);

    state.screen = platform::PauseScreen::capture_binding;
    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 0U);

    state.screen = platform::PauseScreen::closed;
    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 0U);
    return {};
}

test::Failure render_plans_have_exact_screen_orders() noexcept {
    platform::PauseMenuState state = settings_state();
    state.message = nullptr;
    constexpr platform::PauseScreen kScreens[] = {
        platform::PauseScreen::root,
        platform::PauseScreen::settings,
        platform::PauseScreen::capture_binding,
        platform::PauseScreen::quit_confirm,
    };
    for (const platform::PauseScreen screen : kScreens) {
        state.screen = screen;
        state.selected_row = (std::numeric_limits<std::size_t>::max)();
        state.capture_action = settings::SettingAction::light_attack;
        const platform::PauseMenuView view =
            platform::make_pause_menu_view(state);
        const platform::PauseMenuRenderPlan plan =
            platform::make_pause_menu_render_plan(view);
        ARPG_REQUIRE(plan.op_count == view.row_count + 4U);
        ARPG_REQUIRE(plan.ops[0].kind
            == platform::PauseMenuRenderOpKind::dim);
        ARPG_REQUIRE(plan.ops[1].kind
            == platform::PauseMenuRenderOpKind::panel);
        ARPG_REQUIRE(plan.ops[2].kind
            == platform::PauseMenuRenderOpKind::title);
        std::size_t selected_count = 0U;
        for (std::size_t row = 0U; row < view.row_count; ++row) {
            const auto& op = plan.ops[row + 3U];
            ARPG_REQUIRE(op.kind == platform::PauseMenuRenderOpKind::row);
            ARPG_REQUIRE(op.row_index == row);
            ARPG_REQUIRE(op.selected == (row == view.selected_row));
            if (op.selected) ++selected_count;
        }
        ARPG_REQUIRE(selected_count == 1U);
        ARPG_REQUIRE(plan.ops[plan.op_count - 1U].kind
            == platform::PauseMenuRenderOpKind::footer);
        ARPG_REQUIRE(!plan.has_message);
    }

    state.screen = platform::PauseScreen::closed;
    const auto closed = platform::make_pause_menu_render_plan(
        platform::make_pause_menu_view(state));
    ARPG_REQUIRE(closed.op_count == 0U);
    ARPG_REQUIRE(!closed.has_message);
    return {};
}

test::Failure render_plan_inserts_optional_message_before_footer() noexcept {
    platform::PauseMenuState state = settings_state();
    const platform::PauseMenuView view =
        platform::make_pause_menu_view(state);
    const platform::PauseMenuRenderPlan message_plan =
        platform::make_pause_menu_render_plan(view);
    ARPG_REQUIRE(message_plan.has_message);
    ARPG_REQUIRE(message_plan.op_count == view.row_count + 5U);
    ARPG_REQUIRE(platform::kPauseMenuRenderOpCapacity == 26U);
    ARPG_REQUIRE(message_plan.op_count == platform::kPauseMenuRenderOpCapacity);
    ARPG_REQUIRE(message_plan.ops[message_plan.op_count - 2U].kind
        == platform::PauseMenuRenderOpKind::message);
    ARPG_REQUIRE(message_plan.ops[message_plan.op_count - 1U].kind
        == platform::PauseMenuRenderOpKind::footer);

    state.message = "";
    const platform::PauseMenuView empty_view =
        platform::make_pause_menu_view(state);
    const platform::PauseMenuRenderPlan empty_plan =
        platform::make_pause_menu_render_plan(empty_view);
    ARPG_REQUIRE(!empty_plan.has_message);
    ARPG_REQUIRE(empty_plan.op_count == empty_view.row_count + 4U);
    ARPG_REQUIRE(empty_plan.ops[empty_plan.op_count - 1U].kind
        == platform::PauseMenuRenderOpKind::footer);
    return {};
}

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
    states[1].message = nullptr;
    states[2] = settings_state();
    states[3] = settings_state();
    states[3].screen = platform::PauseScreen::capture_binding;
    states[3].capture_action = settings::SettingAction::passive_tree;
    states[3].message = "";
    states[4] = settings_state();
    states[4].screen = platform::PauseScreen::quit_confirm;

    const std::uint64_t before = test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 1000U; ++iteration) {
        for (const auto& state : states) {
            const auto view = platform::make_pause_menu_view(state);
            ARPG_REQUIRE(view.row_count <= 21U);
            const auto plan = platform::make_pause_menu_render_plan(view);
            ARPG_REQUIRE(plan.op_count <= platform::kPauseMenuRenderOpCapacity);
        }
        constexpr std::array<std::array<int, 2>, 3> kSizes{{
            {{1024, 576}}, {{1280, 720}}, {{1920, 1080}},
        }};
        for (const auto& size : kSizes) {
            const auto layout = platform::pause_menu_layout(size[0], size[1]);
            const auto hit = platform::hit_test_pause_row(layout,
                {layout.rows[iteration % 21U].x,
                 layout.rows[iteration % 21U].y});
            ARPG_REQUIRE(hit.has_value());
            ARPG_REQUIRE(*hit == iteration % 21U);
            ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
                {layout.footer.x, layout.footer.y}).has_value());
        }
    }
    ARPG_REQUIRE(test::allocation_count() == before);
    return {};
}

constexpr test::TestCase kCases[] = {
    {"layouts are bounded and centered", &layouts_are_bounded_centered_and_fixed},
    {"layout has exact 1024 golden geometry", &layout_has_exact_1024_golden_geometry},
    {"full-HD layout scales physical pixels",
        &full_hd_layout_scales_panel_and_rows_by_one_and_a_half},
    {"hit test is half open", &hit_test_uses_half_open_rows_only},
    {"root and quit content", &root_and_quit_views_have_complete_content},
    {"settings draft and committed content", &settings_view_shows_all_draft_and_committed_values},
    {"capture and closed content", &capture_and_closed_views_are_explicit},
    {"selected rows clamp to visible views", &selected_rows_clamp_to_each_visible_view},
    {"render plans have exact screen orders", &render_plans_have_exact_screen_orders},
    {"render plan inserts optional message", &render_plan_inserts_optional_message_before_footer},
    {"row buffers terminate deterministically", &row_buffers_terminate_and_generation_is_deterministic},
    {"view layout hit have zero allocations", &all_view_layout_and_hit_paths_allocate_nothing},
};

}  // namespace

arpg::test::TestSuite pause_menu_view_suite() noexcept {
    return arpg::test::make_suite("pause_menu_view", kCases);
}
