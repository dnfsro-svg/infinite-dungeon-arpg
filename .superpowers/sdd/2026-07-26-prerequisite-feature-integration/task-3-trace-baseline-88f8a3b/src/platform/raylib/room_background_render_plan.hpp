#pragma once

#include "material_asset_types.hpp"

#include "dungeon/dungeon_types.hpp"

namespace arpg::platform {

struct RoomBackgroundRenderPlan final {
    MaterialAtlasId atlas{MaterialAtlasId::fire_room_background};
    Rectangle source{0.0F, 0.0F, 2560.0F, 1440.0F};
};

[[nodiscard]] RoomBackgroundRenderPlan room_background_render_plan(
    dungeon::DungeonElement ecology) noexcept;

[[nodiscard]] float room_background_scale(
    float viewport_width, float viewport_height) noexcept;

}  // namespace arpg::platform
