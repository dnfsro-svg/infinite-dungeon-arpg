#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "pause_menu_view.hpp"

#include <array>
#include <cstddef>
#include <cstring>

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
    state.draft.window_mode = settings::WindowMode::fullscreen;
    state.draft.vsync_enabled = false;
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
    float panel_width = 0.0F;
    float panel_height = 0.0F;
    float row_height = 0.0F;
    for (const auto& size : kSizes) {
        const Rectangle screen{
            0.0F, 0.0F,
            static_cast<float>(size[0]), static_cast<float>(size[1])};
        const platform::PauseMenuLayout layout =
            platform::pause_menu_layout(size[0], size[1]);
        ARPG_REQUIRE(inside(layout.panel, screen));
        ARPG_REQUIRE(layout.panel.width <= 760.0F);
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
        for (std::size_t row = 0U; row < 16U; ++row) {
            ARPG_REQUIRE(inside(layout.rows[row], layout.panel));
            ARPG_REQUIRE(separated(layout.rows[row], layout.footer));
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
         layout.rows[0].y + layout.rows[0].height + 1.0F}).has_value());
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
    ARPG_REQUIRE(view.row_count == 16U);
    ARPG_REQUIRE(view.selected_row == 12U);
    ARPG_REQUIRE(view.message == state.message);
    constexpr const char* kRows[] = {
        "Master SFX: 55% (saved 100%)",
        "Window Mode: Fullscreen (saved Windowed)",
        "VSync: Off (saved On)",
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
    states[2] = settings_state();
    states[3] = settings_state();
    states[3].screen = platform::PauseScreen::capture_binding;
    states[3].capture_action = settings::SettingAction::passive_tree;
    states[4] = settings_state();
    states[4].screen = platform::PauseScreen::quit_confirm;

    const std::uint64_t before = test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 1000U; ++iteration) {
        for (const auto& state : states) {
            const auto view = platform::make_pause_menu_view(state);
            ARPG_REQUIRE(view.row_count <= 16U);
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
            ARPG_REQUIRE(*hit == iteration % 16U);
            ARPG_REQUIRE(!platform::hit_test_pause_row(layout,
                {layout.footer.x, layout.footer.y}).has_value());
        }
    }
    ARPG_REQUIRE(test::allocation_count() == before);
    return {};
}

constexpr test::TestCase kCases[] = {
    {"layouts are bounded and centered", &layouts_are_bounded_centered_and_fixed},
    {"hit test is half open", &hit_test_uses_half_open_rows_only},
    {"root and quit content", &root_and_quit_views_have_complete_content},
    {"settings draft and committed content", &settings_view_shows_all_draft_and_committed_values},
    {"capture and closed content", &capture_and_closed_views_are_explicit},
    {"row buffers terminate deterministically", &row_buffers_terminate_and_generation_is_deterministic},
    {"view layout hit have zero allocations", &all_view_layout_and_hit_paths_allocate_nothing},
};

}  // namespace

arpg::test::TestSuite pause_menu_view_suite() noexcept {
    return arpg::test::make_suite("pause_menu_view", kCases);
}
