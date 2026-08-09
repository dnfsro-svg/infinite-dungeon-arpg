#include "test_framework.hpp"

#include "death_overlay_font.hpp"
#include "hud_font.hpp"
#include "ui_text_catalog.hpp"

#include <cstddef>
#include <cstring>

namespace {

namespace platform = arpg::platform;

struct CatalogCoverage final {
    const platform::UiGlyphPlan* plan{};
    bool covered{true};
};

void record_catalog_coverage(const char* text, void* context) noexcept {
    auto& coverage = *static_cast<CatalogCoverage*>(context);
    coverage.covered = coverage.covered
        && platform::ui_glyph_plan_covers_text(*coverage.plan, text);
}

arpg::test::Failure complete_catalog_is_covered_by_one_shared_plan() noexcept {
    const platform::UiGlyphPlan plan = platform::ui_glyph_plan();
    CatalogCoverage coverage{&plan};
    platform::visit_ui_text_catalog(&record_catalog_coverage, &coverage);

    ARPG_REQUIRE(coverage.covered);
    ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(plan, u8"最后一击"));
    ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(plan, u8"主动技能石槽"));
    ARPG_REQUIRE(plan.codepoint_count > 384U);
    ARPG_REQUIRE(plan.codepoint_count <= platform::kUiGlyphCapacity);
    ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(plan, u8"混沌之眼"));
    ARPG_REQUIRE(platform::death_overlay_font_covers_text(
        platform::death_overlay_font_plan(), u8"混沌之眼"));
    ARPG_REQUIRE(platform::hud_font_plan().covers_required_text);
    return {};
}

arpg::test::Failure plan_codepoints_are_unique() noexcept {
    const platform::UiGlyphPlan plan = platform::ui_glyph_plan();
    for (std::size_t lhs = 0U; lhs < plan.codepoint_count; ++lhs) {
        for (std::size_t rhs = lhs + 1U; rhs < plan.codepoint_count; ++rhs) {
            ARPG_REQUIRE(plan.codepoints[lhs] != plan.codepoints[rhs]);
        }
    }
    return {};
}

arpg::test::Failure capacity_and_atlas_budget_are_explicitly_bounded() noexcept {
    static_assert(platform::kUiFontAtlasInstanceCount == 1U);
    static_assert(platform::kUiFontAtlasByteBudget == 32U * 1024U * 1024U);
    static_assert(platform::ui_font_atlas_bytes(4096, 4096)
        == 32U * 1024U * 1024U);
    const platform::UiGlyphPlan plan = platform::ui_glyph_plan();
    ARPG_REQUIRE(!plan.capacity_exceeded);

    platform::UiGlyphPlan overflow = plan;
    overflow.capacity_exceeded = true;
    ARPG_REQUIRE(!platform::ui_glyph_plan_covers_text(overflow, u8"混沌之眼"));
    return {};
}

arpg::test::Failure bundled_noto_font_is_the_only_candidate() noexcept {
    const platform::UiGlyphPlan plan = platform::ui_glyph_plan();
    ARPG_REQUIRE(plan.candidate_count == 1U);
    ARPG_REQUIRE(plan.candidate_paths[0] != nullptr);
    ARPG_REQUIRE(std::strcmp(plan.candidate_paths[0],
        "assets/fonts/NotoSansCJKsc-Medium.otf") == 0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"complete catalog coverage", &complete_catalog_is_covered_by_one_shared_plan},
    {"unique glyph codepoints", &plan_codepoints_are_unique},
    {"bounded capacity and atlas", &capacity_and_atlas_budget_are_explicitly_bounded},
    {"bundled Noto candidate", &bundled_noto_font_is_the_only_candidate},
};

}  // namespace

arpg::test::TestSuite shared_ui_font_suite() noexcept {
    return arpg::test::make_suite("shared_ui_font", kCases);
}
