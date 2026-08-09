#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "pause_menu_renderer.hpp"
#include "pause_menu_view.hpp"
#include "shared_ui_font.hpp"
#include "ui_utf8.hpp"

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

bool row_equals(
    const platform::PauseMenuRowView& row,
    const char* primary,
    const char* saved = "") noexcept {
    return std::strcmp(row.primary.data(), primary) == 0
        && std::strcmp(row.saved.data(), saved) == 0;
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
    ARPG_REQUIRE(std::strcmp(root.title, u8"暂停") == 0);
    ARPG_REQUIRE(root.row_count == 3U);
    ARPG_REQUIRE(root.selected_row == 1U);
    ARPG_REQUIRE(row_equals(root.rows[0], u8"继续游戏"));
    ARPG_REQUIRE(row_equals(root.rows[1], u8"设置"));
    ARPG_REQUIRE(row_equals(root.rows[2], u8"退出游戏"));
    ARPG_REQUIRE(root.message == state.message);
    ARPG_REQUIRE(root.footer != nullptr);
    ARPG_REQUIRE(std::strcmp(root.footer,
        u8"方向键导航｜Enter 选择｜Esc 返回｜R 重置房间") == 0);

    ARPG_REQUIRE(settings::assign_or_swap(state.draft,
        settings::SettingAction::light_attack, settings::StableKey::r));
    const auto draft_only_rebound_root =
        platform::make_pause_menu_view(state);
    ARPG_REQUIRE(draft_only_rebound_root.footer != nullptr);
    ARPG_REQUIRE(std::strcmp(draft_only_rebound_root.footer,
        u8"方向键导航｜Enter 选择｜Esc 返回｜R 重置房间") == 0);

    ARPG_REQUIRE(settings::assign_or_swap(state.committed,
        settings::SettingAction::light_attack, settings::StableKey::r));
    const auto committed_rebound_root =
        platform::make_pause_menu_view(state);
    ARPG_REQUIRE(committed_rebound_root.footer != nullptr);
    ARPG_REQUIRE(std::strcmp(committed_rebound_root.footer,
        u8"方向键导航｜Enter 选择｜Esc 返回") == 0);

    state.committed = settings::default_settings();
    state.screen = platform::PauseScreen::quit_confirm;
    state.selected_row = 0U;
    const auto quit = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(quit.title, u8"确认退出？") == 0);
    ARPG_REQUIRE(quit.row_count == 2U);
    ARPG_REQUIRE(quit.selected_row == 0U);
    ARPG_REQUIRE(row_equals(quit.rows[0], u8"退出游戏"));
    ARPG_REQUIRE(row_equals(quit.rows[1], u8"返回"));
    ARPG_REQUIRE(quit.footer != nullptr);
    ARPG_REQUIRE(std::strcmp(quit.footer,
        u8"方向键导航｜Enter 选择｜Esc 返回") == 0);
    return {};
}

test::Failure root_layout_is_compact_and_legible_at_reference_viewport() noexcept {
    const platform::PauseMenuLayout layout =
        platform::pause_menu_layout(1280, 720, 3U);
    const Rectangle viewport{0.0F, 0.0F, 1280.0F, 720.0F};

    ARPG_REQUIRE(layout.visible_row_count == 3U);
    ARPG_REQUIRE(inside(layout.panel, viewport));
    ARPG_REQUIRE(layout.panel.width >= 560.0F);
    ARPG_REQUIRE(layout.panel.width <= 680.0F);
    ARPG_REQUIRE(layout.panel.height >= 320.0F);
    ARPG_REQUIRE(layout.panel.height <= 400.0F);
    ARPG_REQUIRE(layout.title_font_size >= 30.0F);
    ARPG_REQUIRE(layout.row_font_size >= 24.0F);
    ARPG_REQUIRE(layout.footer_font_size >= 18.0F);
    for (std::size_t row = 0U; row < layout.visible_row_count; ++row) {
        ARPG_REQUIRE(inside(layout.rows[row], layout.panel));
        ARPG_REQUIRE(layout.rows[row].height >= 44.0F);
        if (row != 0U) {
            ARPG_REQUIRE(separated(layout.rows[row - 1U], layout.rows[row]));
        }
    }
    ARPG_REQUIRE(layout.rows[2].y + layout.rows[2].height + 24.0F
        <= layout.footer.y);
    return {};
}

