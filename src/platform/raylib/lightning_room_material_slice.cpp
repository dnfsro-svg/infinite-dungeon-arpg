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
}}};

}  // namespace

const LightningRoomMaterialSlice& lightning_room_material_slice() noexcept {
    return kLightningRoomSlice;
}

LightningRoomRenderPlan lightning_room_render_plan(
    dungeon::DungeonElement ecology) noexcept {
    return {ecology == dungeon::DungeonElement::lightning,
        MaterialAtlasId::lightning_environment,
        {0.0F, 0.0F, 512.0F, 512.0F}};
}

}  // namespace arpg::platform
