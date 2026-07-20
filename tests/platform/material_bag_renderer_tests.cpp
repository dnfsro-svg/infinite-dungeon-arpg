#include "test_framework.hpp"

#include "material_bag_renderer.hpp"

namespace {

namespace items = arpg::items;
namespace platform = arpg::platform;

arpg::test::Failure material_bag_has_all_fourteen_slots() noexcept {
    const platform::MaterialBagLayout layout =
        platform::material_bag_layout(1280, 720);
    ARPG_REQUIRE(layout.contains_all_slots());
    ARPG_REQUIRE(layout.slots.size() == items::kMaterialCount);
    return {};
}

arpg::test::Failure minimum_layout_keeps_detail_above_material_bag() noexcept {
    const platform::MaterialBagLayout bag =
        platform::material_bag_layout(800, 450);
    const Rectangle detail = platform::material_bag_detail_bounds(800, 450);
    ARPG_REQUIRE(bag.contains_all_slots());
    ARPG_REQUIRE(detail.height > 0.0F);
    ARPG_REQUIRE(detail.y + detail.height <= bag.panel.y);
    return {};
}

arpg::test::Failure selected_material_requires_an_owned_material_slot() noexcept {
    items::ItemOwnershipState state{};
    state.materials[items::material_index(items::MaterialId::chaos)] = 4U;
    platform::MaterialBagRenderer renderer{};
    ARPG_REQUIRE(!renderer.selected_material().has_value());
    ARPG_REQUIRE(renderer.select_slot(
        items::material_index(items::MaterialId::chaos), state));
    ARPG_REQUIRE(renderer.selected_material()
        == items::MaterialId::chaos);
    ARPG_REQUIRE(!renderer.select_slot(
        items::material_index(items::MaterialId::exalt), state));
    ARPG_REQUIRE(renderer.selected_material()
        == items::MaterialId::chaos);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"material bag contains all slots", &material_bag_has_all_fourteen_slots},
    {"minimum layout separates detail and bag", &minimum_layout_keeps_detail_above_material_bag},
    {"material bag selects owned material", &selected_material_requires_an_owned_material_slot},
};

}  // namespace

arpg::test::TestSuite material_bag_renderer_suite() noexcept {
    return arpg::test::make_suite("material_bag_renderer", kCases);
}
