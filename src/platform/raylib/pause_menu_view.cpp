#include "pause_menu_view.hpp"

#include "stable_key_raylib.hpp"
#include "ui_text_catalog.hpp"
#include "ui_typography.hpp"
#include "ui_utf8.hpp"

#include <algorithm>
#include <cstring>

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
    u8"方向键导航｜Enter 选择｜Esc 返回";
constexpr const char* kPauseRootResetFooter =
    u8"方向键导航｜Enter 选择｜Esc 返回｜R 重置房间";
constexpr const char* kSettingsSelectionFooter =
    u8"上下键导航｜Enter 选择｜Esc 返回";
constexpr const char* kSettingsAdjustmentFooter =
    u8"上下键导航｜左右键调整｜Enter 选择｜Esc 返回";
constexpr const char* kSettingsAudioFooter =
    u8"上下导航｜左右键调整｜点击左减右增｜返回行 Enter｜Esc 返回";
constexpr const char* kCaptureFooter =
    u8"请按下可绑定按键｜Esc 取消";

template <typename... Arguments>
void write_primary(
    PauseMenuView& view,
    std::size_t row,
    const char* format,
    Arguments... arguments) noexcept {
    if (row >= view.rows.size()) return;
    static_cast<void>(ui_utf8_format(
        view.rows[row].primary.data(), view.rows[row].primary.size(),
        format, arguments...));
}

template <typename... Arguments>
void write_saved(
    PauseMenuView& view,
    std::size_t row,
    const char* format,
    Arguments... arguments) noexcept {
    if (row >= view.rows.size()) return;
    static_cast<void>(ui_utf8_format(
        view.rows[row].saved.data(), view.rows[row].saved.size(),
        format, arguments...));
}

[[nodiscard]] const char* window_mode_label(
    settings::WindowMode mode) noexcept {
    switch (mode) {
        case settings::WindowMode::windowed:
            return u8"窗口化";
        case settings::WindowMode::fullscreen:
            return u8"全屏";
    }
    return ui_text(UiTextId::settings_unknown);
}

[[nodiscard]] const char* enabled_label(bool enabled) noexcept {
    return enabled ? u8"开" : u8"关";
}

[[nodiscard]] const char* chinese_loot_filter_label(
    settings::LootFilterMode mode) noexcept {
    switch (mode) {
        case settings::LootFilterMode::show_all:
            return u8"显示全部";
        case settings::LootFilterMode::magic_or_better:
            return u8"魔法及以上";
        case settings::LootFilterMode::rare_only:
            return u8"仅稀有";
    }
    return ui_text(UiTextId::settings_unknown);
}

[[nodiscard]] const char* key_label(settings::StableKey key) noexcept {
    return key == settings::StableKey::count
        ? ui_text(UiTextId::settings_unknown)
        : stable_key_label(key);
}

[[nodiscard]] std::size_t clamp_selected_row(
    std::size_t selected_row,
    std::size_t row_count) noexcept {
    if (row_count == 0U) return 0U;
    return selected_row < row_count ? selected_row : row_count - 1U;
}

void build_settings_home_rows(PauseMenuView& view) noexcept {
    write_primary(view, 0U, "%s", ui_text(UiTextId::settings_audio));
    write_primary(view, 1U, "%s", ui_text(UiTextId::settings_display));
    write_primary(view, 2U, "%s", ui_text(UiTextId::settings_bindings));
    write_primary(view, 3U, "%s", ui_text(UiTextId::settings_restore_defaults));
    write_primary(view, 4U, "%s", ui_text(UiTextId::settings_apply));
    write_primary(view, 5U, "%s", ui_text(UiTextId::settings_cancel));
}

void build_settings_audio_rows(
    PauseMenuView& view,
    const PauseMenuState& state) noexcept {
    struct VolumeRow final {
        const char* label;
        std::uint8_t draft;
        std::uint8_t committed;
    };
    const VolumeRow rows[] = {
        {u8"总音量", state.draft.master_sfx_percent,
            state.committed.master_sfx_percent},
        {u8"音效", state.draft.sfx_percent, state.committed.sfx_percent},
        {u8"音乐", state.draft.music_percent, state.committed.music_percent},
        {u8"环境音", state.draft.ambience_percent,
            state.committed.ambience_percent},
        {u8"界面音", state.draft.ui_percent, state.committed.ui_percent},
    };
    for (std::size_t row = 0U; row < std::size(rows); ++row) {
        write_primary(view, row, u8"%s：%u%%", rows[row].label,
            static_cast<unsigned>(rows[row].draft));
        if (rows[row].draft != rows[row].committed) {
            write_saved(view, row, u8"%s%u%%",
                ui_text(UiTextId::settings_saved_prefix),
                static_cast<unsigned>(rows[row].committed));
        }
    }
    write_primary(view, 5U, "%s", u8"返回");
}

