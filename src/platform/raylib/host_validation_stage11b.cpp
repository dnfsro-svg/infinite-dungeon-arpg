#include "host_validation_stage11b.hpp"

#include "host_input.hpp"
#include "host_validation_input.hpp"
#include "pause_menu_state.hpp"
#include "raylib_host.hpp"
#include "stable_key_raylib.hpp"

#include <raylib.h>

#include <fstream>

namespace arpg::platform::host_validation {
namespace {

void inject_stage11b_open_settings(PhysicalKeySnapshot& snapshot,
    std::uint32_t frame) noexcept {
    if (frame == 1U) snapshot.escape = true;
    if (frame == 2U) inject_validation_pressed(snapshot, settings::StableKey::arrow_down);
    if (frame == 3U) snapshot.enter = true;
}

}  // namespace

PhysicalKeySnapshot inject_stage11b_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    Stage11BValidationState& state) noexcept {
    if (config.stage11b_validation == Stage11BValidationScenario::none) {
        return snapshot;
    }
    // Preserve every sampled production input.  A deterministic scenario only
    // adds physical key edges after the actual raylib window has focus.
    if (snapshot.focus_lost) return snapshot;
    const std::uint32_t frame = ++state.injected_frame;
    switch (config.stage11b_validation) {
    case Stage11BValidationScenario::paused_freeze:
        if (frame == 2U || (state.paused_presented >= 120U
                && !state.resume_input_injected)) {
            snapshot.escape = true;
            state.resume_input_injected = frame != 2U;
        }
        break;
    case Stage11BValidationScenario::settings_page:
    case Stage11BValidationScenario::restarted_settings:
    case Stage11BValidationScenario::single_slot_recovery:
    case Stage11BValidationScenario::corrupt_defaults:
        inject_stage11b_open_settings(snapshot, frame);
        break;
    case Stage11BValidationScenario::rebound_attack:
    case Stage11BValidationScenario::conflict_swap:
        if (frame == 1U) snapshot.escape = true;
        else if (frame == 2U || (frame >= 4U && frame <= 15U)
            || (frame >= 18U && frame <= 24U)) {
            inject_validation_pressed(snapshot, settings::StableKey::arrow_down);
        } else if (frame == 3U || frame == 16U || frame == 25U) {
            snapshot.enter = true;
        } else if (frame == 17U) {
            inject_validation_pressed(snapshot,
                config.stage11b_validation == Stage11BValidationScenario::rebound_attack
                    ? settings::StableKey::u : settings::StableKey::k);
        } else if (config.stage11b_validation
                       == Stage11BValidationScenario::rebound_attack
                   && (frame == 26U || frame == 27U)) {
            snapshot.escape = true;
        } else if (config.stage11b_validation
                       == Stage11BValidationScenario::rebound_attack
                   && frame == 28U) {
            inject_validation_pressed(snapshot, settings::StableKey::j);
        } else if (config.stage11b_validation
                       == Stage11BValidationScenario::rebound_attack
                   && frame == 29U) {
            inject_validation_pressed(snapshot, settings::StableKey::u);
        }
        break;
    case Stage11BValidationScenario::none:
        break;
    }
    return snapshot;
}

bool stage11b_validation_complete(
    const RaylibHostConfig& config, const Stage11BValidationState& state,
    const PauseMenuState& pause_menu) noexcept {
    switch (config.stage11b_validation) {
    case Stage11BValidationScenario::none: return false;
    case Stage11BValidationScenario::paused_freeze:
        return state.paused_presented >= 120U && state.pause_capture_while_paused
            && state.resume_observed
            && state.resume_ticks_after == state.resume_ticks_before + 1U;
    case Stage11BValidationScenario::settings_page:
    case Stage11BValidationScenario::restarted_settings:
    case Stage11BValidationScenario::single_slot_recovery:
    case Stage11BValidationScenario::corrupt_defaults:
        return state.injected_frame >= 4U
            && pause_menu.screen == PauseScreen::settings;
    case Stage11BValidationScenario::rebound_attack:
        return state.injected_frame >= 29U && state.old_attack_checked
            && state.new_attack_count != 0U
            && pause_menu.screen == PauseScreen::closed;
    case Stage11BValidationScenario::conflict_swap:
        return state.injected_frame >= 25U
            && pause_menu.screen == PauseScreen::settings;
    }
    return false;
}

std::uint64_t stage11b_snapshot_hash(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&hash](std::uint64_t value) noexcept {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    mix(snapshot.depth);
    mix(snapshot.room_index);
    if (snapshot.combat.has_value()) {
        const combat::CombatSnapshot& combat = *snapshot.combat;
        mix(static_cast<std::uint64_t>(combat.player.hp));
        for (const combat::MonsterSnapshot& monster : combat.monsters) {
            mix(static_cast<std::uint64_t>(monster.hp));
            mix(monster.active ? 1U : 0U);
        }
    }
    return hash;
}

void write_stage11b_validation_summary(const RaylibHostConfig& config,
    const Stage11BValidationState& state,
    const PauseMenuState& pause_menu) noexcept {
    if (!config.validation_summary_file.has_value()
        || config.stage11b_validation == Stage11BValidationScenario::none) {
        return;
    }
    try {
        std::ofstream stream(*config.validation_summary_file,
            std::ios::out | std::ios::trunc);
        if (!stream) return;
        const auto light = settings::binding_for(pause_menu.committed,
            settings::SettingAction::light_attack);
        const auto jump = settings::binding_for(pause_menu.committed,
            settings::SettingAction::jump);
        const auto draft_light = settings::binding_for(pause_menu.draft,
            settings::SettingAction::light_attack);
        stream << "scenario=" << static_cast<unsigned>(config.stage11b_validation) << '\n'
               << "injected_frame=" << state.injected_frame << '\n'
               << "paused_tick_before=" << state.paused_ticks_before << '\n'
               << "paused_tick_after=" << state.paused_ticks_after << '\n'
               << "pause_capture_while_paused="
               << (state.pause_capture_while_paused ? 1 : 0) << '\n'
               << "resume_tick_before=" << state.resume_ticks_before << '\n'
               << "resume_tick_after=" << state.resume_ticks_after << '\n'
               << "player_monster_hash_before=" << state.player_monster_hash_before << '\n'
               << "player_monster_hash_after=" << state.player_monster_hash_after << '\n'
               << "committed_revision=" << pause_menu.committed.revision << '\n'
               << "old_attack_count=" << state.old_attack_count << '\n'
               << "new_attack_count=" << state.new_attack_count << '\n'
               << "recovery_notice_visible="
               << (state.recovery_notice_visible ? 1 : 0) << '\n'
               << "pause_screen=" << static_cast<unsigned>(pause_menu.screen) << '\n'
               << "pause_row=" << pause_menu.selected_row << '\n'
               << "light_attack=" << stable_key_label(light) << '\n'
               << "draft_light_attack=" << stable_key_label(draft_light) << '\n'
               << "jump=" << stable_key_label(jump) << '\n'
               << "load_status=" << static_cast<unsigned>(state.load_status) << '\n';
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to write stage11b validation summary");
    }
}

}  // namespace arpg::platform::host_validation
