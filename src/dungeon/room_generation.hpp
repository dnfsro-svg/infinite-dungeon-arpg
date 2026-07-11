#pragma once

#include "dungeon/dungeon_types.hpp"

#include <cstdint>
#include <optional>

namespace arpg::dungeon {

[[nodiscard]] EntrySide entry_side_for_exit(ExitDirection exit) noexcept;

[[nodiscard]] std::uint64_t derive_initial_room_seed(
    std::uint64_t root_seed,
    std::uint64_t room_index) noexcept;

[[nodiscard]] std::uint64_t derive_next_room_seed(
    std::uint64_t current_seed,
    std::uint64_t next_index,
    ExitDirection exit) noexcept;

[[nodiscard]] RoomDescriptor make_initial_room(
    const DungeonSessionConfig& config) noexcept;

[[nodiscard]] std::optional<RoomDescriptor> make_next_room(
    const RoomDescriptor& current,
    ExitDirection exit) noexcept;

}  // namespace arpg::dungeon
