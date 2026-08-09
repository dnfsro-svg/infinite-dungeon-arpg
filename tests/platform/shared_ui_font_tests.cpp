#include "test_framework.hpp"

#include "death_overlay_font.hpp"
#include "hud_font.hpp"
#include "ui_text_catalog.hpp"

#include <cstddef>
#include <cstring>
#include <type_traits>

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

void visit_synthetic_cjk_codepoints(platform::UiTextVisitor visitor,
    void* context, int count) noexcept {
    for (int offset = 0; offset < count; ++offset) {
        const int codepoint = 0x4E00 + offset;
        char text[4]{
            static_cast<char>(0xE0U | ((codepoint >> 12U) & 0x0FU)),
            static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)),
            static_cast<char>(0x80U | (codepoint & 0x3FU)),
            '\0',
        };
        visitor(text, context);
    }
}

void visit_exact_glyph_capacity(platform::UiTextVisitor visitor,
    void* context) noexcept {
    visit_synthetic_cjk_codepoints(visitor, context, 545);
}

void visit_one_codepoint_over_capacity(platform::UiTextVisitor visitor,
    void* context) noexcept {
    visit_synthetic_cjk_codepoints(visitor, context, 546);
}

arpg::test::Failure complete_catalog_is_covered_by_one_shared_plan() noexcept {
    const platform::UiGlyphPlan plan = platform::ui_glyph_plan();
    CatalogCoverage coverage{&plan};
    platform::visit_ui_text_catalog(&record_catalog_coverage, &coverage);

    ARPG_REQUIRE(coverage.covered);
    ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(plan, u8"最后一击"));
    ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(plan, u8"主动技能石槽"));
    ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(plan, u8"…、"));
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
    static_assert(!std::is_copy_constructible_v<platform::SharedUiFont>);
    static_assert(!std::is_copy_assignable_v<platform::SharedUiFont>);
    static_assert(!std::is_move_constructible_v<platform::SharedUiFont>);
    static_assert(!std::is_move_assignable_v<platform::SharedUiFont>);
    static_assert(platform::kUiFontAtlasInstanceCount == 1U);
    static_assert(platform::kUiFontAtlasByteBudget == 32U * 1024U * 1024U);
    static_assert(platform::ui_font_atlas_bytes(4096, 4096)
        == 32U * 1024U * 1024U);
    const platform::UiGlyphPlan plan = platform::ui_glyph_plan();
    ARPG_REQUIRE(!plan.capacity_exceeded);
    for (std::size_t index = 0U; index < 95U; ++index) {
        ARPG_REQUIRE(plan.codepoints[index] == static_cast<int>(32U + index));
    }

    const platform::UiGlyphPlan exact_capacity =
        platform::build_ui_glyph_plan(&visit_exact_glyph_capacity);
    ARPG_REQUIRE(exact_capacity.codepoint_count == platform::kUiGlyphCapacity);
    ARPG_REQUIRE(!exact_capacity.capacity_exceeded);
    ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(exact_capacity, u8"一"));

    const platform::UiGlyphPlan over_capacity =
        platform::build_ui_glyph_plan(&visit_one_codepoint_over_capacity);
    ARPG_REQUIRE(over_capacity.codepoint_count == platform::kUiGlyphCapacity);
    ARPG_REQUIRE(over_capacity.capacity_exceeded);
    ARPG_REQUIRE(!platform::ui_glyph_plan_covers_text(over_capacity, u8"一"));

    platform::UiGlyphPlan candidate_plan{};
    candidate_plan.codepoints[0] = 65;
    candidate_plan.codepoint_count = 1U;
    GlyphInfo glyph{};
    glyph.value = 65;
    Rectangle rec{};
    Font candidate{};
    candidate.baseSize = platform::kUiFontSourceBaseSize;
    candidate.glyphCount = 1;
    candidate.glyphs = &glyph;
    candidate.recs = &rec;
    candidate.texture = Texture2D{1U, 4096, 4096, 1,
        PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA};
    ARPG_REQUIRE(platform::ui_font_candidate_is_acceptable(
        candidate, candidate_plan));
    ARPG_REQUIRE(platform::ui_font_atlas_bytes(4096, 4096,
        candidate.texture.format) == platform::kUiFontAtlasByteBudget);

    candidate.texture.id = 0U;
    ARPG_REQUIRE(!platform::ui_font_candidate_is_acceptable(
        candidate, candidate_plan));

    candidate.texture.id = 1U;
    candidate.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    ARPG_REQUIRE(platform::ui_font_atlas_bytes(4096, 4096,
        candidate.texture.format) == 0U);
    ARPG_REQUIRE(!platform::ui_font_candidate_is_acceptable(
        candidate, candidate_plan));

    candidate.texture.format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
    candidate.texture.width = 4097;
    ARPG_REQUIRE(platform::ui_font_atlas_bytes(candidate.texture.width,
        candidate.texture.height, candidate.texture.format)
        > platform::kUiFontAtlasByteBudget);
    ARPG_REQUIRE(!platform::ui_font_candidate_is_acceptable(
        candidate, candidate_plan));

    platform::SharedUiFont unloaded_owner{};
    ARPG_REQUIRE(unloaded_owner.active_instance_count() == 0U);
    ARPG_REQUIRE(unloaded_owner.total_atlas_bytes() == 0U);

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