test::Failure settings_pages_have_exact_chinese_primary_and_saved_text() noexcept {
    platform::PauseMenuState state = settings_state();
    state.selected_row = 2U;

    state.settings_page = platform::PauseSettingsPage::home;
    auto view = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(view.title, u8"设置") == 0);
    ARPG_REQUIRE(view.row_count == 6U);
    ARPG_REQUIRE(view.selected_row == 2U);
    ARPG_REQUIRE(view.message == state.message);
    ARPG_REQUIRE(std::strcmp(view.footer,
        u8"上下键导航｜左右键调整｜Enter 选择｜Esc 返回") == 0);
    constexpr const char* kHome[] = {
        u8"音频设置", u8"显示设置", u8"按键设置",
        u8"恢复默认设置", u8"应用", u8"取消",
    };
    for (std::size_t row = 0U; row < std::size(kHome); ++row) {
        ARPG_REQUIRE(row_equals(view.rows[row], kHome[row]));
    }

    state.settings_page = platform::PauseSettingsPage::audio;
    view = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(view.title, u8"音频设置") == 0);
    ARPG_REQUIRE(view.row_count == 6U);
    constexpr const char* kAudioPrimary[] = {
        u8"总音量：55%", u8"音效：60%", u8"音乐：35%",
        u8"环境音：25%", u8"界面音：70%", u8"返回",
    };
    constexpr const char* kAudioSaved[] = {
        u8"已保存：100%", u8"已保存：100%", u8"已保存：45%",
        u8"已保存：35%", u8"已保存：80%", "",
    };
    for (std::size_t row = 0U; row < std::size(kAudioPrimary); ++row) {
        ARPG_REQUIRE(row_equals(
            view.rows[row], kAudioPrimary[row], kAudioSaved[row]));
        ARPG_REQUIRE(std::strstr(view.rows[row].primary.data(), "saved") == nullptr);
    }

    state.settings_page = platform::PauseSettingsPage::display;
    view = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(view.title, u8"显示设置") == 0);
    ARPG_REQUIRE(view.row_count == 4U);
    ARPG_REQUIRE(row_equals(view.rows[0], u8"窗口模式：全屏",
        u8"已保存：窗口化"));
    ARPG_REQUIRE(row_equals(view.rows[1], u8"垂直同步：关",
        u8"已保存：开"));
    ARPG_REQUIRE(row_equals(view.rows[2], u8"拾取过滤：魔法及以上",
        u8"已保存：显示全部"));
    ARPG_REQUIRE(row_equals(view.rows[3], u8"返回"));

    state.settings_page = platform::PauseSettingsPage::bindings;
    view = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(view.title, u8"按键设置") == 0);
    ARPG_REQUIRE(view.row_count == 11U);
    constexpr const char* kBindingPrimary[] = {
        u8"上移：0", u8"下移：1", u8"左移：2", u8"右移：3",
        u8"轻攻击：4", u8"跳跃：5", u8"挑飞：6", u8"交互：7",
        u8"背包：8", u8"被动树：9", u8"返回",
    };
    constexpr const char* kBindingSaved[] = {
        u8"已保存：W", u8"已保存：S", u8"已保存：A", u8"已保存：D",
        u8"已保存：J", u8"已保存：K", u8"已保存：L", u8"已保存：E",
        u8"已保存：I", u8"已保存：P", "",
    };
    for (std::size_t row = 0U; row < std::size(kBindingPrimary); ++row) {
        ARPG_REQUIRE(row_equals(
            view.rows[row], kBindingPrimary[row], kBindingSaved[row]));
    }

    state.draft.bindings[0] = state.committed.bindings[0];
    view = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(row_equals(view.rows[0], u8"上移：W"));
    return {};
}

test::Failure row_text_is_centered_inside_full_width_backing() noexcept {
    constexpr std::array<std::array<int, 2>, 3> kSizes{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto& size : kSizes) {
        const auto layout = platform::pause_menu_layout(size[0], size[1], 6U);
        const auto centered = platform::centered_pause_text_layout(
            layout.rows[0], 180.0F, 24.0F, 12.0F);
        ARPG_REQUIRE(test::near(
            centered.text.x + centered.text.width * 0.5F,
            layout.rows[0].x + layout.rows[0].width * 0.5F));
        ARPG_REQUIRE(test::near(
            centered.text.y + centered.text.height * 0.5F,
            layout.rows[0].y + layout.rows[0].height * 0.5F));
        ARPG_REQUIRE(centered.backing.x == layout.rows[0].x);
        ARPG_REQUIRE(centered.backing.y == layout.rows[0].y);
        ARPG_REQUIRE(centered.backing.width == layout.rows[0].width);
        ARPG_REQUIRE(centered.backing.height == layout.rows[0].height);
        ARPG_REQUIRE(centered.text.x >= centered.backing.x + 12.0F);
        ARPG_REQUIRE(centered.text.x + centered.text.width
            <= centered.backing.x + centered.backing.width - 12.0F);
    }
    return {};
}

test::Failure capture_and_closed_views_are_explicit() noexcept {
    platform::PauseMenuState state = settings_state();
    state.screen = platform::PauseScreen::capture_binding;
    state.capture_action = settings::SettingAction::light_attack;
    const auto capture = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(std::strcmp(capture.title, u8"绑定按键") == 0);
    ARPG_REQUIRE(capture.row_count == 1U);
    ARPG_REQUIRE(capture.selected_row == 0U);
    ARPG_REQUIRE(row_equals(capture.rows[0],
        u8"请按下用于轻攻击的按键（当前：4）"));
    ARPG_REQUIRE(std::strcmp(capture.footer,
        u8"请按下可绑定按键｜Esc 取消") == 0);

    state.capture_action.reset();
    const auto missing = platform::make_pause_menu_view(state);
    ARPG_REQUIRE(row_equals(missing.rows[0],
        u8"请按下用于未知的按键（当前：未知）"));

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
    state.settings_page = platform::PauseSettingsPage::home;
    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 5U);
    state.settings_page = platform::PauseSettingsPage::audio;
    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 5U);
    state.settings_page = platform::PauseSettingsPage::display;
    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 3U);
    state.settings_page = platform::PauseSettingsPage::bindings;
    ARPG_REQUIRE(platform::make_pause_menu_view(state).selected_row == 10U);

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

