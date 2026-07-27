#pragma once

#include "combat/combat_types.hpp"
#include "combat/room_bounds.hpp"
#include "core/gameplay_limits.hpp"

#include <cstddef>
#include <cstdint>

namespace arpg::combat::room_spatial {

inline constexpr std::size_t columns = 20U;
inline constexpr std::size_t rows = 20U;
inline constexpr std::size_t maximum_monsters_per_cell = 3U;
inline constexpr float cell_width = room_bounds::width
    / static_cast<float>(columns);
inline constexpr float cell_depth = room_bounds::depth
    / static_cast<float>(rows);
inline constexpr float maximum_view_width = 32.0F;
inline constexpr float maximum_view_depth = 11.0F;
inline constexpr std::size_t maximum_view_columns = 5U;
inline constexpr std::size_t maximum_view_rows = 3U;
inline constexpr std::size_t roaming_halo_cells = 1U;
inline constexpr std::size_t maximum_streaming_columns =
    maximum_view_columns + roaming_halo_cells * 2U;
inline constexpr std::size_t maximum_streaming_rows =
    maximum_view_rows + roaming_halo_cells * 2U;
inline constexpr std::size_t maximum_streaming_monsters =
    maximum_streaming_columns * maximum_streaming_rows
    * maximum_monsters_per_cell;

static_assert(columns * rows * maximum_monsters_per_cell >= 1125U);
static_assert(maximum_view_width
    < cell_width * static_cast<float>(maximum_view_columns));
static_assert(maximum_view_depth
    < cell_depth * static_cast<float>(maximum_view_rows));
static_assert(maximum_streaming_monsters < limits::kActiveMonsterCapacity);
static_assert(limits::kDefeatLedgerCapacity
    >= limits::kActiveMonsterCapacity);

}  // namespace arpg::combat::room_spatial

namespace arpg::combat {

struct RoomStreamingRegion final {
    std::uint8_t first_column{};
    std::uint8_t column_count{};
    std::uint8_t first_row{};
    std::uint8_t row_count{};
};

[[nodiscard]] RoomStreamingRegion make_room_streaming_region(
    Vec3 player_position) noexcept;

}  // namespace arpg::combat
