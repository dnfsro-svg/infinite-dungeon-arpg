#include "test_framework.hpp"

#include "active_skill_assets.hpp"

#include <cstddef>

namespace {

namespace platform = arpg::platform;

arpg::test::Failure active_skill_atlases_define_complete_original_frame_grids()
    noexcept {
    const platform::ActiveSkillAtlasDefinition* const draw =
        platform::active_skill_atlas_definition(
            platform::ActiveSkillAtlasId::draw_slash);
    const platform::ActiveSkillAtlasDefinition* const storm =
        platform::active_skill_atlas_definition(
            platform::ActiveSkillAtlasId::storm_swords);
    ARPG_REQUIRE(draw != nullptr);
    ARPG_REQUIRE(storm != nullptr);
    ARPG_REQUIRE(draw->frame_count == 36U);
    ARPG_REQUIRE(draw->columns == 6U);
    ARPG_REQUIRE(draw->rows == 6U);
    ARPG_REQUIRE(draw->width == 1254);
    ARPG_REQUIRE(draw->height == 1254);
    ARPG_REQUIRE(storm->frame_count == 24U);
    ARPG_REQUIRE(storm->columns == 4U);
    ARPG_REQUIRE(storm->rows == 6U);
    ARPG_REQUIRE(storm->width == 1024);
    ARPG_REQUIRE(storm->height == 1536);

    for (std::size_t frame = 0U; frame < draw->frame_count; ++frame) {
        const auto source = platform::active_skill_atlas_frame(
            platform::ActiveSkillAtlasId::draw_slash, frame);
        ARPG_REQUIRE(source.has_value());
        ARPG_REQUIRE(source->source.x >= 0.0F);
        ARPG_REQUIRE(source->source.y >= 0.0F);
        ARPG_REQUIRE(source->source.x + source->source.width <= draw->width);
        ARPG_REQUIRE(source->source.y + source->source.height <= draw->height);
        ARPG_REQUIRE(source->foot_anchor.x >= 0.0F);
        ARPG_REQUIRE(source->foot_anchor.x <= source->source.width);
        ARPG_REQUIRE(source->foot_anchor.y >= 0.0F);
        ARPG_REQUIRE(source->foot_anchor.y <= source->source.height);
    }
    ARPG_REQUIRE(!platform::active_skill_atlas_frame(
        platform::ActiveSkillAtlasId::storm_swords, storm->frame_count).has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"active skill atlases expose complete original frame grids",
     &active_skill_atlases_define_complete_original_frame_grids},
};

}  // namespace

arpg::test::TestSuite active_skill_asset_suite() noexcept {
    return arpg::test::make_suite("active_skill_assets", kCases);
}
