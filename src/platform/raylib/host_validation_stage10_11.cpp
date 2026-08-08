#include "host_validation_stage10_11.hpp"

#include "combat/active_skill_runtime.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_spatial_grid.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon_view_math.hpp"
#include "host_validation_navigation.hpp"
#include "host_validation_stage11c.hpp"
#include "raylib_host.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace arpg::platform::host_validation {
namespace {

constexpr float kGridRouteArrivalTolerance = 0.20F;
constexpr std::uint8_t kGridRouteMaximumRejoins = 4U;

enum class GridRouteProgress : std::uint8_t {
    none,
    moved,
    recovered,
    unreachable,
};

struct AttackGeometry final {
    bool storm{};
    bool draw{};
    bool light{};

    [[nodiscard]] bool any() const noexcept {
        return storm || draw || light;
    }
};

float squared_xy_distance(combat::Vec3 lhs, combat::Vec3 rhs) noexcept {
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return dx * dx + dy * dy;
}

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

const combat::MonsterSnapshot* stage10_ranged_target(
    const combat::CombatSnapshot& combat,
    Stage10ValidationState& state) noexcept {
    for (const combat::MonsterSnapshot& monster : combat.monsters) {
        if (monster.active && monster.hp > 0
                && monster.monster_ordinal == state.ranged_target_ordinal) {
            return &monster;
        }
    }
    const combat::MonsterSnapshot* target = nullptr;
    bool best_in_light_lane = false;
    float best_distance = 0.0F;
    for (const combat::MonsterSnapshot& monster : combat.monsters) {
        if (!monster.active || monster.hp <= 0
                || monster.monster_ordinal
                    == state.stalled_target_ordinal) {
            continue;
        }
        const bool in_light_lane = state.melee_chain
            && validation_attack_lane(combat, monster);
        if (state.recover_until_light_lane && !in_light_lane) continue;
        const float distance = squared_xy_distance(
            combat.player.position, monster.position);
        const bool better_tier = state.melee_chain
            && in_light_lane && !best_in_light_lane;
        const bool same_tier = !state.melee_chain
            || in_light_lane == best_in_light_lane;
        if (target == nullptr || better_tier
                || (same_tier && (distance < best_distance
                || (distance == best_distance
                    && monster.monster_ordinal
                        < target->monster_ordinal)))) {
            target = &monster;
            best_in_light_lane = in_light_lane;
            best_distance = distance;
        }
    }
    if (target == nullptr) return nullptr;
    state.stalled_target_ordinal = combat::kInvalidMonsterOrdinal;
    state.recover_until_light_lane = false;
    state.recovery_target = {};
    state.recovery_target_valid = false;
    state.ranged_target_ordinal = target->monster_ordinal;
    if (state.melee_chain) {
        state.ranged_stance = {};
        state.ranged_facing = combat.player.facing;
        state.close_for_light = true;
    } else {
        constexpr float room_center_x =
            (combat::room_bounds::min_x
                + combat::room_bounds::max_x) * 0.5F;
        state.ranged_facing = target->position.x >= room_center_x
            ? combat::Facing::right : combat::Facing::left;
        const float facing = state.ranged_facing == combat::Facing::right
            ? 1.0F : -1.0F;
        state.ranged_stance = {
            std::clamp(
                target->position.x - facing * combat::kDrawSlashRange,
                combat::room_bounds::min_x + 0.5F,
                combat::room_bounds::max_x - 0.5F),
            std::clamp(target->position.y,
                combat::room_bounds::min_y
                    + combat::kDrawSlashHalfWidthAtEnd,
                combat::room_bounds::max_y
                    - combat::kDrawSlashHalfWidthAtEnd),
            0.0F,
        };
        state.close_for_light = false;
    }
    state.stance_reached = false;
    state.sweep_escape = false;
    state.sweep_grid = {};
    state.pending_stance_progress_check = false;
    state.pending_close_progress_check = false;
    state.pending_facing_progress_check = false;
    state.previous_stance_distance_squared = 0.0F;
    state.previous_close_position = {};
    state.previous_facing_position = {};
    return target;
}

const combat::MonsterSnapshot* stage10_active_target_by_ordinal(
    const combat::CombatSnapshot& combat,
    combat::MonsterOrdinal ordinal) noexcept {
    for (const combat::MonsterSnapshot& monster : combat.monsters) {
        if (monster.active && monster.hp > 0
                && monster.monster_ordinal == ordinal) {
            return &monster;
        }
    }
    return nullptr;
}

void release_stage10_ranged_target(Stage10ValidationState& state) noexcept {
    state.ranged_target_ordinal = combat::kInvalidMonsterOrdinal;
    state.ranged_stance = {};
    state.ranged_facing = combat::Facing::right;
    state.stance_reached = false;
    state.close_for_light = false;
    state.recovery_target = {};
    state.recovery_target_valid = false;
    state.sweep_grid = {};
    state.pending_stance_progress_check = false;
    state.pending_close_progress_check = false;
    state.pending_facing_progress_check = false;
    state.previous_stance_distance_squared = 0.0F;
    state.previous_close_position = {};
    state.previous_facing_position = {};
}

bool stage10_stance_reached(
    combat::Vec3 player,
    const Stage10ValidationState& state) noexcept {
    const float facing = state.ranged_facing == combat::Facing::right
        ? 1.0F : -1.0F;
    return facing * (player.x - state.ranged_stance.x) >= 0.0F
        && std::fabs(player.x - state.ranged_stance.x) <= 0.25F
        && std::fabs(player.y - state.ranged_stance.y) <= 0.25F;
}

combat::MovementInput stage10_ranged_movement(
    const combat::CombatSnapshot& combat,
    const Stage10ValidationState& state) noexcept {
    combat::MovementInput movement{};
    const float dx = state.ranged_stance.x - combat.player.position.x;
    const float dy = state.ranged_stance.y - combat.player.position.y;
    const float facing = state.ranged_facing == combat::Facing::right
        ? 1.0F : -1.0F;
    if (facing * (combat.player.position.x - state.ranged_stance.x) < 0.0F) {
        movement.x = state.ranged_facing == combat::Facing::right ? 1 : -1;
    } else if (dx > 0.25F) movement.x = 1;
    else if (dx < -0.25F) movement.x = -1;
    if (dy > 0.25F) movement.y = 1;
    else if (dy < -0.25F) movement.y = -1;
    return movement;
}

float grid_boundary_x(std::uint8_t column) noexcept {
    if (column == 0U) return combat::room_bounds::min_x;
    if (column >= combat::room_spatial::columns) {
        return combat::room_bounds::max_x;
    }
    return combat::room_bounds::min_x
        + static_cast<float>(column) * combat::room_spatial::cell_width;
}

std::uint8_t nearest_grid_boundary_column(float player_x) noexcept {
    const float grid_x = std::clamp(
        (player_x - combat::room_bounds::min_x)
            / combat::room_spatial::cell_width,
        0.0F, static_cast<float>(combat::room_spatial::columns));
    return static_cast<std::uint8_t>(std::floor(grid_x + 0.5F));
}

std::uint8_t outward_grid_boundary_column(
    float player_x, dungeon::ExitDirection direction) noexcept {
    const float grid_x = std::clamp(
        (player_x - combat::room_bounds::min_x)
            / combat::room_spatial::cell_width,
        0.0F, static_cast<float>(combat::room_spatial::columns));
    constexpr std::size_t center = combat::room_spatial::columns / 2U;
    const auto column = direction == dungeon::ExitDirection::right
        ? (std::max)(static_cast<std::size_t>(std::ceil(grid_x)), center + 1U)
        : (std::min)(static_cast<std::size_t>(std::floor(grid_x)), center - 1U);
    return static_cast<std::uint8_t>(column);
}

bool grid_boundary_avoids_center_rewards(
    std::uint8_t column, dungeon::ExitDirection direction) noexcept {
    constexpr std::uint8_t center = static_cast<std::uint8_t>(
        combat::room_spatial::columns / 2U);
    if (direction == dungeon::ExitDirection::right) return column > center;
    if (direction == dungeon::ExitDirection::left) return column < center;
    return false;
}

std::uint8_t outwardmost_blocked_grid_boundary(float player_x,
    dungeon::ExitDirection direction, std::uint8_t blocked_column) noexcept {
    const std::uint8_t initial = outward_grid_boundary_column(
        player_x, direction);
    return direction == dungeon::ExitDirection::right
        ? (std::max)(blocked_column, initial)
        : (std::min)(blocked_column, initial);
}

bool recover_farther_outward_grid_boundary(Stage10GridRouteState& route,
    dungeon::ExitDirection direction, std::uint8_t blocked_column,
    bool progress_already_counted = false) noexcept {
    if ((!progress_already_counted
                && ++route.route_rejoins > kGridRouteMaximumRejoins)
            || (progress_already_counted
                && route.route_rejoins > kGridRouteMaximumRejoins)) {
        return false;
    }
    constexpr std::uint8_t center = static_cast<std::uint8_t>(
        combat::room_spatial::columns / 2U);
    if (direction == dungeon::ExitDirection::right) {
        if (blocked_column >= combat::room_spatial::columns) return false;
        route.boundary_column = (std::max)(
            static_cast<std::uint8_t>(blocked_column + 1U),
            static_cast<std::uint8_t>(center + 1U));
    } else {
        if (blocked_column == 0U) return false;
        route.boundary_column = (std::min)(
            static_cast<std::uint8_t>(blocked_column - 1U),
            static_cast<std::uint8_t>(center - 1U));
    }
    route.phase = Stage10GridRoutePhase::join_far_x;
    return true;
}

std::uint8_t alternate_grid_boundary_column(
    float player_x, std::uint8_t blocked_column) noexcept {
    const float grid_x = std::clamp(
        (player_x - combat::room_bounds::min_x)
            / combat::room_spatial::cell_width,
        0.0F, static_cast<float>(combat::room_spatial::columns));
    const std::size_t cell = std::min(
        static_cast<std::size_t>(grid_x),
        combat::room_spatial::columns - 1U);
    const auto left = static_cast<std::uint8_t>(cell);
    const auto right = static_cast<std::uint8_t>(cell + 1U);
    return blocked_column == left ? right : left;
}

combat::MovementInput grid_route_movement(combat::Vec3 player,
    combat::Vec3 target, Stage10GridRouteState& route) noexcept {
    if (route.phase == Stage10GridRoutePhase::need_join) {
        route.boundary_column = nearest_grid_boundary_column(player.x);
        route.phase = Stage10GridRoutePhase::join_near_x;
    }
    if (route.phase == Stage10GridRoutePhase::join_near_x
            || route.phase == Stage10GridRoutePhase::join_far_x) {
        combat::MovementInput movement{};
        const float dx = grid_boundary_x(route.boundary_column) - player.x;
        if (dx > kGridRouteArrivalTolerance) movement.x = 1;
        else if (dx < -kGridRouteArrivalTolerance) movement.x = -1;
        if (movement.x != 0) return movement;
        route.phase = Stage10GridRoutePhase::route;
    }
    combat::MovementInput movement{};
    const float dy = target.y - player.y;
    if (dy > kGridRouteArrivalTolerance) movement.y = 1;
    else if (dy < -kGridRouteArrivalTolerance) movement.y = -1;
    else {
        const float dx = target.x - player.x;
        if (dx > kGridRouteArrivalTolerance) movement.x = 1;
        else if (dx < -kGridRouteArrivalTolerance) movement.x = -1;
    }
    return movement;
}

GridRouteProgress settle_grid_route_movement(
    Stage10GridRouteState& route, combat::Vec3 player) noexcept {
    if (!route.pending_movement_progress_check) {
        return GridRouteProgress::none;
    }
    route.pending_movement_progress_check = false;
    if (squared_xy_distance(player, route.previous_position) > 0.0F) {
        return GridRouteProgress::moved;
    }
    switch (route.phase) {
    case Stage10GridRoutePhase::need_join:
        return GridRouteProgress::unreachable;
    case Stage10GridRoutePhase::join_near_x:
        route.boundary_column = alternate_grid_boundary_column(
            player.x, route.boundary_column);
        route.phase = Stage10GridRoutePhase::join_far_x;
        return GridRouteProgress::recovered;
    case Stage10GridRoutePhase::join_far_x:
        return GridRouteProgress::unreachable;
    case Stage10GridRoutePhase::route:
        if (++route.route_rejoins > kGridRouteMaximumRejoins) {
            return GridRouteProgress::unreachable;
        }
        route.phase = Stage10GridRoutePhase::need_join;
        return GridRouteProgress::recovered;
    }
    return GridRouteProgress::unreachable;
}

constexpr std::size_t kStage10SweepWaypointCount =
    combat::room_spatial::rows * 2U;

combat::Vec3 stage10_sweep_waypoint(std::size_t index) noexcept {
    const std::size_t row = (index / 2U) % combat::room_spatial::rows;
    const bool left = index % 4U == 0U || index % 4U == 3U;
    return {
        left ? combat::room_bounds::min_x : combat::room_bounds::max_x,
        combat::room_bounds::min_y
            + (static_cast<float>(row) + 0.5F)
                * combat::room_spatial::cell_depth,
        0.0F,
    };
}

bool stage10_route_target_reached(
    combat::Vec3 player, combat::Vec3 target) noexcept {
    return std::fabs(player.x - target.x) <= kGridRouteArrivalTolerance
        && std::fabs(player.y - target.y) <= kGridRouteArrivalTolerance;
}

combat::MovementInput stage10_sweep_movement(
    combat::Vec3 player, Stage10ValidationState& state) noexcept {
    const combat::Vec3 target = state.recovery_target_valid
        ? state.recovery_target
        : stage10_sweep_waypoint(state.sweep_waypoint);
    combat::MovementInput movement = grid_route_movement(
        player, target, state.sweep_grid);
    if (movement.x != 0 || movement.y != 0) return movement;
    state.sweep_grid.route_rejoins = 0U;
    if (state.recovery_target_valid) return {};
    state.sweep_waypoint = static_cast<std::uint8_t>(
        (state.sweep_waypoint + 1U) % kStage10SweepWaypointCount);
    return grid_route_movement(
        player, stage10_sweep_waypoint(state.sweep_waypoint), state.sweep_grid);
}

bool controllable_movement_snapshot(
    const combat::CombatSnapshot& combat) noexcept {
    return combat.player.hp > 0
        && combat.player.hurt_ticks == 0U
        && combat.player.hit_stop_ticks == 0U
        && combat.player.active_attack == combat::AttackId::none
        && combat.active_skill.id == skills::ActiveSkillId::none
        && combat.diagnostics.input_size == 0U;
}

combat::MovementInput stage10_exit_route_movement(
    const combat::CombatSnapshot& combat_state,
    dungeon::ExitDirection direction,
    Stage10ValidationState& state,
    bool avoid_center_rewards = false) noexcept {
    const Stage10GridRoutePhase blocked_phase = state.sweep_grid.phase;
    const std::uint8_t blocked_column = state.sweep_grid.boundary_column;
    GridRouteProgress progress = settle_grid_route_movement(
        state.sweep_grid, combat_state.player.position);
    const bool blocked_near_join = blocked_phase
        == Stage10GridRoutePhase::join_near_x;
    const bool blocked_far_join = blocked_phase
        == Stage10GridRoutePhase::join_far_x;
    const bool blocked_route = blocked_phase == Stage10GridRoutePhase::route;
    const bool safe_same_cell_escape = blocked_near_join
        && progress == GridRouteProgress::recovered
        && grid_boundary_avoids_center_rewards(
            state.sweep_grid.boundary_column, direction);
    if (avoid_center_rewards
            && !safe_same_cell_escape
            && (blocked_near_join || blocked_far_join || blocked_route)
            && (progress == GridRouteProgress::recovered
                || progress == GridRouteProgress::unreachable)) {
        const std::uint8_t recovery_column = blocked_far_join
            ? outwardmost_blocked_grid_boundary(
                combat_state.player.position.x, direction, blocked_column)
            : blocked_column;
        progress = recover_farther_outward_grid_boundary(
            state.sweep_grid, direction, recovery_column, blocked_route)
            ? GridRouteProgress::recovered
            : GridRouteProgress::unreachable;
    }
    if (progress == GridRouteProgress::unreachable) {
        state.sweep_grid = {};
    }
    if (avoid_center_rewards
            && state.sweep_grid.phase == Stage10GridRoutePhase::need_join) {
        state.sweep_grid.boundary_column = outward_grid_boundary_column(
            combat_state.player.position.x, direction);
        state.sweep_grid.phase = Stage10GridRoutePhase::join_near_x;
    }
    const combat::MovementInput movement = grid_route_movement(
        combat_state.player.position,
        validation_door_position(direction), state.sweep_grid);
    if (movement.x == 0 && movement.y == 0) {
        return validation_exit_movement(
            combat_state.player.position, direction);
    }
    if (controllable_movement_snapshot(combat_state)) {
        state.sweep_grid.pending_movement_progress_check = true;
        state.sweep_grid.previous_position = combat_state.player.position;
    }
    return movement;
}

}  // namespace

