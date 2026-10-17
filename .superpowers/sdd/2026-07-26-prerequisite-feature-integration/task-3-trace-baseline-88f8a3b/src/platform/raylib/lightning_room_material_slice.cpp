#include "lightning_room_material_slice.hpp"

namespace arpg::platform {
namespace {

constexpr LightningRoomMaterialSlice kLightningRoomSlice{{{
    {LightningRoomPropId::floor, MaterialSpriteId::environment_floor_lightning,
        MaterialLayer::body},
    {LightningRoomPropId::wall, MaterialSpriteId::lightning_wall,
        MaterialLayer::body},
    {LightningRoomPropId::door, MaterialSpriteId::environment_door_lightning,
        MaterialLayer::front_effect},
    {LightningRoomPropId::hole, MaterialSpriteId::lightning_hole,
        MaterialLayer::body},
    {LightningRoomPropId::arc_lamp, MaterialSpriteId::lightning_arc_lamp,
        MaterialLayer::front_effect},
    {LightningRoomPropId::capacitor_bank,
        MaterialSpriteId::lightning_capacitor_bank, MaterialLayer::body},
    {LightningRoomPropId::grounding_rod,
        MaterialSpriteId::lightning_grounding_rod, MaterialLayer::body},
}}, {{
    {MaterialSpriteId::lightning_wall, MaterialLayer::body,
        {10.0F, 8.0F, 236.0F, 236.0F}, {128.0F, 244.0F}, 0.90F},
    {MaterialSpriteId::lightning_hole, MaterialLayer::body,
        {14.0F, 8.0F, 227.0F, 236.0F}, {127.0F, 244.0F}, 0.77F},
    {MaterialSpriteId::lightning_arc_lamp, MaterialLayer::front_effect,
        {63.0F, 12.0F, 129.0F, 226.0F}, {127.0F, 238.0F}, 0.55F},
    {MaterialSpriteId::lightning_capacitor_bank, MaterialLayer::body,
        {62.0F, 11.0F, 139.0F, 229.0F}, {131.0F, 240.0F}, 0.55F},
    {MaterialSpriteId::lightning_grounding_rod, MaterialLayer::body,
        {78.0F, 15.0F, 108.0F, 229.0F}, {132.0F, 244.0F}, 0.55F},
}}};

}  // namespace

const LightningRoomMaterialSlice& lightning_room_material_slice() noexcept {
    return kLightningRoomSlice;
}

}  // namespace arpg::platform
