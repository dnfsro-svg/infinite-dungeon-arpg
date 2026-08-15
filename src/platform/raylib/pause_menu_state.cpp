#include "pause_menu_state.hpp"

#include "ui_text_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::platform {
namespace {

constexpr std::size_t kRootRowCount = 3U;
constexpr std::size_t kSettingsHomeRowCount = 6U;
constexpr std::size_t kSettingsAudioRowCount = 6U;
constexpr std::size_t kSettingsDisplayRowCount = 4U;
constexpr std::size_t kSettingsBindingsRowCount = 11U;
constexpr std::size_t kQuitRowCount = 2U;
constexpr std::size_t kResetRowCount = 2U;
constexpr const char* kResetRejected = u8"当前状态无法重置";
constexpr std::size_t kSettingsHomeAudioRow = 0U;
constexpr std::size_t kSettingsHomeDisplayRow = 1U;
constexpr std::size_t kSettingsHomeBindingsRow = 2U;
constexpr std::size_t kSettingsHomeResetRow = 3U;
constexpr std::size_t kSettingsHomeApplyRow = 4U;
constexpr std::size_t kSettingsHomeCancelRow = 5U;
constexpr std::size_t kSettingsAudioBackRow = 5U;
constexpr std::size_t kSettingsDisplayLootFilterRow = 2U;
constexpr std::size_t kSettingsDisplayBackRow = 3U;
constexpr std::size_t kSettingsBindingsBackRow = 10U;

[[nodiscard]] bool confirmed(const PauseInput& input) noexcept {
    return input.enter || input.activate;
}

[[nodiscard]] bool navigate(
    std::size_t& selected_row,
    std::size_t row_count,
    const PauseInput& input) noexcept {
    selected_row %= row_count;
    if (input.up) {
        selected_row = selected_row == 0U ? row_count - 1U : selected_row - 1U;
        return true;
    }
    if (input.down) {
        selected_row = (selected_row + 1U) % row_count;
        return true;
    }
    return false;
}

void return_to_root(PauseMenuState& state) noexcept {
    state.screen = PauseScreen::root;
    state.settings_page = PauseSettingsPage::home;
    state.selected_row = 0U;
    state.capture_action.reset();
    state.reset_room_context = PauseRoomResetContext::normal;
    state.reset_room_origin = PauseRoomResetOrigin::root;
}

void open_reset_confirm(
    PauseMenuState& state,
    const PauseContext& context,
    PauseRoomResetOrigin origin) noexcept {
    state.screen = PauseScreen::reset_confirm;
    state.reset_room_context = context.reset_room_context;
    state.reset_room_origin = origin;
    state.selected_row = context.reset_room_context
            == PauseRoomResetContext::cleared_abyss
        ? 0U : 1U;
    state.capture_action.reset();
    state.message = nullptr;
}

[[nodiscard]] PauseCommand return_from_reset_confirm(
    PauseMenuState& state) noexcept {
    const PauseRoomResetOrigin origin = state.reset_room_origin;
    state.message = nullptr;
    if (origin == PauseRoomResetOrigin::closed) {
        state.screen = PauseScreen::closed;
        state.settings_page = PauseSettingsPage::home;
        state.selected_row = 0U;
        state.capture_action.reset();
        state.reset_room_context = PauseRoomResetContext::normal;
        state.reset_room_origin = PauseRoomResetOrigin::root;
        return PauseCommand::resume;
    }
    return_to_root(state);
    return PauseCommand::none;
}

[[nodiscard]] constexpr std::size_t home_row_for(
    PauseSettingsPage page) noexcept {
    switch (page) {
        case PauseSettingsPage::audio:
            return kSettingsHomeAudioRow;
        case PauseSettingsPage::display:
            return kSettingsHomeDisplayRow;
        case PauseSettingsPage::bindings:
            return kSettingsHomeBindingsRow;
        case PauseSettingsPage::home:
            return 0U;
    }
    return 0U;
}

void return_to_settings_home(PauseMenuState& state) noexcept {
    const std::size_t selected_row = home_row_for(state.settings_page);
    state.settings_page = PauseSettingsPage::home;
    state.selected_row = selected_row;
}

[[nodiscard]] constexpr std::size_t settings_row_count(
    PauseSettingsPage page) noexcept {
    switch (page) {
        case PauseSettingsPage::home:
            return kSettingsHomeRowCount;
        case PauseSettingsPage::audio:
            return kSettingsAudioRowCount;
        case PauseSettingsPage::display:
            return kSettingsDisplayRowCount;
        case PauseSettingsPage::bindings:
            return kSettingsBindingsRowCount;
    }
    return kSettingsHomeRowCount;
}

[[nodiscard]] PauseCommand rollback_to_root(PauseMenuState& state) noexcept {
    state.draft = state.committed;
    state.message = nullptr;
    return_to_root(state);
    return PauseCommand::rollback;
}

[[nodiscard]] PauseCommand update_closed(
    PauseMenuState& state,
    const PauseContext& context,
    const PauseInput& input) noexcept {
    if (!input.escape && !input.reset) {
        return PauseCommand::none;
    }
    if (context.recovery_active || context.death_active || context.inventory_open ||
        context.passive_open || context.pending_save) {
        return PauseCommand::none;
    }
    if (input.escape) {
        state.screen = PauseScreen::root;
        state.selected_row = 0U;
        state.capture_action.reset();
        state.message = nullptr;
        return PauseCommand::none;
    }
    open_reset_confirm(state, context, PauseRoomResetOrigin::closed);
    return PauseCommand::none;
}

[[nodiscard]] PauseCommand update_root(
    PauseMenuState& state,
    const PauseContext& context,
    const PauseInput& input) noexcept {
    if (input.escape) {
        state.screen = PauseScreen::closed;
        state.selected_row = 0U;
        state.capture_action.reset();
        state.message = nullptr;
        return PauseCommand::resume;
    }
    if (input.reset) {
        if (context.pending_save) {
            state.message = kResetRejected;
            return PauseCommand::none;
        }
        open_reset_confirm(state, context, PauseRoomResetOrigin::root);
        return PauseCommand::none;
    }
    if (navigate(state.selected_row, kRootRowCount, input)) {
        return PauseCommand::none;
    }
    if (!confirmed(input)) {
        return PauseCommand::none;
    }
    if (state.selected_row == 0U) {
        state.screen = PauseScreen::closed;
        state.selected_row = 0U;
        state.capture_action.reset();
        state.message = nullptr;
        return PauseCommand::resume;
    }
    if (state.selected_row == 1U) {
        state.screen = PauseScreen::settings;
        state.settings_page = PauseSettingsPage::home;
        state.selected_row = 0U;
        state.capture_action.reset();
        state.draft = state.committed;
        state.message = nullptr;
        return PauseCommand::none;
    }
    state.screen = PauseScreen::quit_confirm;
    state.selected_row = 1U;
    state.capture_action.reset();
    state.message = nullptr;
    return PauseCommand::none;
}

[[nodiscard]] PauseCommand update_volume(
    std::uint8_t& volume,
    const PauseInput& input) noexcept {
    const std::uint8_t before = volume;
    if (input.left) {
        volume = before <= 5U
            ? 0U
            : static_cast<std::uint8_t>(before - 5U);
    } else if (input.right) {
        volume = before >= 95U
            ? 100U
            : static_cast<std::uint8_t>(before + 5U);
    }
    return volume == before
        ? PauseCommand::none
        : PauseCommand::preview;
}

[[nodiscard]] PauseCommand update_loot_filter(
    PauseMenuState& state,
    const PauseInput& input) noexcept {
    if (!input.left && !input.right && !confirmed(input)) {
        return PauseCommand::none;
    }
    const settings::LootFilterMode before = state.draft.loot_filter_mode;
    if (input.left) {
        switch (before) {
            case settings::LootFilterMode::show_all:
                state.draft.loot_filter_mode = settings::LootFilterMode::rare_only;
                break;
            case settings::LootFilterMode::magic_or_better:
                state.draft.loot_filter_mode = settings::LootFilterMode::show_all;
                break;
            case settings::LootFilterMode::rare_only:
                state.draft.loot_filter_mode = settings::LootFilterMode::magic_or_better;
                break;
        }
    } else {
        switch (before) {
            case settings::LootFilterMode::show_all:
                state.draft.loot_filter_mode = settings::LootFilterMode::magic_or_better;
                break;
            case settings::LootFilterMode::magic_or_better:
                state.draft.loot_filter_mode = settings::LootFilterMode::rare_only;
                break;
            case settings::LootFilterMode::rare_only:
                state.draft.loot_filter_mode = settings::LootFilterMode::show_all;
                break;
        }
    }
    return PauseCommand::preview;
}

[[nodiscard]] PauseCommand update_settings(
    PauseMenuState& state,
    const PauseInput& input) noexcept {
    if (input.escape) {
        if (state.settings_page != PauseSettingsPage::home) {
            return_to_settings_home(state);
            return PauseCommand::none;
        }
        return rollback_to_root(state);
    }
    if (navigate(
            state.selected_row, settings_row_count(state.settings_page), input)) {
        return PauseCommand::none;
    }
    switch (state.settings_page) {
        case PauseSettingsPage::home:
            if (!confirmed(input)) {
                return PauseCommand::none;
            }
            if (state.selected_row <= kSettingsHomeBindingsRow) {
                state.message = nullptr;
                state.settings_page = static_cast<PauseSettingsPage>(
                    state.selected_row + 1U);
                state.selected_row = 0U;
                return PauseCommand::none;
            }
            if (state.selected_row == kSettingsHomeResetRow) {
                const std::uint64_t revision = state.draft.revision;
                state.draft = settings::default_settings();
                state.draft.revision = revision;
                return PauseCommand::preview;
            }
            if (state.selected_row == kSettingsHomeApplyRow) {
                if (settings::validate_settings(state.draft) !=
                    settings::SettingsValidationError::none) {
                    state.message = ui_text(UiTextId::settings_invalid);
                    return PauseCommand::none;
                }
                if (state.committed.revision ==
                    std::numeric_limits<std::uint64_t>::max()) {
                    state.message = ui_text(UiTextId::settings_revision_limit);
                    return PauseCommand::none;
                }
                state.draft.revision = state.committed.revision + 1U;
                return PauseCommand::apply;
            }
            if (state.selected_row == kSettingsHomeCancelRow) {
                return rollback_to_root(state);
            }
            return PauseCommand::none;

        case PauseSettingsPage::audio:
            if (state.selected_row < kSettingsAudioBackRow) {
                std::uint8_t* volumes[] = {
                    &state.draft.master_sfx_percent, &state.draft.sfx_percent,
                    &state.draft.music_percent, &state.draft.ambience_percent,
                    &state.draft.ui_percent,
                };
                return update_volume(*volumes[state.selected_row], input);
            }
            if (confirmed(input)) {
                return_to_settings_home(state);
            }
            return PauseCommand::none;

        case PauseSettingsPage::display:
            if (state.selected_row == 0U &&
                (input.left || input.right || confirmed(input))) {
                state.draft.window_mode =
                    state.draft.window_mode == settings::WindowMode::windowed
                    ? settings::WindowMode::fullscreen
                    : settings::WindowMode::windowed;
                return PauseCommand::preview;
            }
            if (state.selected_row == 1U &&
                (input.left || input.right || confirmed(input))) {
                state.draft.vsync_enabled = !state.draft.vsync_enabled;
                return PauseCommand::preview;
            }
            if (state.selected_row == kSettingsDisplayLootFilterRow) {
                return update_loot_filter(state, input);
            }
            if (state.selected_row == kSettingsDisplayBackRow &&
                confirmed(input)) {
                return_to_settings_home(state);
            }
            return PauseCommand::none;

        case PauseSettingsPage::bindings:
            if (!confirmed(input)) {
                return PauseCommand::none;
            }
            if (state.selected_row < kSettingsBindingsBackRow) {
                state.capture_action = static_cast<settings::SettingAction>(
                    state.selected_row);
                state.screen = PauseScreen::capture_binding;
                state.message = nullptr;
                return PauseCommand::none;
            }
            if (state.selected_row == kSettingsBindingsBackRow) {
                return_to_settings_home(state);
            }
            return PauseCommand::none;
    }
    return PauseCommand::none;
}

[[nodiscard]] PauseCommand update_reset_confirm(
    PauseMenuState& state,
    const PauseInput& input) noexcept {
    if (input.escape) {
        return return_from_reset_confirm(state);
    }
    const bool cleared_abyss = state.reset_room_context
        == PauseRoomResetContext::cleared_abyss;
    const std::size_t row_count = cleared_abyss ? 1U : kResetRowCount;
    if (navigate(state.selected_row, row_count, input)) {
        return PauseCommand::none;
    }
    if (!confirmed(input)) {
        return PauseCommand::none;
    }
    if (!cleared_abyss && state.selected_row == 0U) {
        state.message = nullptr;
        return PauseCommand::reset_room;
    }
    return return_from_reset_confirm(state);
}

[[nodiscard]] PauseCommand update_capture(
    PauseMenuState& state,
    const PauseContext& context,
    const PauseInput& input) noexcept {
    if (input.escape || input.focus_lost || !context.window_focused) {
        state.screen = PauseScreen::settings;
        state.settings_page = PauseSettingsPage::bindings;
        state.capture_action.reset();
        state.message = nullptr;
        return PauseCommand::none;
    }
    if (!input.captured_key.has_value()) {
        return PauseCommand::none;
    }
    if (!state.capture_action.has_value() ||
        !settings::assign_or_swap(
            state.draft, *state.capture_action, *input.captured_key)) {
        state.message = ui_text(UiTextId::settings_invalid_binding);
        return PauseCommand::none;
    }
    state.screen = PauseScreen::settings;
    state.settings_page = PauseSettingsPage::bindings;
    state.capture_action.reset();
    state.message = nullptr;
    return PauseCommand::none;
}

[[nodiscard]] PauseCommand update_quit_confirm(
    PauseMenuState& state,
    const PauseInput& input) noexcept {
    if (input.escape) {
        return_to_root(state);
        return PauseCommand::none;
    }
    if (navigate(state.selected_row, kQuitRowCount, input)) {
        return PauseCommand::none;
    }
    if (!confirmed(input)) {
        return PauseCommand::none;
    }
    if (state.selected_row == 0U) {
        return PauseCommand::quit;
    }
    return_to_root(state);
    return PauseCommand::none;
}

}  // namespace

