#include "test_framework.hpp"

#include "core/fixed_step.hpp"
#include "dungeon/dungeon_session.hpp"
#include "host_input.hpp"
#include "pause_menu_state.hpp"
#include "platform/settings/settings_store.hpp"
#include "raylib_host.hpp"
#include "window_settings.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace combat = arpg::combat;
namespace core = arpg::core;
namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;
namespace settings = arpg::settings;

struct MemorySettingsFiles final {
    std::filesystem::path path{};
    std::vector<std::uint8_t> bytes{};
    bool fail_replace{};
};

bool memory_read(void* context, const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes) {
    auto& files = *static_cast<MemorySettingsFiles*>(context);
    if (files.bytes.empty() || files.path != path) return false;
    bytes = files.bytes;
    return true;
}

bool memory_replace(void* context, const std::filesystem::path& path,
    const std::uint8_t* bytes, std::size_t size) {
    auto& files = *static_cast<MemorySettingsFiles*>(context);
    if (files.fail_replace || bytes == nullptr) return false;
    files.path = path;
    files.bytes.assign(bytes, bytes + size);
    return true;
}

settings::SettingsStore memory_store(MemorySettingsFiles& files) {
    return settings::SettingsStore{"memory-settings",
        {&files, &memory_read, &memory_replace}};
}

struct ApplyRollbackFailureBackend final {
    settings::WindowMode mode{settings::WindowMode::windowed};
    std::size_t set_window_mode_calls{};
};

bool fail_rollback_window_mode(
    void* context,
    settings::WindowMode mode) noexcept {
    auto& backend = *static_cast<ApplyRollbackFailureBackend*>(context);
    backend.mode = mode;
    ++backend.set_window_mode_calls;
    return backend.set_window_mode_calls != 2U;
}

settings::WindowMode read_failure_backend_window_mode(void* context) noexcept {
    return static_cast<ApplyRollbackFailureBackend*>(context)->mode;
}

platform::WindowSettingsBackend rollback_failure_backend(
    ApplyRollbackFailureBackend& backend) noexcept {
    return {&backend, nullptr, &fail_rollback_window_mode, nullptr,
        &read_failure_backend_window_mode, nullptr};
}

void run_frame(dungeon::DungeonSession& session,
    const platform::HostFrameGateResult& frame,
    bool request_action) noexcept {
    if (frame.forward_gameplay && request_action) {
        static_cast<void>(session.queue_action(combat::Action::light));
    }
    for (std::uint32_t step = 0U; step < frame.fixed_step.steps; ++step) {
        session.tick({1, 0});
    }
}

