#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <cstdint>

namespace arpg::dungeon {

[[nodiscard]] checkpoint::DeathCheckpoint make_death_checkpoint(
    const combat::CombatDeathSnapshot& combat_death,
    const checkpoint::RoomDescriptor& death_room,
    const checkpoint::RoomDescriptor& target_room) noexcept;

[[nodiscard]] bool valid_death_checkpoint_dungeon(
    const checkpoint::DeathCheckpoint& death,
    const checkpoint::RoomDescriptor& death_room,
    std::uint64_t commit_generation,
    std::uint64_t next_death_sequence,
    const DungeonRules& rules) noexcept;

}  // namespace arpg::dungeon