PauseCommand update_pause_menu(
    PauseMenuState& state,
    const PauseContext& context,
    const PauseInput& input) noexcept {
    if (state.screen != PauseScreen::capture_binding
        && (input.focus_lost || !context.window_focused)) {
        return PauseCommand::none;
    }
    switch (state.screen) {
        case PauseScreen::closed:
            return update_closed(state, context, input);
        case PauseScreen::root:
            return update_root(state, context, input);
        case PauseScreen::settings:
            return update_settings(state, input);
        case PauseScreen::capture_binding:
            return update_capture(state, context, input);
        case PauseScreen::reset_confirm:
            return update_reset_confirm(state, input);
        case PauseScreen::quit_confirm:
            return update_quit_confirm(state, input);
        case PauseScreen::quit_saving:
            return PauseCommand::none;
    }
    state.screen = PauseScreen::closed;
    state.settings_page = PauseSettingsPage::home;
    state.selected_row = 0U;
    state.capture_action.reset();
    state.reset_room_context = PauseRoomResetContext::normal;
    state.reset_room_origin = PauseRoomResetOrigin::root;
    return PauseCommand::none;
}

void begin_pause_quit_saving(PauseMenuState& state) noexcept {
    state.screen = PauseScreen::quit_saving;
    state.selected_row = 0U;
    state.capture_action.reset();
    state.message = nullptr;
}

void cancel_pause_quit_saving(PauseMenuState& state) noexcept {
    if (state.screen != PauseScreen::quit_saving) return;
    state.screen = PauseScreen::quit_confirm;
    state.selected_row = 1U;
    state.capture_action.reset();
    state.message = ui_text(UiTextId::save_error);
}

void resolve_pause_room_reset(PauseMenuState& state, bool accepted) noexcept {
    if (state.screen != PauseScreen::reset_confirm) return;
    if (accepted) {
        static_cast<void>(return_from_reset_confirm(state));
        return;
    }
    state.selected_row = state.reset_room_context
            == PauseRoomResetContext::cleared_abyss
        ? 0U : 1U;
    state.message = kResetRejected;
}

}  // namespace arpg::platform