void build_settings_display_rows(
    PauseMenuView& view,
    const PauseMenuState& state) noexcept {
    write_primary(view, 0U, u8"窗口模式：%s",
        window_mode_label(state.draft.window_mode));
    if (state.draft.window_mode != state.committed.window_mode) {
        write_saved(view, 0U, "%s%s",
            ui_text(UiTextId::settings_saved_prefix),
            window_mode_label(state.committed.window_mode));
    }
    write_primary(view, 1U, u8"垂直同步：%s",
        enabled_label(state.draft.vsync_enabled));
    if (state.draft.vsync_enabled != state.committed.vsync_enabled) {
        write_saved(view, 1U, "%s%s",
            ui_text(UiTextId::settings_saved_prefix),
            enabled_label(state.committed.vsync_enabled));
    }
    write_primary(view, 2U, u8"拾取过滤：%s",
        chinese_loot_filter_label(state.draft.loot_filter_mode));
    if (state.draft.loot_filter_mode != state.committed.loot_filter_mode) {
        write_saved(view, 2U, "%s%s",
            ui_text(UiTextId::settings_saved_prefix),
            chinese_loot_filter_label(state.committed.loot_filter_mode));
    }
    write_primary(view, 3U, "%s", u8"返回");
}

void build_settings_bindings_rows(
    PauseMenuView& view,
    const PauseMenuState& state) noexcept {
    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(settings::SettingAction::count);
         ++index) {
        const auto action = static_cast<settings::SettingAction>(index);
        const settings::StableKey draft_key =
            settings::binding_for(state.draft, action);
        const settings::StableKey committed_key =
            settings::binding_for(state.committed, action);
        write_primary(view, index, u8"%s：%s",
            ui_setting_action_name(action), key_label(draft_key));
        if (draft_key != committed_key) {
            write_saved(view, index, "%s%s",
                ui_text(UiTextId::settings_saved_prefix),
                key_label(committed_key));
        }
    }
    write_primary(view, 10U, "%s", u8"返回");
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
    const float font_scale = scale;
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

PauseMenuCenteredTextLayout centered_pause_text_layout(
    Rectangle row_bounds,
    float text_width,
    float text_height,
    float horizontal_inset) noexcept {
    const float inset = (std::max)(0.0F, horizontal_inset);
    const float available_width = (std::max)(
        0.0F, row_bounds.width - inset * 2.0F);
    const float clamped_text_width = (std::min)(
        (std::max)(0.0F, text_width), available_width);
    const float clamped_text_height = (std::max)(0.0F, text_height);
    return {
        row_bounds,
        {
            row_bounds.x + (row_bounds.width - clamped_text_width) * 0.5F,
            row_bounds.y + (row_bounds.height - clamped_text_height) * 0.5F,
            clamped_text_width,
            clamped_text_height,
        },
    };
}

PauseMenuFittedTextLayout fit_centered_pause_text_layout(
    Font font,
    const char* source,
    char* output,
    std::size_t output_capacity,
    Rectangle row_bounds,
    float preferred_font_size,
    float preferred_spacing,
    float minimum_font_size,
    float horizontal_inset) noexcept {
    const float desired_font_size = (std::max)(0.0F, preferred_font_size);
    const float minimum = (std::min)(desired_font_size,
        (std::max)(0.0F, minimum_font_size));
    float font_size = desired_font_size;
    float spacing = (std::max)(0.0F, preferred_spacing);
    bool truncated = false;
    if (output == nullptr || output_capacity == 0U) {
        return {row_bounds, {}, font_size, spacing, true};
    }

    const UiUtf8CopyResult copied = ui_utf8_fit_with_ellipsis(
        output, output_capacity, source == nullptr ? "" : source);
    truncated = copied.truncated;
    const float inset = (std::max)(0.0F, horizontal_inset);
    const float available_width = (std::max)(
        0.0F, row_bounds.width - inset * 2.0F);
    Vector2 measured = MeasureTextEx(font, output, font_size, spacing);
    if (measured.x > available_width && measured.x > 0.0F) {
        const float ratio = available_width / measured.x;
        font_size = (std::max)(minimum, font_size * ratio);
        spacing *= ratio;
        measured = MeasureTextEx(font, output, font_size, spacing);
        while (measured.x > available_width && font_size > minimum) {
            font_size = (std::max)(minimum, font_size - 0.25F);
            spacing = desired_font_size > 0.0F
                ? (std::max)(0.0F, preferred_spacing)
                    * (font_size / desired_font_size)
                : 0.0F;
            measured = MeasureTextEx(font, output, font_size, spacing);
        }
    }

    constexpr char kEllipsis[] = u8"…";
    constexpr std::size_t kEllipsisBytes = sizeof(kEllipsis) - 1U;
    while (measured.x > available_width && output[0] != '\0') {
        std::size_t length = std::strlen(output);
        if (length >= kEllipsisBytes
                && std::memcmp(output + length - kEllipsisBytes,
                    kEllipsis, kEllipsisBytes) == 0) {
            length -= kEllipsisBytes;
        }
        if (length == 0U) {
            output[0] = '\0';
            break;
        }
        do {
            --length;
        } while (length > 0U
            && (static_cast<unsigned char>(output[length]) & 0xC0U) == 0x80U);
        output[length] = '\0';
        if (length + kEllipsisBytes < output_capacity) {
            std::memcpy(output + length, kEllipsis, sizeof(kEllipsis));
        }
        truncated = true;
        measured = MeasureTextEx(font, output, font_size, spacing);
    }

    const PauseMenuCenteredTextLayout centered = centered_pause_text_layout(
        row_bounds, measured.x, measured.y, inset);
    return {centered.backing, centered.text, font_size, spacing, truncated};
}

