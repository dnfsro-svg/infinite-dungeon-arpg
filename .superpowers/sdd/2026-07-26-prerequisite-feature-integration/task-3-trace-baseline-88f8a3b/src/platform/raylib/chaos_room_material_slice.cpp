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
}}, {{
    {MaterialSpriteId::chaos_wall, MaterialLayer::body,
        {10.0F, 8.0F, 236.0F, 236.0F}, {128.0F, 244.0F}, 0.90F},
    {MaterialSpriteId::chaos_hole, MaterialLayer::body,
        {53.0F, 46.0F, 150.0F, 198.0F}, {128.0F, 244.0F}, 0.77F},
    {MaterialSpriteId::chaos_rift_lantern, MaterialLayer::front_effect,
        {109.0F, 170.0F, 38.0F, 66.0F}, {128.0F, 236.0F}, 0.55F},
    {MaterialSpriteId::chaos_anomaly_condenser, MaterialLayer::body,
        {80.0F, 67.0F, 87.0F, 169.0F}, {123.0F, 236.0F}, 0.55F},
    {MaterialSpriteId::chaos_warning_obelisk, MaterialLayer::body,
        {76.0F, 68.0F, 96.0F, 168.0F}, {124.0F, 236.0F}, 0.55F},
}}};

}  // namespace

const ChaosRoomMaterialSlice& chaos_room_material_slice() noexcept {
    return kChaosRoomSlice;
}

}  // namespace arpg::platform
