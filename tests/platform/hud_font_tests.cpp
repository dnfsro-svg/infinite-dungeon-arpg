#include "test_framework.hpp"

#include "death_overlay_font.hpp"
#include "hud_font.hpp"
#include "hud_palette.hpp"
#include "hud_renderer.hpp"

#include <cstddef>

namespace {

namespace platform = arpg::platform;

bool same_color(Color lhs, Color rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b
        && lhs.a == rhs.a;
}

arpg::test::Failure required_hud_text_is_covered_by_shared_font_plan() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    constexpr const char* kRequiredText[] = {
        u8"生命", u8"护盾", u8"剩余", u8"出口已开放", u8"保存失败",
        u8"未分配点", u8"火焰", u8"水", u8"闪电", u8"混沌",
        u8"减速", u8"腐蚀", u8"无敌",
    };

    ARPG_REQUIRE(plan.covers_required_text);
    for (const char* text : kRequiredText) {
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(plan.shared, text));
    }
    return {};
}

arpg::test::Failure task6_visible_chinese_text_is_covered_without_exhausting_shared_capacity() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    constexpr const char* kTask6Text[] = {
        u8"深度", u8"层房间", u8"生态", u8"第", u8"波", u8"下一波即将开始",
        u8"待领奖励", u8"未领取", u8"正在保存房间", u8"正在处理撤退",
        u8"房间状态异常", u8"深渊", u8"火", u8"水", u8"电", u8"混沌",
    };

    ARPG_REQUIRE(plan.covers_required_text);
    ARPG_REQUIRE(plan.shared.codepoint_count < plan.shared.codepoints.size());
    for (const char* text : kTask6Text) {
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(plan.shared, text));
    }
    return {};
}

arpg::test::Failure shared_codepoints_are_unique_and_fixed_capacity() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    ARPG_REQUIRE(plan.shared.candidate_count
        <= plan.shared.candidate_paths.size());
    ARPG_REQUIRE(plan.shared.codepoint_count > 0U);
    ARPG_REQUIRE(plan.shared.codepoint_count <= plan.shared.codepoints.size());
    for (std::size_t lhs = 0U; lhs < plan.shared.codepoint_count; ++lhs) {
        for (std::size_t rhs = lhs + 1U; rhs < plan.shared.codepoint_count;
             ++rhs) {
            ARPG_REQUIRE(plan.shared.codepoints[lhs] != plan.shared.codepoints[rhs]);
        }
    }
    return {};
}

arpg::test::Failure hud_palette_key_colors_are_opaque_and_distinct() noexcept {
    const platform::HudPalette palette = platform::hud_palette();
    constexpr std::size_t kColorCount = 9U;
    const Color colors[kColorCount] = {
        palette.health, palette.barrier, palette.experience, palette.fire,
        palette.water, palette.lightning, palette.chaos, palette.text,
        palette.error,
    };

    for (std::size_t lhs = 0U; lhs < kColorCount; ++lhs) {
        ARPG_REQUIRE(colors[lhs].a == 255U);
        for (std::size_t rhs = lhs + 1U; rhs < kColorCount; ++rhs) {
            ARPG_REQUIRE(!same_color(colors[lhs], colors[rhs]));
        }
    }
    return {};
}

arpg::test::Failure renderer_shutdown_is_safe_before_initialization() noexcept {
    platform::HudRenderer renderer{};
    ARPG_REQUIRE(!renderer.font_ready());
    renderer.shutdown();
    renderer.shutdown();
    ARPG_REQUIRE(!renderer.font_ready());
    renderer.draw({}, {});
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"required Chinese coverage", &required_hud_text_is_covered_by_shared_font_plan},
    {"Task6 Chinese coverage has capacity", &task6_visible_chinese_text_is_covered_without_exhausting_shared_capacity},
    {"fixed unique shared codepoints", &shared_codepoints_are_unique_and_fixed_capacity},
    {"opaque distinct HUD palette", &hud_palette_key_colors_are_opaque_and_distinct},
    {"safe uninitialized renderer shutdown", &renderer_shutdown_is_safe_before_initialization},
};

}  // namespace

arpg::test::TestSuite hud_font_suite() noexcept {
    return arpg::test::make_suite("hud_font", kCases);
}
