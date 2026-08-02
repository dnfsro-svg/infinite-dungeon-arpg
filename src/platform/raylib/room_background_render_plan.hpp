#pragma once

#include "material_asset_types.hpp"

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

struct CombatCameraView;

struct RoomBackgroundRenderPlan final {
    MaterialAtlasId atlas{MaterialAtlasId::fire_room_background};
    Rectangle source{0.0F, 0.0F, 2560.0F, 1440.0F};
};

[[nodiscard]] RoomBackgroundRenderPlan room_background_render_plan(
    dungeon::DungeonElement ecology) noexcept;

[[nodiscard]] float room_background_scale(
    float viewport_width, float viewport_height) noexcept;

inline constexpr std::size_t kRoomBackgroundWorldTileCapacity = 25U;

struct RoomBackgroundWorldTile final {
    Rectangle source{};
    combat::Aabb world_bounds{};
    std::uint8_t row{};
    std::uint8_t column{};
};

struct RoomBackgroundWorldTilePlan final {
    MaterialAtlasId atlas{MaterialAtlasId::fire_room_background};
    std::array<RoomBackgroundWorldTile,
        kRoomBackgroundWorldTileCapacity> tiles{};
    std::size_t count{};
    bool valid{};
};

struct ProjectedRoomBackgroundWorldTile final {
    Rectangle destination{};
    bool valid{};
};

[[nodiscard]] RoomBackgroundWorldTilePlan room_background_world_tile_plan(
    dungeon::DungeonElement ecology,
    const CombatCameraView& camera) noexcept;
[[nodiscard]] ProjectedRoomBackgroundWorldTile
project_room_background_world_tile(
    const RoomBackgroundWorldTile& tile,
    const CombatCameraView& camera,
    float viewport_width,
    float viewport_height) noexcept;

}  // namespace arpg::platform
