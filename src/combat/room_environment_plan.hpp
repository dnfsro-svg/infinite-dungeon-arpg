#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {

inline constexpr std::size_t kRoomEnvironmentCellCount = 400U;
inline constexpr std::size_t kRoomEnvironmentRecordCapacity = 1200U;

enum class RoomPropKind : std::uint8_t {
    torch,
    banner,
    weapon_rack,
    bone_pile,
    crate,
    brazier,
    lantern,
    coral,
    grate,
    arc_lamp,
    capacitor_bank,
    grounding_rod,
    rift_lantern,
    anomaly_condenser,
    warning_obelisk,
    count,
};

enum class RoomObstacleKind : std::uint8_t {
    none,
    solid,
    breakable,
    count,
};

struct RoomObstacleSpec final {
    Aabb bounds{};
    RoomObstacleKind kind{RoomObstacleKind::none};
    std::uint16_t max_hp{};
};

struct RoomEnvironmentRecord final {
    std::uint16_t ordinal{};
    std::uint16_t home_cell{};
    RoomPropKind prop{RoomPropKind::torch};
    Vec3 anchor{};
    std::uint16_t scale_bp{10000U};
    std::uint8_t quarter_turns{};
    bool mirror_x{};
    RoomObstacleSpec obstacle{};
};

struct RoomObstaclePlanView final {
    const RoomEnvironmentRecord* records{};
    const std::uint16_t* cell_offsets{};
    const std::uint8_t* cell_counts{};
    std::uint16_t record_count{};
};

class RoomEnvironmentBlueprint final {
public:
    RoomEnvironmentBlueprint() = default;
    RoomEnvironmentBlueprint(const RoomEnvironmentBlueprint&) = delete;
    RoomEnvironmentBlueprint& operator=(const RoomEnvironmentBlueprint&) =
        delete;
    RoomEnvironmentBlueprint(RoomEnvironmentBlueprint&&) = delete;
    RoomEnvironmentBlueprint& operator=(RoomEnvironmentBlueprint&&) = delete;

    std::array<RoomEnvironmentRecord,
        kRoomEnvironmentRecordCapacity> records{};
    std::array<std::uint16_t,
        kRoomEnvironmentCellCount + 1U> cell_offsets{};
    std::array<std::uint8_t,
        kRoomEnvironmentCellCount> cell_counts{};
    std::uint16_t record_count{};
    std::uint16_t obstacle_count{};
    std::uint32_t generator_version{};
    std::uint64_t blueprint_hash{};
};

[[nodiscard]] inline RoomObstaclePlanView room_obstacle_plan_view(
    const RoomEnvironmentBlueprint& blueprint) noexcept {
    return {
        blueprint.records.data(),
        blueprint.cell_offsets.data(),
        blueprint.cell_counts.data(),
        blueprint.record_count,
    };
}

static_assert(kRoomEnvironmentCellCount == 400U);
static_assert(kRoomEnvironmentRecordCapacity == 1200U);

}  // namespace arpg::combat