arpg::test::Failure six_hundred_paused_presented_frames_freeze_simulation()
    noexcept {
    dungeon::DungeonSession session{};
    core::FixedStepRunner fixed_step{};
    bool pause_latched = false;

    const auto partial = platform::gate_host_frame(
        fixed_step, pause_latched, false,
        core::FixedStepRunner::kStepSeconds * 0.5);
    ARPG_REQUIRE(partial.forward_gameplay);
    ARPG_REQUIRE(partial.fixed_step.steps == 0U);

    const auto entered = platform::gate_host_frame(
        fixed_step, pause_latched, true,
        core::FixedStepRunner::kStepSeconds);
    ARPG_REQUIRE(!entered.forward_gameplay);
    ARPG_REQUIRE(entered.fixed_step.steps == 0U);
    ARPG_REQUIRE(pause_latched);

    const dungeon::DungeonSnapshot frozen = session.snapshot();
    for (int frame_index = 0; frame_index < 600; ++frame_index) {
        const auto paused = platform::gate_host_frame(
            fixed_step, pause_latched, true, 1.0);
        ARPG_REQUIRE(!paused.forward_gameplay);
        ARPG_REQUIRE(paused.fixed_step.steps == 0U);
        run_frame(session, paused, true);
    }

    const dungeon::DungeonSnapshot after = session.snapshot();
    ARPG_REQUIRE(after.session_tick == frozen.session_tick);
    ARPG_REQUIRE(after.phase == frozen.phase);
    ARPG_REQUIRE(after.room_seed == frozen.room_seed);
    ARPG_REQUIRE(after.remaining_targets == frozen.remaining_targets);
    ARPG_REQUIRE(after.combat.has_value() == frozen.combat.has_value());
    if (after.combat.has_value()) {
        ARPG_REQUIRE(after.combat->tick == frozen.combat->tick);
        ARPG_REQUIRE(after.combat->player.position.x
            == frozen.combat->player.position.x);
        ARPG_REQUIRE(after.combat->player.position.y
            == frozen.combat->player.position.y);
        ARPG_REQUIRE(after.combat->player.hp == frozen.combat->player.hp);
        ARPG_REQUIRE(after.combat->diagnostics.input_size
            == frozen.combat->diagnostics.input_size);
    }

    const auto resumed = platform::gate_host_frame(
        fixed_step, pause_latched, false,
        core::FixedStepRunner::kStepSeconds);
    ARPG_REQUIRE(resumed.forward_gameplay);
    ARPG_REQUIRE(!pause_latched);
    ARPG_REQUIRE(resumed.fixed_step.steps == 1U);
    ARPG_REQUIRE(resumed.fixed_step.steps
        <= core::FixedStepRunner::kMaxStepsPerFrame);
    run_frame(session, resumed, false);
    ARPG_REQUIRE(session.snapshot().session_tick == frozen.session_tick + 1U);
    return {};
}

arpg::test::Failure pause_entry_discards_same_frame_action_and_accumulator()
    noexcept {
    dungeon::DungeonSession session{};
    core::FixedStepRunner fixed_step{};
    bool pause_latched = false;

    const auto partial = platform::gate_host_frame(
        fixed_step, pause_latched, false,
        core::FixedStepRunner::kStepSeconds * 0.75);
    run_frame(session, partial, false);
    const auto before = session.snapshot();

    const auto entered = platform::gate_host_frame(
        fixed_step, pause_latched, true,
        core::FixedStepRunner::kStepSeconds);
    run_frame(session, entered, true);
    ARPG_REQUIRE(!entered.forward_gameplay);
    ARPG_REQUIRE(session.snapshot().session_tick == before.session_tick);

    const auto resumed = platform::gate_host_frame(
        fixed_step, pause_latched, false,
        core::FixedStepRunner::kStepSeconds * 0.25);
    ARPG_REQUIRE(resumed.fixed_step.steps == 0U);
    ARPG_REQUIRE(resumed.fixed_step.total_ticks == 0U);
    return {};
}

arpg::test::Failure room_reset_gate_keeps_pause_gameplay_frozen() noexcept {
    core::FixedStepRunner fixed_step{};
    bool pause_latched = false;
    const platform::HostFrameGateResult paused = platform::gate_host_frame(
        fixed_step, pause_latched, true,
        core::FixedStepRunner::kStepSeconds);
    ARPG_REQUIRE(!paused.forward_gameplay);

    platform::HostRoomResetGateInput input{};
    input.pause_screen = platform::PauseScreen::root;
    input.reset_pressed = true;
    input.gameplay_armed = true;
    input.death_allows_gameplay = true;
    input.forward_actions = paused.forward_gameplay;
    input.inventory_allows_room_reset = false;
    ARPG_REQUIRE(platform::host_requests_room_reset(input));

    for (const platform::PauseScreen screen : {
             platform::PauseScreen::settings,
             platform::PauseScreen::capture_binding,
             platform::PauseScreen::quit_confirm,
         }) {
        input.pause_screen = screen;
        ARPG_REQUIRE(!platform::host_requests_room_reset(input));
    }

    input.pause_screen = platform::PauseScreen::closed;
    ARPG_REQUIRE(!platform::host_requests_room_reset(input));
    input.forward_actions = true;
    input.inventory_allows_room_reset = true;
    ARPG_REQUIRE(platform::host_requests_room_reset(input));

    input.gameplay_armed = false;
    ARPG_REQUIRE(!platform::host_requests_room_reset(input));
    input.gameplay_armed = true;
    input.death_allows_gameplay = false;
    ARPG_REQUIRE(!platform::host_requests_room_reset(input));
    input.death_allows_gameplay = true;
    input.reset_pressed = false;
    ARPG_REQUIRE(!platform::host_requests_room_reset(input));
    return {};
}

