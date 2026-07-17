#include "allocation_probe.hpp"
#include "pause_menu_state.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

namespace platform = arpg::platform;
namespace settings = arpg::settings;
namespace test = arpg::test;

platform::PauseMenuState make_state() noexcept {
    platform::PauseMenuState state{};
    state.committed = settings::default_settings();
    state.draft = state.committed;
    return state;
}

platform::PauseCommand update(
    platform::PauseMenuState& state,
    platform::PauseInput input = {},
    platform::PauseContext context = {}) noexcept {
    return platform::update_pause_menu(state, context, input);
}

enum class InputKind : std::uint8_t {
    escape,
    enter,
    up,
    down,
    left,
    right,
    activate,
    focus_lost
};

platform::PauseInput input_for(InputKind kind) noexcept {
    platform::PauseInput input{};
    switch (kind) {
        case InputKind::escape: input.escape = true; break;
        case InputKind::enter: input.enter = true; break;
        case InputKind::up: input.up = true; break;
        case InputKind::down: input.down = true; break;
        case InputKind::left: input.left = true; break;
        case InputKind::right: input.right = true; break;
        case InputKind::activate: input.activate = true; break;
        case InputKind::focus_lost: input.focus_lost = true; break;
    }
    return input;
}

platform::PauseInput capture_input(settings::StableKey key) noexcept {
    platform::PauseInput input{};
    input.captured_key = key;
    return input;
}

enum class ContextOwner : std::uint8_t {
    death,
    recovery,
    inventory,
    passive,
    pending_save
};

platform::PauseContext context_for(ContextOwner owner) noexcept {
    platform::PauseContext context{};
    switch (owner) {
        case ContextOwner::death: context.death_active = true; break;
        case ContextOwner::recovery: context.recovery_active = true; break;
        case ContextOwner::inventory: context.inventory_open = true; break;
        case ContextOwner::passive: context.passive_open = true; break;
        case ContextOwner::pending_save: context.pending_save = true; break;
    }
    return context;
}

platform::PauseContext unfocused_context() noexcept {
    platform::PauseContext context{};
    context.window_focused = false;
    return context;
}

test::Failure closed_is_idle_without_escape() noexcept {
    auto state = make_state();
    ARPG_REQUIRE(update(state) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::closed);
    return {};
}

test::Failure higher_priority_contexts_block_pause_opening() noexcept {
    const platform::PauseContext contexts[] = {
        context_for(ContextOwner::death),
        context_for(ContextOwner::recovery),
        context_for(ContextOwner::inventory),
        context_for(ContextOwner::passive),
        context_for(ContextOwner::pending_save),
    };
    for (const auto context : contexts) {
        auto state = make_state();
        ARPG_REQUIRE(update(state, input_for(InputKind::escape), context) ==
            platform::PauseCommand::none);
        ARPG_REQUIRE(state.screen == platform::PauseScreen::closed);
    }
    return {};
}

test::Failure opening_pause_drops_same_frame_inputs() noexcept {
    auto state = make_state();
    platform::PauseInput input{};
    input.escape = true;
    input.enter = true;
    input.down = true;
    input.activate = true;
    input.captured_key = settings::StableKey::q;
    ARPG_REQUIRE(update(state, input) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
    ARPG_REQUIRE(state.selected_row == 0U);
    return {};
}

test::Failure root_navigation_wraps_three_rows() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::root;
    ARPG_REQUIRE(update(state, input_for(InputKind::up)) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.selected_row == 2U);
    for (std::size_t expected = 0U; expected < 3U; ++expected) {
        ARPG_REQUIRE(update(state, input_for(InputKind::down)) == platform::PauseCommand::none);
        ARPG_REQUIRE(state.selected_row == expected);
    }
    return {};
}

test::Failure root_continue_supports_enter_and_escape() noexcept {
    {
        auto state = make_state();
        state.screen = platform::PauseScreen::root;
        ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::resume);
        ARPG_REQUIRE(state.screen == platform::PauseScreen::closed);
    }
    {
        auto state = make_state();
        state.screen = platform::PauseScreen::root;
        state.selected_row = 2U;
        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::resume);
        ARPG_REQUIRE(state.screen == platform::PauseScreen::closed);
    }
    return {};
}

