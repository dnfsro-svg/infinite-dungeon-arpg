#pragma once

#include "checkpoint/room_checkpoint_schema.hpp"
#include "core/gameplay_limits.hpp"
#include "dungeon/dungeon_types.hpp"
#include "dungeon/room_drop_spatial_index.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::dungeon {

inline constexpr std::size_t kAuthoritativeEquipmentDropCapacity =
    limits::kRoomMonsterCapacity;
inline constexpr std::size_t kAuthoritativeSecondaryDropCapacity =
    limits::kRoomMonsterCapacity * 2U + 16U;
inline constexpr std::size_t kAuthoritativeHealthPotionCapacity =
    limits::kRoomMonsterCapacity;
inline constexpr std::uint16_t kAbyssSecondaryOrdinalBegin =
    ::arpg::checkpoint::kAbyssSecondaryOrdinalBegin;

[[nodiscard]] constexpr std::uint16_t equipment_drop_ordinal(
    const std::uint16_t spawn_ordinal) noexcept {
    return spawn_ordinal;
}

[[nodiscard]] constexpr std::uint16_t common_material_ordinal(
    const std::uint16_t spawn_ordinal) noexcept {
    return static_cast<std::uint16_t>(spawn_ordinal * 2U);
}

[[nodiscard]] constexpr std::uint16_t secondary_drop_ordinal(
    const std::uint16_t spawn_ordinal) noexcept {
    return static_cast<std::uint16_t>(spawn_ordinal * 2U + 1U);
}

class RoomDropState final {
public:
    RoomDropState() = default;
    RoomDropState(const RoomDropState&) = delete;
    RoomDropState& operator=(const RoomDropState&) = delete;
    RoomDropState(RoomDropState&&) noexcept = default;
    RoomDropState& operator=(RoomDropState&&) noexcept = default;

    [[nodiscard]] bool reset(const combat::RoomMonsterPlan& plan) noexcept;
    void clear() noexcept;
    [[nodiscard]] bool place_equipment(const GroundItem& item) noexcept;
    [[nodiscard]] bool place_material(const GroundMaterial& material) noexcept;
    [[nodiscard]] bool place_health_potion(
        const GroundHealthPotion& potion) noexcept;
    [[nodiscard]] bool can_place_equipment(
        const GroundItem& item) const noexcept;
    [[nodiscard]] bool can_mark_equipment_claimed(
        std::uint16_t ordinal) const noexcept;
    [[nodiscard]] bool can_mark_secondary_claimed(
        std::uint16_t ordinal) const noexcept;
    [[nodiscard]] bool mark_equipment_claimed(std::uint16_t ordinal) noexcept;
    [[nodiscard]] bool mark_secondary_claimed(std::uint16_t ordinal) noexcept;
    [[nodiscard]] bool equipment_claimed(std::uint16_t ordinal) const noexcept;
    [[nodiscard]] bool secondary_claimed(std::uint16_t ordinal) const noexcept;
    [[nodiscard]] std::uint16_t first_health_potion_spawn() const noexcept;

    [[nodiscard]] const auto& equipment() const noexcept { return equipment_; }
    [[nodiscard]] auto& equipment() noexcept { return equipment_; }
    [[nodiscard]] const auto& materials() const noexcept { return materials_; }
    [[nodiscard]] auto& materials() noexcept { return materials_; }
    [[nodiscard]] const auto& health_potions() const noexcept {
        return health_potions_;
    }
    [[nodiscard]] auto& health_potions() noexcept { return health_potions_; }
    [[nodiscard]] const auto& equipment_claim_bits() const noexcept {
        return equipment_claim_bits_;
    }
    [[nodiscard]] const auto& secondary_claim_bits() const noexcept {
        return secondary_claim_bits_;
    }
    void restore_claim_bits(
        const std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>&
            equipment,
        const std::array<std::uint64_t, limits::kRoomSecondaryClaimWords>&
            secondary) noexcept;

    [[nodiscard]] RoomDropSpatialIndex& spatial_index() noexcept {
        return spatial_index_;
    }
    [[nodiscard]] const RoomDropSpatialIndex& spatial_index() const noexcept {
        return spatial_index_;
    }
    [[nodiscard]] std::uint16_t active_equipment_count() const noexcept {
        return active_equipment_count_;
    }
    [[nodiscard]] std::uint16_t active_material_count() const noexcept {
        return active_material_count_;
    }
    [[nodiscard]] std::uint16_t active_health_potion_count() const noexcept {
        return active_health_potion_count_;
    }

private:
    std::array<GroundItem, kAuthoritativeEquipmentDropCapacity> equipment_{};
    std::array<GroundMaterial, kAuthoritativeSecondaryDropCapacity> materials_{};
    std::array<GroundHealthPotion, kAuthoritativeHealthPotionCapacity>
        health_potions_{};
    std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>
        equipment_claim_bits_{};
    std::array<std::uint64_t, limits::kRoomSecondaryClaimWords>
        secondary_claim_bits_{};
    std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>
        health_potion_presence_bits_{};
    RoomDropSpatialIndex spatial_index_{};
    std::uint16_t active_equipment_count_{};
    std::uint16_t active_material_count_{};
    std::uint16_t active_health_potion_count_{};
};

static_assert(kAuthoritativeEquipmentDropCapacity == 1152U);
static_assert(kAuthoritativeSecondaryDropCapacity == 2320U);
static_assert(kAuthoritativeHealthPotionCapacity == 1152U);
static_assert(common_material_ordinal(1124U) == 2248U);
static_assert(secondary_drop_ordinal(1124U) == 2249U);
static_assert(secondary_drop_ordinal(1124U) < kAbyssSecondaryOrdinalBegin);

}  // namespace arpg::dungeon
