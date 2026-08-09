#include "pause_menu_view.hpp"

#include "stable_key_raylib.hpp"
#include "ui_typography.hpp"

#include <cstdio>
#include <algorithm>

namespace arpg::platform {
namespace {

constexpr float kPanelWidth = 760.0F;
constexpr float kPanelHeight = 620.0F;
constexpr float kRowInsetX = 24.0F;
constexpr float kFirstRowY = 62.0F;
constexpr float kRowHeight = 23.0F;
constexpr float kRowStride = 24.0F;
constexpr float kFooterY = 580.0F;
constexpr float kFooterHeight = 28.0F;
constexpr float kCompactPanelWidth = 640.0F;
constexpr float kCompactPanelHeight = 380.0F;
constexpr float kCompactRowInsetX = 48.0F;
constexpr float kCompactTitleY = 24.0F;
constexpr float kCompactTitleHeight = 44.0F;
constexpr float kCompactFirstRowY = 92.0F;
constexpr float kCompactRowHeight = 50.0F;
constexpr float kCompactRowStride = 58.0F;
constexpr float kCompactFooterY = 330.0F;
constexpr float kCompactFooterHeight = 32.0F;
constexpr std::size_t kCompactRowCapacity = 3U;
constexpr const char* kPauseFooter =
    "Arrow keys navigate | Enter select | Esc back";
constexpr const char* kPauseRootResetFooter =
    "Arrow keys navigate | Enter select | Esc back | R Reset Room";

template <typename... Arguments>
void write_row(
    PauseMenuView& view,
    std::size_t row,
    const char* format,
    Arguments... arguments) noexcept {
    if (row >= view.rows.size()) return;
    static_cast<void>(std::snprintf(
        view.rows[row].data(), view.rows[row].size(),
        format, arguments...));
    view.rows[row].back() = '\0';
}

[[nodiscard]] const char* window_mode_label(
    settings::WindowMode mode) noexcept {
    switch (mode) {
        case settings::WindowMode::windowed:
            return "Windowed";
        case settings::WindowMode::fullscreen:
            return "Fullscreen";
    }
    return "Unknown";
}

[[nodiscard]] const char* enabled_label(bool enabled) noexcept {
    return enabled ? "On" : "Off";
}

[[nodiscard]] std::size_t clamp_selected_row(
    std::size_t selected_row,
    std::size_t row_count) noexcept {
    return selected_row < row_count ? selected_row : row_count - 1U;
}

void build_settings_rows(
    PauseMenuView& view,
    const PauseMenuState& state) noexcept {
    write_row(view, 0U, "Master: %u%% (saved %u%%)",
        static_cast<unsigned>(state.draft.master_sfx_percent),
        static_cast<unsigned>(state.committed.master_sfx_percent));
    write_row(view, 1U, "SFX: %u%% (saved %u%%)",
        static_cast<unsigned>(state.draft.sfx_percent),
        static_cast<unsigned>(state.committed.sfx_percent));
    write_row(view, 2U, "Music: %u%% (saved %u%%)",
        static_cast<unsigned>(state.draft.music_percent),
        static_cast<unsigned>(state.committed.music_percent));
    write_row(view, 3U, "Ambience: %u%% (saved %u%%)",
        static_cast<unsigned>(state.draft.ambience_percent),
        static_cast<unsigned>(state.committed.ambience_percent));
    write_row(view, 4U, "UI: %u%% (saved %u%%)",
        static_cast<unsigned>(state.draft.ui_percent),
        static_cast<unsigned>(state.committed.ui_percent));
    write_row(view, 5U, "Window Mode: %s (saved %s)",
        window_mode_label(state.draft.window_mode),
        window_mode_label(state.committed.window_mode));
    write_row(view, 6U, "VSync: %s (saved %s)",
        enabled_label(state.draft.vsync_enabled),
        enabled_label(state.committed.vsync_enabled));
    write_row(view, 7U, "Loot Filter: %s (saved %s)",
        settings::loot_filter_label(state.draft.loot_filter_mode),
        settings::loot_filter_label(state.committed.loot_filter_mode));

    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(settings::SettingAction::count);
         ++index) {
        const auto action = static_cast<settings::SettingAction>(index);
        write_row(view, index + 8U, "%s: %s (saved %s)",
            settings::action_label(action),
            stable_key_label(settings::binding_for(state.draft, action)),
            stable_key_label(settings::binding_for(state.committed, action)));
    }
    write_row(view, 18U, "%s", "Reset Defaults");
    write_row(view, 19U, "%s", "Apply");
    write_row(view, 20U, "%s", "Cancel");
}

}  // namespace

PauseMenuLayout pause_menu_layout(int width, int height) noexcept {
    return pause_menu_layout(width, height, kPauseMenuRowCapacity);
}

