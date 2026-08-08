#pragma once

#include "combat/room_environment_plan.hpp"
#include "combat/room_monster_plan.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::dungeon {

inline constexpr std::uint32_t kRoomMonsterGeneratorVersion = 1U;
inline constexpr std::uint32_t kRoomEnvironmentGeneratorVersion = 1U;

struct RoomEnvironmentBuildResult final {
    DungeonFault fault{DungeonFault::none};
};

inline constexpr std::size_t kVisibleEnvironmentCapacity = 128U;
inline constexpr std::size_t kEnvironmentQueryCandidateCapacity = 105U;

struct VisibleEnvironmentSet final {
    std::array<combat::RoomEnvironmentRecord, kVisibleEnvironmentCapacity>
        records{};
    std::uint16_t count{};
    std::uint16_t candidates_examined{};
};

enum class VisibleEnvironmentQueryStatus : std::uint8_t {
    ok,
    invalid_query,
    hard_fault,
};

struct VisibleEnvironmentQueryResult final {
    VisibleEnvironmentQueryStatus status{
        VisibleEnvironmentQueryStatus::invalid_query};
    DungeonFault fault{DungeonFault::none};
};

[[nodiscard]] VisibleEnvironmentQueryResult query_visible_environment(
    const combat::RoomEnvironmentBlueprint& blueprint,
    const combat::Aabb& world_bounds,
    VisibleEnvironmentSet& output) noexcept;

[[nodiscard]] bool write_visible_environment(
    const combat::RoomEnvironmentBlueprint& blueprint,
    const combat::Aabb& world_bounds,
    VisibleEnvironmentSet& output) noexcept;

static_assert(kEnvironmentQueryCandidateCapacity == 7U * 5U * 3U);
static_assert(kEnvironmentQueryCandidateCapacity
    <= kVisibleEnvironmentCapacity);

[[nodiscard]] RoomEnvironmentBuildResult build_room_environment(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    const combat::RoomMonsterPlan& monsters,
    combat::RoomEnvironmentBlueprint& out_environment) noexcept;

[[nodiscard]] bool room_environment_legal(
    const checkpoint::RoomDescriptor& room,
    const combat::RoomMonsterPlan& monsters,
    const combat::RoomEnvironmentBlueprint& environment) noexcept;

namespace test_support {

[[nodiscard]] VisibleEnvironmentQueryResult
query_visible_environment_with_output_capacity(
    const combat::RoomEnvironmentBlueprint& blueprint,
    const combat::Aabb& world_bounds,
    std::size_t output_capacity,
    VisibleEnvironmentSet& output) noexcept;

[[nodiscard]] RoomEnvironmentBuildResult
build_room_environment_with_record_count(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    const combat::RoomMonsterPlan& monsters,
    std::uint16_t requested_record_count,
    combat::RoomEnvironmentBlueprint& out_environment) noexcept;

[[nodiscard]] RoomEnvironmentBuildResult
build_room_environment_with_forced_placement_failure(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    const combat::RoomMonsterPlan& monsters,
    combat::RoomEnvironmentBlueprint& out_environment) noexcept;

}  // namespace test_support
}  // namespace arpg::dungeon
