#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_types.hpp"

namespace arpg::platform {
struct RaylibHostConfig;

namespace host_validation {

[[nodiscard]] combat::MovementInput validation_route_fire_movement(
    combat::Vec3 player, combat::Vec3 target,
    combat::MovementInput requested) noexcept;
[[nodiscard]] const combat::MonsterSnapshot* nearest_living_monster(
    const combat::CombatSnapshot&) noexcept;
[[nodiscard]] combat::MovementInput validation_movement_toward(
    combat::Vec3 from, combat::Vec3 to) noexcept;
[[nodiscard]] combat::Vec3 validation_door_position(
    dungeon::ExitDirection) noexcept;
[[nodiscard]] combat::MovementInput validation_exit_movement(
    combat::Vec3 player, dungeon::ExitDirection) noexcept;
[[nodiscard]] bool validation_attack_lane(
    const combat::CombatSnapshot&,
    const combat::MonsterSnapshot&) noexcept;
[[nodiscard]] dungeon::ExitDirection validation_direction(
    const RaylibHostConfig&) noexcept;

}  // namespace host_validation
}  // namespace arpg::platform
