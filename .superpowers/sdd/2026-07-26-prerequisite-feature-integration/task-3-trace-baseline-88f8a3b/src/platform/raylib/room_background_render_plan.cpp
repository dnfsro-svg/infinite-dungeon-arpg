#include "room_background_render_plan.hpp"

#include <algorithm>

namespace arpg::platform {

RoomBackgroundRenderPlan room_background_render_plan(
    dungeon::DungeonElement ecology) noexcept {
    switch (ecology) {
    case dungeon::DungeonElement::water:
        return {MaterialAtlasId::water_room_background};
    case dungeon::DungeonElement::lightning:
        return {MaterialAtlasId::lightning_room_background};
    case dungeon::DungeonElement::chaos:
        return {MaterialAtlasId::chaos_room_background};
    case dungeon::DungeonElement::fire:
        break;
    }
    return {MaterialAtlasId::fire_room_background};
}

float room_background_scale(float viewport_width,
    float viewport_height) noexcept {
    if (viewport_width <= 0.0F || viewport_height <= 0.0F) return 0.0F;
    return (std::min)({1.0F, viewport_width / 2560.0F,
        viewport_height / 1440.0F});
}

}  // namespace arpg::platform