bool stage10_validation_abyss_skills_enabled(
    Stage10ValidationScenario scenario) noexcept {
    return scenario == Stage10ValidationScenario::reward_chest
        || scenario == Stage10ValidationScenario::pending_reward
        || scenario == Stage10ValidationScenario::exit_confirmation
        || scenario == Stage10ValidationScenario::abyss_hole_descent;
}

combat::MovementInput stage10_validation_sweep_movement(
    const combat::CombatSnapshot& combat_state,
    Stage10GridRouteState& route,
    std::uint8_t& waypoint) noexcept {
    if (settle_grid_route_movement(route, combat_state.player.position)
            == GridRouteProgress::unreachable) {
        route = {};
        waypoint = static_cast<std::uint8_t>(
            (waypoint + 1U) % kStage10SweepWaypointCount);
    }
    combat::Vec3 target = stage10_sweep_waypoint(waypoint);
    if (stage10_route_target_reached(
            combat_state.player.position, target)) {
        route = {};
        waypoint = static_cast<std::uint8_t>(
            (waypoint + 1U) % kStage10SweepWaypointCount);
        target = stage10_sweep_waypoint(waypoint);
    }
    const combat::MovementInput movement = grid_route_movement(
        combat_state.player.position, target, route);
    if (controllable_movement_snapshot(combat_state)
            && (movement.x != 0 || movement.y != 0)) {
        route.pending_movement_progress_check = true;
        route.previous_position = combat_state.player.position;
    }
    return movement;
}

