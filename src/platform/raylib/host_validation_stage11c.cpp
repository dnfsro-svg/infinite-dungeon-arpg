#include "host_validation_stage11c.hpp"

#include "combat/active_skill_runtime.hpp"
#include "combat/combat_types.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon/dungeon_types.hpp"
#include "host_input.hpp"
#include "host_validation_input.hpp"
#include "host_validation_navigation.hpp"
#include "raylib_host.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
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

[[nodiscard]] bool stage11c_player_controllable(
    const combat::CombatSnapshot& snapshot) noexcept {
    return snapshot.player.hp > 0
        && snapshot.player.hurt_ticks == 0U
        && snapshot.player.hit_stop_ticks == 0U
        && snapshot.player.active_attack == combat::AttackId::none
        && snapshot.active_skill.id == skills::ActiveSkillId::none
        && snapshot.diagnostics.input_size == 0U;
}

[[nodiscard]] combat::MovementInput stage11c_recovery_movement(
    combat::MovementInput requested,
    Stage11CHudValidationState& state) noexcept {
    if (state.recovery_movement_frames == 0U) return requested;
    --state.recovery_movement_frames;
    combat::MovementInput recovery{};
    const bool positive = (state.recovery_direction & 1U) == 0U;
    if (requested.x != 0) {
        recovery.y = positive ? 1 : -1;
    } else {
        recovery.x = positive ? 1 : -1;
    }
    return recovery;
}

void stage11c_observe_movement_progress(
    const combat::CombatSnapshot& snapshot,
    Stage11CHudValidationState& state) noexcept {
    const combat::Vec3 player = snapshot.player.position;
    if (state.previous_player_position_valid
            && state.movement_was_requested
            && stage11c_player_controllable(snapshot)) {
        const float dx = player.x - state.previous_player_position.x;
        const float dy = player.y - state.previous_player_position.y;
        if (dx * dx + dy * dy <= 1.0e-6F) {
            if (state.stalled_movement_frames < 0xFFFFU) {
                ++state.stalled_movement_frames;
            }
        } else {
            state.stalled_movement_frames = 0U;
        }
    } else if (!state.movement_was_requested) {
        state.stalled_movement_frames = 0U;
    }
    if (state.stalled_movement_frames >= 4U) {
        state.stalled_movement_frames = 0U;
        state.recovery_movement_frames = 12U;
        state.recovery_direction = static_cast<std::uint8_t>(
            state.recovery_direction + 1U);
    }
    state.previous_player_position = player;
    state.previous_player_position_valid = true;
    state.movement_was_requested = false;
}

[[nodiscard]] bool stage11c_inject_area_skill(
    PhysicalKeySnapshot& snapshot,
    const combat::CombatSnapshot& combat_snapshot) noexcept {
    if (!stage11c_player_controllable(combat_snapshot)) return false;
    const combat::PlayerSnapshot& player = combat_snapshot.player;
    const float facing = player.facing == combat::Facing::right ? 1.0F : -1.0F;
    const float storm_x = player.position.x
        + facing * combat::kStormCenterForward;
    bool storm_target = false;
    bool draw_target = false;
    for (const combat::MonsterSnapshot& monster : combat_snapshot.monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float storm_dx = monster.position.x - storm_x;
        const float storm_dy = monster.position.y - player.position.y;
        storm_target = storm_target
            || storm_dx * storm_dx + storm_dy * storm_dy
                <= combat::kStormStrikeRadius * combat::kStormStrikeRadius;
        const float forward =
            (monster.position.x - player.position.x) * facing;
        const float half_width = forward >= 0.0F
                && forward <= combat::kDrawSlashRange
            ? combat::kDrawSlashHalfWidthAtEnd
                * (forward / combat::kDrawSlashRange)
            : -1.0F;
        draw_target = draw_target || (half_width >= 0.0F
            && std::fabs(monster.position.y - player.position.y)
                <= half_width);
    }
    if (storm_target && combat_snapshot.skill_cooldowns[1U] == 0U) {
        snapshot.active_skill_slots[1U] = true;
        return true;
    }
    if (draw_target && combat_snapshot.skill_cooldowns[0U] == 0U) {
        snapshot.active_skill_slots[0U] = true;
        return true;
    }
    return false;
}