test::Failure opening_settings_copies_committed_and_clears_message() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::root;
    state.selected_row = 1U;
    state.committed.master_sfx_percent = 35U;
    state.draft.master_sfx_percent = 80U;
    state.message = "old error";
    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
    ARPG_REQUIRE(state.selected_row == 0U);
    ARPG_REQUIRE(state.draft.master_sfx_percent == 35U);
    ARPG_REQUIRE(state.message == nullptr);
    return {};
}

test::Failure opening_quit_confirmation_defaults_to_back() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::root;
    state.selected_row = 2U;
    ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::quit_confirm);
    ARPG_REQUIRE(state.selected_row == 1U);
    return {};
}

test::Failure settings_navigation_wraps_all_sixteen_rows() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    ARPG_REQUIRE(update(state, input_for(InputKind::up)) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.selected_row == 15U);
    for (std::size_t expected = 0U; expected < 16U; ++expected) {
        ARPG_REQUIRE(update(state, input_for(InputKind::down)) == platform::PauseCommand::none);
        ARPG_REQUIRE(state.selected_row == expected);
    }
    return {};
}

test::Failure volume_preview_clamps_and_noop_returns_none() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 0U;
    state.draft.master_sfx_percent = 5U;
    ARPG_REQUIRE(update(state, input_for(InputKind::left)) == platform::PauseCommand::preview);
    ARPG_REQUIRE(state.draft.master_sfx_percent == 0U);
    ARPG_REQUIRE(update(state, input_for(InputKind::left)) == platform::PauseCommand::none);
    state.draft.master_sfx_percent = 95U;
    ARPG_REQUIRE(update(state, input_for(InputKind::right)) == platform::PauseCommand::preview);
    ARPG_REQUIRE(state.draft.master_sfx_percent == 100U);
    ARPG_REQUIRE(update(state, input_for(InputKind::right)) == platform::PauseCommand::none);
    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
    return {};
}

test::Failure window_mode_preview_toggles_for_all_controls() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 1U;
    const platform::PauseInput inputs[] = {
        input_for(InputKind::left),
        input_for(InputKind::right),
        input_for(InputKind::activate),
        input_for(InputKind::enter)};
    auto expected = settings::WindowMode::fullscreen;
    for (const auto input : inputs) {
        ARPG_REQUIRE(update(state, input) == platform::PauseCommand::preview);
        ARPG_REQUIRE(state.draft.window_mode == expected);
        expected = expected == settings::WindowMode::windowed
            ? settings::WindowMode::fullscreen
            : settings::WindowMode::windowed;
    }
    return {};
}

test::Failure vsync_preview_toggles_for_all_controls() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 2U;
    const platform::PauseInput inputs[] = {
        input_for(InputKind::left),
        input_for(InputKind::right),
        input_for(InputKind::activate),
        input_for(InputKind::enter)};
    bool expected = false;
    for (const auto input : inputs) {
        ARPG_REQUIRE(update(state, input) == platform::PauseCommand::preview);
        ARPG_REQUIRE(state.draft.vsync_enabled == expected);
        expected = !expected;
    }
    return {};
}

test::Failure every_binding_row_enters_matching_capture() noexcept {
    for (std::size_t action = 0U;
         action < static_cast<std::size_t>(settings::SettingAction::count);
         ++action) {
        auto state = make_state();
        state.screen = platform::PauseScreen::settings;
        state.selected_row = action + 3U;
        const InputKind confirm = action % 2U == 0U
            ? InputKind::activate
            : InputKind::enter;
        ARPG_REQUIRE(update(state, input_for(confirm)) == platform::PauseCommand::none);
        ARPG_REQUIRE(state.screen == platform::PauseScreen::capture_binding);
        ARPG_REQUIRE(state.capture_action == static_cast<settings::SettingAction>(action));
        ARPG_REQUIRE(state.message == nullptr);
    }
    return {};
}

test::Failure valid_capture_assigns_without_preview() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::capture_binding;
    state.selected_row = 7U;
    state.capture_action = settings::SettingAction::light_attack;
    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::q)) ==
        platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
    ARPG_REQUIRE(!state.capture_action.has_value());
    ARPG_REQUIRE(settings::binding_for(
        state.draft, settings::SettingAction::light_attack) == settings::StableKey::q);
    ARPG_REQUIRE(state.message == nullptr);
    return {};
}

