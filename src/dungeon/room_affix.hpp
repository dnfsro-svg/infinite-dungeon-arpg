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
    std::uint8_t minimum{};
    std::uint8_t maximum{};
};

struct RoomDensityRoll final {
    RoomDensityAffix affix{RoomDensityAffix::crowded};
    std::uint8_t base_count{};
    std::uint8_t monster_count{};
};

[[nodiscard]] RoomDensityRoll roll_room_density(
    std::uint64_t room_seed,
    bool is_abyss) noexcept;

[[nodiscard]] const RoomDensityDefinition* room_density_definition(
    RoomDensityAffix affix) noexcept;

}  // namespace arpg::dungeon
