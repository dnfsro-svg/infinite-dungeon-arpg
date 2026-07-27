#pragma once

#include "combat/room_combat_checkpoint.hpp"
#include "core/gameplay_limits.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "items/item_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::dungeon::checkpoint {

inline constexpr std::size_t kRoomEquipmentGroundCapacity =
    limits::kRoomMonsterCapacity;
inline constexpr std::size_t kRoomSecondaryGroundCapacity =
    limits::kRoomMonsterCapacity * 2U + 16U;

enum class RoomProgressLifecycle : std::uint8_t {
    none,
    active,
    death_pending,
};

enum class SecondaryGroundTag : std::uint8_t {
    material,
    health_potion,
};

struct EquipmentGroundCheckpoint final {
    std::uint16_t ordinal{0xFFFFU};
    std::uint8_t source{};
    std::uint8_t reward_ordinal{0xFFU};
    combat::Vec3 position{};
    items::ItemInstance item{};
};

struct SecondaryGroundCheckpoint final {
    SecondaryGroundTag tag{SecondaryGroundTag::material};
    std::uint16_t ordinal{0xFFFFU};
    std::uint8_t source{};
    combat::Vec3 position{};
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
    std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>
        defeat_bits{};
    std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>
        equipment_claim_bits{};
    std::array<std::uint64_t, limits::kRoomSecondaryClaimWords>
        secondary_claim_bits{};
    combat::RoomCombatCheckpoint combat{};
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

void clear_room_progress_checkpoint(RoomProgressCheckpoint& out) noexcept;
void clear_save_checkpoint_slot(SaveCheckpointSlot& out) noexcept;

[[nodiscard]] bool valid_room_progress_checkpoint_structural(
    const RoomProgressCheckpoint& room,
    const DungeonRunState& state) noexcept;

[[nodiscard]] bool same_room_progress_checkpoint(
    const RoomProgressCheckpoint& left,
    const RoomProgressCheckpoint& right) noexcept;

}  // namespace arpg::dungeon::checkpoint
