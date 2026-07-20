#include "pause_menu_view.hpp"

#include "stable_key_raylib.hpp"

#include <cstdio>

namespace arpg::platform {
namespace {

constexpr float kPanelWidth = 760.0F;
constexpr float kPanelHeight = 544.0F;
constexpr float kRowInsetX = 24.0F;
constexpr float kFirstRowY = 60.0F;
constexpr float kRowHeight = 18.0F;
constexpr float kRowStride = 20.0F;
constexpr float kFooterY = 496.0F;
constexpr float kFooterHeight = 28.0F;

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
    const float screen_width = static_cast<float>(width);
    const float screen_height = static_cast<float>(height);
    PauseMenuLayout layout{};
    layout.panel = {
        (screen_width - kPanelWidth) * 0.5F,
        (screen_height - kPanelHeight) * 0.5F,
        kPanelWidth,
        kPanelHeight,
    };
    for (std::size_t row = 0U; row < kPauseMenuRowCapacity; ++row) {
        layout.rows[row] = {
            layout.panel.x + kRowInsetX,
            layout.panel.y + kFirstRowY
                + static_cast<float>(row) * kRowStride,
            layout.panel.width - kRowInsetX * 2.0F,
            kRowHeight,
        };
    }
    layout.footer = {
        layout.panel.x + kRowInsetX,
        layout.panel.y + kFooterY,
        layout.panel.width - kRowInsetX * 2.0F,
        kFooterHeight,
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
            view.row_count = 3U;
            view.selected_row = clamp_selected_row(
                state.selected_row, view.row_count);
            write_row(view, 0U, "%s", "Continue");
            write_row(view, 1U, "%s", "Settings");
            write_row(view, 2U, "%s", "Quit Game");
            return view;
        case PauseScreen::settings:
            view.title = "SETTINGS";
            view.row_count = kPauseMenuRowCapacity;
            view.selected_row = clamp_selected_row(
                state.selected_row, view.row_count);
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
    for (std::size_t row = 0U; row < kPauseMenuRowCapacity; ++row) {
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
