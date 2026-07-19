#include "test_framework.hpp"

#include "dungeon/dungeon_types.hpp"
#include "environment_render_plan.hpp"
#include "material_animation.hpp"

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
