#pragma once

#include "checkpoint/room_combat_checkpoint.hpp"
#include "combat/player_damage_history.hpp"

namespace arpg::combat {

using PlayerCombatCheckpoint = ::arpg::checkpoint::PlayerCombatCheckpoint;
using AttackCheckpoint = ::arpg::checkpoint::AttackCheckpoint;
using MonsterCombatCheckpoint = ::arpg::checkpoint::MonsterCombatCheckpoint;
using RoomObstacleCheckpoint = ::arpg::checkpoint::RoomObstacleCheckpoint;
using RoomCombatCheckpoint = ::arpg::checkpoint::RoomCombatCheckpoint;

[[nodiscard]] bool valid_room_combat_checkpoint_structural(
    const RoomCombatCheckpoint& checkpoint,
    std::uint32_t generated_monsters) noexcept;

}  // namespace arpg::combat
