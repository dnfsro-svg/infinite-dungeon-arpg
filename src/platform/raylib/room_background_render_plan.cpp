#include "room_background_render_plan.hpp"

#include "combat/room_bounds.hpp"
#include "render_layout.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::platform {

RoomBackgroundRenderPlan room_background_render_plan(
    dungeon::DungeonElement ecology) noexcept {
    switch (ecology) {
    case dungeon::DungeonElement::water:
        return {MaterialAtlasId::water_room_background};
    case dungeon::DungeonElement::lightning:
        return {MaterialAtlasId::lightning_room_background};
    case dungeon::DungeonElement::chaos:
        return {MaterialAtlasId::chaos_room_background};
    case dungeon::DungeonElement::fire:
        break;
    }
    return {MaterialAtlasId::fire_room_background};
}

float room_background_scale(float viewport_width,
    float viewport_height) noexcept {
    if (viewport_width <= 0.0F || viewport_height <= 0.0F) return 0.0F;
    return (std::min)({1.0F, viewport_width / 2560.0F,
        viewport_height / 1440.0F});
}

RoomBackgroundWorldTilePlan room_background_world_tile_plan(
    dungeon::DungeonElement ecology,
    const CombatCameraView& camera) noexcept {
    constexpr std::size_t kTileAxisCount = 10U;
    constexpr float kSourceTileWidth = 256.0F;
    constexpr float kSourceTileHeight = 144.0F;
    const float world_tile_width = combat::room_bounds::width
        / static_cast<float>(kTileAxisCount);
    const float world_tile_depth = combat::room_bounds::depth
        / static_cast<float>(kTileAxisCount);
    RoomBackgroundWorldTilePlan plan{};
    plan.atlas = room_background_render_plan(ecology).atlas;
    if (!std::isfinite(camera.center.x)
            || !std::isfinite(camera.center.y)
            || !std::isfinite(camera.visible_width)
            || !std::isfinite(camera.visible_depth)
            || camera.visible_width <= 0.0F
            || camera.visible_depth <= 0.0F) {
        return plan;
    }

    const float query_min_x = camera.center.x
        - camera.visible_width * 0.5F - world_tile_width;
    const float query_max_x = camera.center.x
        + camera.visible_width * 0.5F + world_tile_width;
    const float query_min_y = camera.center.y
        - camera.visible_depth * 0.5F - world_tile_depth;
    const float query_max_y = camera.center.y
        + camera.visible_depth * 0.5F + world_tile_depth;
    for (std::uint8_t row = 0U; row < kTileAxisCount; ++row) {
        const float tile_min_y = combat::room_bounds::min_y
            + static_cast<float>(row) * world_tile_depth;
        const float tile_max_y = tile_min_y + world_tile_depth;
        if (tile_max_y < query_min_y || tile_min_y > query_max_y) continue;
        for (std::uint8_t column = 0U;
                column < kTileAxisCount; ++column) {
            const float tile_min_x = combat::room_bounds::min_x
                + static_cast<float>(column) * world_tile_width;
            const float tile_max_x = tile_min_x + world_tile_width;
            if (tile_max_x < query_min_x || tile_min_x > query_max_x) {
                continue;
            }
            if (plan.count >= plan.tiles.size()) return {};
            plan.tiles[plan.count++] = {
                {static_cast<float>(column) * kSourceTileWidth,
                    static_cast<float>(row) * kSourceTileHeight,
                    kSourceTileWidth, kSourceTileHeight},
                {{tile_min_x, tile_min_y, -1.0F},
                    {tile_max_x, tile_max_y, 32.0F}},
                row,
                column,
            };
        }
    }
    plan.valid = true;
    return plan;
}

ProjectedRoomBackgroundWorldTile project_room_background_world_tile(
    const RoomBackgroundWorldTile& tile,
    const CombatCameraView& camera,
    float viewport_width,
    float viewport_height) noexcept {
    ProjectedRoomBackgroundWorldTile result{};
    const combat::Aabb& bounds = tile.world_bounds;
    if (!std::isfinite(viewport_width) || !std::isfinite(viewport_height)
            || viewport_width <= 0.0F || viewport_height <= 0.0F
            || !std::isfinite(tile.source.x)
            || !std::isfinite(tile.source.y)
            || !std::isfinite(tile.source.width)
            || !std::isfinite(tile.source.height)
            || tile.source.x < 0.0F || tile.source.y < 0.0F
            || tile.source.width <= 0.0F || tile.source.height <= 0.0F
            || !std::isfinite(bounds.minimum.x)
            || !std::isfinite(bounds.minimum.y)
            || !std::isfinite(bounds.maximum.x)
            || !std::isfinite(bounds.maximum.y)
            || bounds.minimum.x >= bounds.maximum.x
            || bounds.minimum.y >= bounds.maximum.y) {
        return result;
    }
    const RenderProjection back_left = project_render_world(
        bounds.minimum.x, bounds.minimum.y, 0.0F,
        camera, viewport_width, viewport_height);
    const RenderProjection back_right = project_render_world(
        bounds.maximum.x, bounds.minimum.y, 0.0F,
        camera, viewport_width, viewport_height);
    const RenderProjection front_left = project_render_world(
        bounds.minimum.x, bounds.maximum.y, 0.0F,
        camera, viewport_width, viewport_height);
    const RenderProjection front_right = project_render_world(
        bounds.maximum.x, bounds.maximum.y, 0.0F,
        camera, viewport_width, viewport_height);
    const auto finite_point = [](Vector2 point) noexcept {
        return std::isfinite(point.x) && std::isfinite(point.y);
    };
    result.destination = {
        {back_left.x, back_left.ground_y},
        {front_left.x, front_left.ground_y},
        {front_right.x, front_right.ground_y},
        {back_right.x, back_right.ground_y},
    };
    if (!finite_point(result.destination.top_left)
            || !finite_point(result.destination.bottom_left)
            || !finite_point(result.destination.bottom_right)
            || !finite_point(result.destination.top_right)
            || result.destination.top_right.x
                <= result.destination.top_left.x
            || result.destination.bottom_right.x
                <= result.destination.bottom_left.x
            || result.destination.bottom_left.y
                <= result.destination.top_left.y
            || result.destination.bottom_right.y
                <= result.destination.top_right.y) {
        result.destination = {};
        return result;
    }
    result.valid = true;
    return result;
}

}  // namespace arpg::platform