test::Failure occupied_capture_swaps_without_preview() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::capture_binding;
    state.capture_action = settings::SettingAction::light_attack;
    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::k)) ==
        platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
    ARPG_REQUIRE(settings::binding_for(
        state.draft, settings::SettingAction::light_attack) == settings::StableKey::k);
    ARPG_REQUIRE(settings::binding_for(
        state.draft, settings::SettingAction::jump) == settings::StableKey::j);
    return {};
}

test::Failure reserved_and_invalid_capture_remain_active() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::capture_binding;
    state.capture_action = settings::SettingAction::light_attack;
    const auto original = state.draft;
    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::v)) ==
        platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::capture_binding);
    ARPG_REQUIRE(state.message != nullptr);
    ARPG_REQUIRE(state.draft.bindings == original.bindings);
    const char* reserved_message = state.message;
    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::count)) ==
        platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::capture_binding);
    ARPG_REQUIRE(state.message != nullptr);
    ARPG_REQUIRE(state.message == reserved_message);
    ARPG_REQUIRE(state.draft.bindings == original.bindings);
    return {};
}

test::Failure capture_cancellation_paths_preserve_draft() noexcept {
    const platform::PauseInput inputs[] = {
        input_for(InputKind::escape), input_for(InputKind::focus_lost), {}};
    const platform::PauseContext contexts[] = {
        {}, {}, unfocused_context()};
    for (std::size_t index = 0U; index < 3U; ++index) {
        auto state = make_state();
        state.screen = platform::PauseScreen::capture_binding;
        state.selected_row = 5U;
        state.capture_action = settings::SettingAction::move_left;
        state.message = "capture error";
        const auto original = state.draft;
        ARPG_REQUIRE(update(state, inputs[index], contexts[index]) ==
            platform::PauseCommand::none);
        ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
        ARPG_REQUIRE(state.selected_row == 5U);
        ARPG_REQUIRE(!state.capture_action.has_value());
        ARPG_REQUIRE(state.message == nullptr);
        ARPG_REQUIRE(state.draft.bindings == original.bindings);
    }
    return {};
}

test::Failure reset_defaults_preserves_revision_and_previews() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 13U;
    state.draft.master_sfx_percent = 25U;
    state.draft.window_mode = settings::WindowMode::fullscreen;
    state.draft.vsync_enabled = false;
    state.draft.revision = 42U;
    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::preview);
    const auto defaults = settings::default_settings();
    ARPG_REQUIRE(state.draft.master_sfx_percent == defaults.master_sfx_percent);
    ARPG_REQUIRE(state.draft.window_mode == defaults.window_mode);
    ARPG_REQUIRE(state.draft.vsync_enabled == defaults.vsync_enabled);
    ARPG_REQUIRE(state.draft.bindings == defaults.bindings);
    ARPG_REQUIRE(state.draft.revision == 42U);
    return {};
}

test::Failure apply_increments_draft_without_publishing_or_closing() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 14U;
    state.committed.revision = 7U;
    state.draft.revision = 2U;
    state.draft.master_sfx_percent = 55U;
    ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::apply);
    ARPG_REQUIRE(state.draft.revision == 8U);
    ARPG_REQUIRE(state.committed.revision == 7U);
    ARPG_REQUIRE(state.committed.master_sfx_percent == 100U);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
    return {};
}

test::Failure apply_preserves_host_retry_message() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 14U;
    state.message = "save failed; retry";
    const char* message = state.message;
    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::apply);
    ARPG_REQUIRE(state.message == message);
    return {};
}

test::Failure apply_rejects_invalid_draft() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 14U;
    state.draft.master_sfx_percent = 53U;
    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
    ARPG_REQUIRE(state.draft.revision == 0U);
    ARPG_REQUIRE(state.message != nullptr);
    return {};
}

test::Failure apply_rejects_revision_overflow() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 14U;
    state.committed.revision = std::numeric_limits<std::uint64_t>::max();
    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.draft.revision == 0U);
    ARPG_REQUIRE(state.message != nullptr);
    return {};
}

test::Failure settings_escape_rolls_back_to_committed() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.draft.master_sfx_percent = 20U;
    state.message = "error";
    ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::rollback);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
    ARPG_REQUIRE(state.selected_row == 0U);
    ARPG_REQUIRE(state.draft.master_sfx_percent == state.committed.master_sfx_percent);
    ARPG_REQUIRE(state.message == nullptr);
    return {};
}

test::Failure settings_cancel_row_rolls_back_to_committed() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 15U;
    state.draft.vsync_enabled = false;
    ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::rollback);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
    ARPG_REQUIRE(state.draft.vsync_enabled == state.committed.vsync_enabled);
    return {};
}