test::Failure render_plan_draws_footer_surface_before_message_text() noexcept {
    platform::PauseMenuState state = settings_state();
    const platform::PauseMenuView view =
        platform::make_pause_menu_view(state);
    const platform::PauseMenuRenderPlan message_plan =
        platform::make_pause_menu_render_plan(view);
    ARPG_REQUIRE(message_plan.has_message);
    ARPG_REQUIRE(message_plan.op_count == view.row_count + 5U);
    ARPG_REQUIRE(platform::kPauseMenuRenderOpCapacity == 26U);
    ARPG_REQUIRE(message_plan.op_count < platform::kPauseMenuRenderOpCapacity);
    ARPG_REQUIRE(message_plan.ops[message_plan.op_count - 2U].kind
        == platform::PauseMenuRenderOpKind::footer);
    ARPG_REQUIRE(message_plan.ops[message_plan.op_count - 1U].kind
        == platform::PauseMenuRenderOpKind::message);

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
        ARPG_REQUIRE(first.rows[row].primary[95U] == '\0');
        ARPG_REQUIRE(first.rows[row].saved[63U] == '\0');
        ARPG_REQUIRE(platform::ui_utf8_is_valid(
            first.rows[row].primary.data()));
        ARPG_REQUIRE(platform::ui_utf8_is_valid(
            first.rows[row].saved.data()));
        ARPG_REQUIRE(std::memcmp(first.rows[row].primary.data(),
            second.rows[row].primary.data(), first.rows[row].primary.size()) == 0);
        ARPG_REQUIRE(std::memcmp(first.rows[row].saved.data(),
            second.rows[row].saved.data(), first.rows[row].saved.size()) == 0);
    }
    return {};
}

test::Failure every_static_pause_view_text_has_shared_font_coverage() noexcept {
    platform::PauseMenuState state = settings_state();
    constexpr platform::PauseScreen kScreens[] = {
        platform::PauseScreen::root,
        platform::PauseScreen::settings,
        platform::PauseScreen::capture_binding,
        platform::PauseScreen::quit_confirm,
    };
    constexpr platform::PauseSettingsPage kPages[] = {
        platform::PauseSettingsPage::home,
        platform::PauseSettingsPage::audio,
        platform::PauseSettingsPage::display,
        platform::PauseSettingsPage::bindings,
    };
    const platform::UiGlyphPlan glyphs = platform::ui_glyph_plan();
    for (const platform::PauseScreen screen : kScreens) {
        state.screen = screen;
        state.capture_action = settings::SettingAction::light_attack;
        const std::size_t page_count = screen == platform::PauseScreen::settings
            ? std::size(kPages) : 1U;
        for (std::size_t page = 0U; page < page_count; ++page) {
            state.settings_page = kPages[page];
            const platform::PauseMenuView view =
                platform::make_pause_menu_view(state);
            ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(
                glyphs, view.title));
            ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(
                glyphs, view.footer));
            for (std::size_t row = 0U; row < view.row_count; ++row) {
                ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(
                    glyphs, view.rows[row].primary.data()));
                ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(
                    glyphs, view.rows[row].saved.data()));
            }
        }
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
    {"root layout is compact and legible",
        &root_layout_is_compact_and_legible_at_reference_viewport},
    {"hit test is half open", &hit_test_uses_half_open_rows_only},
    {"root and quit content", &root_and_quit_views_have_complete_content},
    {"settings pages have exact Chinese text",
        &settings_pages_have_exact_chinese_primary_and_saved_text},
    {"row text is centered in full backing",
        &row_text_is_centered_inside_full_width_backing},
    {"capture and closed content", &capture_and_closed_views_are_explicit},
    {"selected rows clamp to visible views", &selected_rows_clamp_to_each_visible_view},
    {"render plans have exact screen orders", &render_plans_have_exact_screen_orders},
    {"render plan layers message over footer",
        &render_plan_draws_footer_surface_before_message_text},
    {"row buffers terminate deterministically", &row_buffers_terminate_and_generation_is_deterministic},
    {"pause view text has shared font coverage",
        &every_static_pause_view_text_has_shared_font_coverage},
    {"view layout hit have zero allocations", &all_view_layout_and_hit_paths_allocate_nothing},
};

}  // namespace

arpg::test::TestSuite pause_menu_view_suite() noexcept {
    return arpg::test::make_suite("pause_menu_view", kCases);
}
