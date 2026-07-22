#include "chaos_room_material_slice.hpp"

namespace arpg::platform {
namespace {

constexpr ChaosRoomMaterialSlice kChaosRoomSlice{{{
    {ChaosRoomPropId::floor, MaterialSpriteId::environment_floor_chaos,
        MaterialLayer::body},
    {ChaosRoomPropId::wall, MaterialSpriteId::chaos_wall,
        MaterialLayer::body},
    {ChaosRoomPropId::door, MaterialSpriteId::environment_door_chaos,
        MaterialLayer::front_effect},
    {ChaosRoomPropId::hole, MaterialSpriteId::chaos_hole,
        MaterialLayer::body},
    {ChaosRoomPropId::rift_lantern, MaterialSpriteId::chaos_rift_lantern,
        MaterialLayer::front_effect},
    {ChaosRoomPropId::anomaly_condenser,
        MaterialSpriteId::chaos_anomaly_condenser, MaterialLayer::body},
    {ChaosRoomPropId::warning_obelisk,
        MaterialSpriteId::chaos_warning_obelisk, MaterialLayer::body},
}}};

}  // namespace

const ChaosRoomMaterialSlice& chaos_room_material_slice() noexcept {
    return kChaosRoomSlice;
}

ChaosRoomRenderPlan chaos_room_render_plan(
    dungeon::DungeonElement ecology) noexcept {
    return {ecology == dungeon::DungeonElement::chaos,
        MaterialAtlasId::chaos_environment,
        {0.0F, 0.0F, 512.0F, 512.0F}};
}

}  // namespace arpg::platform
