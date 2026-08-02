#include "test_framework.hpp"

#include "combat/room_bounds.hpp"
#include "combat/room_spatial_grid.hpp"
#include "combat_view_math.hpp"
#include "dungeon/dungeon_types.hpp"
#include "material_manifest.hpp"
#include "render_layout.hpp"
#include "room_background_render_plan.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

using arpg::dungeon::DungeonElement;
using arpg::platform::MaterialAtlasDefinition;
using arpg::platform::MaterialAtlasId;
using arpg::platform::MaterialEcology;

const MaterialAtlasDefinition* find_atlas(MaterialAtlasId id) noexcept {
    const auto manifest = arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        if (manifest.atlases[index].id == id) return &manifest.atlases[index];
    }
    return nullptr;
}

arpg::test::Failure background_manifest_preserves_old_ids_and_adds_four_pairs() noexcept {
    static_assert(static_cast<std::size_t>(MaterialAtlasId::ui_material) == 21U);
    static_assert(static_cast<std::size_t>(
        MaterialAtlasId::fire_room_background) == 22U);
    static_assert(static_cast<std::size_t>(
        MaterialAtlasId::water_room_background) == 23U);
    static_assert(static_cast<std::size_t>(
        MaterialAtlasId::lightning_room_background) == 24U);
    static_assert(static_cast<std::size_t>(
        MaterialAtlasId::chaos_room_background) == 25U);
    static_assert(static_cast<std::size_t>(MaterialAtlasId::count) == 29U);

    const auto manifest = arpg::platform::default_material_manifest();
    ARPG_REQUIRE(manifest.atlas_count
        == static_cast<std::size_t>(MaterialAtlasId::count));
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        ARPG_REQUIRE(static_cast<std::size_t>(manifest.atlases[index].id)
            == index);
    }

    struct Expected final {
        MaterialAtlasId id;
        MaterialEcology ecology;
        std::string_view color_path;
        std::string_view material_path;
    };
    constexpr std::array<Expected, 4> expected{{
        {MaterialAtlasId::fire_room_background, MaterialEcology::fire,
            "assets/stage12/fire_room_background.png",
            "assets/stage12/fire_room_background_material.png"},
        {MaterialAtlasId::water_room_background, MaterialEcology::water,
            "assets/stage12/water_room_background.png",
            "assets/stage12/water_room_background_material.png"},
        {MaterialAtlasId::lightning_room_background, MaterialEcology::lightning,
            "assets/stage12/lightning_room_background.png",
            "assets/stage12/lightning_room_background_material.png"},
        {MaterialAtlasId::chaos_room_background, MaterialEcology::chaos,
            "assets/stage12/chaos_room_background.png",
            "assets/stage12/chaos_room_background_material.png"},
    }};
    for (const Expected& item : expected) {
        const MaterialAtlasDefinition* const atlas = find_atlas(item.id);
        ARPG_REQUIRE(atlas != nullptr);
        ARPG_REQUIRE(atlas->width == 2560);
        ARPG_REQUIRE(atlas->height == 1440);
        ARPG_REQUIRE(atlas->rgba_bytes == 14'745'600U);
        ARPG_REQUIRE(atlas->ecology == item.ecology);
        ARPG_REQUIRE(std::string_view{atlas->color_path} == item.color_path);
        ARPG_REQUIRE(std::string_view{atlas->material_path} == item.material_path);
    }
    return {};
}

arpg::test::Failure background_plan_maps_every_ecology_to_its_dedicated_atlas() noexcept {
    constexpr std::array<DungeonElement, 4> ecologies{{
        DungeonElement::fire, DungeonElement::water,
        DungeonElement::lightning, DungeonElement::chaos,
    }};
    constexpr std::array<MaterialAtlasId, 4> atlases{{
        MaterialAtlasId::fire_room_background,
        MaterialAtlasId::water_room_background,
        MaterialAtlasId::lightning_room_background,
        MaterialAtlasId::chaos_room_background,
    }};
    for (std::size_t index{}; index < ecologies.size(); ++index) {
        const auto plan = arpg::platform::room_background_render_plan(
            ecologies[index]);
        ARPG_REQUIRE(plan.atlas == atlases[index]);
        ARPG_REQUIRE(arpg::test::near(plan.source.x, 0.0F));
        ARPG_REQUIRE(arpg::test::near(plan.source.y, 0.0F));
        ARPG_REQUIRE(arpg::test::near(plan.source.width, 2560.0F));
        ARPG_REQUIRE(arpg::test::near(plan.source.height, 1440.0F));
    }
    return {};
}

