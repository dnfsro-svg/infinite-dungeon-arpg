#pragma once

#include "combat/room_monster_plan.hpp"
#include "dungeon/dungeon_rules.hpp"
#include "dungeon/room_affix.hpp"

#include <cstdint>

namespace arpg::dungeon {

struct RoomMonsterPlanBuildResult final {
    DungeonFault fault{DungeonFault::none};
    RoomDensityRoll density{};
};

[[nodiscard]] RoomMonsterPlanBuildResult build_room_monster_plan(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    combat::RoomMonsterPlan& out_plan) noexcept;

[[nodiscard]] bool room_monster_plan_legal(
    const checkpoint::RoomDescriptor& room,
    const combat::RoomMonsterPlan& plan) noexcept;

[[nodiscard]] bool room_monster_plan_equal_fields(
    const combat::RoomMonsterPlan& left,
    const combat::RoomMonsterPlan& right) noexcept;

namespace test_support {

[[nodiscard]] RoomMonsterPlanBuildResult build_room_monster_plan_with_count(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    std::uint16_t forced_count,
    combat::RoomMonsterPlan& out_plan) noexcept;

}  // namespace test_support

}  // namespace arpg::dungeon