combat::Vec3 stage10_validation_sweep_waypoint(
    std::uint8_t waypoint) noexcept {
    return stage10_sweep_waypoint(waypoint);
}

Stage10RangedValidationPlan stage10_validation_ranged_plan(
    const combat::CombatSnapshot& combat_state,
    Stage10ValidationState& state) noexcept {
    Stage10RangedValidationPlan plan{};
    const auto& player = combat_state.player;
    if (state.pending_stance_progress_check) {
        state.pending_stance_progress_check = false;
        const float distance = squared_xy_distance(
            player.position, state.ranged_stance);
        if (state.previous_stance_distance_squared - distance < 1e-4F) {
            const combat::Vec3 recovery_target = state.ranged_stance;
            state.stalled_target_ordinal = state.ranged_target_ordinal;
            release_stage10_ranged_target(state);
            state.recovery_target = recovery_target;
            state.recovery_target_valid = true;
            state.sweep_escape = true;
        }
    }
    if (state.pending_close_progress_check) {
        state.pending_close_progress_check = false;
        const auto* close_target = stage10_active_target_by_ordinal(
            combat_state, state.ranged_target_ordinal);
        if (close_target != nullptr
                && squared_xy_distance(
                        state.previous_close_position,
                        close_target->position)
                    - squared_xy_distance(
                        player.position, close_target->position) < 1e-4F) {
            state.stalled_target_ordinal = state.ranged_target_ordinal;
            release_stage10_ranged_target(state);
            state.recover_until_light_lane = true;
            state.sweep_escape = true;
        }
    }
    if (state.pending_facing_progress_check) {
        state.pending_facing_progress_check = false;
        if (player.facing != state.ranged_facing
                && squared_xy_distance(
                    state.previous_facing_position,
                    player.position) < 1e-4F) {
            constexpr float room_center_y =
                (combat::room_bounds::min_y
                    + combat::room_bounds::max_y) * 0.5F;
            const float recovery_direction =
                player.position.y >= room_center_y ? -1.0F : 1.0F;
            const combat::Vec3 recovery_target{
                player.position.x,
                std::clamp(player.position.y + recovery_direction * 3.0F,
                    combat::room_bounds::min_y + 0.5F,
                    combat::room_bounds::max_y - 0.5F),
                player.position.z,
            };
            state.stalled_target_ordinal = state.ranged_target_ordinal;
            release_stage10_ranged_target(state);
            state.recovery_target = recovery_target;
            state.recovery_target_valid = true;
            state.sweep_escape = true;
        }
    }
    if (state.sweep_escape) {
        const GridRouteProgress progress = settle_grid_route_movement(
            state.sweep_grid, player.position);
        const bool local_recovery = state.recovery_target_valid;
        const combat::Vec3 recovery_target = local_recovery
            ? state.recovery_target
            : stage10_sweep_waypoint(state.sweep_waypoint);
        if (stage10_route_target_reached(
                player.position, recovery_target)) {
            state.stalled_target_ordinal = combat::kInvalidMonsterOrdinal;
            state.recovery_target = {};
            state.recovery_target_valid = false;
            state.sweep_escape = false;
            state.sweep_grid = {};
        } else if (progress == GridRouteProgress::unreachable) {
            state.sweep_grid = {};
            if (local_recovery) {
                state.recovery_target = {};
                state.recovery_target_valid = false;
            } else {
                state.sweep_waypoint = static_cast<std::uint8_t>(
                    (state.sweep_waypoint + 1U)
                        % kStage10SweepWaypointCount);
                state.stalled_target_ordinal =
                    combat::kInvalidMonsterOrdinal;
                state.sweep_escape = false;
            }
        }
        if (state.sweep_escape) {
            if (state.recover_until_light_lane) {
                for (const combat::MonsterSnapshot& monster
                        : combat_state.monsters) {
                    if (monster.active && monster.hp > 0
                            && monster.monster_ordinal
                                == state.stalled_target_ordinal
                            && validation_attack_lane(
                                combat_state, monster)) {
                        state.stalled_target_ordinal =
                            combat::kInvalidMonsterOrdinal;
                        break;
                    }
                }
                const combat::MonsterSnapshot* blocker =
                    stage10_ranged_target(combat_state, state);
                if (blocker != nullptr) {
                    plan.target_ordinal = blocker->monster_ordinal;
                    plan.stance_reached = validation_attack_lane(
                        combat_state, *blocker);
                    plan.facing_target = plan.stance_reached;
                    return plan;
                }
            }
            plan.movement = stage10_sweep_movement(
                player.position, state);
            if (plan.movement.x != 0 || plan.movement.y != 0) {
                plan.movement_target = state.recovery_target_valid
                    ? state.recovery_target
                    : stage10_sweep_waypoint(state.sweep_waypoint);
                plan.movement_target_valid = true;
            }
            if (controllable_movement_snapshot(combat_state)
                    && (plan.movement.x != 0 || plan.movement.y != 0)) {
                state.sweep_grid.pending_movement_progress_check = true;
                state.sweep_grid.previous_position = player.position;
            }
            return plan;
        }
    }
    const combat::MonsterSnapshot* target = stage10_ranged_target(
        combat_state, state);
    if (target == nullptr) return plan;

    plan.target_ordinal = target->monster_ordinal;
    if (state.close_for_light) {
        plan.movement = validation_movement_toward(
            player.position, target->position);
        plan.stance_reached = validation_attack_lane(combat_state, *target);
        plan.facing_target = plan.stance_reached;
        if (plan.stance_reached) plan.movement = {};
        if (plan.movement.x == 0 && plan.movement.y == 0
                && !plan.stance_reached) {
            const float dx = target->position.x - player.position.x;
            if (std::fabs(dx) > 0.20F) {
                plan.movement.x = dx > 0.0F ? 1 : -1;
            }
        }
        if (plan.movement.x != 0 || plan.movement.y != 0) {
            plan.movement_target = target->position;
            plan.movement_target_valid = true;
            if (controllable_movement_snapshot(combat_state)) {
                state.pending_close_progress_check = true;
                state.previous_close_position = player.position;
            }
        } else if (plan.stance_reached) {
            state.stance_reached = true;
        }
        return plan;
    }
    plan.movement = stage10_ranged_movement(combat_state, state);
    if (plan.movement.x != 0 || plan.movement.y != 0) {
        plan.movement_target = state.ranged_stance;
        plan.movement_target_valid = true;
    }
    plan.stance_reached = stage10_stance_reached(
        player.position, state);
    if (!plan.stance_reached) {
        if (controllable_movement_snapshot(combat_state)
                && (plan.movement.x != 0 || plan.movement.y != 0)) {
            state.pending_stance_progress_check = true;
            state.previous_stance_distance_squared = squared_xy_distance(
                player.position, state.ranged_stance);
        }
        return plan;
    }

    plan.movement = {};
    plan.facing_target = player.facing == state.ranged_facing;
    if (!plan.facing_target) {
        constexpr float kFacingTurnReserve = 0.15F;
        const float stance_offset = player.position.x - state.ranged_stance.x;
        if (state.ranged_facing == combat::Facing::right) {
            plan.movement.x = stance_offset > kFacingTurnReserve ? -1 : 1;
        } else {
            plan.movement.x = stance_offset < -kFacingTurnReserve ? 1 : -1;
        }
        if (controllable_movement_snapshot(combat_state)) {
            state.pending_facing_progress_check = true;
            state.previous_facing_position = player.position;
        }
        return plan;
    }
    state.stance_reached = true;
    return plan;
}

