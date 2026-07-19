#include "test_framework.hpp"

#include "dungeon/dungeon_types.hpp"
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

constexpr arpg::test::TestCase kCases[] = {
    {"maps every dungeon element to a floor sprite",
        &environment_maps_each_dungeon_element_to_a_floor_sprite},
};

}  // namespace

arpg::test::TestSuite stage12_environment_render_suite() noexcept {
    return arpg::test::make_suite("stage12_environment_render", kCases);
}
