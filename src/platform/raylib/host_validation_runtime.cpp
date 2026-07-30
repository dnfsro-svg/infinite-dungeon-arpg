#include "host_validation.hpp"

#include "host_input.hpp"
#include "host_validation_stage10_11.hpp"
#include "host_validation_stage11b.hpp"
#include "host_validation_stage11c.hpp"
#include "host_validation_stage11d.hpp"
#include "host_validation_stage17.hpp"
#include "host_validation_state.hpp"
#include "dungeon_runtime.hpp"
#include "pause_menu_state.hpp"
#include "raylib_host.hpp"

#include <new>
#include <utility>

namespace arpg::platform {

namespace {

constexpr char kSettingsRecoveredDefaults[] = u8"设置已恢复默认值";

}  // namespace

struct HostValidationRuntime::Impl final {
    explicit Impl(const RaylibHostConfig& host_config,
        settings::SettingsLoadStatus load_status) noexcept
        : config(&host_config) {
        states.stage11b.load_status = load_status;
    }

    const RaylibHostConfig* config{};
    HostValidationStates states{};
    PhysicalKeySnapshot death_input_snapshot{};
    bool pending_stage11b_paused_visible_capture{};
    bool pending_stage11c_reached{};
    bool pending_stage11d_target_visible{};
    bool generic_capture_complete{};
};

std::unique_ptr<HostValidationRuntime> HostValidationRuntime::create(
    const RaylibHostConfig& config,
    settings::SettingsLoadStatus load_status) noexcept {
    std::unique_ptr<Impl> impl{
        new (std::nothrow) Impl{config, load_status}};
    if (impl == nullptr) return {};

    std::unique_ptr<HostValidationRuntime> runtime{
        new (std::nothrow) HostValidationRuntime{std::move(impl)}};
    if (runtime == nullptr) return {};
    return runtime;
}

HostValidationRuntime::HostValidationRuntime(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

HostValidationRuntime::~HostValidationRuntime() = default;

void HostValidationRuntime::set_render_readiness(
    bool cjk_font_ready, bool active_skill_atlases_ready) noexcept {
    impl_->states.stage11c.cjk_font_ready = cjk_font_ready;
    impl_->states.stage17.active_skill_atlases_ready = active_skill_atlases_ready;
}

PhysicalKeySnapshot HostValidationRuntime::inject_physical_edges(
    PhysicalKeySnapshot snapshot, const settings::SettingsData& input_settings,
    const dungeon::DungeonSnapshot& dungeon_snapshot,
    bool gameplay_rearm_required) noexcept {
    const PhysicalKeySnapshot stage11b_physical_keys =
        host_validation::inject_stage11b_physical_edges(
            snapshot, *impl_->config, impl_->states.stage11b);
    const PhysicalKeySnapshot stage11c_physical_keys =
        host_validation::inject_stage11c_physical_edges(
            stage11b_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11c);
    const PhysicalKeySnapshot stage11d_physical_keys =
        host_validation::inject_stage11d_physical_edges(
            stage11c_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11d);
    impl_->death_input_snapshot = stage11d_physical_keys;
    impl_->states.stage17.suspend_injection = gameplay_rearm_required;
    return host_validation::inject_stage17_physical_edges(
        stage11d_physical_keys, *impl_->config, input_settings,
        dungeon_snapshot, impl_->states.stage17);
}

bool HostValidationRuntime::should_continue_death(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    const bool pending = snapshot.death.has_value()
        && snapshot.death->can_continue && !snapshot.death->saving;
    const auto scenario = impl_->config->stage11_validation;
    const bool validation_continue =
        scenario == Stage11ValidationScenario::deep_continue
        || scenario == Stage11ValidationScenario::floor_one_continue;
    return pending && validation_continue
        && !impl_->states.stage11.continue_requested;
}

combat::MovementInput HostValidationRuntime::fixed_step_movement(
    dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    combat::MovementInput production_input) noexcept {
    if (impl_->config->stage11_validation
            != Stage11ValidationScenario::none) {
        return host_validation::stage11_validation_input(
            session, snapshot, *impl_->config, impl_->states.stage11);
    }
    if (impl_->config->stage10_validation
            != Stage10ValidationScenario::none) {
        return host_validation::stage10_validation_input(
            session, snapshot, *impl_->config, impl_->states.stage10);
    }
    return production_input;
}

void HostValidationRuntime::observe_fixed_tick() noexcept {
    ++impl_->states.stage11b.fixed_ticks;
}

void HostValidationRuntime::observe_death_continue_result(
    dungeon::RequestResult result) noexcept {
    const auto scenario = impl_->config->stage11_validation;
    const bool validation_continue =
        scenario == Stage11ValidationScenario::deep_continue
        || scenario == Stage11ValidationScenario::floor_one_continue;
    if (validation_continue
            && result != dungeon::RequestResult::rejected) {
        impl_->states.stage11.continue_requested = true;
    }
}

void HostValidationRuntime::observe_post_fixed_tick(
    const dungeon::DungeonSnapshot& snapshot,
    const items::ItemOwnershipState* ownership) noexcept {
    if (ownership == nullptr) return;
    host_validation::observe_stage11d_abyss_claim(
        impl_->states.stage11d, snapshot, *ownership);
}

bool HostValidationRuntime::fixed_step_target_reached(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    return host_validation::stage10_validation_reached(
        snapshot, *impl_->config, impl_->states.stage10)
        || host_validation::stage11_validation_reached(
            snapshot, *impl_->config, impl_->states.stage11);
}

const PhysicalKeySnapshot&
HostValidationRuntime::death_input_snapshot() const noexcept {
    return impl_->death_input_snapshot;
}

void HostValidationRuntime::observe_pause_transition(
    bool was_open, bool is_open) noexcept {
    if (impl_->config->stage11b_validation
            == Stage11BValidationScenario::paused_freeze
        && was_open && !is_open
        && impl_->states.stage11b.resume_input_injected) {
        impl_->states.stage11b.resume_observed = true;
        impl_->states.stage11b.resume_ticks_before =
            impl_->states.stage11b.fixed_ticks;
    }
}

void HostValidationRuntime::prepare_hud_snapshot(
    dungeon::DungeonSnapshot& snapshot) noexcept {
    if (impl_->config->stage11c_hud_validation
            == Stage11CHudValidationScenario::low_health_status
        && snapshot.combat.has_value()) {
        snapshot.combat->player.max_barrier = 1000;
        snapshot.combat->player.barrier = 625;
    }
}

void HostValidationRuntime::observe_hud(
    const dungeon::DungeonSnapshot& snapshot,
    const HudViewModel& model, HudNoticeView notices, bool draw_debug,
    int screen_width, int screen_height) noexcept {
    impl_->states.stage11c.debug_visible = draw_debug;
    const bool target_visible = host_validation::stage11c_hud_validation_reached(
        snapshot, impl_->config->stage11c_hud_validation,
        impl_->states.stage11c, draw_debug);
    if (target_visible) {
        ++impl_->states.stage11c.target_presented_frames;
        impl_->states.stage11c.production_snapshot_hash =
            host_validation::stage11c_production_snapshot_hash(snapshot);
        impl_->states.stage11c.model = model;
        impl_->states.stage11c.notices = notices;
        impl_->states.stage11c.layout = make_hud_layout(
            screen_width, screen_height, true);
    } else {
        impl_->states.stage11c.target_presented_frames = 0U;
    }
}

void HostValidationRuntime::observe_active_skill_draw(
    const dungeon::DungeonSnapshot& snapshot,
    const ActiveSkillDrawRuntimeStatus& draw_status) noexcept {
    const ActiveSkillDrawRuntimeStatus& status = draw_status;
    host_validation::observe_stage17_draw_runtime(
        *impl_->config, impl_->states.stage17, snapshot, status);
}

void HostValidationRuntime::observe_ground_loot(
    const dungeon::DungeonSnapshot& snapshot,
    const PauseMenuState& pause_menu,
    const DungeonRenderStatus& render_status,
    settings::LootFilterMode loot_filter,
    const GroundLootView& ground_loot_view, HudNoticeView notices,
    const items::ItemOwnershipState* ownership,
    int screen_width, int screen_height) noexcept {
    impl_->states.stage11d.target_visible =
        host_validation::stage11d_target_visible(
            *impl_->config, snapshot, pause_menu, render_status, loot_filter,
            ground_loot_view, notices, impl_->states.stage11d,
            screen_width, screen_height);
    if (impl_->states.stage11d.target_visible
            && !impl_->states.stage11d.captured) {
        host_validation::stage11d_record_semantics(
            impl_->states.stage11d, snapshot, ownership,
            ground_loot_view, notices);
    }
}

PresentationDecision HostValidationRuntime::observe_presented_frame(
    const dungeon::DungeonSnapshot& snapshot,
    const PauseMenuState& pause_menu, bool pause_cjk_ready) noexcept {
    if (impl_->states.stage11b.resume_observed) {
        impl_->states.stage11b.resume_ticks_after =
            impl_->states.stage11b.fixed_ticks;
    }
    if (impl_->config->stage11b_validation
            == Stage11BValidationScenario::corrupt_defaults
        && pause_menu.message == kSettingsRecoveredDefaults
        && pause_cjk_ready) {
        impl_->states.stage11b.recovery_notice_visible = true;
    }
    if (impl_->config->stage11b_validation
            == Stage11BValidationScenario::paused_freeze
        && pause_menu.screen != PauseScreen::closed) {
        if (impl_->states.stage11b.paused_presented == 0U) {
            impl_->states.stage11b.paused_ticks_before =
                impl_->states.stage11b.fixed_ticks;
            impl_->states.stage11b.player_monster_hash_before =
                host_validation::stage11b_snapshot_hash(snapshot);
        }
        ++impl_->states.stage11b.paused_presented;
        impl_->states.stage11b.paused_ticks_after =
            impl_->states.stage11b.fixed_ticks;
        impl_->states.stage11b.player_monster_hash_after =
            host_validation::stage11b_snapshot_hash(snapshot);
    }

    const bool stage10_target_visible =
        host_validation::stage10_validation_reached(
            snapshot, *impl_->config, impl_->states.stage10);
    const bool stage11_target_visible =
        host_validation::stage11_validation_reached(
            snapshot, *impl_->config, impl_->states.stage11);
    if (stage11_target_visible) {
        ++impl_->states.stage11.target_presented_frames;
    } else {
        impl_->states.stage11.target_presented_frames = 0U;
    }
    if (impl_->config->stage10_validation
            == Stage10ValidationScenario::chaos_expansion
        && stage10_target_visible) {
        ++impl_->states.stage10.chaos_presented_frames;
    }

    const bool stage10_reached = stage10_target_visible
        && (impl_->config->stage10_validation
                != Stage10ValidationScenario::chaos_expansion
            || impl_->states.stage10.chaos_presented_frames >= 16U);
    const bool stage11_reached = stage11_target_visible
        && impl_->states.stage11.target_presented_frames >= 4U;
    const bool stage11b_reached = host_validation::stage11b_validation_complete(
        *impl_->config, impl_->states.stage11b, pause_menu);
    const bool stage11c_reached =
        impl_->states.stage11c.target_presented_frames >= 4U;
    const bool stage11d_reached = impl_->states.stage11d.captured
        && (impl_->config->stage11d_loot_validation
                != Stage11DLootValidationScenario::rare_only_abyss
            || impl_->states.stage11d.abyss_claimed);
    const bool stage17_reached = host_validation::stage17_validation_complete(
        *impl_->config, impl_->states.stage17);
    const bool stage11b_visible_capture =
        (impl_->config->stage11b_validation
                == Stage11BValidationScenario::rebound_attack
            || impl_->config->stage11b_validation
                == Stage11BValidationScenario::conflict_swap)
        && impl_->states.stage11b.injected_frame == 21U;
    const bool stage11b_paused_visible_capture =
        impl_->config->stage11b_validation
            == Stage11BValidationScenario::paused_freeze
        && pause_menu.screen != PauseScreen::closed
        && impl_->states.stage11b.paused_presented >= 120U
        && !impl_->states.stage11b.pause_capture_while_paused;

    impl_->pending_stage11b_paused_visible_capture =
        stage11b_paused_visible_capture;
    impl_->pending_stage11c_reached = stage11c_reached;
    impl_->pending_stage11d_target_visible =
        impl_->states.stage11d.target_visible;

    PresentationDecision decision{};
    decision.validation_complete = stage10_reached || stage11_reached
        || stage11b_reached || stage11c_reached
        || stage11d_reached || stage17_reached;
    decision.generic_capture_visible = decision.validation_complete
        || impl_->states.stage11d.target_visible
        || stage11b_visible_capture
        || stage11b_paused_visible_capture;
    decision.generic_capture_complete = impl_->generic_capture_complete;
    decision.stage17_capture_path = host_validation::stage17_capture_path(
        *impl_->config, impl_->states.stage17);
    if (decision.stage17_capture_path.has_value()) {
        decision.capture_owner = CaptureOwner::stage17;
    } else if (decision.generic_capture_visible
            && !decision.generic_capture_complete
            && impl_->config->validation_capture_file.has_value()) {
        decision.capture_owner = CaptureOwner::generic_validation;
    }
    return decision;
}

void HostValidationRuntime::observe_capture_result(
    CaptureOwner owner, bool succeeded) noexcept {
    if (owner != CaptureOwner::generic_validation) {
        if (owner == CaptureOwner::stage17 && succeeded) {
            host_validation::mark_stage17_capture_complete(
                impl_->states.stage17);
        }
        return;
    }
    impl_->states.stage11b.pause_capture_while_paused =
        impl_->pending_stage11b_paused_visible_capture;
    if (!succeeded) return;
    impl_->generic_capture_complete = true;
    if (impl_->pending_stage11c_reached) {
        impl_->states.stage11c.captured = true;
    }
    if (impl_->pending_stage11d_target_visible) {
        impl_->states.stage11d.captured = true;
    }
}

void HostValidationRuntime::write_summaries(
    CleanShutdownState clean_shutdown_state,
    const PauseMenuState& pause_menu) noexcept {
    impl_->states.stage17.clean_shutdown_exact_ready =
        clean_shutdown_state == CleanShutdownState::ready;
    host_validation::write_stage11b_validation_summary(
        *impl_->config, impl_->states.stage11b, pause_menu);
    host_validation::write_stage11c_hud_validation_summary(
        *impl_->config, impl_->states.stage11c);
    host_validation::write_stage11d_loot_validation_summary(
        *impl_->config, impl_->states.stage11d, pause_menu);
    host_validation::write_stage17_validation_summary(
        *impl_->config, impl_->states.stage17);
}

void HostValidationRuntime::observe_combat_event(
    const combat::CombatEvent& event) noexcept {
    host_validation::observe_stage17_combat_event(&impl_->states.stage17, event);
}

void HostValidationRuntime::observe_snapshot(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    host_validation::observe_stage17_snapshot(
        *impl_->config, impl_->states.stage17, snapshot);
}

void HostValidationRuntime::observe_inventory(
    const InventoryRenderer& inventory,
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    host_validation::observe_stage17_inventory(
        *impl_->config, impl_->states.stage17, inventory, snapshot);
}

void HostValidationRuntime::observe_submitted_actions(
    const SubmittedFrameActions& submitted_actions) noexcept {
    host_validation::observe_stage17_submitted_actions(
        *impl_->config, impl_->states.stage17, submitted_actions);
    if (impl_->config->stage11b_validation
            == Stage11BValidationScenario::rebound_attack) {
        if (impl_->states.stage11b.injected_frame == 28U) {
            impl_->states.stage11b.old_attack_checked = true;
            impl_->states.stage11b.old_attack_count +=
                submitted_actions.combat[0] ? 1U : 0U;
        } else if (impl_->states.stage11b.injected_frame == 29U) {
            impl_->states.stage11b.new_attack_count +=
                submitted_actions.combat[0] ? 1U : 0U;
        }
    }
}

}  // namespace arpg::platform
