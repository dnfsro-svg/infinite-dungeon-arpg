#include "test_framework.hpp"

#include "hud_layout.hpp"
#include "hud_renderer.hpp"
#include "ui_text_contrast.hpp"

#include <array>

namespace {

namespace platform = arpg::platform;

constexpr float kLogicalWidth = 1280.0F;
constexpr float kLogicalHeight = 720.0F;
constexpr float kLogicalMargin = 16.0F;
constexpr float kAbyssConfirmationHeight = 56.0F;

[[nodiscard]] bool empty(platform::HudRect rect) noexcept {
    return rect.width == 0.0F && rect.height == 0.0F;
}

[[nodiscard]] platform::HudRect abyss_confirmation_area(
    const platform::HudLayout& layout) noexcept {
    return {
        layout.safe_area.x,
        layout.safe_area.y + layout.safe_area.height
            - (kAbyssConfirmationHeight * layout.scale),
        layout.safe_area.width,
        kAbyssConfirmationHeight * layout.scale,
    };
}

arpg::test::Failure resolution_matrices_are_safe_and_do_not_cover_combat() noexcept {
    struct ResolutionCase final {
        int width;
        int height;
        float expected_scale;
    };
    constexpr std::array<ResolutionCase, 3> kCases{{
        {1024, 576, 0.8F},
        {1280, 720, 1.0F},
        {1920, 1080, 1.5F},
    }};

    for (const ResolutionCase values : kCases) {
        const platform::HudLayout layout =
            platform::make_hud_layout(values.width, values.height, false);
        const platform::HudRect visible[] = {
            layout.player_panel,
            layout.objective_panel,
            layout.navigation_panel,
            layout.primary_notice,
            layout.secondary_notice,
        };

        ARPG_REQUIRE(arpg::test::near(layout.scale, values.expected_scale, 1.0e-6));
        ARPG_REQUIRE(arpg::test::near(layout.safe_area.x,
            kLogicalMargin * values.expected_scale, 1.0e-6));
        ARPG_REQUIRE(arpg::test::near(layout.safe_area.y,
            kLogicalMargin * values.expected_scale, 1.0e-6));
        for (const platform::HudRect panel : visible) {
            ARPG_REQUIRE(platform::hud_rect_inside(panel, layout.safe_area));
            ARPG_REQUIRE(!platform::hud_rects_overlap(panel,
                layout.combat_exclusion));
        }
        ARPG_REQUIRE(!platform::hud_rects_overlap(
            layout.player_panel, layout.objective_panel));
        ARPG_REQUIRE(!platform::hud_rects_overlap(
            layout.player_panel, layout.navigation_panel));
        ARPG_REQUIRE(!platform::hud_rects_overlap(
            layout.objective_panel, layout.navigation_panel));
        ARPG_REQUIRE(empty(layout.debug_panel));
    }
    return {};
}

arpg::test::Failure notices_remain_above_the_bottom_abyss_confirmation_area() noexcept {
    constexpr std::array<std::array<int, 2>, 3> kViewports{{
        {{1024, 576}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto viewport : kViewports) {
        const platform::HudLayout layout =
            platform::make_hud_layout(viewport[0], viewport[1], false);
        const platform::HudRect confirmation = abyss_confirmation_area(layout);
        ARPG_REQUIRE(!platform::hud_rects_overlap(
            layout.primary_notice, confirmation));
        ARPG_REQUIRE(!platform::hud_rects_overlap(
            layout.secondary_notice, confirmation));
        ARPG_REQUIRE(layout.primary_notice.y + layout.primary_notice.height
            <= confirmation.y);
        ARPG_REQUIRE(layout.secondary_notice.y + layout.secondary_notice.height
            <= layout.primary_notice.y);
    }
    return {};
}

arpg::test::Failure debug_panel_is_opt_in_and_keeps_the_combat_exclusion_clear() noexcept {
    const platform::HudLayout hidden = platform::make_hud_layout(1280, 720, false);
    const platform::HudLayout visible = platform::make_hud_layout(1280, 720, true);

    ARPG_REQUIRE(empty(hidden.debug_panel));
    ARPG_REQUIRE(!empty(visible.debug_panel));
    ARPG_REQUIRE(platform::hud_rect_inside(visible.debug_panel, visible.safe_area));
    ARPG_REQUIRE(!platform::hud_rects_overlap(
        visible.debug_panel, visible.combat_exclusion));
    ARPG_REQUIRE(!platform::hud_rects_overlap(
        visible.debug_panel, visible.player_panel));
    ARPG_REQUIRE(!platform::hud_rects_overlap(
        visible.debug_panel, visible.objective_panel));
    ARPG_REQUIRE(!platform::hud_rects_overlap(
        visible.debug_panel, visible.navigation_panel));
    return {};
}

arpg::test::Failure invalid_viewports_return_an_empty_layout() noexcept {
    constexpr std::array<std::array<int, 2>, 3> kInvalid{{
        {{0, 720}}, {{1280, 0}}, {{-1, -1}},
    }};
    for (const auto viewport : kInvalid) {
        const platform::HudLayout layout =
            platform::make_hud_layout(viewport[0], viewport[1], true);
        ARPG_REQUIRE(empty(layout.safe_area));
        ARPG_REQUIRE(empty(layout.player_panel));
        ARPG_REQUIRE(empty(layout.objective_panel));
        ARPG_REQUIRE(empty(layout.navigation_panel));
        ARPG_REQUIRE(empty(layout.primary_notice));
        ARPG_REQUIRE(empty(layout.secondary_notice));
        ARPG_REQUIRE(empty(layout.debug_panel));
        ARPG_REQUIRE(empty(layout.combat_exclusion));
        ARPG_REQUIRE(arpg::test::near(layout.scale, 0.0F));
    }
    return {};
}

arpg::test::Failure hud_rect_helpers_distinguish_touching_from_overlapping() noexcept {
    const platform::HudRect first{0.0F, 0.0F, 10.0F, 10.0F};
    const platform::HudRect touching{10.0F, 0.0F, 10.0F, 10.0F};
    const platform::HudRect overlapping{9.0F, 0.0F, 10.0F, 10.0F};
    const platform::HudRect contained{1.0F, 1.0F, 8.0F, 8.0F};

    ARPG_REQUIRE(!platform::hud_rects_overlap(first, touching));
    ARPG_REQUIRE(platform::hud_rects_overlap(first, overlapping));
    ARPG_REQUIRE(platform::hud_rect_inside(contained, first));
    ARPG_REQUIRE(!platform::hud_rect_inside(overlapping, first));
    return {};
}

arpg::test::Failure hud_text_safe_boxes_do_not_intersect_or_cross_panels() noexcept {
    constexpr std::array<std::array<int, 2>, 2> kViewports{{
        {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto viewport : kViewports) {
        const platform::HudLayout layout =
            platform::make_hud_layout(viewport[0], viewport[1], false);
        const platform::HudTextSafeLayout text =
            platform::make_hud_text_safe_layout(layout);
        ARPG_REQUIRE(platform::hud_rect_inside(
            text.objective_title, layout.objective_panel));
        ARPG_REQUIRE(platform::hud_rect_inside(
            text.objective_hint, layout.objective_panel));
        ARPG_REQUIRE(!platform::hud_rects_overlap(
            text.objective_title, text.objective_hint));
        ARPG_REQUIRE(platform::hud_rect_inside(
            text.navigation_title, layout.navigation_panel));
        ARPG_REQUIRE(platform::hud_rect_inside(
            text.navigation_ecology, layout.navigation_panel));
        ARPG_REQUIRE(!platform::hud_rects_overlap(
            text.navigation_title, text.navigation_ecology));
    }
    return {};
}

arpg::test::Failure hud_typography_has_readable_minimums_and_contrast() noexcept {
    const platform::HudReadabilityStyle style =
        platform::hud_readability_style();
    ARPG_REQUIRE(style.panel_minimum_font_size >= 17.0F);
    ARPG_REQUIRE(style.player_bar_font_size >= 20.0F);
    ARPG_REQUIRE(style.objective_primary_font_size >= 23.0F);
    ARPG_REQUIRE(style.objective_secondary_font_size >= 19.0F);
    ARPG_REQUIRE(style.navigation_secondary_font_size >= 19.0F);
    ARPG_REQUIRE(style.outline_pixels == 1);
    ARPG_REQUIRE(style.shadow_pixels >= 2);
    ARPG_REQUIRE(style.embolden_pixels >= 1);
    const platform::UiTextContrastStyle contrast =
        platform::ui_text_contrast_style();
    ARPG_REQUIRE(contrast.primary.a == 255U);
    ARPG_REQUIRE(contrast.secondary.a == 255U);
    ARPG_REQUIRE(contrast.interaction.r == 194U);
    ARPG_REQUIRE(contrast.interaction.g == 229U);
    ARPG_REQUIRE(contrast.interaction.b == 255U);
    ARPG_REQUIRE(contrast.interaction.a == 255U);
    ARPG_REQUIRE(contrast.primary.r == 248U);
    ARPG_REQUIRE(contrast.primary.g == 246U);
    ARPG_REQUIRE(contrast.primary.b == 238U);
    ARPG_REQUIRE(contrast.muted.a == 255U);
    ARPG_REQUIRE(contrast.warning.r == 255U);
    ARPG_REQUIRE(contrast.warning.g == 210U);
    ARPG_REQUIRE(contrast.warning.b == 118U);
    ARPG_REQUIRE(contrast.warning.a == 255U);
    ARPG_REQUIRE(contrast.shadow.a >= 235U);
    ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
        contrast.primary, contrast.backing) >= 7.0F);
    ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
        contrast.secondary, contrast.backing) >= 6.0F);
    ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
        contrast.muted, contrast.backing) >= 4.5F);
    ARPG_REQUIRE(contrast.outline_pixels >= 1);
    ARPG_REQUIRE(contrast.shadow_pixels >= 2);
    ARPG_REQUIRE(contrast.backing.a >= 220U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"resolution safe layout matrices", &resolution_matrices_are_safe_and_do_not_cover_combat},
    {"notices above abyss confirmation", &notices_remain_above_the_bottom_abyss_confirmation_area},
    {"debug layout opt in", &debug_panel_is_opt_in_and_keeps_the_combat_exclusion_clear},
    {"invalid viewport layout", &invalid_viewports_return_an_empty_layout},
    {"HUD rectangle helpers", &hud_rect_helpers_distinguish_touching_from_overlapping},
    {"HUD text safe boxes",
        &hud_text_safe_boxes_do_not_intersect_or_cross_panels},
    {"HUD typography readability",
        &hud_typography_has_readable_minimums_and_contrast},
};

}  // namespace

arpg::test::TestSuite hud_layout_suite() noexcept {
    return arpg::test::make_suite("hud_layout", kCases);
}
