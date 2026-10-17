#pragma once

#include "combat/combat_types.hpp"
#include "material_asset_types.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {

enum class FireRoomPropId : std::uint8_t {
    floor,
    wall,
    door,
    torch,
    chain,
    banner,
    weapon_rack,
    bone_pile,
    breakable_crate,
    solid_brazier,
    count,
};

struct FireRoomPropDefinition final {
    FireRoomPropId id{FireRoomPropId::floor};
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    MaterialLayer layer{MaterialLayer::body};
    bool blocks_navigation{};
    bool breakable{};
};

struct FireRoomMaterialSlice final {
    std::array<FireRoomPropDefinition,
        static_cast<std::size_t>(FireRoomPropId::count)> props{};
};

struct FireRoomObstacle final {
    float center_x{};
    float center_y{};
    float half_width{};
    float half_height{};
    bool breakable{};
};

[[nodiscard]] const FireRoomMaterialSlice& fire_room_material_slice() noexcept;
[[nodiscard]] const std::array<FireRoomObstacle, 3>&
fire_room_obstacles() noexcept;
[[nodiscard]] bool fire_room_route_is_clear(float from_y, float to_y) noexcept;
[[nodiscard]] bool fire_room_crate_visible(
    const combat::CombatSnapshot& snapshot, std::size_t crate_index) noexcept;
[[nodiscard]] bool fire_room_has_two_navigation_routes() noexcept;

}  // namespace arpg::platform