PauseMenuLayout pause_menu_layout(
    int width, int height, std::size_t visible_row_count) noexcept {
    const float screen_width = static_cast<float>(width);
    const float screen_height = static_cast<float>(height);
    const std::size_t clamped_row_count = (std::min)(
        visible_row_count, kPauseMenuRowCapacity);
    const bool compact = clamped_row_count <= kCompactRowCapacity;
    const float panel_base_width = compact
        ? kCompactPanelWidth : kPanelWidth;
    const float panel_base_height = compact
        ? kCompactPanelHeight : kPanelHeight;
    const float row_inset_x = compact ? kCompactRowInsetX : kRowInsetX;
    const float title_y = compact ? kCompactTitleY : 20.0F;
    const float title_height = compact ? kCompactTitleHeight : 30.0F;
    const float first_row_y = compact ? kCompactFirstRowY : kFirstRowY;
    const float row_height = compact ? kCompactRowHeight : kRowHeight;
    const float row_stride = compact ? kCompactRowStride : kRowStride;
    const float footer_y = compact ? kCompactFooterY : kFooterY;
    const float footer_height = compact ? kCompactFooterHeight : kFooterHeight;
    const float desired_scale = ui_viewport_scale(width, height);
    const float scale = (std::min)({desired_scale,
        (std::max)(0.0F, screen_width - 64.0F) / panel_base_width,
        (std::max)(0.0F, screen_height - 32.0F) / panel_base_height});
    const float panel_width = panel_base_width * scale;
    const float panel_height = panel_base_height * scale;
    const float font_scale = compact ? scale : desired_scale;
    PauseMenuLayout layout{};
    layout.visible_row_count = clamped_row_count;
    layout.title_font_size = (compact ? 32.0F : 26.0F) * font_scale;
    layout.row_font_size = (compact
        ? 26.0F : ui_typography().kPauseRowFontSize) * font_scale;
    layout.footer_font_size = (compact
        ? 20.0F : ui_typography().pause_footer_font_size) * font_scale;
    layout.panel = {
        (screen_width - panel_width) * 0.5F,
        (screen_height - panel_height) * 0.5F,
        panel_width,
        panel_height,
    };
    layout.title = {
        layout.panel.x + row_inset_x * scale,
        layout.panel.y + title_y * scale,
        layout.panel.width - row_inset_x * 2.0F * scale,
        title_height * scale,
    };
    for (std::size_t row = 0U; row < kPauseMenuRowCapacity; ++row) {
        layout.rows[row] = {
            layout.panel.x + row_inset_x * scale,
            layout.panel.y + (first_row_y
                + static_cast<float>(row) * row_stride) * scale,
            layout.panel.width - row_inset_x * 2.0F * scale,
            row_height * scale,
        };
    }
    layout.footer = {
        layout.panel.x + row_inset_x * scale,
        layout.panel.y + footer_y * scale,
        layout.panel.width - row_inset_x * 2.0F * scale,
        footer_height * scale,
    };
    return layout;
}

PauseMenuView make_pause_menu_view(const PauseMenuState& state) noexcept {
    PauseMenuView view{};
    view.message = state.message;
    switch (state.screen) {
        case PauseScreen::closed:
            return view;
        case PauseScreen::root:
            view.title = "PAUSED";
            view.footer = std::find(state.committed.bindings.begin(),
                state.committed.bindings.end(), settings::StableKey::r)
                    == state.committed.bindings.end()
                ? kPauseRootResetFooter : kPauseFooter;
            view.row_count = 3U;
            view.selected_row = clamp_selected_row(
                state.selected_row, view.row_count);
            write_row(view, 0U, "%s", "Continue");
            write_row(view, 1U, "%s", "Settings");
            write_row(view, 2U, "%s", "Quit Game");
            return view;
        case PauseScreen::settings:
            view.title = "SETTINGS";
            view.footer = kPauseFooter;
            view.row_count = kPauseMenuRowCapacity;
            view.selected_row = clamp_selected_row(
                state.selected_row, view.row_count);
            build_settings_rows(view, state);
            return view;
        case PauseScreen::capture_binding: {
            view.title = "BIND KEY";
            view.footer = kPauseFooter;
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
            view.footer = kPauseFooter;
            view.row_count = 2U;
            view.selected_row = clamp_selected_row(
                state.selected_row, view.row_count);
            write_row(view, 0U, "%s", "Quit Game");
            write_row(view, 1U, "%s", "Back");
            return view;
    }
    return view;
}

std::optional<std::size_t> hit_test_pause_row(
    const PauseMenuLayout& layout,
    Vector2 point) noexcept {
    for (std::size_t row = 0U; row < layout.visible_row_count; ++row) {
        const Rectangle bounds = layout.rows[row];
        if (point.x >= bounds.x && point.x < bounds.x + bounds.width
                && point.y >= bounds.y
                && point.y < bounds.y + bounds.height) {
            return row;
        }
    }
    return std::nullopt;
}

}  // namespace arpg::platform
