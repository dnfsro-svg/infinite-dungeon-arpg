#pragma once

#include "environment_prop_layout.hpp"
#include "material_asset_types.hpp"

#include "dungeon/dungeon_types.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {

enum class LightningRoomPropId : std::uint8_t {
    floor,
    wall,
    door,
    hole,
    arc_lamp,
    capacitor_bank,
    grounding_rod,
    count,
};

struct LightningRoomPropDefinition final {
    LightningRoomPropId id{LightningRoomPropId::floor};
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    MaterialLayer layer{MaterialLayer::body};
};

struct LightningRoomMaterialSlice final {
    std::array<LightningRoomPropDefinition,
        static_cast<std::size_t>(LightningRoomPropId::count)> props{};
    std::array<EnvironmentPropDefinition, 5> environment_props{};
};

[[nodiscard]] const LightningRoomMaterialSlice&
lightning_room_material_slice() noexcept;

}  // namespace arpg::platform
