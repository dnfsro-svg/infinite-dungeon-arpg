#include "host_validation.hpp"

#include "host_input.hpp"
#include "host_validation_stage10_11.hpp"
#include "host_validation_stage11b.hpp"
#include "host_validation_stage11c.hpp"
#include "host_validation_stage11d.hpp"
#include "host_validation_stage17.hpp"
#include "host_validation_state.hpp"
#include "raylib_host.hpp"

#include <new>
#include <utility>

namespace arpg::platform {

struct HostValidationRuntime::Impl final {
    explicit Impl(const RaylibHostConfig& host_config,
        settings::SettingsLoadStatus load_status) noexcept
        : config(&host_config) {
        states.stage11b.load_status = load_status;
    }

    const RaylibHostConfig* config{};
    HostValidationStates states{};
    PhysicalKeySnapshot death_input_snapshot{};
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

host_validation::Stage10ValidationState& HostValidationStateAccess::stage10(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage10;
}

host_validation::Stage11ValidationState& HostValidationStateAccess::stage11(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage11;
}

host_validation::Stage11BValidationState& HostValidationStateAccess::stage11b(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage11b;
}

host_validation::Stage11CHudValidationState& HostValidationStateAccess::stage11c(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage11c;
}

host_validation::Stage11DLootValidationState& HostValidationStateAccess::stage11d(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage11d;
}

host_validation::Stage17SkillStonesValidationState&
HostValidationStateAccess::stage17(HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage17;
}

const PhysicalKeySnapshot& HostValidationStateAccess::death_input_snapshot(
    const HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->death_input_snapshot;
}

}  // namespace arpg::platform
