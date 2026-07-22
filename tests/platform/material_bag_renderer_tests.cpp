#include "test_framework.hpp"

#include "material_bag_renderer.hpp"
#include "material_manifest.hpp"

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

arpg::test::Failure selected_material_can_be_cancelled_without_changing_counts() noexcept {
    items::ItemOwnershipState state{};
    const std::size_t chaos = items::material_index(items::MaterialId::chaos);
    state.materials[chaos] = 4U;
    platform::MaterialBagRenderer renderer{};
    ARPG_REQUIRE(renderer.select_slot(chaos, state));
    ARPG_REQUIRE(renderer.clear_selection());
    ARPG_REQUIRE(!renderer.selected_material().has_value());
    ARPG_REQUIRE(state.materials[chaos] == 4U);
    ARPG_REQUIRE(!renderer.clear_selection());
    return {};
}

arpg::test::Failure reinforcement_stone_requires_explicit_destroy_confirmation() noexcept {
    platform::MaterialBagRenderer renderer{};
    ARPG_REQUIRE(!renderer.begin_reinforcement_confirmation(77U, 11U));
    ARPG_REQUIRE(renderer.begin_reinforcement_confirmation(77U, 12U));
    ARPG_REQUIRE(renderer.reinforcement_confirmation_item() == 77U);
    ARPG_REQUIRE(!renderer.resolve_reinforcement_confirmation(false).has_value());
    ARPG_REQUIRE(!renderer.reinforcement_confirmation_item().has_value());
    ARPG_REQUIRE(renderer.begin_reinforcement_confirmation(77U, 13U));
    ARPG_REQUIRE(renderer.resolve_reinforcement_confirmation(true) == 77U);
    ARPG_REQUIRE(!renderer.reinforcement_confirmation_item().has_value());
    return {};
}

arpg::test::Failure material_bag_and_skill_stones_use_authored_icon_resources() noexcept {
    for (std::size_t index{}; index < items::kMaterialCount; ++index) {
        const auto id = static_cast<items::MaterialId>(index);
        ARPG_REQUIRE(platform::material_bag_sprite(id)
            == platform::material_loot_sprite(id));
    }
    const auto active = platform::skill_stone_sprite(
        platform::SkillStoneVisualKind::active);
    const auto support = platform::skill_stone_sprite(
        platform::SkillStoneVisualKind::support);
    ARPG_REQUIRE(active != platform::MaterialSpriteId::missing);
    ARPG_REQUIRE(support != platform::MaterialSpriteId::missing);
    ARPG_REQUIRE(active != support);

    const platform::MaterialManifestDefinition manifest =
        platform::default_material_manifest();
    const auto* active_frame = platform::find_material_frame(manifest, active);
    const auto* support_frame = platform::find_material_frame(manifest, support);
    ARPG_REQUIRE(active_frame != nullptr);
    ARPG_REQUIRE(support_frame != nullptr);
    ARPG_REQUIRE(active_frame->atlas == platform::MaterialAtlasId::items_ui);
    ARPG_REQUIRE(support_frame->atlas == platform::MaterialAtlasId::items_ui);
    ARPG_REQUIRE(active_frame->perceptual_hash
        != support_frame->perceptual_hash);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"material bag contains all slots", &material_bag_has_all_fourteen_slots},
    {"minimum layout separates detail and bag", &minimum_layout_keeps_detail_above_material_bag},
    {"material bag selects owned material", &selected_material_requires_an_owned_material_slot},
    {"material bag selection cancels", &selected_material_can_be_cancelled_without_changing_counts},
    {"reinforcement requires destroy confirmation", &reinforcement_stone_requires_explicit_destroy_confirmation},
    {"bag and skill stones use authored resources", &material_bag_and_skill_stones_use_authored_icon_resources},
};

}  // namespace

arpg::test::TestSuite material_bag_renderer_suite() noexcept {
    return arpg::test::make_suite("material_bag_renderer", kCases);
}
