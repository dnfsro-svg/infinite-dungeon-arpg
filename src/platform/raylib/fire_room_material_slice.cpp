#include "fire_room_material_slice.hpp"

namespace arpg::platform {
namespace {

constexpr std::array<FireRoomObstacle, 3> kFireRoomObstacles{{
    {0.0F, 0.0F, 1.40F, 1.70F, false},
    {-3.20F, 0.0F, 0.80F, 0.80F, true},
    {3.20F, 0.0F, 0.80F, 0.80F, true},
}};

constexpr FireRoomMaterialSlice kFireRoomSlice{
    std::array<FireRoomPropDefinition,
        static_cast<std::size_t>(FireRoomPropId::count)>{{
    {FireRoomPropId::floor, MaterialSpriteId::environment_floor_fire,
        MaterialLayer::body, false, false},
    {FireRoomPropId::wall, MaterialSpriteId::fire_wall,
        MaterialLayer::body, false, false},
    {FireRoomPropId::door, MaterialSpriteId::environment_door_fire,
        MaterialLayer::front_effect, false, false},
    {FireRoomPropId::torch, MaterialSpriteId::fire_torch,
        MaterialLayer::front_effect, false, false},
    {FireRoomPropId::chain, MaterialSpriteId::fire_chain,
        MaterialLayer::front_effect, false, false},
    {FireRoomPropId::banner, MaterialSpriteId::fire_banner,
        MaterialLayer::front_effect, false, false},
    {FireRoomPropId::weapon_rack, MaterialSpriteId::fire_weapon_rack,
        MaterialLayer::body, false, false},
    {FireRoomPropId::bone_pile, MaterialSpriteId::fire_bone_pile,
        MaterialLayer::body, false, false},
    {FireRoomPropId::breakable_crate, MaterialSpriteId::fire_breakable_crate,
        MaterialLayer::body, false, true},
    {FireRoomPropId::solid_brazier, MaterialSpriteId::fire_solid_brazier,
        MaterialLayer::body, true, false},
}}};

}  // namespace

const FireRoomMaterialSlice& fire_room_material_slice() noexcept {
    return kFireRoomSlice;
}

const std::array<FireRoomObstacle, 3>& fire_room_obstacles() noexcept {
    return kFireRoomObstacles;
}

bool fire_room_route_is_clear(float from_y, float to_y) noexcept {
    constexpr float kRouteHalfWidth = 0.45F;
    for (const FireRoomObstacle& obstacle : kFireRoomObstacles) {
        if (obstacle.breakable) continue;
        const float low = obstacle.center_y - obstacle.half_height - kRouteHalfWidth;
        const float high = obstacle.center_y + obstacle.half_height + kRouteHalfWidth;
        if (from_y >= low && from_y <= high && to_y >= low && to_y <= high) {
            return false;
        }
    }
    return true;
}

bool fire_room_has_two_navigation_routes() noexcept {
    return fire_room_route_is_clear(-4.0F, -4.0F)
        && fire_room_route_is_clear(4.0F, 4.0F);
}

}  // namespace arpg::platform