[[nodiscard]] combat::Vec3 stage11c_ranged_stance(
    const combat::CombatSnapshot& combat_snapshot,
    const combat::MonsterSnapshot& target) noexcept {
    constexpr float room_center_x =
        (combat::room_bounds::min_x + combat::room_bounds::max_x) * 0.5F;
    const combat::Facing facing = target.position.x >= room_center_x
        ? combat::Facing::right : combat::Facing::left;
    const float direction = facing == combat::Facing::right ? 1.0F : -1.0F;
    return {
        std::clamp(target.position.x
                - direction * combat::kDrawSlashRange,
            combat::room_bounds::min_x + 0.5F,
            combat::room_bounds::max_x - 0.5F),
        std::clamp(target.position.y,
            combat::room_bounds::min_y
                + combat::kDrawSlashHalfWidthAtEnd,
            combat::room_bounds::max_y
                - combat::kDrawSlashHalfWidthAtEnd),
        combat_snapshot.player.position.z,
    };
}

[[nodiscard]] bool stage11c_nearby_threat(
    const combat::CombatSnapshot& combat_snapshot) noexcept {
    for (const combat::MonsterSnapshot& monster : combat_snapshot.monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float dx = monster.position.x
            - combat_snapshot.player.position.x;
        const float dy = monster.position.y
            - combat_snapshot.player.position.y;
        if (dx * dx + dy * dy <= 2.6F * 2.6F) return true;
    }
    return false;
}

}  // namespace

PhysicalKeySnapshot inject_stage11c_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    const settings::SettingsData& settings_data,
    const dungeon::DungeonSnapshot& current,
    Stage11CHudValidationState& state) noexcept {
    using Scenario = Stage11CHudValidationScenario;
    if (config.stage11c_hud_validation != Scenario::none
            && !snapshot.focus_lost) {
        ++state.injected_frames;
        if (config.stage11c_hud_validation == Scenario::debug_overlay) {
            if (!state.debug_visible) snapshot.f1 = true;
        } else if (current.combat.has_value()) {
            const combat::CombatSnapshot& combat_snapshot = *current.combat;
            stage11c_observe_movement_progress(combat_snapshot, state);
            if (current.phase == dungeon::RoomPhase::combat) {
                const combat::MonsterSnapshot* const target =
                    nearest_living_monster(combat_snapshot);
                if (target != nullptr) {
                    const bool clears_room = config.stage11c_hud_validation
                            == Scenario::cleared_exit
                        || config.stage11c_hud_validation
                            == Scenario::level_up_points
                        || config.stage11c_hud_validation
                            == Scenario::abyss_abandon;
                    combat::Vec3 destination = target->position;
                    if (clears_room) {
                        destination = stage11c_ranged_stance(
                            combat_snapshot, *target);
                    }
                    const combat::MovementInput movement =
                        stage11c_recovery_movement(
                            validation_movement_toward(
                                combat_snapshot.player.position, destination),
                            state);
                    inject_validation_movement(
                        snapshot, settings_data, movement);
                    state.movement_was_requested =
                        movement.x != 0 || movement.y != 0;
                    const bool skill_requested = clears_room
                        && stage11c_inject_area_skill(
                            snapshot, combat_snapshot);
                    if (clears_room && !skill_requested
                            && stage11c_player_controllable(combat_snapshot)
                            && combat_snapshot.player.position.z <= 0.01F
                            && stage11c_nearby_threat(combat_snapshot)) {
                        inject_validation_action(snapshot, settings_data,
                            settings::SettingAction::jump, true);
                    } else if (clears_room && !skill_requested
                            && validation_attack_lane(combat_snapshot, *target)) {
                        inject_validation_action(snapshot, settings_data,
                            settings::SettingAction::light_attack, true);
                    }
                }
            } else if (config.stage11c_hud_validation == Scenario::abyss_abandon
                    && current.phase == dungeon::RoomPhase::awaiting_exit) {
                const dungeon::ExitDirection direction = current.is_abyss
                    ? dungeon::ExitDirection::right : validation_direction(config);
                inject_validation_movement(snapshot, settings_data,
                    validation_exit_movement(
                        combat_snapshot.player.position, direction));
            }
        }
    }
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
    if (config.validation_summary_file.has_value()
            && config.stage11c_hud_validation
                != Stage11CHudValidationScenario::none) {
        try {
        std::ofstream stream(*config.validation_summary_file,
            std::ios::out | std::ios::trunc);
        if (stream) {
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
        }
        } catch (...) {
            TraceLog(LOG_WARNING, "failed to write stage11c HUD validation summary");
        }
    }
}

}  // namespace arpg::platform::host_validation
