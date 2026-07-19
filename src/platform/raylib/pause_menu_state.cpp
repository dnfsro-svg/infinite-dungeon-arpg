#include "pause_menu_state.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::platform {
namespace {

constexpr std::size_t kRootRowCount = 3U;
constexpr std::size_t kSettingsRowCount = 17U;
constexpr std::size_t kQuitRowCount = 2U;
constexpr std::size_t kLootFilterRow = 3U;
constexpr std::size_t kFirstBindingRow = 4U;
constexpr std::size_t kLastBindingRow = 13U;
constexpr std::size_t kResetRow = 14U;
constexpr std::size_t kApplyRow = 15U;
constexpr std::size_t kCancelRow = 16U;

constexpr char kInvalidBindingMessage[] = "Key cannot be assigned";
constexpr char kInvalidSettingsMessage[] = "Settings are invalid";
constexpr char kRevisionOverflowMessage[] = "Settings revision limit reached";

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
    state.selected_row = 0U;
    state.capture_action.reset();
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
    if (!input.escape) {
        return PauseCommand::none;
    }
    if (context.recovery_active || context.death_active || context.inventory_open ||
        context.passive_open || context.pending_save) {
        return PauseCommand::none;
    }
    state.screen = PauseScreen::root;
    state.selected_row = 0U;
    state.capture_action.reset();
    return PauseCommand::none;
}

[[nodiscard]] PauseCommand update_root(
    PauseMenuState& state,
    const PauseInput& input) noexcept {
    if (input.escape) {
        state.screen = PauseScreen::closed;
        state.selected_row = 0U;
        state.capture_action.reset();
        return PauseCommand::resume;
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
        return PauseCommand::resume;
    }
    if (state.selected_row == 1U) {
        state.screen = PauseScreen::settings;
        state.selected_row = 0U;
        state.capture_action.reset();
        state.draft = state.committed;
        state.message = nullptr;
        return PauseCommand::none;
    }
    state.screen = PauseScreen::quit_confirm;
    state.selected_row = 1U;
    state.capture_action.reset();
    return PauseCommand::none;
}

[[nodiscard]] PauseCommand update_volume(
    PauseMenuState& state,
    const PauseInput& input) noexcept {
    const std::uint8_t before = state.draft.master_sfx_percent;
    if (input.left) {
        state.draft.master_sfx_percent = before <= 5U
            ? 0U
            : static_cast<std::uint8_t>(before - 5U);
    } else if (input.right) {
        state.draft.master_sfx_percent = before >= 95U
            ? 100U
            : static_cast<std::uint8_t>(before + 5U);
    }
    return state.draft.master_sfx_percent == before
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
        return rollback_to_root(state);
    }
    if (navigate(state.selected_row, kSettingsRowCount, input)) {
        return PauseCommand::none;
    }
    if (state.selected_row == 0U) {
        return update_volume(state, input);
    }
    if (state.selected_row == 1U &&
        (input.left || input.right || confirmed(input))) {
        state.draft.window_mode =
            state.draft.window_mode == settings::WindowMode::windowed
            ? settings::WindowMode::fullscreen
            : settings::WindowMode::windowed;
        return PauseCommand::preview;
    }
    if (state.selected_row == 2U &&
        (input.left || input.right || confirmed(input))) {
        state.draft.vsync_enabled = !state.draft.vsync_enabled;
        return PauseCommand::preview;
    }
    if (state.selected_row == kLootFilterRow) {
        return update_loot_filter(state, input);
    }
    if (!confirmed(input)) {
        return PauseCommand::none;
    }
    if (state.selected_row >= kFirstBindingRow &&
        state.selected_row <= kLastBindingRow) {
        state.capture_action = static_cast<settings::SettingAction>(
            state.selected_row - kFirstBindingRow);
        state.screen = PauseScreen::capture_binding;
        state.message = nullptr;
        return PauseCommand::none;
    }
    if (state.selected_row == kResetRow) {
        const std::uint64_t revision = state.draft.revision;
        state.draft = settings::default_settings();
        state.draft.revision = revision;
        return PauseCommand::preview;
    }
    if (state.selected_row == kApplyRow) {
        if (settings::validate_settings(state.draft) !=
            settings::SettingsValidationError::none) {
            state.message = kInvalidSettingsMessage;
            return PauseCommand::none;
        }
        if (state.committed.revision ==
            std::numeric_limits<std::uint64_t>::max()) {
            state.message = kRevisionOverflowMessage;
            return PauseCommand::none;
        }
        state.draft.revision = state.committed.revision + 1U;
        return PauseCommand::apply;
    }
    if (state.selected_row == kCancelRow) {
        return rollback_to_root(state);
    }
    return PauseCommand::none;
}

[[nodiscard]] PauseCommand update_capture(
    PauseMenuState& state,
    const PauseContext& context,
    const PauseInput& input) noexcept {
    if (input.escape || input.focus_lost || !context.window_focused) {
        state.screen = PauseScreen::settings;
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
        state.message = kInvalidBindingMessage;
        return PauseCommand::none;
    }
    state.screen = PauseScreen::settings;
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
    switch (state.screen) {
        case PauseScreen::closed:
            return update_closed(state, context, input);
        case PauseScreen::root:
            return update_root(state, input);
        case PauseScreen::settings:
            return update_settings(state, input);
        case PauseScreen::capture_binding:
            return update_capture(state, context, input);
        case PauseScreen::quit_confirm:
            return update_quit_confirm(state, input);
    }
    state.screen = PauseScreen::closed;
    state.selected_row = 0U;
    state.capture_action.reset();
    return PauseCommand::none;
}

}  // namespace arpg::platform
