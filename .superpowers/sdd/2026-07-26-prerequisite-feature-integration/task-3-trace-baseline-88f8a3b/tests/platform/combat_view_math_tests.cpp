#include "test_framework.hpp"

#include "combat_view_math.hpp"
#include "render_layout.hpp"

#include <array>

namespace {

using arpg::combat::Vec3;
using arpg::platform::ActorDrawItem;
using arpg::platform::ScreenProjection;

arpg::test::Failure back_and_front_projection_are_exact() noexcept {
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