void stage10_validation_release_ranged_target(
    Stage10ValidationState& state) noexcept {
    release_stage10_ranged_target(state);
    state.stalled_target_ordinal = combat::kInvalidMonsterOrdinal;
    state.sweep_escape = false;
}

combat::MovementInput stage10_validation_input(
    dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    Stage10ValidationState& state) noexcept {
    const Stage10ValidationScenario scenario = config.stage10_validation;
    const bool force_full_clear = stage11c_uses_full_clear_driver(
        config.stage11c_hud_validation);
    const bool entering_abyss = !state.entered_abyss && snapshot.is_abyss;
    state.entered_abyss = state.entered_abyss || snapshot.is_abyss;
    if (entering_abyss) {
        release_stage10_ranged_target(state);
        state.melee_chain = false;
        state.recover_until_light_lane = false;
        state.normal_exit_started = false;
        state.stalled_target_ordinal = combat::kInvalidMonsterOrdinal;
        state.sweep_waypoint = 0U;
        state.sweep_escape = false;
    }
    if (scenario == Stage10ValidationScenario::room_reset
            && snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::combat
            && !state.reset_requested) {
        state.reset_requested = session.reset_current_room()
            != dungeon::RequestResult::rejected;
        return {};
    }
    if (scenario == Stage10ValidationScenario::player_death
            && snapshot.is_abyss) return {};
    if (!snapshot.combat.has_value()) return {};
    if (snapshot.phase == dungeon::RoomPhase::combat) {
        if (!force_full_clear && !snapshot.is_abyss
                && snapshot.exits_unlocked) {
            if (!state.normal_exit_started) {
                release_stage10_ranged_target(state);
                state.normal_exit_started = true;
            }
            const auto& combat_state = *snapshot.combat;
            const auto direction = validation_direction(config);
            return stage10_exit_route_movement(
                combat_state, direction, state);
        }
        const bool full_clear_driver = force_full_clear || !snapshot.is_abyss
            || stage10_validation_abyss_skills_enabled(scenario);
        const bool recover_navigation = full_clear_driver;
        const auto& combat_state = *snapshot.combat;
        const auto& player = combat_state.player;
        if (recover_navigation && state.pending_stance_progress_check) {
            state.pending_stance_progress_check = false;
            const float distance = squared_xy_distance(
                player.position, state.ranged_stance);
            if (state.previous_stance_distance_squared - distance < 1e-4F) {
                const combat::Vec3 recovery_target = state.ranged_stance;
                state.stalled_target_ordinal = state.ranged_target_ordinal;
                release_stage10_ranged_target(state);
                state.recovery_target = recovery_target;
                state.recovery_target_valid = true;
                state.sweep_escape = true;
            }
        }
        if (recover_navigation && state.pending_close_progress_check) {
            state.pending_close_progress_check = false;
            const auto* close_target = stage10_active_target_by_ordinal(
                combat_state, state.ranged_target_ordinal);
            if (close_target != nullptr
                    && squared_xy_distance(
                            state.previous_close_position,
                            close_target->position)
                        - squared_xy_distance(
                            player.position, close_target->position) < 1e-4F) {
                state.stalled_target_ordinal = state.ranged_target_ordinal;
                release_stage10_ranged_target(state);
                state.recover_until_light_lane = true;
                state.sweep_escape = true;
            }
        }
        if (recover_navigation && state.sweep_escape) {
            const GridRouteProgress progress = settle_grid_route_movement(
                state.sweep_grid, player.position);
            bool attack_lane_recovered = false;
            for (const combat::MonsterSnapshot& monster
                    : combat_state.monsters) {
                if (monster.active && monster.hp > 0
                        && monster.monster_ordinal
                            == state.stalled_target_ordinal
                        && validation_attack_lane(combat_state, monster)) {
                    attack_lane_recovered = true;
                    break;
                }
            }
            const combat::Vec3 recovery_target =
                state.recovery_target_valid
                ? state.recovery_target
                : stage10_sweep_waypoint(state.sweep_waypoint);
            const bool local_recovery = state.recovery_target_valid;
            if (attack_lane_recovered
                    || stage10_route_target_reached(
                        player.position, recovery_target)) {
                state.stalled_target_ordinal =
                    combat::kInvalidMonsterOrdinal;
                state.recovery_target = {};
                state.recovery_target_valid = false;
                if (state.recover_until_light_lane
                        && (snapshot.remaining_targets == 1U
                            || force_full_clear)
                        && !local_recovery
                        && !attack_lane_recovered) {
                    state.sweep_waypoint = static_cast<std::uint8_t>(
                        (state.sweep_waypoint + 1U)
                            % kStage10SweepWaypointCount);
                    state.recover_until_light_lane = false;
                    state.sweep_escape = false;
                } else if (!state.recover_until_light_lane) {
                    state.sweep_escape = false;
                }
            } else if (progress == GridRouteProgress::unreachable) {
                state.sweep_grid = {};
                if (local_recovery) {
                    state.recovery_target = {};
                    state.recovery_target_valid = false;
                    if (!state.recover_until_light_lane) {
                        state.sweep_escape = false;
                    }
                } else {
                    state.sweep_waypoint = static_cast<std::uint8_t>(
                        (state.sweep_waypoint + 1U)
                            % kStage10SweepWaypointCount);
                    if (state.recover_until_light_lane
                            && snapshot.remaining_targets == 1U) {
                        state.stalled_target_ordinal =
                            combat::kInvalidMonsterOrdinal;
                        state.recover_until_light_lane = false;
                        state.sweep_escape = false;
                    }
                }
            }
        }
        if (force_full_clear && state.sweep_escape
                && !state.recovery_target_valid
                && !state.recover_until_light_lane
                && state.stalled_target_ordinal
                    != combat::kInvalidMonsterOrdinal) {
            state.sweep_escape = false;
            state.sweep_grid = {};
        }
        const bool retry_released_sole_target =
            snapshot.remaining_targets == 1U
            && state.sweep_escape
            && state.stalled_target_ordinal
                == combat::kInvalidMonsterOrdinal
            && !state.recovery_target_valid
            && !state.recover_until_light_lane
            && state.melee_chain;
        const combat::MonsterSnapshot* target = full_clear_driver
            && (state.recover_until_light_lane || !state.sweep_escape
                || retry_released_sole_target)
            ? stage10_ranged_target(combat_state, state)
            : full_clear_driver ? nullptr
            : nearest_living_monster(combat_state);
        AttackGeometry geometry{};
        const float facing = player.facing == combat::Facing::right
            ? 1.0F : -1.0F;
        const float storm_center_x =
            player.position.x + facing * combat::kStormCenterForward;
        for (const combat::MonsterSnapshot& monster : combat_state.monsters) {
            if (!monster.active || monster.hp <= 0) continue;
            if (!full_clear_driver && (target == nullptr
                    || monster.monster_ordinal
                        != target->monster_ordinal)) {
                continue;
            }
            const float storm_dx = monster.position.x - storm_center_x;
            const float storm_dy = monster.position.y - player.position.y;
            geometry.storm = geometry.storm
                || storm_dx * storm_dx + storm_dy * storm_dy
                    <= combat::kStormStrikeRadius
                        * combat::kStormStrikeRadius;
            const float forward =
                (monster.position.x - player.position.x) * facing;
            const float half_width = forward >= 0.0F
                    && forward <= combat::kDrawSlashRange
                ? combat::kDrawSlashHalfWidthAtEnd
                    * (forward / combat::kDrawSlashRange)
                : -1.0F;
            geometry.draw = geometry.draw || (half_width >= 0.0F
                && std::fabs(monster.position.y - player.position.y)
                    <= half_width);
            geometry.light = geometry.light
                || validation_attack_lane(combat_state, monster);
        }
        combat::MovementInput movement{};
        bool stance_movement_requested = false;
        bool close_movement_requested = false;
        if (target != nullptr) {
            if (full_clear_driver && state.close_for_light) {
                movement = validation_movement_toward(
                    player.position, target->position);
                if (movement.x == 0 && movement.y == 0
                        && !validation_attack_lane(
                            combat_state, *target)) {
                    const float dx = target->position.x - player.position.x;
                    if (std::fabs(dx) > 0.20F) {
                        movement.x = dx > 0.0F ? 1 : -1;
                    }
                }
                close_movement_requested =
                    movement.x != 0 || movement.y != 0;
            } else {
                movement = full_clear_driver
                    ? stage10_ranged_movement(combat_state, state)
                    : validation_movement_toward(
                        player.position, target->position);
                stance_movement_requested = full_clear_driver
                    && (movement.x != 0 || movement.y != 0);
            }
            if (full_clear_driver && !state.close_for_light
                    && stage10_stance_reached(player.position, state)) {
                if (player.facing != state.ranged_facing) {
                    movement = {};
                    movement.x = state.ranged_facing == combat::Facing::right
                        ? 1 : -1;
                    stance_movement_requested = false;
                } else {
                    state.stance_reached = true;
                }
            }
            if (recover_navigation && !state.close_for_light
                    && state.stance_reached
                    && !geometry.any()) {
                state.stalled_target_ordinal =
                    state.ranged_target_ordinal;
                release_stage10_ranged_target(state);
                target = stage10_ranged_target(combat_state, state);
                movement = {};
                stance_movement_requested = false;
                if (target != nullptr) {
                    movement = stage10_ranged_movement(
                        combat_state, state);
                    stance_movement_requested =
                        movement.x != 0 || movement.y != 0;
                } else if (snapshot.remaining_targets != 0U) {
                    state.sweep_escape = true;
                }
            }
        } else if (full_clear_driver && snapshot.remaining_targets != 0U) {
            state.sweep_escape = recover_navigation;
        }
        if (recover_navigation && state.sweep_escape
                && snapshot.remaining_targets != 0U) {
            movement = stage10_sweep_movement(player.position, state);
            stance_movement_requested = false;
        }
        const bool action_ready = player.hp > 0
            && player.hurt_ticks == 0U && player.hit_stop_ticks == 0U
            && player.active_attack == combat::AttackId::none
            && snapshot.combat->active_skill.id == skills::ActiveSkillId::none
            && snapshot.combat->diagnostics.input_size == 0U;
        const bool skill_ready = action_ready && (force_full_clear
            || !snapshot.is_abyss
            || stage10_validation_abyss_skills_enabled(scenario));
        bool action_requested = false;
        bool skill_accepted = false;
        if (skill_ready) {
            combat::SkillCastResult storm = combat::SkillCastResult::none;
            if (geometry.storm) {
                storm = session.request_active_skill_slot(1U);
                skill_accepted =
                    storm == combat::SkillCastResult::accepted;
                action_requested = skill_accepted;
            }
            if (!action_requested && (!geometry.storm
                    || storm == combat::SkillCastResult::cooling_down)) {
                if (geometry.draw
                        && session.request_active_skill_slot(0U)
                            == combat::SkillCastResult::accepted) {
                    skill_accepted = true;
                    action_requested = true;
                }
            }
        }
        if (!action_requested
                && snapshot.combat->player.hurt_ticks == 0U
                && snapshot.combat->player.active_attack
                    == combat::AttackId::none
                && snapshot.combat->diagnostics.input_size == 0U
                && geometry.light) {
            action_requested = session.queue_action(combat::Action::light);
        }
        if (target != nullptr && full_clear_driver
                && state.stance_reached && !state.close_for_light
                && action_ready && !skill_accepted && !geometry.light) {
            state.close_for_light = true;
            state.melee_chain = true;
            movement = validation_movement_toward(
                player.position, target->position);
            stance_movement_requested = false;
            close_movement_requested =
                movement.x != 0 || movement.y != 0;
        }
        const bool controllable = controllable_movement_snapshot(combat_state)
            && !action_requested;
        if (recover_navigation && controllable
                && stance_movement_requested) {
            state.pending_stance_progress_check = true;
            state.previous_stance_distance_squared = squared_xy_distance(
                player.position, state.ranged_stance);
        } else if (recover_navigation && controllable
                && close_movement_requested) {
            state.pending_close_progress_check = true;
            state.previous_close_position = player.position;
        }
        if (recover_navigation && state.sweep_escape && controllable
                && (movement.x != 0 || movement.y != 0)) {
            state.sweep_grid.pending_movement_progress_check = true;
            state.sweep_grid.previous_position = player.position;
        }
        return movement;
    }
    if (snapshot.phase != dungeon::RoomPhase::awaiting_exit) return {};
    if (!snapshot.is_abyss) {
        if (force_full_clear) {
            if (!state.normal_exit_started) {
                release_stage10_ranged_target(state);
                state.normal_exit_started = true;
            }
            return stage10_exit_route_movement(*snapshot.combat,
                validation_direction(config), state);
        }
        return validation_exit_movement(snapshot.combat->player.position,
            validation_direction(config));
    }
    if (config.stage11c_hud_validation
            == Stage11CHudValidationScenario::abyss_abandon) {
        if (snapshot.abyss_exit_confirmation_armed) return {};
        return stage10_exit_route_movement(*snapshot.combat,
            dungeon::ExitDirection::right, state, true);
    }
    if (scenario == Stage10ValidationScenario::exit_confirmation) {
        if (snapshot.abyss_exit_confirmation_armed) return {};
        const dungeon::ExitDirection direction =
            snapshot.combat->player.position.x < 0.0F
            ? dungeon::ExitDirection::left
            : dungeon::ExitDirection::right;
        return stage10_exit_route_movement(
            *snapshot.combat, direction, state, true);
    }
    if (scenario == Stage10ValidationScenario::abyss_hole_descent) {
        state.descent_warning_seen = state.descent_warning_seen
            || snapshot.abyss_exit_confirmation_armed;
        const combat::Vec3 player = snapshot.combat->player.position;
        if (settle_grid_route_movement(state.sweep_grid, player)
                == GridRouteProgress::unreachable) {
            state.sweep_grid = {};
        }
        const combat::MovementInput movement = grid_route_movement(
            player, kHoleCenter, state.sweep_grid);
        if (can_prompt_descent(snapshot, player)) {
            static_cast<void>(session.request_descent(true));
        }
        if (controllable_movement_snapshot(*snapshot.combat)
                && (movement.x != 0 || movement.y != 0)) {
            state.sweep_grid.pending_movement_progress_check = true;
            state.sweep_grid.previous_position = player;
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
    if (drive_to_depth && snapshot.exits_unlocked && snapshot.has_hole
            && (snapshot.phase == dungeon::RoomPhase::combat
                || snapshot.phase == dungeon::RoomPhase::awaiting_exit)) {
        const combat::Vec3 player = snapshot.combat->player.position;
        if (settle_grid_route_movement(state.sweep_grid, player)
                == GridRouteProgress::unreachable) {
            state.sweep_grid = {};
        }
        const combat::MovementInput movement = grid_route_movement(
            player, kHoleCenter, state.sweep_grid);
        if (can_prompt_descent(snapshot, player)) {
            static_cast<void>(session.request_descent(true));
        }
        if (controllable_movement_snapshot(*snapshot.combat)
                && (movement.x != 0 || movement.y != 0)) {
            state.sweep_grid.pending_movement_progress_check = true;
            state.sweep_grid.previous_position = player;
        }
        return movement;
    }
    if (snapshot.phase == dungeon::RoomPhase::combat) {
        if (drive_to_abyss && !snapshot.is_abyss
                && snapshot.exits_unlocked) {
            return validation_exit_movement(
                snapshot.combat->player.position,
                validation_direction(config));
        }
        return stage10_validation_input(
            session, snapshot, config, state.combat_driver);
    }
    if (snapshot.phase != dungeon::RoomPhase::awaiting_exit) return {};
    if (drive_to_abyss) {
        return validation_exit_movement(snapshot.combat->player.position,
            validation_direction(config));
    }
    return {};
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
            && snapshot.exits_unlocked
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
        return snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && snapshot.ground_item_count != 0U;
    case Stage10ValidationScenario::pending_reward:
        return snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::committing
            && snapshot.abyss_pending_rewards != 0U;
    case Stage10ValidationScenario::exit_confirmation:
        return snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && snapshot.abyss_exit_confirmation_armed;
    case Stage10ValidationScenario::player_death:
    case Stage10ValidationScenario::room_reset:
        return state.entered_abyss && !snapshot.is_abyss;
    case Stage10ValidationScenario::leave_started:
        return snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::combat;
    case Stage10ValidationScenario::restarted_failed:
        // Task 9 freezes this legacy enum name; Square Task 5 V9 exact resume
        // replaced its old restart-as-failure behavior.
        return state.entered_abyss && snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::combat;
    case Stage10ValidationScenario::abyss_hole_descent:
        return state.entered_abyss && state.descent_warning_seen
            && snapshot.depth > 1U;
    }
    return false;
}

}  // namespace arpg::platform::host_validation
