#include "combat/room_spatial_grid.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {
namespace {

[[nodiscard]] std::size_t cell_for(
    float coordinate,
    float minimum,
    float cell_size,
    std::size_t cell_count) noexcept {
    if (!std::isfinite(coordinate)) coordinate = 0.0F;
    const float normalized = (coordinate - minimum) / cell_size;
    if (normalized <= 0.0F) return 0U;
    if (normalized >= static_cast<float>(cell_count)) {
        return cell_count - 1U;
    }
    return static_cast<std::size_t>(normalized);
}

struct AxisRegion final {
    std::uint8_t first{};
    std::uint8_t count{};
};

[[nodiscard]] AxisRegion make_axis_region(
    float player_coordinate,
    float minimum,
    float maximum,
    float cell_size,
    std::size_t cell_count,
    float view_extent) noexcept {
    if (!std::isfinite(player_coordinate)) player_coordinate = 0.0F;
    const float half_view = view_extent * 0.5F;
    const float camera_center = std::clamp(player_coordinate,
        minimum + half_view, maximum - half_view);
    const std::size_t first_visible = cell_for(
        camera_center - half_view, minimum, cell_size, cell_count);
    const std::size_t last_visible = cell_for(
        camera_center + half_view, minimum, cell_size, cell_count);
    const std::size_t first = first_visible > room_spatial::roaming_halo_cells
        ? first_visible - room_spatial::roaming_halo_cells : 0U;
    const std::size_t last = (std::min)(
        last_visible + room_spatial::roaming_halo_cells, cell_count - 1U);
    return {static_cast<std::uint8_t>(first),
        static_cast<std::uint8_t>(last - first + 1U)};
}

}  // namespace

RoomStreamingRegion make_room_streaming_region(
    Vec3 player_position) noexcept {
    const AxisRegion columns = make_axis_region(
        player_position.x, room_bounds::min_x, room_bounds::max_x,
        room_spatial::cell_width, room_spatial::columns,
        room_spatial::maximum_view_width);
    const AxisRegion rows = make_axis_region(
        player_position.y, room_bounds::min_y, room_bounds::max_y,
        room_spatial::cell_depth, room_spatial::rows,
        room_spatial::maximum_view_depth);
    return {
        columns.first, columns.count, rows.first, rows.count,
    };
}

}  // namespace arpg::combat