arpg::test::Failure death_continue_uses_fixed_e_from_authoritative_snapshot()
    noexcept {
    settings::SettingsData rebound = settings::default_settings();
    ARPG_REQUIRE(settings::assign_or_swap(rebound,
        settings::SettingAction::interact, settings::StableKey::q));

    platform::PhysicalKeySnapshot physical{};
    physical.pressed[static_cast<std::size_t>(settings::StableKey::q)] = true;
    physical.escape = true;
    const platform::HostFrameInput rebound_input =
        platform::map_host_frame_input(rebound, physical);
    ARPG_REQUIRE(rebound_input.keys.e);
    const platform::DeathInputGate rebound_gate =
        platform::host_death_input_gate(
            false, true, rebound_input.keys, physical);
    ARPG_REQUIRE(!rebound_gate.continue_death);
    ARPG_REQUIRE(rebound_gate.exit);
    ARPG_REQUIRE(!rebound_gate.forward_gameplay);

    physical = {};
    physical.pressed[static_cast<std::size_t>(settings::StableKey::e)] = true;
    const platform::HostFrameInput fixed_e_input =
        platform::map_host_frame_input(rebound, physical);
    ARPG_REQUIRE(!fixed_e_input.keys.e);
    const platform::DeathInputGate fixed_e_gate =
        platform::host_death_input_gate(
            false, true, fixed_e_input.keys, physical);
    ARPG_REQUIRE(fixed_e_gate.continue_death);
    ARPG_REQUIRE(!fixed_e_gate.exit);
    return {};
}

arpg::test::Failure corrupt_settings_notice_is_deferred_until_first_settings()
    noexcept {
    platform::HostSettingsNotice notice = platform::make_host_settings_notice(
        settings::SettingsLoadStatus::defaults_corrupt);
    ARPG_REQUIRE(notice.recovered_defaults_pending);

    platform::PauseMenuState state{};
    state.committed = settings::default_settings();
    state.draft = state.committed;
    state.screen = platform::PauseScreen::root;
    state.selected_row = 1U;
    platform::PauseInput enter{};
    enter.enter = true;
    const platform::PauseScreen previous = state.screen;
    ARPG_REQUIRE(platform::update_pause_menu(state, {}, enter)
        == platform::PauseCommand::none);
    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
    ARPG_REQUIRE(state.message == nullptr);

    platform::consume_host_settings_notice(notice, previous, state);
    ARPG_REQUIRE(!notice.recovered_defaults_pending);
    ARPG_REQUIRE(state.message != nullptr);
    ARPG_REQUIRE(std::strcmp(state.message,
        u8"设置已恢复默认值") == 0);

    state.message = nullptr;
    platform::consume_host_settings_notice(
        notice, platform::PauseScreen::root, state);
    ARPG_REQUIRE(state.message == nullptr);
    return {};
}

arpg::test::Failure healthy_settings_loads_do_not_queue_recovery_notice()
    noexcept {
    const settings::SettingsLoadStatus statuses[] = {
        settings::SettingsLoadStatus::defaults_missing,
        settings::SettingsLoadStatus::recovered_single_slot,
    };
    for (const settings::SettingsLoadStatus status : statuses) {
        platform::HostSettingsNotice notice =
            platform::make_host_settings_notice(status);
        ARPG_REQUIRE(!notice.recovered_defaults_pending);
        platform::PauseMenuState state{};
        state.screen = platform::PauseScreen::settings;
        platform::consume_host_settings_notice(
            notice, platform::PauseScreen::root, state);
        ARPG_REQUIRE(state.message == nullptr);
    }
    return {};
}

