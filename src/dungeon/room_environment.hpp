#pragma once

#include "combat/room_environment_plan.hpp"
#include "combat/room_monster_plan.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <cstdint>

namespace arpg::dungeon {

inline constexpr std::uint32_t kRoomMonsterGeneratorVersion = 1U;
inline constexpr std::uint32_t kRoomEnvironmentGeneratorVersion = 1U;

struct RoomEnvironmentBuildResult final {
    DungeonFault fault{DungeonFault::none};
};

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
