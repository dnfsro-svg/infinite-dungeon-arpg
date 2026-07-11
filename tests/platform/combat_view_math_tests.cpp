#include "test_framework.hpp"

#include "combat_view_math.hpp"

#include <array>

namespace {

using arpg::combat::Vec3;
using arpg::platform::ActorDrawItem;
using arpg::platform::ScreenProjection;

arpg::test::Failure back_and_front_projection_are_exact() noexcept {
    const ScreenProjection back = arpg::platform::project_combat_position(
        Vec3{0.0F, -3.5F, 0.0F}, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(back.x, 640.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.ground_y, 273.6, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.y, back.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(back.scale, 0.70, 1.0e-4));

    const ScreenProjection front = arpg::platform::project_combat_position(
        Vec3{0.0F, 3.5F, 0.0F}, 1280.0F, 720.0F);
    ARPG_REQUIRE(arpg::test::near(front.x, 640.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.ground_y, 633.6, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.y, front.ground_y, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(front.scale, 1.0, 1.0e-4));
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

constexpr arpg::test::TestCase kCases[] = {
    {"back and front projection", &back_and_front_projection_are_exact},
    {"Z-only actor offset", &z_only_offsets_actor_screen_y},
    {"stable actor draw order", &actor_order_is_y_z_x_then_index},
};

}  // namespace

arpg::test::TestSuite combat_view_math_suite() noexcept {
    return arpg::test::make_suite("combat_view_math", kCases);
}
