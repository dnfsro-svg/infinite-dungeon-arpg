#include "test_framework.hpp"

#include "items/item_types.hpp"
#include "material_asset_validation.hpp"
#include "material_manifest.hpp"
#include "material_pack.hpp"

namespace {

arpg::test::Failure effect_manifest_stays_within_texture_budget() noexcept {
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(
        arpg::platform::default_material_manifest()).valid);
    return {};
}

arpg::test::Failure effect_sprites_require_effect_atlas() noexcept {
    arpg::platform::MaterialPackState state{};
    state.set_available(arpg::platform::MaterialAtlasId::actors, true);
    ARPG_REQUIRE(!state.can_draw(arpg::platform::MaterialSpriteId::effect_fire));
    state.set_available(arpg::platform::MaterialAtlasId::effects_ui, true);
    ARPG_REQUIRE(state.can_draw(arpg::platform::MaterialSpriteId::effect_fire));
    return {};
}

arpg::test::Failure loot_rarity_uses_distinct_material_icons() noexcept {
    ARPG_REQUIRE(arpg::platform::select_loot_sprite(
        arpg::items::ItemRarity::normal)
        != arpg::platform::select_loot_sprite(arpg::items::ItemRarity::rare));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"effect manifest texture budget", &effect_manifest_stays_within_texture_budget},
    {"loot rarity material icons", &loot_rarity_uses_distinct_material_icons},
    {"effect sprites use effects atlas", &effect_sprites_require_effect_atlas},
};

}  // namespace

arpg::test::TestSuite stage12_material_render_suite() noexcept {
    return arpg::test::make_suite("stage12_material_render", kCases);
}
