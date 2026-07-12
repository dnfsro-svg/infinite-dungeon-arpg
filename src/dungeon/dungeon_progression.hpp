#pragma once

#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/dungeon_rules.hpp"
#include "dungeon/room_generation.hpp"

#include <cstdint>

namespace arpg::dungeon {

struct RunStateBuildResult final {
    DungeonFault fault{DungeonFault::none};
    checkpoint::DungeonRunState state{};
    RoomRandomSamples samples{};
};

[[nodiscard]] RunStateBuildResult make_initial_run_state(
    std::uint64_t root_seed,
    const DungeonRules& rules) noexcept;

[[nodiscard]] RunStateBuildResult make_door_transition(
    const checkpoint::DungeonRunState& current,
    checkpoint::ExitDirection direction,
    const DungeonRules& rules) noexcept;

[[nodiscard]] RunStateBuildResult make_descent_transition(
    const checkpoint::DungeonRunState& current,
    const DungeonRules& rules) noexcept;

[[nodiscard]] bool same_run_state(
    const checkpoint::DungeonRunState& lhs,
    const checkpoint::DungeonRunState& rhs) noexcept;

}  // namespace arpg::dungeon
