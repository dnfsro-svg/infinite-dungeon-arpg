#include "water_room_material_slice.hpp"

namespace arpg::platform {
namespace {

constexpr WaterRoomMaterialSlice kWaterRoomSlice{{{
    {WaterRoomPropId::floor, MaterialSpriteId::environment_floor_water,
        MaterialLayer::body},
    {WaterRoomPropId::wall, MaterialSpriteId::water_wall,
        MaterialLayer::body},
    {WaterRoomPropId::door, MaterialSpriteId::environment_door_water,
        MaterialLayer::front_effect},
    {WaterRoomPropId::hole, MaterialSpriteId::water_hole,
        MaterialLayer::body},
    {WaterRoomPropId::lantern, MaterialSpriteId::water_lantern,
        MaterialLayer::front_effect},
    {WaterRoomPropId::coral, MaterialSpriteId::water_coral,
        MaterialLayer::body},
    {WaterRoomPropId::grate, MaterialSpriteId::water_grate,
        MaterialLayer::body},
}}};

}  // namespace

const WaterRoomMaterialSlice& water_room_material_slice() noexcept {
    return kWaterRoomSlice;
}

WaterRoomRenderPlan water_room_render_plan(
    dungeon::DungeonElement ecology) noexcept {
    return {ecology == dungeon::DungeonElement::water,
        MaterialAtlasId::water_environment,
        {0.0F, 0.0F, 512.0F, 512.0F}};
}

}  // namespace arpg::platform