arpg::test::Failure background_plan_uses_native_source_and_downscale_only() noexcept {
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::room_background_scale(2560.0F, 1440.0F), 1.0F));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::room_background_scale(1920.0F, 1080.0F), 0.75F));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::room_background_scale(1280.0F, 720.0F), 0.5F));
    ARPG_REQUIRE(arpg::platform::room_background_scale(3840.0F, 2160.0F)
        <= 1.0F);

    constexpr std::array<MaterialAtlasId, 5> legacy_environment_atlases{{
        MaterialAtlasId::environment,
        MaterialAtlasId::fire_environment,
        MaterialAtlasId::water_environment,
        MaterialAtlasId::lightning_environment,
        MaterialAtlasId::chaos_environment,
    }};
    for (const DungeonElement ecology : {DungeonElement::fire,
             DungeonElement::water, DungeonElement::lightning,
             DungeonElement::chaos}) {
        const auto plan = arpg::platform::room_background_render_plan(ecology);
        for (const MaterialAtlasId legacy : legacy_environment_atlases) {
            ARPG_REQUIRE(plan.atlas != legacy);
        }
    }
    return {};
}

bool same_tile(const arpg::platform::RoomBackgroundWorldTile& tile,
    std::uint8_t row, std::uint8_t column) noexcept {
    constexpr float kSourceTileWidth = 256.0F;
    constexpr float kSourceTileHeight = 144.0F;
    const float world_tile_width = arpg::combat::room_bounds::width / 10.0F;
    const float world_tile_depth = arpg::combat::room_bounds::depth / 10.0F;
    return tile.row == row && tile.column == column
        && arpg::test::near(tile.source.x,
            static_cast<float>(column) * kSourceTileWidth)
        && arpg::test::near(tile.source.y,
            static_cast<float>(row) * kSourceTileHeight)
        && arpg::test::near(tile.source.width, kSourceTileWidth)
        && arpg::test::near(tile.source.height, kSourceTileHeight)
        && arpg::test::near(tile.world_bounds.minimum.x,
            arpg::combat::room_bounds::min_x
                + static_cast<float>(column) * world_tile_width)
        && arpg::test::near(tile.world_bounds.maximum.x,
            arpg::combat::room_bounds::min_x
                + static_cast<float>(column + 1U) * world_tile_width)
        && arpg::test::near(tile.world_bounds.minimum.y,
            arpg::combat::room_bounds::min_y
                + static_cast<float>(row) * world_tile_depth)
        && arpg::test::near(tile.world_bounds.maximum.y,
            arpg::combat::room_bounds::min_y
                + static_cast<float>(row + 1U) * world_tile_depth);
}

arpg::test::Failure world_tile_plan_exactly_covers_camera_plus_one_tile_margin()
    noexcept {
    constexpr std::size_t kTileAxisCount = 10U;
    const float world_tile_width = arpg::combat::room_bounds::width
        / static_cast<float>(kTileAxisCount);
    const float world_tile_depth = arpg::combat::room_bounds::depth
        / static_cast<float>(kTileAxisCount);
    bool observed_left_edge = false;
    bool observed_center = false;
    bool observed_right_edge = false;

    for (std::size_t camera_row = 0U;
            camera_row < arpg::combat::room_spatial::rows; ++camera_row) {
        for (std::size_t camera_column = 0U;
                camera_column < arpg::combat::room_spatial::columns;
                ++camera_column) {
            const arpg::combat::Vec3 requested_center{
                arpg::combat::room_bounds::min_x
                    + (static_cast<float>(camera_column) + 0.5F)
                        * arpg::combat::room_spatial::cell_width,
                arpg::combat::room_bounds::min_y
                    + (static_cast<float>(camera_row) + 0.5F)
                        * arpg::combat::room_spatial::cell_depth,
                0.0F,
            };
            const arpg::platform::CombatCameraView camera =
                arpg::platform::make_combat_camera_view(
                    requested_center, 1280.0F, 720.0F);
            const auto plan = arpg::platform::room_background_world_tile_plan(
                DungeonElement::lightning, camera);
            ARPG_REQUIRE(plan.valid);
            ARPG_REQUIRE(plan.atlas == MaterialAtlasId::lightning_room_background);
            ARPG_REQUIRE(plan.count <= plan.tiles.size());
            ARPG_REQUIRE(plan.count <= 25U);

            const float query_min_x = camera.center.x
                - camera.visible_width * 0.5F - world_tile_width;
            const float query_max_x = camera.center.x
                + camera.visible_width * 0.5F + world_tile_width;
            const float query_min_y = camera.center.y
                - camera.visible_depth * 0.5F - world_tile_depth;
            const float query_max_y = camera.center.y
                + camera.visible_depth * 0.5F + world_tile_depth;
            std::size_t expected_count{};
            for (std::uint8_t row = 0U; row < kTileAxisCount; ++row) {
                const float tile_min_y = arpg::combat::room_bounds::min_y
                    + static_cast<float>(row) * world_tile_depth;
                const float tile_max_y = tile_min_y + world_tile_depth;
                if (tile_max_y < query_min_y || tile_min_y > query_max_y) {
                    continue;
                }
                for (std::uint8_t column = 0U;
                        column < kTileAxisCount; ++column) {
                    const float tile_min_x = arpg::combat::room_bounds::min_x
                        + static_cast<float>(column) * world_tile_width;
                    const float tile_max_x = tile_min_x + world_tile_width;
                    if (tile_max_x < query_min_x || tile_min_x > query_max_x) {
                        continue;
                    }
                    ARPG_REQUIRE(expected_count < plan.count);
                    ARPG_REQUIRE(same_tile(
                        plan.tiles[expected_count], row, column));
                    ++expected_count;
                }
            }
            ARPG_REQUIRE(plan.count == expected_count);
            observed_left_edge = observed_left_edge
                || plan.tiles[0U].column == 0U;
            observed_center = observed_center
                || (camera_column == 10U && camera_row == 10U);
            observed_right_edge = observed_right_edge
                || plan.tiles[plan.count - 1U].column == 9U;
        }
    }
    ARPG_REQUIRE(observed_left_edge);
    ARPG_REQUIRE(observed_center);
    ARPG_REQUIRE(observed_right_edge);
    return {};
}