PauseMenuView make_pause_menu_view(const PauseMenuState& state) noexcept {
    PauseMenuView view{};
    view.message = state.message;
    switch (state.screen) {
        case PauseScreen::closed:
            return view;
        case PauseScreen::root:
            view.title = ui_text(UiTextId::pause_title);
            view.footer = std::find(state.committed.bindings.begin(),
                state.committed.bindings.end(), settings::StableKey::r)
                    == state.committed.bindings.end()
                ? kPauseRootResetFooter : kPauseFooter;
            view.row_count = 3U;
            view.selected_row = clamp_selected_row(
                state.selected_row, view.row_count);
            write_primary(view, 0U, "%s", ui_text(UiTextId::pause_continue));
            write_primary(view, 1U, "%s", ui_text(UiTextId::pause_settings));
            write_primary(view, 2U, "%s", ui_text(UiTextId::pause_quit));
            return view;
        case PauseScreen::settings: {
            switch (state.settings_page) {
                case PauseSettingsPage::home:
                    view.title = ui_text(UiTextId::settings_title);
                    view.footer = kSettingsSelectionFooter;
                    view.row_count = 6U;
                    build_settings_home_rows(view);
                    break;
                case PauseSettingsPage::audio:
                    view.title = ui_text(UiTextId::settings_audio);
                    view.footer = kSettingsAudioFooter;
                    view.row_count = 6U;
                    build_settings_audio_rows(view, state);
                    break;
                case PauseSettingsPage::display:
                    view.title = ui_text(UiTextId::settings_display);
                    view.footer = kSettingsAdjustmentFooter;
                    view.row_count = 4U;
                    build_settings_display_rows(view, state);
                    break;
                case PauseSettingsPage::bindings:
                    view.title = ui_text(UiTextId::settings_bindings);
                    view.footer = kSettingsSelectionFooter;
                    view.row_count = 11U;
                    build_settings_bindings_rows(view, state);
                    break;
            }
            view.selected_row = clamp_selected_row(
                state.selected_row, view.row_count);
            return view;
        }
        case PauseScreen::capture_binding: {
            view.title = u8"绑定按键";
            view.footer = kCaptureFooter;
            view.row_count = 1U;
            view.selected_row = 0U;
            const settings::SettingAction action = state.capture_action.value_or(
                settings::SettingAction::count);
            write_primary(view, 0U, u8"请按下用于%s的按键（当前：%s）",
                ui_setting_action_name(action),
                key_label(settings::binding_for(state.draft, action)));
            return view;
        }
        case PauseScreen::quit_confirm:
            view.title = u8"确认退出？";
            view.footer = kPauseFooter;
            view.row_count = 2U;
            view.selected_row = clamp_selected_row(
                state.selected_row, view.row_count);
            write_primary(view, 0U, "%s", ui_text(UiTextId::pause_quit));
            write_primary(view, 1U, "%s", u8"返回");
            return view;
        case PauseScreen::quit_saving:
            view.title = ui_text(UiTextId::pause_quit);
            view.row_count = 1U;
            view.selected_row = view.row_count;
            write_primary(view, 0U, "%s", ui_text(UiTextId::save_saving));
            return view;
    }
    return view;
}

std::optional<std::size_t> hit_test_pause_row(
    const PauseMenuLayout& layout,
    Vector2 point) noexcept {
    const std::size_t row_count = (std::min)(
        layout.visible_row_count, kPauseMenuRowCapacity);
    for (std::size_t row = 0U; row < row_count; ++row) {
        const Rectangle bounds = layout.rows[row];
        if (point.x >= bounds.x && point.x < bounds.x + bounds.width
                && point.y >= bounds.y
                && point.y < bounds.y + bounds.height) {
            return row;
        }
    }
    return std::nullopt;
}

PausePointerHit hit_test_pause_pointer(
    const PauseMenuState& state,
    const PauseMenuView& view,
    const PauseMenuLayout& layout,
    Vector2 point) noexcept {
    const std::optional<std::size_t> row = hit_test_pause_row(layout, point);
    if (!row.has_value() || *row >= view.row_count
            || *row >= kPauseMenuRowCapacity) {
        return {};
    }

    if (state.screen == PauseScreen::settings
            && state.settings_page == PauseSettingsPage::audio) {
        constexpr std::size_t kVolumeRowCount = 5U;
        constexpr std::size_t kBackRow = 5U;
        if (*row < kVolumeRowCount) {
            const Rectangle bounds = layout.rows[*row];
            const float midpoint = bounds.x + bounds.width * 0.5F;
            return {*row, point.x < midpoint
                ? PausePointerIntent::decrement
                : PausePointerIntent::increment};
        }
        if (*row != kBackRow) return {};
    }

    return {*row, PausePointerIntent::activate};
}

}  // namespace arpg::platform
