#include "host_validation_navigation.hpp"

#include "combat/combat_types.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon/dungeon_types.hpp"
#include "raylib_host.hpp"

#include <cmath>
#include <cstdint>

namespace arpg::platform::host_validation {

combat::MovementInput validation_route_fire_movement(
    combat::Vec3 player,
    combat::Vec3 target,
    combat::MovementInput movement) noexcept {
    combat::Vec3 candidate = player;
    candidate.x += 0.10F * static_cast<float>(movement.x);
    candidate.y += 0.10F * static_cast<float>(movement.y);
    if (!combat::fire_room_obstacle::blocks_player(player, candidate)) {
        return movement;
    }
    const combat::Vec3 routed = combat::fire_room_obstacle::route_monster(
        player, candidate, target);
    return {
        static_cast<std::int8_t>(routed.x > player.x ? 1
            : (routed.x < player.x ? -1 : 0)),
        static_cast<std::int8_t>(routed.y > player.y ? 1
            : (routed.y < player.y ? -1 : 0)),
    };
}

const combat::MonsterSnapshot* nearest_living_monster(
    const combat::CombatSnapshot& state) noexcept {
    const combat::MonsterSnapshot* best = nullptr;
    float best_distance = 0.0F;
    for (const combat::MonsterSnapshot& monster : state.monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float x = monster.position.x - state.player.position.x;
        const float y = monster.position.y - state.player.position.y;
        const float distance = x * x + y * y;
        if (best == nullptr || distance < best_distance) {
            best = &monster;
            best_distance = distance;
        }
    }
    return best;
}

combat::MovementInput validation_movement_toward(
    combat::Vec3 from, combat::Vec3 to) noexcept {
    combat::MovementInput movement{};
    if (to.x - from.x > 0.45F) movement.x = 1;
    else if (to.x - from.x < -0.45F) movement.x = -1;
    if (to.y - from.y > 0.25F) movement.y = 1;
    else if (to.y - from.y < -0.25F) movement.y = -1;
    return movement;
}

combat::Vec3 validation_door_position(
    dungeon::ExitDirection direction) noexcept {
    switch (direction) {
    case dungeon::ExitDirection::up:
        return {0.0F, combat::room_bounds::min_y, 0.0F};
    case dungeon::ExitDirection::down:
        return {0.0F, combat::room_bounds::max_y, 0.0F};
    case dungeon::ExitDirection::left:
        return {combat::room_bounds::min_x, 0.0F, 0.0F};
    case dungeon::ExitDirection::right:
        return {combat::room_bounds::max_x, 0.0F, 0.0F};
    case dungeon::ExitDirection::none: return {};
    }
    return {};
}

combat::MovementInput validation_exit_movement(
    combat::Vec3 player,
    dungeon::ExitDirection direction) noexcept {
    combat::MovementInput movement = validation_movement_toward(
        player, validation_door_position(direction));
    switch (direction) {
    case dungeon::ExitDirection::up: movement.y = -1; break;
    case dungeon::ExitDirection::down: movement.y = 1; break;
    case dungeon::ExitDirection::left: movement.x = -1; break;
    case dungeon::ExitDirection::right: movement.x = 1; break;
    case dungeon::ExitDirection::none: break;
    }
    return movement;
}

bool validation_attack_lane(
    const combat::CombatSnapshot& state,
    const combat::MonsterSnapshot& target) noexcept {
    const float x = target.position.x - state.player.position.x;
    const float y = target.position.y - state.player.position.y;
    const bool facing = std::fabs(x) <= 0.20F
        || (x > 0.0F && state.player.facing == combat::Facing::right)
        || (x < 0.0F && state.player.facing == combat::Facing::left);
    return facing && std::fabs(x) <= 1.70F && std::fabs(y) <= 0.55F;
}

dungeon::ExitDirection validation_direction(
    const RaylibHostConfig& config) noexcept {
    return config.validation_abyss_direction < 4U
        ? static_cast<dungeon::ExitDirection>(
            config.validation_abyss_direction)
        : dungeon::ExitDirection::none;
}

}  // namespace arpg::platform::host_validation
