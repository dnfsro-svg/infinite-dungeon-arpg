#include "host_validation_stage11c.hpp"

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_types.hpp"
#include "host_input.hpp"
#include "host_validation_input.hpp"
#include "host_validation_navigation.hpp"
#include "raylib_host.hpp"

#include <raylib.h>

#include <fstream>

namespace arpg::platform::host_validation {
namespace {

[[nodiscard]] const char* stage11c_scenario_name(
    Stage11CHudValidationScenario scenario) noexcept {
    switch (scenario) {
    case Stage11CHudValidationScenario::none: return "none";
    case Stage11CHudValidationScenario::normal_combat: return "normal_combat";
    case Stage11CHudValidationScenario::low_health_status: return "low_health_status";
    case Stage11CHudValidationScenario::cleared_exit: return "cleared_exit";
    case Stage11CHudValidationScenario::abyss_abandon: return "abyss_abandon";
    case Stage11CHudValidationScenario::level_up_points: return "level_up_points";
    case Stage11CHudValidationScenario::debug_overlay: return "debug_overlay";
    }
    return "invalid";
}

void write_stage11c_rect(std::ostream& stream, const char* name,
    HudRect rect) {
    stream << name << '=' << rect.x << ',' << rect.y << ','
           << rect.width << ',' << rect.height << '\n';
}

}  // namespace

PhysicalKeySnapshot inject_stage11c_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    const settings::SettingsData& settings_data,
    const dungeon::DungeonSnapshot& current,
    Stage11CHudValidationState& state) noexcept {
    using Scenario = Stage11CHudValidationScenario;
    if (config.stage11c_hud_validation == Scenario::none
            || snapshot.focus_lost) {
        return snapshot;
    }
    ++state.injected_frames;
    if (config.stage11c_hud_validation == Scenario::debug_overlay) {
        if (!state.debug_visible) snapshot.f1 = true;
        return snapshot;
    }
    if (!current.combat.has_value()) {
        return snapshot;
    }
    const combat::CombatSnapshot& combat_snapshot = *current.combat;
    if (current.phase == dungeon::RoomPhase::combat) {
        const combat::MonsterSnapshot* const target =
            nearest_living_monster(combat_snapshot);
        if (target == nullptr) return snapshot;
        const bool clears_room = config.stage11c_hud_validation
                == Scenario::cleared_exit
            || config.stage11c_hud_validation == Scenario::level_up_points
            || config.stage11c_hud_validation == Scenario::abyss_abandon;
        combat::Vec3 destination = target->position;
        if (clears_room) {
            destination.x += combat_snapshot.player.position.x
                    <= target->position.x ? -1.0F : 1.0F;
        }
        inject_validation_movement(snapshot, settings_data,
            validation_movement_toward(
                combat_snapshot.player.position, destination));
        if (clears_room && validation_attack_lane(combat_snapshot, *target)) {
            inject_validation_action(snapshot, settings_data,
                settings::SettingAction::light_attack, true);
        }
        return snapshot;
    }
    if (config.stage11c_hud_validation != Scenario::abyss_abandon
            || current.phase != dungeon::RoomPhase::awaiting_exit) {
        return snapshot;
    }
    const dungeon::ExitDirection direction = current.is_abyss
        ? dungeon::ExitDirection::right : validation_direction(config);
    inject_validation_movement(snapshot, settings_data,
        validation_exit_movement(combat_snapshot.player.position, direction));
    return snapshot;
}

std::uint64_t stage11c_production_snapshot_hash(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&hash](std::uint64_t value) noexcept {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    mix(snapshot.session_tick);
    mix(snapshot.root_seed);
    mix(snapshot.commit_generation);
    mix(snapshot.room_index);
    mix(snapshot.room_seed);
    mix(snapshot.depth);
    mix(snapshot.floor_room_index);
    mix(static_cast<std::uint64_t>(snapshot.phase));
    mix(snapshot.is_abyss ? 1U : 0U);
    mix(snapshot.abyss_exit_confirmation_armed ? 1U : 0U);
    mix(snapshot.remaining_targets);
    mix(snapshot.progression.level);
    mix(snapshot.progression.experience);
    mix(snapshot.progression.unspent_passive_points);
    if (snapshot.combat.has_value()) {
        const combat::PlayerSnapshot& player = snapshot.combat->player;
        mix(static_cast<std::uint64_t>(player.hp));
        mix(static_cast<std::uint64_t>(player.max_hp));
        mix(static_cast<std::uint64_t>(player.barrier));
        mix(player.slow_ticks);
        mix(player.corrosion_ticks);
        mix(player.invulnerability_ticks);
        for (const combat::MonsterSnapshot& monster : snapshot.combat->monsters) {
            mix(monster.active ? 1U : 0U);
            mix(static_cast<std::uint64_t>(monster.hp));
            mix(monster.spawn_ordinal);
        }
    }
    return hash;
}