arpg::test::Failure close_same_frame_waits_for_successful_apply() noexcept {
    MemorySettingsFiles files{};
    settings::SettingsStore store = memory_store(files);
    platform::PauseMenuState state{};
    state.committed = settings::default_settings();
    state.draft = state.committed;
    state.draft.revision = 1U;
    state.draft.loot_filter_mode = settings::LootFilterMode::rare_only;
    ARPG_REQUIRE(settings::assign_or_swap(state.draft,
        settings::SettingAction::light_attack, settings::StableKey::q));
    settings::SettingsData live = state.committed;
    settings::SettingsData input = state.committed;

    const bool exit_requested = platform::settle_host_pause_command(
        platform::PauseCommand::apply, true, state, live, input, store, {});
    ARPG_REQUIRE(exit_requested);
    ARPG_REQUIRE(state.committed.revision == 1U);
    ARPG_REQUIRE(settings::binding_for(state.committed,
        settings::SettingAction::light_attack) == settings::StableKey::q);
    ARPG_REQUIRE(input.bindings == state.committed.bindings);
    ARPG_REQUIRE(live.bindings == state.committed.bindings);
    ARPG_REQUIRE(state.committed.loot_filter_mode
        == settings::LootFilterMode::rare_only);
    ARPG_REQUIRE(live.loot_filter_mode == state.committed.loot_filter_mode);
    return {};
}

arpg::test::Failure close_same_frame_waits_for_rejected_apply() noexcept {
    MemorySettingsFiles files{};
    files.fail_replace = true;
    settings::SettingsStore store = memory_store(files);
    platform::PauseMenuState state{};
    state.committed = settings::default_settings();
    state.draft = state.committed;
    state.draft.revision = 1U;
    state.draft.loot_filter_mode = settings::LootFilterMode::rare_only;
    ARPG_REQUIRE(settings::assign_or_swap(state.draft,
        settings::SettingAction::light_attack, settings::StableKey::q));
    const settings::SettingsData original = state.committed;
    settings::SettingsData live = state.committed;
    settings::SettingsData input = state.committed;

    const bool exit_requested = platform::settle_host_pause_command(
        platform::PauseCommand::apply, true, state, live, input, store, {});
    ARPG_REQUIRE(exit_requested);
    ARPG_REQUIRE(state.committed.revision == original.revision);
    ARPG_REQUIRE(state.committed.bindings == original.bindings);
    ARPG_REQUIRE(input.bindings == original.bindings);
    ARPG_REQUIRE(state.draft.bindings != original.bindings);
    ARPG_REQUIRE(live.loot_filter_mode == original.loot_filter_mode);
    ARPG_REQUIRE(state.draft.loot_filter_mode == original.loot_filter_mode);
    ARPG_REQUIRE(platform::renderer_loot_filter_mode(
        state.screen, live, state.draft) == original.loot_filter_mode);
    ARPG_REQUIRE(state.message != nullptr);
    return {};
}

