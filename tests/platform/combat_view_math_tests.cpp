#include "test_framework.hpp"

#include "combat_view_math.hpp"
#include "render_layout.hpp"

#include <algorithm>
#include <array>

namespace {

using arpg::combat::Vec3;
using arpg::platform::ActorDrawItem;
using arpg::platform::CombatCameraView;
using arpg::platform::ScreenProjection;

Vec3 independently_unproject_ground_position(
    ScreenProjection projection,
    CombatCameraView view,
    float width,
    float height) noexcept {
    // This deliberately does not call any production projection helper.  It is
    // the analytic inverse of the documented ground band, so a matching
    // forward-only implementation cannot make this test vacuous.
    constexpr float kGroundBandStart = 0.38F;
    constexpr float kGroundBandDepth = 0.50F;
    constexpr float kMinimumScale = 0.70F;
    constexpr float kScaleDepth = 0.30F;
    constexpr float kHorizontalFill = 0.92F;
    const float depth = (projection.ground_y / height - kGroundBandStart)
        / kGroundBandDepth;
    const float visual_depth = (std::max)(0.0F, (std::min)(depth, 1.0F));
    const float scale = kMinimumScale + kScaleDepth * visual_depth;
    return {
        view.center.x + (projection.x - width * 0.5F) * view.visible_width
            / (width * kHorizontalFill * scale),
        view.center.y - view.visible_depth * 0.5F + depth * view.visible_depth,
        0.0F,
    };
}

arpg::test::Failure camera_tracks_immediately_and_clamps_to_room() noexcept {
    const CombatCameraView center = arpg::platform::make_combat_camera_view(
        {0.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(center.visible_width, 24.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(center.visible_depth, 11.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(center.center.x, 0.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(center.center.y, 0.0, 1.0e-4));

    const CombatCameraView right = arpg::platform::make_combat_camera_view(
        {24.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
    const CombatCameraView left = arpg::platform::make_combat_camera_view(
        {-24.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
    const CombatCameraView front = arpg::platform::make_combat_camera_view(
        {0.0F, 11.0F, 0.0F}, 1280.0F, 720.0F);
    const CombatCameraView back = arpg::platform::make_combat_camera_view(
        {0.0F, -11.0F, 0.0F}, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(right.center.x, 12.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(left.center.x, -12.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.center.y, 5.5, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.center.y, -5.5, 1.0e-4));

    const Vec3 first_player{3.0F, 2.0F, 0.0F};
    const CombatCameraView first = arpg::platform::make_combat_camera_view(
        first_player, 1280.0F, 720.0F);
    const ScreenProjection first_projection =
        arpg::platform::project_combat_position(
            first_player, first, 1280.0F, 720.0F);
    const Vec3 next_player{-2.0F, -1.0F, 0.0F};
    const CombatCameraView next = arpg::platform::make_combat_camera_view(
        next_player, 1280.0F, 720.0F);
    const ScreenProjection next_projection =
        arpg::platform::project_combat_position(
            next_player, next, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(first.center.x, 3.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(next.center.x, -2.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(first_projection.x, 640.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(next_projection.x, 640.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(first_projection.ground_y, 453.6, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(next_projection.ground_y, 453.6, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(first_projection.y, first_projection.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(next_projection.y, next_projection.ground_y, 1.0e-4));
    return {};
}

arpg::test::Failure four_camera_clamp_edges_project_the_visible_boundary() noexcept {
    constexpr float kWidth = 1280.0F;
    constexpr float kHeight = 720.0F;
    struct ClampCase final {
        Vec3 player{};
        Vec3 visible_boundary{};
        float expected_x{};
        float expected_ground_y{};
    };
    constexpr std::array<ClampCase, 4> kCases{{
        {{24.0F, 0.0F, 0.0F}, {24.0F, 0.0F, 0.0F}, 1140.48F, 453.6F},
        {{-24.0F, 0.0F, 0.0F}, {-24.0F, 0.0F, 0.0F}, 139.52F, 453.6F},
        {{0.0F, 11.0F, 0.0F}, {0.0F, 11.0F, 0.0F}, 640.0F, 633.6F},
        {{0.0F, -11.0F, 0.0F}, {0.0F, -11.0F, 0.0F}, 640.0F, 273.6F},
    }};
    for (const ClampCase& item : kCases) {
        const CombatCameraView view = arpg::platform::make_combat_camera_view(
            item.player, kWidth, kHeight);
        const ScreenProjection projection = arpg::platform::project_combat_position(
            item.visible_boundary, view, kWidth, kHeight);
        ARPG_REQUIRE(arpg::test::near(projection.x, item.expected_x, 1.0e-4));
        ARPG_REQUIRE(arpg::test::near(
            projection.ground_y, item.expected_ground_y, 1.0e-4));
        ARPG_REQUIRE(projection.x >= 0.0F);
        ARPG_REQUIRE(projection.x <= kWidth);
        ARPG_REQUIRE(projection.ground_y >= 0.0F);
        ARPG_REQUIRE(projection.ground_y <= kHeight);
    }
    return {};
}

arpg::test::Failure camera_projection_round_trips_through_independent_math() noexcept {
    constexpr float kWidth = 1280.0F;
    constexpr float kHeight = 720.0F;
    const CombatCameraView view = arpg::platform::make_combat_camera_view(
        {5.0F, 3.0F, 0.0F}, kWidth, kHeight);
    constexpr std::array<Vec3, 4> kWorldPoints{{
        {-3.0F, -1.0F, 0.0F}, {5.0F, 3.0F, 0.0F},
        {12.0F, 5.5F, 0.0F}, {16.0F, 8.5F, 0.0F},
    }};
    for (const Vec3 world : kWorldPoints) {
        const ScreenProjection projected = arpg::platform::project_combat_position(
            world, view, kWidth, kHeight);
        const Vec3 recovered = independently_unproject_ground_position(
            projected, view, kWidth, kHeight);
        ARPG_REQUIRE(arpg::test::near(recovered.x, world.x, 1.0e-4));
        ARPG_REQUIRE(arpg::test::near(recovered.y, world.y, 1.0e-4));
        ARPG_REQUIRE(arpg::test::near(projected.y, projected.ground_y, 1.0e-4));
    }
    return {};
}

arpg::test::Failure ultrawide_expands_width_without_stretching() noexcept {
    const CombatCameraView standard = arpg::platform::make_combat_camera_view(
        {0.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
    const CombatCameraView ultrawide = arpg::platform::make_combat_camera_view(
        {24.0F, 0.0F, 0.0F}, 2560.0F, 1080.0F);
    ARPG_REQUIRE(arpg::test::near(ultrawide.visible_depth, 11.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(ultrawide.visible_width, 32.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(ultrawide.center.x, 8.0, 1.0e-4));

    const ScreenProjection standard_player =
        arpg::platform::project_combat_position(
            {0.0F, 0.0F, 0.0F}, standard, 1280.0F, 720.0F);
    const ScreenProjection ultrawide_player =
        arpg::platform::project_combat_position(
            {8.0F, 0.0F, 0.0F}, ultrawide, 2560.0F, 1080.0F);
    ARPG_REQUIRE(arpg::test::near(
        standard_player.scale, ultrawide_player.scale, 1.0e-4));

    const ScreenProjection standard_step =
        arpg::platform::project_combat_position(
            {1.0F, 0.0F, 0.0F}, standard, 1280.0F, 720.0F);
    const ScreenProjection ultrawide_step =
        arpg::platform::project_combat_position(
            {9.0F, 0.0F, 0.0F}, ultrawide, 2560.0F, 1080.0F);
    const double standard_pixels_per_height =
        (standard_step.x - standard_player.x) / 720.0;
    const double ultrawide_pixels_per_height =
        (ultrawide_step.x - ultrawide_player.x) / 1080.0;
    ARPG_REQUIRE(arpg::test::near(standard_pixels_per_height,
        ultrawide_pixels_per_height, 1.0e-4));
    return {};
}

arpg::test::Failure back_and_front_projection_are_exact() noexcept {
    if (const arpg::test::Failure failure =
            camera_tracks_immediately_and_clamps_to_room();
            failure.expression != nullptr) {
        return failure;
    }
    const ScreenProjection back = arpg::platform::project_combat_position(
        Vec3{0.0F, -5.5F, 0.0F}, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(back.x, 640.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.ground_y, 273.6, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.y, back.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.scale, 0.70, 1.0e-4));

    const ScreenProjection front = arpg::platform::project_combat_position(
        Vec3{0.0F, 5.5F, 0.0F}, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(front.x, 640.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.ground_y, 633.6, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.y, front.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.scale, 1.0, 1.0e-4));
    return {};
}

arpg::test::Failure expanded_room_corners_remain_in_viewport() noexcept {
    if (const arpg::test::Failure failure =
            ultrawide_expands_width_without_stretching();
            failure.expression != nullptr) {
        return failure;
    }
    constexpr std::array<Vec3, 4> corners{{
        {-12.0F, -5.5F, 0.0F}, {12.0F, -5.5F, 0.0F},
        {-12.0F, 5.5F, 0.0F}, {12.0F, 5.5F, 0.0F},
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
    const CombatCameraView view = arpg::platform::make_combat_camera_view(
        {4.0F, 1.0F, 0.0F}, 1280.0F, 720.0F);
    const RenderProjection north = arpg::platform::project_render_world(
        0.0F, -5.5F, 0.0F, view, 1280.0F, 720.0F);
    const ScreenProjection expected_north = arpg::platform::project_combat_position(
        Vec3{0.0F, -5.5F, 0.0F}, view, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(north.x, expected_north.x, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(north.y, expected_north.y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        north.ground_y, expected_north.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(north.scale, expected_north.scale, 1.0e-4));

    const RenderProjection east = arpg::platform::project_render_world(
        12.0F, 0.0F, 0.0F, view, 1280.0F, 720.0F);
    const ScreenProjection expected_east = arpg::platform::project_combat_position(
        Vec3{12.0F, 0.0F, 0.0F}, view, 1280.0F, 720.0F);
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
    {"camera clamp edge projection", &four_camera_clamp_edges_project_the_visible_boundary},
    {"camera projection independent round trip", &camera_projection_round_trips_through_independent_math},
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
