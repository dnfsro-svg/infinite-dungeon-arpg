#include "dungeon/room_progress_checkpoint.hpp"

#include "checkpoint/room_checkpoint_validation.hpp"

namespace arpg::dungeon::checkpoint {

void clear_room_progress_checkpoint(RoomProgressCheckpoint& out) noexcept {
    ::arpg::checkpoint::clear_room_progress_checkpoint(out);
}

void clear_save_checkpoint_slot(SaveCheckpointSlot& out) noexcept {
    ::arpg::checkpoint::clear_save_checkpoint_slot(out);
}

bool valid_room_progress_checkpoint_structural(
    const RoomProgressCheckpoint& room,
    const DungeonRunState& state) noexcept {
    return ::arpg::checkpoint::valid_room_progress_checkpoint_structural(
        room, state);
}

bool same_room_progress_checkpoint(
    const RoomProgressCheckpoint& left,
    const RoomProgressCheckpoint& right) noexcept {
    return ::arpg::checkpoint::same_room_progress_checkpoint(left, right);
}

}  // namespace arpg::dungeon::checkpoint
