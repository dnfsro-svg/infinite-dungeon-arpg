#pragma once

#include "checkpoint/dungeon_run_state.hpp"
#include "checkpoint/room_combat_checkpoint.hpp"
#include "checkpoint/room_progress_checkpoint.hpp"

#include <cstdint>

namespace arpg::checkpoint {

void clear_room_combat_checkpoint(RoomCombatCheckpoint& out) noexcept;
void clear_room_progress_checkpoint(RoomProgressCheckpoint& out) noexcept;
void clear_save_checkpoint_slot(SaveCheckpointSlot& out) noexcept;

[[nodiscard]] bool valid_room_combat_checkpoint_structural(
    const RoomCombatCheckpoint& checkpoint,
    std::uint32_t generated_monsters) noexcept;
[[nodiscard]] bool valid_room_progress_checkpoint_structural(
    const RoomProgressCheckpoint& room,
    const DungeonRunState& state) noexcept;
[[nodiscard]] bool same_room_combat_checkpoint(
    const RoomCombatCheckpoint& left,
    const RoomCombatCheckpoint& right) noexcept;
[[nodiscard]] bool same_room_progress_checkpoint(
    const RoomProgressCheckpoint& left,
    const RoomProgressCheckpoint& right) noexcept;

}  // namespace arpg::checkpoint
