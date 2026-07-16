#pragma once

#include "dungeon/dungeon_types.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

[[nodiscard]] EntrySide entry_side_for_exit(ExitDirection exit) noexcept;

[[nodiscard]] std::uint64_t derive_initial_room_seed(
    std::uint64_t root_seed,
    std::uint64_t room_index) noexcept;

[[nodiscard]] std::uint64_t derive_door_room_seed(
    std::uint64_t current_seed,
    std::uint64_t next_serial,
    checkpoint::ExitDirection direction) noexcept;

[[nodiscard]] std::uint64_t derive_descent_room_seed(
    std::uint64_t current_seed,
    std::uint64_t next_serial) noexcept;

[[nodiscard]] std::array<bool, 4> preview_abyss_doors(
    const checkpoint::RoomDescriptor& current) noexcept;

struct RoomRandomSamples final {
    std::uint64_t ecology{};
    std::uint32_t hole{};
    std::uint32_t abyss{};
};

struct RoomGenerationResult final {
    DungeonFault fault{DungeonFault::none};
    checkpoint::RoomDescriptor room{};
    RoomRandomSamples samples{};
};

[[nodiscard]] RoomGenerationResult generate_room_descriptor(
    std::uint64_t seed,
    std::uint64_t global_index,
    std::uint64_t depth,
    std::uint64_t floor_room_index,
    checkpoint::EntrySide entry,
    const std::array<std::uint32_t, 4>& biases,
    const DungeonRules& rules) noexcept;

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
