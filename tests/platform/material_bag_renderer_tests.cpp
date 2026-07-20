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
    {"material bag selects owned material", &selected_material_requires_an_owned_material_slot},
};

}  // namespace

arpg::test::TestSuite material_bag_renderer_suite() noexcept {
    return arpg::test::make_suite("material_bag_renderer", kCases);
}
