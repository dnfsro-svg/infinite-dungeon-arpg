#pragma once

#include "combat/room_environment_plan.hpp"
#include "modifiers/effect_set.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {

inline constexpr std::size_t kRoomObstacleCapacity =
    kRoomEnvironmentCellCount;
inline constexpr std::uint16_t kInvalidRoomObstacleStateIndex = 0xFFFFU;

struct RoomObstacleState final {
    std::uint16_t ordinal{0xFFFFU};
    std::uint16_t home_cell{};
    RoomObstacleKind kind{RoomObstacleKind::none};
    Aabb bounds{};
    std::uint16_t hp{};
    std::uint16_t max_hp{};
    std::uint64_t broken_tick{};
    bool intact{};
    modifiers::EffectSet effects{};
};

class RoomObstacleRuntime final {
public:
    RoomObstacleRuntime() = default;
    RoomObstacleRuntime(const RoomObstacleRuntime&) = delete;
    RoomObstacleRuntime& operator=(const RoomObstacleRuntime&) = delete;
    RoomObstacleRuntime(RoomObstacleRuntime&&) = delete;
    RoomObstacleRuntime& operator=(RoomObstacleRuntime&&) = delete;

    [[nodiscard]] bool initialize(RoomObstaclePlanView view) noexcept;
    void clear() noexcept;
    [[nodiscard]] const RoomObstacleState* state(
        std::uint16_t environment_ordinal) const noexcept;
    [[nodiscard]] RoomObstacleState* state(
        std::uint16_t environment_ordinal) noexcept;
    [[nodiscard]] modifiers::EffectSet* effects(
        std::uint16_t environment_ordinal) noexcept;
    [[nodiscard]] bool apply_damage(
        std::uint16_t environment_ordinal,
        std::uint16_t damage,
        std::uint64_t tick) noexcept;
    [[nodiscard]] std::size_t obstacle_count() const noexcept;
    [[nodiscard]] RoomObstaclePlanView plan_view() const noexcept;
    [[nodiscard]] bool blocks_player(
        Vec3 current, Vec3 candidate) const noexcept;
    [[nodiscard]] Vec3 route_monster(
        Vec3 previous, Vec3 candidate) const noexcept;
    [[nodiscard]] std::size_t damage_overlapping(
        Aabb bounds, std::uint64_t tick) noexcept;

private:
    RoomObstaclePlanView plan_view_{};
    std::array<std::uint16_t,
        kRoomEnvironmentRecordCapacity> ordinal_to_state_{};
    std::array<RoomObstacleState, kRoomObstacleCapacity> states_{};
    std::uint16_t obstacle_count_{};
};

}  // namespace arpg::combat