test::Failure quit_navigation_wraps_two_rows() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::quit_confirm;
    state.selected_row = 1U;
    ARPG_REQUIRE(update(state, input_for(InputKind::down)) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.selected_row == 0U);
    ARPG_REQUIRE(update(state, input_for(InputKind::up)) == platform::PauseCommand::none);
    ARPG_REQUIRE(state.selected_row == 1U);
    return {};
}

test::Failure quit_back_and_escape_return_to_root() noexcept {
    {
        auto state = make_state();
        state.screen = platform::PauseScreen::quit_confirm;
        state.selected_row = 1U;
        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
        ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
    }
    {
        auto state = make_state();
        state.screen = platform::PauseScreen::quit_confirm;
        state.selected_row = 0U;
        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::none);
        ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
    }
    return {};
}

test::Failure quit_confirm_only_row_zero_quits() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::quit_confirm;
    state.selected_row = 0U;
    ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::quit);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::quit_confirm);
    return {};
}

test::Failure non_action_inputs_do_not_mutate_settings() noexcept {
    auto state = make_state();
    state.screen = platform::PauseScreen::settings;
    state.selected_row = 8U;
    const auto original = state.draft;
    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::q)) ==
        platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
    ARPG_REQUIRE(state.draft.bindings == original.bindings);
    return {};
}

test::Failure state_machine_performs_no_heap_allocations() noexcept {
    auto state = make_state();
    const std::uint64_t before = test::allocation_count();
    for (int iteration = 0; iteration < 1000; ++iteration) {
        state.screen = platform::PauseScreen::closed;
        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::none);
        state.selected_row = 1U;
        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
        state.selected_row = 0U;
        ARPG_REQUIRE(update(state, input_for(InputKind::left)) != platform::PauseCommand::apply);
        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::rollback);
    }
    ARPG_REQUIRE(test::allocation_count() == before);
    return {};
}

constexpr test::TestCase kCases[] = {
    {"closed is idle without escape", &closed_is_idle_without_escape},
    {"higher priority contexts block pause", &higher_priority_contexts_block_pause_opening},
    {"opening drops same-frame inputs", &opening_pause_drops_same_frame_inputs},
    {"root navigation wraps", &root_navigation_wraps_three_rows},
    {"root continue uses enter and escape", &root_continue_supports_enter_and_escape},
    {"settings copies committed", &opening_settings_copies_committed_and_clears_message},
    {"quit defaults to back", &opening_quit_confirmation_defaults_to_back},
    {"settings navigation wraps", &settings_navigation_wraps_all_sixteen_rows},
    {"volume preview clamps and noops", &volume_preview_clamps_and_noop_returns_none},
    {"window mode toggles", &window_mode_preview_toggles_for_all_controls},
    {"vsync toggles", &vsync_preview_toggles_for_all_controls},
    {"all binding rows capture", &every_binding_row_enters_matching_capture},
    {"valid capture assigns", &valid_capture_assigns_without_preview},
    {"occupied capture swaps", &occupied_capture_swaps_without_preview},
    {"invalid capture remains active", &reserved_and_invalid_capture_remain_active},
    {"capture cancellation preserves draft", &capture_cancellation_paths_preserve_draft},
    {"defaults preserve revision", &reset_defaults_preserves_revision_and_previews},
    {"apply increments without publish", &apply_increments_draft_without_publishing_or_closing},
    {"apply preserves retry message", &apply_preserves_host_retry_message},
    {"apply rejects invalid draft", &apply_rejects_invalid_draft},
    {"apply rejects overflow", &apply_rejects_revision_overflow},
    {"settings escape rolls back", &settings_escape_rolls_back_to_committed},
    {"cancel row rolls back", &settings_cancel_row_rolls_back_to_committed},
    {"quit navigation wraps", &quit_navigation_wraps_two_rows},
    {"quit back and escape", &quit_back_and_escape_return_to_root},
    {"quit row zero quits", &quit_confirm_only_row_zero_quits},
    {"non-action input is ignored", &non_action_inputs_do_not_mutate_settings},
    {"state machine has zero allocations", &state_machine_performs_no_heap_allocations},
};

}  // namespace

arpg::test::TestSuite pause_menu_state_suite() noexcept {
    return arpg::test::make_suite("pause_menu_state", kCases);
}
