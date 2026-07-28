#include "host_validation_stage10_11.hpp"

#include "combat/active_skill_runtime.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon_view_math.hpp"
#include "host_validation_navigation.hpp"
#include "raylib_host.hpp"

#include <cmath>
#include <cstddef>

namespace arpg::platform::host_validation {
namespace {

bool has_environment_visual(
    const dungeon::DungeonSnapshot& snapshot,
    combat::HazardKind kind,
    bool require_warning) noexcept {
    if (!snapshot.combat.has_value()) return false;
    for (std::size_t index = 0U;
         index < snapshot.combat->hazard_count; ++index) {
        const combat::HazardSnapshot& hazard = snapshot.combat->hazards[index];
        if (hazard.active
                && hazard.source == combat::HazardSource::abyss_environment
                && hazard.kind == kind
                && (!require_warning || hazard.telegraph_ticks != 0U)) {
            return true;
        }
    }
    return false;
}

}  // namespace

combat::MovementInput stage10_validation_input(
    dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    Stage10ValidationState& state) noexcept {
    const Stage10ValidationScenario scenario = config.stage10_validation;
    state.entered_abyss = state.entered_abyss || snapshot.is_abyss;
    if (scenario == Stage10ValidationScenario::room_reset
            && snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::combat
            && !state.reset_requested) {
        state.reset_requested = session.reset_current_room()
            != dungeon::RequestResult::rejected;
        return {};
    }
    if (scenario == Stage10ValidationScenario::player_death
            && snapshot.is_abyss) {
        return {};
    }
    if (!snapshot.combat.has_value()) return {};
    if (snapshot.phase == dungeon::RoomPhase::combat) {
        const auto* const target = nearest_living_monster(*snapshot.combat);
        if (target == nullptr) return {};
        if (scenario == Stage10ValidationScenario::abyss_hole_descent
                && snapshot.is_abyss && snapshot.remaining_targets == 1U) {
            const auto& monster = target->position;
            const float x = monster.x - kHoleCenter.x;
            const float y = monster.y - kHoleCenter.y;
            if (x * x + y * y > 1.44F) {
                return validation_movement_toward(
                    snapshot.combat->player.position, kHoleCenter);
            }
        }
        const auto movement = validation_movement_toward(
            snapshot.combat->player.position, target->position);
        const auto& player = snapshot.combat->player;
        const bool skill_ready = (!snapshot.is_abyss
                || scenario == Stage10ValidationScenario::abyss_hole_descent)
            && player.hp > 0
            && player.hurt_ticks == 0U && player.hit_stop_ticks == 0U
            && player.active_attack == combat::AttackId::none
            && snapshot.combat->active_skill.id == skills::ActiveSkillId::none
            && snapshot.combat->diagnostics.input_size == 0U;
        if (skill_ready) {
            const float facing = player.facing == combat::Facing::right
                ? 1.0F : -1.0F;
            const float storm_center_x = player.position.x
                + facing * combat::kStormCenterForward;
            const float storm_dx = target->position.x - storm_center_x;
            const float storm_dy = target->position.y - player.position.y;
            const bool storm_target = storm_dx * storm_dx
                    + storm_dy * storm_dy
                <= combat::kStormStrikeRadius * combat::kStormStrikeRadius;
            combat::SkillCastResult storm = combat::SkillCastResult::none;
            if (storm_target) {
                storm = session.request_active_skill_slot(1U);
                if (storm == combat::SkillCastResult::accepted) return movement;
            }
            if (!storm_target
                    || storm == combat::SkillCastResult::cooling_down) {
                const float forward =
                    (target->position.x - player.position.x) * facing;
                const float half_width = forward >= 0.0F
                        && forward <= combat::kDrawSlashRange
                    ? combat::kDrawSlashHalfWidthAtEnd
                        * (forward / combat::kDrawSlashRange)
                    : -1.0F;
                const bool draw_target = half_width >= 0.0F
                    && std::fabs(target->position.y - player.position.y)
                        <= half_width;
                if (draw_target
                        && session.request_active_skill_slot(0U)
                            == combat::SkillCastResult::accepted) {
                    return movement;
                }
            }
        }
        if (snapshot.combat->player.hurt_ticks == 0U
                && snapshot.combat->player.active_attack
                    == combat::AttackId::none
                && snapshot.combat->diagnostics.input_size == 0U
                && validation_attack_lane(*snapshot.combat, *target)) {
            static_cast<void>(session.queue_action(combat::Action::light));
        }
        return movement;
    }
    if (snapshot.phase != dungeon::RoomPhase::awaiting_exit) return {};
    if (!snapshot.is_abyss) {
        return validation_exit_movement(snapshot.combat->player.position,
            validation_direction(config));
    }
    if (scenario == Stage10ValidationScenario::exit_confirmation) {
        return snapshot.abyss_exit_confirmation_armed
            ? combat::MovementInput{}
            : validation_exit_movement(snapshot.combat->player.position,
                dungeon::ExitDirection::right);
    }
    if (scenario == Stage10ValidationScenario::abyss_hole_descent) {
        state.descent_warning_seen = state.descent_warning_seen
            || snapshot.abyss_exit_confirmation_armed;
        const combat::MovementInput movement = validation_movement_toward(
            snapshot.combat->player.position, kHoleCenter);
        if (can_prompt_descent(snapshot, snapshot.combat->player.position)) {
            static_cast<void>(session.request_descent(true));
        }
        return movement;
    }
    return {};
}

combat::MovementInput stage11_validation_input(
    dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    Stage11ValidationState& state) noexcept {
    state.entered_abyss = state.entered_abyss || snapshot.is_abyss;
    state.saw_depth_two = state.saw_depth_two || snapshot.depth > 1U;
    const auto scenario = config.stage11_validation;
    const bool drive_to_abyss = scenario
        == Stage11ValidationScenario::abyss_death_recap
        && !state.entered_abyss;
    const bool drive_to_depth = scenario
        == Stage11ValidationScenario::deep_continue
        && !state.saw_depth_two;
    if (!drive_to_abyss && !drive_to_depth) return {};
    if (!snapshot.combat.has_value()) return {};
    if (snapshot.phase == dungeon::RoomPhase::combat) {
        const auto* const target = nearest_living_monster(*snapshot.combat);
        if (target == nullptr) return {};
        const auto movement = validation_movement_toward(
            snapshot.combat->player.position, target->position);
        if (snapshot.combat->diagnostics.input_size == 0U
                && validation_attack_lane(*snapshot.combat, *target)) {
            static_cast<void>(session.queue_action(combat::Action::light));
        }
        return movement;
    }
    if (snapshot.phase != dungeon::RoomPhase::awaiting_exit) return {};
    if (drive_to_abyss) {
        return validation_exit_movement(snapshot.combat->player.position,
            validation_direction(config));
    }
    const auto movement = validation_movement_toward(
        snapshot.combat->player.position, kHoleCenter);
    if (can_prompt_descent(snapshot, snapshot.combat->player.position)) {
        static_cast<void>(session.request_descent(true));
    }
    return movement;
}

bool stage11_validation_reached(const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    const Stage11ValidationState& state) noexcept {
    const bool pending = snapshot.death.has_value()
        && snapshot.death->can_continue && !snapshot.death->saving;
    switch (config.stage11_validation) {
    case Stage11ValidationScenario::none: return false;
    case Stage11ValidationScenario::normal_death_recap:
    case Stage11ValidationScenario::restart_same_recap:
        return pending && !snapshot.death->checkpoint.death_was_abyss;
    case Stage11ValidationScenario::abyss_death_recap:
        return pending && state.entered_abyss
            && snapshot.death->checkpoint.death_was_abyss;
    case Stage11ValidationScenario::deep_continue:
        return state.continue_requested && state.saw_depth_two
            && !snapshot.death.has_value() && snapshot.depth == 1U
            && snapshot.combat.has_value()
            && snapshot.combat->player.hp > 0;
    case Stage11ValidationScenario::floor_one_continue:
        return state.continue_requested && !snapshot.death.has_value()
            && snapshot.depth == 1U && snapshot.combat.has_value()
            && snapshot.combat->player.hp > 0;
    }
    return false;
}

bool stage10_validation_reached(
    const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    const Stage10ValidationState& state) noexcept {
    switch (config.stage10_validation) {
    case Stage10ValidationScenario::none:
        return false;
    case Stage10ValidationScenario::abyss_door: {
        const auto direction = validation_direction(config);
        const std::size_t index = static_cast<std::size_t>(direction);
        return !snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && index < snapshot.abyss_doors.size()
            && snapshot.abyss_doors[index];
    }
    case Stage10ValidationScenario::thunderstorm_warning:
        return snapshot.abyss_rule == abyss::AbyssRuleId::thunderstorm
            && has_environment_visual(snapshot,
                combat::HazardKind::thunderstorm, true);
    case Stage10ValidationScenario::hunting_flames_warning:
        return snapshot.abyss_rule == abyss::AbyssRuleId::hunting_flames
            && has_environment_visual(snapshot,
                combat::HazardKind::hunting_flame, true);
    case Stage10ValidationScenario::chaos_expansion:
        return snapshot.abyss_rule == abyss::AbyssRuleId::chaos_expansion
            && has_environment_visual(snapshot,
                combat::HazardKind::chaos_expansion, false);
    case Stage10ValidationScenario::reward_chest:
        return snapshot.is_abyss && snapshot.ground_item_count != 0U;
    case Stage10ValidationScenario::pending_reward:
        return snapshot.is_abyss && snapshot.abyss_pending_rewards != 0U;
    case Stage10ValidationScenario::exit_confirmation:
        return snapshot.is_abyss
            && snapshot.abyss_exit_confirmation_armed;
    case Stage10ValidationScenario::player_death:
    case Stage10ValidationScenario::room_reset:
        return state.entered_abyss && !snapshot.is_abyss;
    case Stage10ValidationScenario::leave_started:
        return snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::combat;
    case Stage10ValidationScenario::restarted_failed:
        return !snapshot.is_abyss && snapshot.room_index == 1U;
    case Stage10ValidationScenario::abyss_hole_descent:
        return state.entered_abyss && state.descent_warning_seen
            && snapshot.depth > 1U;
    }
    return false;
}

}  // namespace arpg::platform::host_validation