bool stage11c_hud_validation_reached(
    const dungeon::DungeonSnapshot& snapshot,
    Stage11CHudValidationScenario scenario,
    const Stage11CHudValidationState& state, bool draw_debug) noexcept {
    using Scenario = Stage11CHudValidationScenario;
    if (!snapshot.combat.has_value()) {
        return false;
    }
    const combat::PlayerSnapshot& player = snapshot.combat->player;
    switch (scenario) {
    case Scenario::none:
        return false;
    case Scenario::normal_combat:
        return state.injected_frames >= 4U
            && snapshot.phase == dungeon::RoomPhase::combat
            && snapshot.remaining_targets != 0U;
    case Scenario::low_health_status:
        return player.hp > 0 && player.max_hp > 0
            && static_cast<std::int64_t>(player.hp) * 4
                <= static_cast<std::int64_t>(player.max_hp)
            && (player.slow_ticks != 0U || player.corrosion_ticks != 0U
                || player.invulnerability_ticks != 0U);
    case Scenario::cleared_exit:
        return snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && snapshot.progression.level > 1U;
    case Scenario::abyss_abandon:
        return snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && snapshot.abyss_exit_confirmation_armed;
    case Scenario::level_up_points:
        return snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && snapshot.last_levels_gained != 0U
            && snapshot.progression.unspent_passive_points != 0U;
    case Scenario::debug_overlay:
        return state.injected_frames >= 4U && draw_debug;
    }
    return false;
}

void write_stage11c_hud_validation_summary(const RaylibHostConfig& config,
    const Stage11CHudValidationState& state) noexcept {
    if (!config.validation_summary_file.has_value()
            || config.stage11c_hud_validation
                == Stage11CHudValidationScenario::none) {
        return;
    }
    try {
        std::ofstream stream(*config.validation_summary_file,
            std::ios::out | std::ios::trunc);
        if (!stream) return;
        stream << "scenario="
               << stage11c_scenario_name(config.stage11c_hud_validation) << '\n';
        write_stage11c_rect(stream, "safe_rect", state.layout.safe_area);
        write_stage11c_rect(stream, "player_rect", state.layout.player_panel);
        write_stage11c_rect(stream, "objective_rect", state.layout.objective_panel);
        write_stage11c_rect(stream, "navigation_rect", state.layout.navigation_panel);
        write_stage11c_rect(stream, "primary_notice_rect", state.layout.primary_notice);
        write_stage11c_rect(stream, "secondary_notice_rect", state.layout.secondary_notice);
        write_stage11c_rect(stream, "debug_rect", state.layout.debug_panel);
        const PlayerHudModel& player = state.model.player;
        stream << "player_values=" << player.hp << ',' << player.max_hp << ','
               << player.barrier << ',' << player.max_barrier << ','
               << static_cast<unsigned>(player.level) << ','
               << player.experience << ',' << player.required_experience << ','
               << static_cast<unsigned>(player.unspent_passive_points) << '\n'
               << "status_tags=";
        for (std::uint8_t index = 0U; index < player.status_tag_count; ++index) {
            if (index != 0U) stream << ',';
            stream << static_cast<unsigned>(player.status_tags[index]);
        }
        stream << '\n'
               << "objective=" << state.model.room.objective.bytes.data() << '\n'
               << "notice_kinds="
               << static_cast<unsigned>(state.notices.primary.kind) << ','
               << static_cast<unsigned>(state.notices.secondary.kind) << '\n'
               << "notice_texts="
               << state.notices.primary.text.bytes.data() << '|'
               << state.notices.secondary.text.bytes.data() << '\n'
               << "navigation_values=" << state.model.navigation.depth << ','
               << state.model.navigation.floor_room << ','
               << static_cast<unsigned>(state.model.navigation.ecology);
        for (std::uint32_t bias : state.model.navigation.biases) {
            stream << ',' << bias;
        }
        const bool stage11c_validation_result = state.captured
            && state.cjk_font_ready && state.production_snapshot_hash != 0U;
        stream << '\n'
               << "font_ready=" << (state.cjk_font_ready ? 1 : 0) << '\n'
               << "f1=" << (state.debug_visible ? 1 : 0) << '\n'
               << "production_snapshot_hash="
               << state.production_snapshot_hash << '\n'
               << "result="
               << (stage11c_validation_result ? "pass" : "fail") << '\n';
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to write stage11c HUD validation summary");
    }
}

}  // namespace arpg::platform::host_validation
