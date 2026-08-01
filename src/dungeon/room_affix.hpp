#pragma once

#include <cstdint>

namespace arpg::dungeon {

enum class RoomDensityAffix : std::uint8_t {
    crowded,
    dense,
    horde,
    count,
};

struct RoomDensityDefinition final {
    RoomDensityAffix id{RoomDensityAffix::crowded};
    std::uint8_t weight{};
    std::uint16_t minimum{};
    std::uint16_t maximum{};
};

struct RoomDensityRoll final {
    RoomDensityAffix affix{RoomDensityAffix::crowded};
    std::uint16_t base_count{};
    std::uint16_t total_count{};
};

[[nodiscard]] RoomDensityRoll roll_room_density(
    std::uint64_t room_seed,
    bool abyss) noexcept;

[[nodiscard]] const RoomDensityDefinition* room_density_definition(
    RoomDensityAffix affix) noexcept;

}  // namespace arpg::dungeon
