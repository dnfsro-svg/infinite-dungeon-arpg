#pragma once

#include "combat/room_bounds.hpp"
#include "combat_view_math.hpp"
#include "dungeon_view_math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

struct EnvironmentFrameAvailability final {
    bool floor{};
    bool door{};
    bool hole{};
    bool door_required{};
    bool hole_required{};
};

inline constexpr std::array<combat::Vec3, 4> kRoomDoorCenters{{
    {0.0F, combat::room_bounds::min_y, 0.0F},
    {0.0F, combat::room_bounds::max_y, 0.0F},
    {combat::room_bounds::min_x, 0.0F, 0.0F},
    {combat::room_bounds::max_x, 0.0F, 0.0F},
}};

inline constexpr std::size_t kRoomVerticalGridLineCount = 25U;
inline constexpr std::size_t kRoomHorizontalGridLineCount = 12U;
inline constexpr std::size_t kRoomGridLineCapacity =
    kRoomVerticalGridLineCount + kRoomHorizontalGridLineCount;

struct RoomProjectedLine final {
    combat::Vec3 world_start{};
    combat::Vec3 world_end{};
    ScreenProjection screen_start{};
    ScreenProjection screen_end{};
    bool visible{};
};

struct RoomGeometryPlan final {
    std::array<ScreenProjection, 4> corners{};
    std::array<ScreenProjection, 4> doors{};
    ScreenProjection hole{};
    std::array<RoomProjectedLine, kRoomGridLineCapacity> grid_lines{};
    std::uint8_t grid_line_count{};
};

[[nodiscard]] inline bool projected_line_intersects_viewport(
    ScreenProjection start, ScreenProjection end,
    float width, float height) noexcept {
    if (!(width > 0.0F) || !(height > 0.0F)
        || !std::isfinite(width) || !std::isfinite(height)) {
        return false;
    }
    const float minimum_x = (std::min)(start.x, end.x);
    const float maximum_x = (std::max)(start.x, end.x);
    const float minimum_y = (std::min)(start.ground_y, end.ground_y);
    const float maximum_y = (std::max)(start.ground_y, end.ground_y);
    return maximum_x >= 0.0F && minimum_x <= width
        && maximum_y >= 0.0F && minimum_y <= height;
}

[[nodiscard]] inline RoomGeometryPlan make_room_geometry_plan(
    CombatCameraView view, float width, float height) noexcept {
    RoomGeometryPlan plan{};
    constexpr std::array<combat::Vec3, 4> kCorners{{
        {combat::room_bounds::min_x, combat::room_bounds::min_y, 0.0F},
        {combat::room_bounds::max_x, combat::room_bounds::min_y, 0.0F},
        {combat::room_bounds::min_x, combat::room_bounds::max_y, 0.0F},
        {combat::room_bounds::max_x, combat::room_bounds::max_y, 0.0F},
    }};
    for (std::size_t index = 0U; index < kCorners.size(); ++index) {
        plan.corners[index] = project_combat_position(
            kCorners[index], view, width, height);
        plan.doors[index] = project_combat_position(
            kRoomDoorCenters[index], view, width, height);
    }
    plan.hole = project_combat_position(kHoleCenter, view, width, height);

    const auto append_line = [&](combat::Vec3 start,
                                 combat::Vec3 end) noexcept {
        RoomProjectedLine& line = plan.grid_lines[plan.grid_line_count++];
        line.world_start = start;
        line.world_end = end;
        line.screen_start = project_combat_position(start, view, width, height);
        line.screen_end = project_combat_position(end, view, width, height);
        line.visible = projected_line_intersects_viewport(
            line.screen_start, line.screen_end, width, height);
    };
    for (std::size_t index = 0U;
         index < kRoomVerticalGridLineCount; ++index) {
        const float x = combat::room_bounds::min_x
            + 2.0F * static_cast<float>(index);
        append_line({x, combat::room_bounds::min_y, 0.0F},
            {x, combat::room_bounds::max_y, 0.0F});
    }
    for (std::size_t index = 0U;
         index < kRoomHorizontalGridLineCount; ++index) {
        const float y = combat::room_bounds::min_y
            + 2.0F * static_cast<float>(index);
        append_line({combat::room_bounds::min_x, y, 0.0F},
            {combat::room_bounds::max_x, y, 0.0F});
    }
    return plan;
}

[[nodiscard]] constexpr bool should_draw_material_environment(
    EnvironmentFrameAvailability availability) noexcept {
    return availability.floor
        && (!availability.door_required || availability.door)
        && (!availability.hole_required || availability.hole);
}

}  // namespace arpg::platform
