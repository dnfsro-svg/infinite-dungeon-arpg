#include "test_framework.hpp"

#include "combat/room_bounds.hpp"
#include "combat_view_math.hpp"
#include "render_layout.hpp"

#include <array>

namespace {

using arpg::combat::Vec3;
using arpg::platform::ActorDrawItem;
using arpg::platform::CombatCameraView;
using arpg::platform::ScreenProjection;

arpg::test::Failure camera_view_is_24_by_11_with_a_32_unit_wide_cap() noexcept {
    const CombatCameraView standard =
        arpg::platform::make_combat_camera_view(
            {12.0F, -9.0F, 0.0F}, 1920.0F, 1080.0F);
    ARPG_REQUIRE(arpg::test::near(standard.visible_width, 24.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(standard.visible_depth, 11.0, 1.0e-4));

    const CombatCameraView narrower =
        arpg::platform::make_combat_camera_view(
            {12.0F, -9.0F, 0.0F}, 1280.0F, 1024.0F);
    ARPG_REQUIRE(arpg::test::near(narrower.visible_width, 24.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(narrower.visible_depth, 11.0, 1.0e-4));

    const CombatCameraView wider =
        arpg::platform::make_combat_camera_view(
            {12.0F, -9.0F, 0.0F}, 2520.0F, 1080.0F);
    ARPG_REQUIRE(arpg::test::near(wider.visible_width, 31.5, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(wider.visible_depth, 11.0, 1.0e-4));

    const CombatCameraView ultrawide =
        arpg::platform::make_combat_camera_view(
            {12.0F, -9.0F, 0.0F}, 7680.0F, 1080.0F);
    ARPG_REQUIRE(arpg::test::near(ultrawide.visible_width, 32.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(ultrawide.visible_depth, 11.0, 1.0e-4));
    return {};
}

arpg::test::Failure camera_tracks_immediately_and_clamps_to_room_edges() noexcept {
    constexpr float kWidth = 1280.0F;
    constexpr float kHeight = 720.0F;

    const CombatCameraView first =
        arpg::platform::make_combat_camera_view(
            {18.0F, -13.0F, 0.0F}, kWidth, kHeight);
    const CombatCameraView second =
        arpg::platform::make_combat_camera_view(
            {-27.0F, 19.0F, 0.0F}, kWidth, kHeight);
    ARPG_REQUIRE(arpg::test::near(first.center.x, 18.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(first.center.y, -13.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(second.center.x, -27.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(second.center.y, 19.0, 1.0e-4));

    const CombatCameraView maximum =
        arpg::platform::make_combat_camera_view(
            {arpg::combat::room_bounds::max_x,
                arpg::combat::room_bounds::max_y, 0.0F},
            kWidth, kHeight);
    ARPG_REQUIRE(arpg::test::near(
        maximum.center.x,
        arpg::combat::room_bounds::max_x - 12.0F, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        maximum.center.y,
        arpg::combat::room_bounds::max_y - 5.5F, 1.0e-4));

    const CombatCameraView minimum =
        arpg::platform::make_combat_camera_view(
            {arpg::combat::room_bounds::min_x,
                arpg::combat::room_bounds::min_y, 0.0F},
            kWidth, kHeight);
    ARPG_REQUIRE(arpg::test::near(
        minimum.center.x,
        arpg::combat::room_bounds::min_x + 12.0F, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        minimum.center.y,
        arpg::combat::room_bounds::min_y + 5.5F, 1.0e-4));
    return {};
}

arpg::test::Failure world_view_query_exactly_matches_camera_bounds() noexcept {
    const CombatCameraView camera =
        arpg::platform::make_combat_camera_view(
            {19.0F, -23.0F, 0.0F}, 1920.0F, 1080.0F);
    const arpg::dungeon::WorldViewQuery query =
        arpg::platform::make_world_view_query(
            camera, 1920.0F, 1080.0F, 73U);
    ARPG_REQUIRE(arpg::test::near(query.world_bounds.minimum.x,
        camera.center.x - camera.visible_width * 0.5F, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(query.world_bounds.maximum.x,
        camera.center.x + camera.visible_width * 0.5F, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(query.world_bounds.minimum.y,
        camera.center.y - camera.visible_depth * 0.5F, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(query.world_bounds.maximum.y,
        camera.center.y + camera.visible_depth * 0.5F, 1.0e-4));
    ARPG_REQUIRE(query.world_bounds.minimum.z == -1.0F);
    ARPG_REQUIRE(query.world_bounds.maximum.z == 32.0F);
    ARPG_REQUIRE(query.screen_width == 1920);
    ARPG_REQUIRE(query.screen_height == 1080);
    ARPG_REQUIRE(query.camera_version == 73U);
    return {};
}

arpg::test::Failure combat_position_interpolation_clamps_frame_alpha() noexcept {
    constexpr Vec3 kFrom{-4.0F, 8.0F, 1.0F};
    constexpr Vec3 kTo{12.0F, -4.0F, 5.0F};
    const Vec3 before = arpg::platform::interpolate_combat_position(
        kFrom, kTo, -2.0F);
    const Vec3 after = arpg::platform::interpolate_combat_position(
        kFrom, kTo, 3.0F);
    const Vec3 quarter = arpg::platform::interpolate_combat_position(
        kFrom, kTo, 0.25F);
    ARPG_REQUIRE(before.x == kFrom.x && before.y == kFrom.y
        && before.z == kFrom.z);
    ARPG_REQUIRE(after.x == kTo.x && after.y == kTo.y
        && after.z == kTo.z);
    ARPG_REQUIRE(quarter.x == 0.0F);
    ARPG_REQUIRE(quarter.y == 5.0F);
    ARPG_REQUIRE(quarter.z == 2.0F);
    return {};
}

arpg::test::Failure camera_projection_keeps_actor_pixel_height_constant() noexcept {
    constexpr Vec3 kPlayer{34.0F, 41.0F, 0.0F};
    const CombatCameraView low_resolution =
        arpg::platform::make_combat_camera_view(
            kPlayer, 1280.0F, 720.0F);
    const CombatCameraView high_resolution =
        arpg::platform::make_combat_camera_view(
            kPlayer, 3840.0F, 2160.0F);

    const ScreenProjection low_ground =
        arpg::platform::project_combat_position(
            kPlayer, low_resolution, 1280.0F, 720.0F);
    const ScreenProjection low_raised =
        arpg::platform::project_combat_position(
            {kPlayer.x, kPlayer.y, 1.0F},
            low_resolution, 1280.0F, 720.0F);
    const ScreenProjection high_ground =
        arpg::platform::project_combat_position(
            kPlayer, high_resolution, 3840.0F, 2160.0F);
    const ScreenProjection high_raised =
        arpg::platform::project_combat_position(
            {kPlayer.x, kPlayer.y, 1.0F},
            high_resolution, 3840.0F, 2160.0F);

    ARPG_REQUIRE(arpg::test::near(low_ground.x, 640.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(high_ground.x, 1920.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(low_ground.scale, 0.85, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(high_ground.scale, 0.85, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        low_ground.y - low_raised.y, 59.5, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        high_ground.y - high_raised.y, 59.5, 1.0e-4));
    return {};
}

arpg::test::Failure all_projection_wrappers_share_the_camera_contract() noexcept {
    const CombatCameraView camera =
        arpg::platform::make_combat_camera_view(
            {23.0F, -17.0F, 0.0F}, 1920.0F, 1080.0F);
    constexpr Vec3 kWorldPoint{25.0F, -15.0F, 1.25F};
    const ScreenProjection direct =
        arpg::platform::project_combat_position(
            kWorldPoint, camera, 1920.0F, 1080.0F);

    arpg::combat::ProjectileSnapshot projectile{};
    projectile.position = kWorldPoint;
    const ScreenProjection projectile_projection =
        arpg::platform::project_projectile_position(
            projectile, camera, 1920.0F, 1080.0F);

    arpg::combat::HazardSnapshot hazard{};
    hazard.center = kWorldPoint;
    const ScreenProjection hazard_projection =
        arpg::platform::project_hazard_center(
            hazard, camera, 1920.0F, 1080.0F);
    const arpg::platform::RenderProjection render_projection =
        arpg::platform::project_render_world(
            kWorldPoint.x, kWorldPoint.y, kWorldPoint.z,
            camera, 1920.0F, 1080.0F);

    ARPG_REQUIRE(projectile_projection.x == direct.x);
    ARPG_REQUIRE(projectile_projection.y == direct.y);
    ARPG_REQUIRE(projectile_projection.ground_y == direct.ground_y);
    ARPG_REQUIRE(projectile_projection.scale == direct.scale);
    ARPG_REQUIRE(hazard_projection.x == direct.x);
    ARPG_REQUIRE(hazard_projection.y == direct.y);
    ARPG_REQUIRE(hazard_projection.ground_y == direct.ground_y);
    ARPG_REQUIRE(hazard_projection.scale == direct.scale);
    ARPG_REQUIRE(render_projection.x == direct.x);
    ARPG_REQUIRE(render_projection.y == direct.y);
    ARPG_REQUIRE(render_projection.ground_y == direct.ground_y);
    ARPG_REQUIRE(render_projection.scale == direct.scale);
    return {};
}

arpg::test::Failure back_and_front_projection_are_exact() noexcept {
    const ScreenProjection back = arpg::platform::project_combat_position(
        Vec3{0.0F, arpg::combat::room_bounds::min_y, 0.0F},
        1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(back.x, 640.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.ground_y, 273.6, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.y, back.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.scale, 0.70, 1.0e-4));

    const ScreenProjection front = arpg::platform::project_combat_position(
        Vec3{0.0F, arpg::combat::room_bounds::max_y, 0.0F},
        1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(front.x, 640.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.ground_y, 633.6, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.y, front.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.scale, 1.0, 1.0e-4));
    return {};
}

arpg::test::Failure expanded_room_corners_remain_in_viewport() noexcept {
    constexpr std::array<Vec3, 4> corners{{
        {arpg::combat::room_bounds::min_x,
            arpg::combat::room_bounds::min_y, 0.0F},
        {arpg::combat::room_bounds::max_x,
            arpg::combat::room_bounds::min_y, 0.0F},
        {arpg::combat::room_bounds::min_x,
            arpg::combat::room_bounds::max_y, 0.0F},
        {arpg::combat::room_bounds::max_x,
            arpg::combat::room_bounds::max_y, 0.0F},
    }};
    for (const Vec3 corner : corners) {
        const ScreenProjection projected = arpg::platform::project_combat_position(
            corner, 1280.0F, 720.0F);
        ARPG_REQUIRE(projected.x >= 48.0F);
        ARPG_REQUIRE(projected.x <= 1232.0F);
        ARPG_REQUIRE(projected.ground_y >= 240.0F);
        ARPG_REQUIRE(projected.ground_y <= 660.0F);
    }
    return {};
}

arpg::test::Failure z_only_offsets_actor_screen_y() noexcept {
    const ScreenProjection ground = arpg::platform::project_combat_position(
        Vec3{2.0F, 0.0F, 0.0F}, 900.0F, 600.0F);
    const ScreenProjection raised = arpg::platform::project_combat_position(
        Vec3{2.0F, 0.0F, 1.5F}, 900.0F, 600.0F);
    ARPG_REQUIRE(arpg::test::near(ground.x, raised.x, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        ground.ground_y, raised.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(ground.scale, raised.scale, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        raised.y, ground.y - 1.5 * 70.0 * ground.scale, 1.0e-4));
    return {};
}

arpg::test::Failure actor_order_is_y_z_x_then_index() noexcept {
    std::array<ActorDrawItem, 4> items{{
        {Vec3{2.0F, 1.0F, 0.0F}, 3},
        {Vec3{1.0F, 0.0F, 1.0F}, 2},
        {Vec3{1.0F, 0.0F, 0.0F}, 1},
        {Vec3{1.0F, 0.0F, 0.0F}, 0},
    }};
    arpg::platform::sort_actor_draw_items(items);
    ARPG_REQUIRE(items[0].index == 0);
    ARPG_REQUIRE(items[1].index == 1);
    ARPG_REQUIRE(items[2].index == 2);
    ARPG_REQUIRE(items[3].index == 3);

    items = {{
        {Vec3{2.0F, 0.0F, 0.0F}, 2},
        {Vec3{1.0F, 0.0F, 0.0F}, 3},
        {Vec3{1.0F, 0.0F, 0.0F}, 1},
        {Vec3{1.0F, 0.0F, 1.0F}, 0},
    }};
    arpg::platform::sort_actor_draw_items(items);
    ARPG_REQUIRE(items[0].index == 1);
    ARPG_REQUIRE(items[1].index == 3);
    ARPG_REQUIRE(items[2].index == 2);
    ARPG_REQUIRE(items[3].index == 0);
    return {};
}

arpg::test::Failure render_layout_keeps_baseline_projection_and_hud_values() noexcept {
    using arpg::platform::RenderProjection;
    const RenderProjection north = arpg::platform::project_render_world(
        0.0F, -5.5F, 0.0F, 1280.0F, 720.0F);
    const ScreenProjection expected_north = arpg::platform::project_combat_position(
        Vec3{0.0F, -5.5F, 0.0F}, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(north.x, expected_north.x, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(north.y, expected_north.y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        north.ground_y, expected_north.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(north.scale, expected_north.scale, 1.0e-4));

    const RenderProjection east = arpg::platform::project_render_world(
        12.0F, 0.0F, 0.0F, 1280.0F, 720.0F);
    const ScreenProjection expected_east = arpg::platform::project_combat_position(
        Vec3{12.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(east.x, expected_east.x, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(east.y, expected_east.y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        east.ground_y, expected_east.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(east.scale, expected_east.scale, 1.0e-4));

    const auto normal = arpg::platform::render_layout(false);
    ARPG_REQUIRE(arpg::test::near(normal.hud_x, 30.0, 1.0e-4));
    ARPG_REQUIRE(normal.hud_first_line_y == 28);
    ARPG_REQUIRE(normal.hud_second_instruction_y == 53);
    ARPG_REQUIRE(normal.hud_status_y == 81);
    ARPG_REQUIRE(normal.hud_line_step == 23);
    ARPG_REQUIRE(normal.hud_debug_start_step == 25);
    ARPG_REQUIRE(arpg::test::near(normal.hud_panel_height, 400.0, 1.0e-4));

    const auto debug = arpg::platform::render_layout(true);
    ARPG_REQUIRE(arpg::test::near(debug.hud_panel_height, 660.0, 1.0e-4));
    ARPG_REQUIRE(arpg::platform::debug_overlay_start_y(265) == 290);
    return {};
}

arpg::test::Failure generic_hazard_pass_excludes_abyss_environment() noexcept {
    arpg::combat::HazardSnapshot hazard{};
    hazard.active = true;
    hazard.source = arpg::combat::HazardSource::monster;
    ARPG_REQUIRE(arpg::platform::uses_generic_hazard_pass(hazard));

    hazard.source = arpg::combat::HazardSource::abyss_environment;
    ARPG_REQUIRE(!arpg::platform::uses_generic_hazard_pass(hazard));

    hazard.active = false;
    hazard.source = arpg::combat::HazardSource::monster;
    ARPG_REQUIRE(!arpg::platform::uses_generic_hazard_pass(hazard));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"fixed camera dimensions", &camera_view_is_24_by_11_with_a_32_unit_wide_cap},
    {"immediate clamped camera", &camera_tracks_immediately_and_clamps_to_room_edges},
    {"camera creates exact world query",
        &world_view_query_exactly_matches_camera_bounds},
    {"frame interpolation clamps alpha",
        &combat_position_interpolation_clamps_frame_alpha},
    {"fixed actor pixel height", &camera_projection_keeps_actor_pixel_height_constant},
    {"shared camera projection", &all_projection_wrappers_share_the_camera_contract},
    {"back and front projection", &back_and_front_projection_are_exact},
    {"expanded room corners", &expanded_room_corners_remain_in_viewport},
    {"Z-only actor offset", &z_only_offsets_actor_screen_y},
    {"stable actor draw order", &actor_order_is_y_z_x_then_index},
    {"render layout baseline", &render_layout_keeps_baseline_projection_and_hud_values},
    {"generic hazard pass routing", &generic_hazard_pass_excludes_abyss_environment},
};

}  // namespace

arpg::test::TestSuite combat_view_math_suite() noexcept {
    return arpg::test::make_suite("combat_view_math", kCases);
}