arpg::test::Failure loot_filter_preview_is_renderer_only_until_commit() noexcept {
    MemorySettingsFiles files{};
    settings::SettingsStore store = memory_store(files);
    platform::PauseMenuState state{};
    state.screen = platform::PauseScreen::settings;
    state.committed = settings::default_settings();
    state.draft = state.committed;
    state.draft.loot_filter_mode = settings::LootFilterMode::rare_only;
    settings::SettingsData live = state.committed;
    settings::SettingsData input = state.committed;

    ARPG_REQUIRE(!platform::settle_host_pause_command(
        platform::PauseCommand::preview, false, state, live, input, store, {}));
    ARPG_REQUIRE(live.loot_filter_mode == state.committed.loot_filter_mode);
    ARPG_REQUIRE(platform::loot_pickup_policy(live.loot_filter_mode).minimum_rarity
        == platform::loot_pickup_policy(
            state.committed.loot_filter_mode).minimum_rarity);
    ARPG_REQUIRE(platform::renderer_loot_filter_mode(
        state.screen, live, state.draft)
        == settings::LootFilterMode::rare_only);

    platform::PauseInput cancel{};
    cancel.escape = true;
    const platform::PauseCommand rollback =
        platform::update_pause_menu(state, {}, cancel);
    ARPG_REQUIRE(rollback == platform::PauseCommand::rollback);
    ARPG_REQUIRE(!platform::settle_host_pause_command(
        rollback, false, state, live, input, store, {}));
    ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
    ARPG_REQUIRE(state.draft.loot_filter_mode
        == state.committed.loot_filter_mode);
    ARPG_REQUIRE(platform::renderer_loot_filter_mode(
        state.screen, live, state.draft)
        == state.committed.loot_filter_mode);
    return {};
}

arpg::test::Failure apply_rollback_failure_still_restores_committed_loot_policy()
    noexcept {
    MemorySettingsFiles files{};
    files.fail_replace = true;
    settings::SettingsStore store = memory_store(files);
    ApplyRollbackFailureBackend backend{};
    platform::PauseMenuState state{};
    state.screen = platform::PauseScreen::settings;
    state.committed = settings::default_settings();
    state.draft = state.committed;
    state.draft.revision = 1U;
    state.draft.window_mode = settings::WindowMode::fullscreen;
    state.draft.loot_filter_mode = settings::LootFilterMode::rare_only;
    const settings::SettingsData original = state.committed;
    settings::SettingsData live = state.committed;
    settings::SettingsData input = state.committed;

    ARPG_REQUIRE(!platform::settle_host_pause_command(
        platform::PauseCommand::apply, false, state, live, input, store,
        rollback_failure_backend(backend)));
    ARPG_REQUIRE(backend.set_window_mode_calls == 3U);
    ARPG_REQUIRE(state.message != nullptr);
    ARPG_REQUIRE(state.committed.loot_filter_mode == original.loot_filter_mode);
    ARPG_REQUIRE(live.loot_filter_mode == original.loot_filter_mode);
    ARPG_REQUIRE(state.draft.loot_filter_mode == original.loot_filter_mode);
    ARPG_REQUIRE(platform::loot_pickup_policy(live.loot_filter_mode).minimum_rarity
        == platform::loot_pickup_policy(original.loot_filter_mode).minimum_rarity);
    ARPG_REQUIRE(platform::renderer_loot_filter_mode(
        state.screen, live, state.draft) == original.loot_filter_mode);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"600 paused frames freeze simulation",
        &six_hundred_paused_presented_frames_freeze_simulation},
    {"pause entry discards action and accumulator",
        &pause_entry_discards_same_frame_action_and_accumulator},
    {"room reset gate keeps pause gameplay frozen",
        &room_reset_gate_keeps_pause_gameplay_frozen},
    {"death continue uses fixed E snapshot",
        &death_continue_uses_fixed_e_from_authoritative_snapshot},
    {"corrupt settings notice is deferred",
        &corrupt_settings_notice_is_deferred_until_first_settings},
    {"healthy settings loads have no notice",
        &healthy_settings_loads_do_not_queue_recovery_notice},
    {"close waits for successful Apply",
        &close_same_frame_waits_for_successful_apply},
    {"close waits for rejected Apply",
        &close_same_frame_waits_for_rejected_apply},
    {"loot filter preview is renderer-only until commit",
        &loot_filter_preview_is_renderer_only_until_commit},
    {"Apply rollback failure restores committed loot policy",
        &apply_rollback_failure_still_restores_committed_loot_policy},
};

}  // namespace

arpg::test::TestSuite pause_host_gate_suite() noexcept {
    return arpg::test::make_suite("pause_host_gate", kCases);
}
