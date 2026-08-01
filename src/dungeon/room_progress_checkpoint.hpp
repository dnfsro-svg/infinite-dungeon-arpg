#pragma once

#include "checkpoint/room_progress_checkpoint.hpp"
#include "dungeon/dungeon_checkpoint.hpp"

namespace arpg::dungeon::checkpoint {

inline constexpr std::size_t kRoomEquipmentGroundCapacity =
    ::arpg::checkpoint::kRoomEquipmentGroundCapacity;
inline constexpr std::size_t kRoomSecondaryGroundCapacity =
    ::arpg::checkpoint::kRoomSecondaryGroundCapacity;

using RoomProgressLifecycle = ::arpg::checkpoint::RoomProgressLifecycle;
using SecondaryGroundTag = ::arpg::checkpoint::SecondaryGroundTag;
using EquipmentGroundCheckpoint =
    ::arpg::checkpoint::EquipmentGroundCheckpoint;
using SecondaryGroundCheckpoint =
    ::arpg::checkpoint::SecondaryGroundCheckpoint;
using RoomProgressCheckpoint = ::arpg::checkpoint::RoomProgressCheckpoint;
using SaveCheckpointSlot = ::arpg::checkpoint::SaveCheckpointSlot;

void clear_room_progress_checkpoint(RoomProgressCheckpoint& out) noexcept;
void clear_save_checkpoint_slot(SaveCheckpointSlot& out) noexcept;
[[nodiscard]] bool valid_room_progress_checkpoint_structural(
    const RoomProgressCheckpoint& room,
    const DungeonRunState& state) noexcept;
[[nodiscard]] bool same_room_progress_checkpoint(
    const RoomProgressCheckpoint& left,
    const RoomProgressCheckpoint& right) noexcept;

}  // namespace arpg::dungeon::checkpoint
