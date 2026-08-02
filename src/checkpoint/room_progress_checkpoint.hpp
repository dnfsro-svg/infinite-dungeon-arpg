#pragma once

#include "checkpoint/dungeon_run_state.hpp"
#include "checkpoint/room_combat_checkpoint.hpp"
#include "checkpoint/room_checkpoint_schema.hpp"
#include "core/gameplay_limits.hpp"
#include "items/item_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::checkpoint {

enum class RoomProgressLifecycle : std::uint8_t {
    none = 0,
    active = 1,
    death_pending = 2,
};

enum class SecondaryGroundTag : std::uint8_t {
    material = 0,
    health_potion = 1,
};

struct EquipmentGroundCheckpoint final {
    std::uint16_t ordinal{0xFFFFU};
    std::uint8_t source{};
    std::uint8_t reward_ordinal{0xFFU};
    CheckpointVec3 position{};
    items::ItemInstance item{};
};

struct SecondaryGroundCheckpoint final {
    SecondaryGroundTag tag{SecondaryGroundTag::material};
    std::uint16_t ordinal{0xFFFFU};
    std::uint8_t source{};
    CheckpointVec3 position{};
    items::MaterialId material{items::MaterialId::count};
};

struct RoomProgressCheckpoint final {
    RoomProgressCheckpoint() noexcept = default;
    RoomProgressCheckpoint(const RoomProgressCheckpoint&) = delete;
    RoomProgressCheckpoint& operator=(const RoomProgressCheckpoint&) = delete;
    RoomProgressCheckpoint(RoomProgressCheckpoint&&) = delete;
    RoomProgressCheckpoint& operator=(RoomProgressCheckpoint&&) = delete;

    RoomProgressLifecycle lifecycle{RoomProgressLifecycle::none};
    std::uint64_t room_index{};
    std::uint64_t room_seed{};
    std::uint32_t monster_generator_version{};
    std::uint64_t monster_blueprint_hash{};
    std::uint32_t environment_generator_version{};
    std::uint64_t environment_blueprint_hash{};
    std::uint32_t generated_monsters{};
    std::uint32_t defeated_monsters{};
    std::uint32_t required_kills{};
    bool exits_unlocked{};
    bool full_clear{};
    bool reward_committed{};
    std::uint64_t pending_room_experience{};
    std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>
        defeat_bits{};
    std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>
        equipment_claim_bits{};
    std::array<std::uint64_t, limits::kRoomSecondaryClaimWords>
        secondary_claim_bits{};
    RoomCombatCheckpoint combat{};
    std::array<EquipmentGroundCheckpoint, kRoomEquipmentGroundCapacity>
        equipment_ground{};
    std::uint16_t equipment_ground_count{};
    std::array<SecondaryGroundCheckpoint, kRoomSecondaryGroundCapacity>
        secondary_ground{};
    std::uint16_t secondary_ground_count{};
};

struct SaveCheckpointSlot final {
    SaveCheckpointSlot() noexcept = default;
    SaveCheckpointSlot(const SaveCheckpointSlot&) = delete;
    SaveCheckpointSlot& operator=(const SaveCheckpointSlot&) = delete;
    SaveCheckpointSlot(SaveCheckpointSlot&&) = delete;
    SaveCheckpointSlot& operator=(SaveCheckpointSlot&&) = delete;

    DungeonRunState state{};
    std::uint64_t persistence_revision{};
    RoomProgressCheckpoint room_progress{};
};

}  // namespace arpg::checkpoint
