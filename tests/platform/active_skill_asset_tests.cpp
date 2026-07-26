#include "test_framework.hpp"

#include "active_skill_assets.hpp"
#include "material_manifest.hpp"

#include <cstddef>

namespace {

namespace platform = arpg::platform;

arpg::test::Failure active_skill_atlases_define_complete_original_frame_grids()
    noexcept {
    for (std::size_t frame = 0U; frame < 36U; ++frame) {
        const auto source = platform::active_skill_atlas_frame(
            arpg::skills::ActiveSkillId::draw_slash, frame);
        ARPG_REQUIRE(source.has_value());
        ARPG_REQUIRE(source->atlas == platform::MaterialAtlasId::skill_draw_slash);
        ARPG_REQUIRE(source->source.x >= 0.0F);
        ARPG_REQUIRE(source->source.y >= 0.0F);
        ARPG_REQUIRE(source->source.x + source->source.width <= 1254.0F);
        ARPG_REQUIRE(source->source.y + source->source.height <= 1254.0F);
        ARPG_REQUIRE(source->foot_anchor.x >= 0.0F);
        ARPG_REQUIRE(source->foot_anchor.x <= source->source.width);
        ARPG_REQUIRE(source->foot_anchor.y >= 0.0F);
        ARPG_REQUIRE(source->foot_anchor.y <= source->source.height);
    }
    for (std::size_t frame = 0U; frame < 24U; ++frame) {
        const auto source = platform::active_skill_atlas_frame(
            arpg::skills::ActiveSkillId::storm_swords, frame);
        ARPG_REQUIRE(source.has_value());
        ARPG_REQUIRE(source->atlas == platform::MaterialAtlasId::skill_storm_swords);
        ARPG_REQUIRE(source->source.x >= 0.0F);
        ARPG_REQUIRE(source->source.y >= 0.0F);
        ARPG_REQUIRE(source->source.x + source->source.width <= 1024.0F);
        ARPG_REQUIRE(source->source.y + source->source.height <= 1536.0F);
    }
    ARPG_REQUIRE(!platform::active_skill_atlas_frame(
        arpg::skills::ActiveSkillId::storm_swords, 24U).has_value());
    return {};
}

arpg::test::Failure active_skill_atlases_are_registered_as_material_pairs()
    noexcept {
    const auto manifest = platform::default_material_manifest();
    bool found_draw{};
    bool found_storm{};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        if (atlas.id == platform::MaterialAtlasId::skill_draw_slash) {
            found_draw = atlas.width == 1254 && atlas.height == 1254
                && atlas.rgba_bytes == 1254U * 1254U * 4U
                && atlas.ecology == platform::MaterialEcology::skill;
        }
        if (atlas.id == platform::MaterialAtlasId::skill_storm_swords) {
            found_storm = atlas.width == 1024 && atlas.height == 1536
                && atlas.rgba_bytes == 1024U * 1536U * 4U
                && atlas.ecology == platform::MaterialEcology::skill;
        }
    }
    ARPG_REQUIRE(found_draw);
    ARPG_REQUIRE(found_storm);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"active skill atlases expose complete original frame grids",
     &active_skill_atlases_define_complete_original_frame_grids},
    {"active skill atlases are registered as material pairs",
     &active_skill_atlases_are_registered_as_material_pairs},
};

}  // namespace

arpg::test::TestSuite active_skill_asset_suite() noexcept {
    return arpg::test::make_suite("active_skill_assets", kCases);
}
