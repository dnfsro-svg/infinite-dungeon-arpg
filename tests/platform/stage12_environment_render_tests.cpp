#include "test_framework.hpp"

#include "dungeon/dungeon_types.hpp"
#include "combat/room_bounds.hpp"
#include "combat_view_math.hpp"
#include "environment_render_plan.hpp"
#include "ground_loot_view.hpp"
#include "material_animation.hpp"
#include "material_loot_view.hpp"
#include "render_layout.hpp"

#include <algorithm>
#include <cmath>

namespace {

arpg::test::Failure environment_maps_each_dungeon_element_to_a_floor_sprite() noexcept {
    using arpg::dungeon::DungeonElement;
    using arpg::platform::MaterialSpriteId;

    ARPG_REQUIRE(arpg::platform::select_floor_sprite(DungeonElement::fire)
        != MaterialSpriteId::missing);
    ARPG_REQUIRE(arpg::platform::select_floor_sprite(DungeonElement::water)
        != MaterialSpriteId::missing);
    ARPG_REQUIRE(arpg::platform::select_floor_sprite(DungeonElement::lightning)
        != MaterialSpriteId::missing);
    ARPG_REQUIRE(arpg::platform::select_floor_sprite(DungeonElement::chaos)
        != MaterialSpriteId::missing);
    return {};
}

arpg::test::Failure environment_falls_back_atomically_when_a_required_frame_is_missing() noexcept {
    using arpg::platform::EnvironmentFrameAvailability;

    ARPG_REQUIRE(!arpg::platform::should_draw_material_environment(
        EnvironmentFrameAvailability{true, false, true, true, true}));
    ARPG_REQUIRE(!arpg::platform::should_draw_material_environment(
        EnvironmentFrameAvailability{true, true, false, true, true}));
    ARPG_REQUIRE(arpg::platform::should_draw_material_environment(
        EnvironmentFrameAvailability{true, true, true, true, true}));

    namespace combat = arpg::combat;
    namespace platform = arpg::platform;
    constexpr float kWidth = 1280.0F;
    constexpr float kHeight = 720.0F;
    const platform::CombatCameraView center =
        platform::make_combat_camera_view({}, kWidth, kHeight);
    const platform::RoomGeometryPlan geometry =
        platform::make_room_geometry_plan(center, kWidth, kHeight);
    ARPG_REQUIRE(geometry.grid_line_count == 37U);
    for (std::size_t index = 0U; index < 25U; ++index) {
        const float expected_x = combat::room_bounds::min_x
            + 2.0F * static_cast<float>(index);
        ARPG_REQUIRE(arpg::test::near(
            geometry.grid_lines[index].world_start.x, expected_x));
        ARPG_REQUIRE(arpg::test::near(
            geometry.grid_lines[index].world_end.x, expected_x));
    }
    for (std::size_t index = 25U; index < geometry.grid_line_count; ++index) {
        const float expected_y = combat::room_bounds::min_y
            + 2.0F * static_cast<float>(index - 25U);
        ARPG_REQUIRE(arpg::test::near(
            geometry.grid_lines[index].world_start.y, expected_y));
        ARPG_REQUIRE(arpg::test::near(
            geometry.grid_lines[index].world_end.y, expected_y));
    }
    std::size_t visible_grid_lines{};
    for (std::size_t index = 0U;
         index < geometry.grid_line_count; ++index) {
        const auto& line = geometry.grid_lines[index];
        ARPG_REQUIRE(line.visible
            == platform::projected_line_intersects_viewport(
                line.screen_start, line.screen_end, kWidth, kHeight));
        if (line.visible) ++visible_grid_lines;
    }
    ARPG_REQUIRE(visible_grid_lines > 0U);
    ARPG_REQUIRE(visible_grid_lines < geometry.grid_line_count);

    const auto same_projection = [](platform::ScreenProjection lhs,
                                     platform::ScreenProjection rhs) noexcept {
        return arpg::test::near(lhs.x, rhs.x)
            && arpg::test::near(lhs.y, rhs.y)
            && arpg::test::near(lhs.ground_y, rhs.ground_y)
            && arpg::test::near(lhs.scale, rhs.scale);
    };
    const combat::Vec3 shared_position{3.0F, 2.0F, 0.0F};
    const platform::ScreenProjection direct =
        platform::project_combat_position(shared_position, center,
            kWidth, kHeight);
    const platform::RenderProjection room = platform::project_render_world(
        shared_position.x, shared_position.y, shared_position.z,
        center, kWidth, kHeight);
    ARPG_REQUIRE(arpg::test::near(room.x, direct.x));
    ARPG_REQUIRE(arpg::test::near(room.y, direct.y));
    ARPG_REQUIRE(arpg::test::near(room.ground_y, direct.ground_y));

    arpg::combat::HazardSnapshot hazard{};
    hazard.center = shared_position;
    ARPG_REQUIRE(same_projection(platform::project_hazard_center(
        hazard, center, kWidth, kHeight), direct));

    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_item_count = 1U;
    snapshot.ground_items[0].position = shared_position;
    snapshot.ground_items[0].ordinal = 7U;
    const platform::GroundLootView loot = platform::build_ground_loot_view(
        snapshot, arpg::settings::LootFilterMode::show_all,
        center, kWidth, kHeight);
    ARPG_REQUIRE(loot.count == 1U);
    ARPG_REQUIRE(arpg::test::near(loot.labels[0].anchor_x, direct.x));
    ARPG_REQUIRE(arpg::test::near(loot.labels[0].anchor_y, direct.y));

    snapshot.ground_material_count = 1U;
    snapshot.ground_materials[0].position = shared_position;
    snapshot.ground_materials[0].ordinal = 9U;
    snapshot.ground_materials[0].material = arpg::items::MaterialId::transmute;
    const platform::MaterialLootView materials =
        platform::build_material_loot_view(snapshot, center, kWidth, kHeight);
    ARPG_REQUIRE(materials.count == 1U);
    ARPG_REQUIRE(arpg::test::near(materials.labels[0].anchor_x, direct.x));
    ARPG_REQUIRE(arpg::test::near(materials.labels[0].anchor_y, direct.y));

    for (std::size_t index = 0U; index < geometry.doors.size(); ++index) {
        const platform::ScreenProjection expected =
            platform::project_combat_position(platform::kRoomDoorCenters[index],
                center, kWidth, kHeight);
        ARPG_REQUIRE(same_projection(geometry.doors[index], expected));
    }
    ARPG_REQUIRE(same_projection(geometry.hole,
        platform::project_combat_position(platform::kHoleCenter,
            center, kWidth, kHeight)));

    const auto inside = [](platform::ScreenProjection point) noexcept {
        return point.x >= 0.0F && point.x <= kWidth
            && point.ground_y >= 0.0F && point.ground_y <= kHeight;
    };
    const combat::Vec3 edge_players[] = {
        {0.0F, combat::room_bounds::min_y, 0.0F},
        {0.0F, combat::room_bounds::max_y, 0.0F},
        {combat::room_bounds::min_x, 0.0F, 0.0F},
        {combat::room_bounds::max_x, 0.0F, 0.0F},
    };
    constexpr std::size_t kOppositeDoors[] = {1U, 0U, 3U, 2U};
    for (std::size_t index = 0U; index < 4U; ++index) {
        const platform::RoomGeometryPlan edge = platform::make_room_geometry_plan(
            platform::make_combat_camera_view(edge_players[index],
                kWidth, kHeight), kWidth, kHeight);
        ARPG_REQUIRE(inside(edge.doors[index]));
        ARPG_REQUIRE(!inside(edge.doors[kOppositeDoors[index]]));
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"maps every dungeon element to a floor sprite",
        &environment_maps_each_dungeon_element_to_a_floor_sprite},
    {"falls back atomically when a required frame is missing",
        &environment_falls_back_atomically_when_a_required_frame_is_missing},
};

}  // namespace

arpg::test::TestSuite stage12_environment_render_suite() noexcept {
    return arpg::test::make_suite("stage12_environment_render", kCases);
}