arpg::test::Failure moving_camera_changes_world_tiles_without_changing_atlas()
    noexcept {
    const auto left_camera = arpg::platform::make_combat_camera_view(
        {-50.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
    const auto right_camera = arpg::platform::make_combat_camera_view(
        {50.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
    const auto left = arpg::platform::room_background_world_tile_plan(
        DungeonElement::water, left_camera);
    const auto right = arpg::platform::room_background_world_tile_plan(
        DungeonElement::water, right_camera);
    ARPG_REQUIRE(left.valid && right.valid);
    ARPG_REQUIRE(left.atlas == MaterialAtlasId::water_room_background);
    ARPG_REQUIRE(right.atlas == left.atlas);
    ARPG_REQUIRE(left.count != 0U && right.count != 0U);
    ARPG_REQUIRE(left.tiles[0U].column != right.tiles[0U].column);
    return {};
}

arpg::test::Failure world_tile_projection_uses_the_shared_camera_geometry()
    noexcept {
    const arpg::platform::CombatCameraView camera =
        arpg::platform::make_combat_camera_view(
            {-32.0F, -12.0F, 0.0F}, 1920.0F, 1080.0F);
    const auto plan = arpg::platform::room_background_world_tile_plan(
        DungeonElement::chaos, camera);
    ARPG_REQUIRE(plan.valid && plan.count != 0U);
    const auto projected = arpg::platform::project_room_background_world_tile(
        plan.tiles[0U], camera, 1920.0F, 1080.0F);
    ARPG_REQUIRE(projected.valid);
    ARPG_REQUIRE(projected.destination.width > 0.0F);
    ARPG_REQUIRE(projected.destination.height > 0.0F);

    const auto& bounds = plan.tiles[0U].world_bounds;
    const auto back_left = arpg::platform::project_render_world(
        bounds.minimum.x, bounds.minimum.y, 0.0F,
        camera, 1920.0F, 1080.0F);
    const auto back_right = arpg::platform::project_render_world(
        bounds.maximum.x, bounds.minimum.y, 0.0F,
        camera, 1920.0F, 1080.0F);
    const auto front_left = arpg::platform::project_render_world(
        bounds.minimum.x, bounds.maximum.y, 0.0F,
        camera, 1920.0F, 1080.0F);
    const auto front_right = arpg::platform::project_render_world(
        bounds.maximum.x, bounds.maximum.y, 0.0F,
        camera, 1920.0F, 1080.0F);
    const float expected_left = (std::min)({back_left.x, back_right.x,
        front_left.x, front_right.x});
    const float expected_right = (std::max)({back_left.x, back_right.x,
        front_left.x, front_right.x});
    ARPG_REQUIRE(arpg::test::near(projected.destination.x, expected_left));
    ARPG_REQUIRE(arpg::test::near(projected.destination.width,
        expected_right - expected_left));
    ARPG_REQUIRE(arpg::test::near(projected.destination.y,
        (std::min)(back_left.ground_y, front_left.ground_y)));
    ARPG_REQUIRE(arpg::test::near(projected.destination.height,
        std::fabs(front_left.ground_y - back_left.ground_y)));

    arpg::platform::CombatCameraView moved = camera;
    moved.center.x += 16.0F;
    const auto moved_projection =
        arpg::platform::project_room_background_world_tile(
            plan.tiles[0U], moved, 1920.0F, 1080.0F);
    ARPG_REQUIRE(moved_projection.valid);
    ARPG_REQUIRE(moved_projection.destination.x != projected.destination.x);
    ARPG_REQUIRE(same_tile(plan.tiles[0U],
        plan.tiles[0U].row, plan.tiles[0U].column));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"preserves old IDs and adds four background pairs",
        &background_manifest_preserves_old_ids_and_adds_four_pairs},
    {"maps every ecology to its dedicated background atlas",
        &background_plan_maps_every_ecology_to_its_dedicated_atlas},
    {"uses native source and downscale only",
        &background_plan_uses_native_source_and_downscale_only},
    {"world tiles exactly cover camera plus one tile margin",
        &world_tile_plan_exactly_covers_camera_plus_one_tile_margin},
    {"camera movement changes world tiles",
        &moving_camera_changes_world_tiles_without_changing_atlas},
    {"world tile projection uses shared camera geometry",
        &world_tile_projection_uses_the_shared_camera_geometry},
};

}  // namespace

arpg::test::TestSuite room_background_render_plan_suite() noexcept {
    return arpg::test::make_suite("room_background_render_plan", kCases);
}
