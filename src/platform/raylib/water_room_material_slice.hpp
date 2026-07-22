#pragma once

#include "material_asset_types.hpp"

#include "dungeon/dungeon_types.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {

enum class WaterRoomPropId : std::uint8_t {
    floor,
    wall,
    door,
    hole,
    lantern,
    coral,
    grate,
    count,
};

struct WaterRoomPropDefinition final {
    WaterRoomPropId id{WaterRoomPropId::floor};
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    MaterialLayer layer{MaterialLayer::body};
};

struct WaterRoomMaterialSlice final {
    std::array<WaterRoomPropDefinition,
        static_cast<std::size_t>(WaterRoomPropId::count)> props{};
};

struct WaterRoomRenderPlan final {
    bool active{};
    MaterialAtlasId background_atlas{MaterialAtlasId::water_environment};
    Rectangle background_source{};
};

[[nodiscard]] const WaterRoomMaterialSlice&
water_room_material_slice() noexcept;
[[nodiscard]] WaterRoomRenderPlan water_room_render_plan(
    dungeon::DungeonElement ecology) noexcept;

}  // namespace arpg::platform
