#pragma once

#include "material_asset_types.hpp"

#include "dungeon/dungeon_types.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {

enum class ChaosRoomPropId : std::uint8_t {
    floor,
    wall,
    door,
    hole,
    rift_lantern,
    anomaly_condenser,
    warning_obelisk,
    count,
};

struct ChaosRoomPropDefinition final {
    ChaosRoomPropId id{ChaosRoomPropId::floor};
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    MaterialLayer layer{MaterialLayer::body};
};

struct ChaosRoomMaterialSlice final {
    std::array<ChaosRoomPropDefinition,
        static_cast<std::size_t>(ChaosRoomPropId::count)> props{};
};

struct ChaosRoomRenderPlan final {
    bool active{};
    MaterialAtlasId background_atlas{MaterialAtlasId::chaos_environment};
    Rectangle background_source{};
};

[[nodiscard]] const ChaosRoomMaterialSlice&
chaos_room_material_slice() noexcept;
[[nodiscard]] ChaosRoomRenderPlan chaos_room_render_plan(
    dungeon::DungeonElement ecology) noexcept;

}  // namespace arpg::platform
