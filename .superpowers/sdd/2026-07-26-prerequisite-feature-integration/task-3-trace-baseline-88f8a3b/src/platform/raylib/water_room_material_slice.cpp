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
}}, {{
    {MaterialSpriteId::water_wall, MaterialLayer::body,
        {10.0F, 8.0F, 236.0F, 236.0F}, {128.0F, 244.0F}, 0.92F},
    {MaterialSpriteId::water_hole, MaterialLayer::body,
        {13.0F, 119.0F, 233.0F, 125.0F}, {129.0F, 244.0F}, 0.77F},
    {MaterialSpriteId::water_lantern, MaterialLayer::front_effect,
        {91.0F, 135.0F, 74.0F, 101.0F}, {128.0F, 236.0F}, 0.58F},
    {MaterialSpriteId::water_coral, MaterialLayer::body,
        {53.0F, 83.0F, 149.0F, 153.0F}, {127.0F, 236.0F}, 0.58F},
    {MaterialSpriteId::water_grate, MaterialLayer::body,
        {17.0F, 112.0F, 222.0F, 129.0F}, {128.0F, 241.0F}, 0.58F},
}}};

}  // namespace

const WaterRoomMaterialSlice& water_room_material_slice() noexcept {
    return kWaterRoomSlice;
}

}  // namespace arpg::platform
