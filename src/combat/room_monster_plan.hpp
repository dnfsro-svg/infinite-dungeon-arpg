#pragma once

#include "combat/combat_types.hpp"
#include "core/gameplay_limits.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {

inline constexpr std::size_t kRoomMonsterCellCount = 400U;
inline constexpr std::size_t kRoomMonsterCellOffsetCount =
    kRoomMonsterCellCount + 1U;

struct RoomMonsterBlueprint final {
    MonsterId id{MonsterId::count};
    MonsterAffixSet affixes{};
    Vec3 initial_position{};
    std::uint16_t spawn_ordinal{};
    std::uint16_t home_cell{};
    std::uint8_t roaming_leash_cells{1U};
};

struct RoomMonsterPlan final {
    RoomMonsterPlan() noexcept = default;
    ~RoomMonsterPlan() = default;

    RoomMonsterPlan(const RoomMonsterPlan&) = delete;
    RoomMonsterPlan& operator=(const RoomMonsterPlan&) = delete;
    RoomMonsterPlan(RoomMonsterPlan&&) = delete;
    RoomMonsterPlan& operator=(RoomMonsterPlan&&) = delete;

    std::array<RoomMonsterBlueprint,
        limits::kRoomMonsterCapacity> monsters{};
    std::array<std::uint16_t, kRoomMonsterCellOffsetCount> cell_offsets{};
    std::array<std::uint8_t, kRoomMonsterCellCount> cell_counts{};
    std::uint16_t monster_count{};
    std::uint32_t threat_total{};
    std::uint32_t generator_version{};
    std::uint64_t blueprint_hash{};
};

[[nodiscard]] constexpr Aabb room_monster_initial_bounds(
    const RoomMonsterBlueprint& monster) noexcept {
    constexpr Vec3 half_extents{0.70F, 0.50F, 1.90F};
    return {
        {
            monster.initial_position.x - half_extents.x,
            monster.initial_position.y - half_extents.y,
            monster.initial_position.z - half_extents.z,
        },
        {
            monster.initial_position.x + half_extents.x,
            monster.initial_position.y + half_extents.y,
            monster.initial_position.z + half_extents.z,
        },
    };
}

static_assert(limits::kRoomMonsterCapacity == 1152U);
static_assert(kRoomMonsterCellOffsetCount == 401U);

}  // namespace arpg::combat
