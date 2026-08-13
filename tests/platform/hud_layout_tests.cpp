#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "active_skill_view.hpp"
#include "dungeon_view_math.hpp"
#include "hud_layout.hpp"
#include "hud_renderer.hpp"
#include "ui_text_contrast.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

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
    constexpr std::array<ResolutionCase, 4> kCases{{
        {800, 450, 0.625F},
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
        const platform::HudRect screen{0.0F, 0.0F,
            static_cast<float>(values.width),
            static_cast<float>(values.height)};

        ARPG_REQUIRE(arpg::test::near(layout.scale, values.expected_scale, 1.0e-6));
        ARPG_REQUIRE(arpg::test::near(layout.safe_area.x,
            kLogicalMargin * values.expected_scale, 1.0e-6));
        ARPG_REQUIRE(arpg::test::near(layout.safe_area.y,
            kLogicalMargin * values.expected_scale, 1.0e-6));
        for (const platform::HudRect panel : visible) {
            ARPG_REQUIRE(platform::hud_rect_inside(panel, layout.safe_area));
            ARPG_REQUIRE(platform::hud_rect_inside(panel, screen));
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
    constexpr std::array<std::array<int, 2>, 4> kViewports{{
        {{800, 450}}, {{1024, 576}}, {{1280, 720}}, {{1920, 1080}},
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
    constexpr std::array<std::array<int, 2>, 3> kViewports{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto viewport : kViewports) {
        const platform::HudLayout layout =
            platform::make_hud_layout(viewport[0], viewport[1], false);
        const platform::HudTextSafeLayout text =
            platform::make_hud_text_safe_layout(layout);
        const platform::HudRect screen{0.0F, 0.0F,
            static_cast<float>(viewport[0]),
            static_cast<float>(viewport[1])};
        for (const platform::HudRect label : text.player_bar_labels) {
            ARPG_REQUIRE(platform::hud_rect_inside(label, layout.player_panel));
            ARPG_REQUIRE(platform::hud_rect_inside(label, screen));
        }
        ARPG_REQUIRE(platform::hud_rect_inside(
            text.player_progression, layout.player_panel));
        ARPG_REQUIRE(platform::hud_rect_inside(text.player_progression, screen));
        ARPG_REQUIRE(platform::hud_rect_inside(
            text.objective_title, layout.objective_panel));
        ARPG_REQUIRE(platform::hud_rect_inside(
            text.objective_hint, layout.objective_panel));
        ARPG_REQUIRE(empty(text.objective_movement));
        for (const platform::HudRect control : text.objective_controls) {
            ARPG_REQUIRE(empty(control));
        }
        ARPG_REQUIRE(empty(text.objective_diagnostics));
        const std::array<platform::HudRect, 2U> objective_rows{{
            text.objective_title,
            text.objective_hint,
        }};
        for (std::size_t first{}; first < objective_rows.size(); ++first) {
            for (std::size_t second = first + 1U;
                 second < objective_rows.size(); ++second) {
                ARPG_REQUIRE(!platform::hud_rects_overlap(
                    objective_rows[first], objective_rows[second]));
            }
        }
        ARPG_REQUIRE(layout.objective_panel.width >= 620.0F * layout.scale);
        ARPG_REQUIRE(layout.objective_panel.height <= 56.0F * layout.scale);
        ARPG_REQUIRE(platform::hud_rect_inside(
            text.navigation_title, layout.navigation_panel));
        ARPG_REQUIRE(platform::hud_rect_inside(
            text.navigation_ecology, layout.navigation_panel));
        ARPG_REQUIRE(!platform::hud_rects_overlap(
            text.navigation_title, text.navigation_ecology));
        for (const platform::HudRect bounds : objective_rows) {
            ARPG_REQUIRE(platform::hud_rect_inside(bounds, screen));
        }
        ARPG_REQUIRE(platform::hud_rect_inside(text.navigation_title, screen));
        ARPG_REQUIRE(platform::hud_rect_inside(text.navigation_ecology, screen));
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
    ARPG_REQUIRE(style.outline_pixels == 0);
    ARPG_REQUIRE(style.shadow_pixels >= 2);
    ARPG_REQUIRE(style.embolden_pixels == 0);
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
    ARPG_REQUIRE(contrast.outline_pixels == 0);
    ARPG_REQUIRE(contrast.shadow_pixels >= 2);
    ARPG_REQUIRE(contrast.backing.a >= 220U);
    return {};
}

[[nodiscard]] bool finite(Rectangle rectangle) noexcept {
    return std::isfinite(rectangle.x) && std::isfinite(rectangle.y)
        && std::isfinite(rectangle.width) && std::isfinite(rectangle.height)
        && std::isfinite(rectangle.x + rectangle.width)
        && std::isfinite(rectangle.y + rectangle.height);
}

[[nodiscard]] bool inside(Rectangle inner, Rectangle outer) noexcept {
    return finite(inner) && finite(outer)
        && inner.width > 0.0F && inner.height > 0.0F
        && inner.x >= outer.x && inner.y >= outer.y
        && inner.x + inner.width <= outer.x + outer.width
        && inner.y + inner.height <= outer.y + outer.height;
}

[[nodiscard]] bool overlaps(Rectangle first, Rectangle second) noexcept {
    return first.x < second.x + second.width
        && second.x < first.x + first.width
        && first.y < second.y + second.height
        && second.y < first.y + first.height;
}

[[nodiscard]] Rectangle translated(
    Rectangle rectangle, Vector2 offset) noexcept {
    rectangle.x += offset.x;
    rectangle.y += offset.y;
    return rectangle;
}

[[nodiscard]] bool centered(Vector2 text_position, Vector2 measured,
    Rectangle backing) noexcept {
    return arpg::test::near(text_position.x + measured.x * 0.5F,
               backing.x + backing.width * 0.5F)
        && arpg::test::near(text_position.y + measured.y * 0.5F,
            backing.y + backing.height * 0.5F);
}

[[nodiscard]] bool same_rectangle(
    Rectangle first, Rectangle second) noexcept {
    return arpg::test::near(first.x, second.x)
        && arpg::test::near(first.y, second.y)
        && arpg::test::near(first.width, second.width)
        && arpg::test::near(first.height, second.height);
}

arpg::test::Failure world_prompt_backings_fit_supported_viewports() noexcept {
    constexpr std::array<std::array<int, 2>, 3U> kViewports{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto viewport : kViewports) {
        const float width = static_cast<float>(viewport[0]);
        const float height = static_cast<float>(viewport[1]);
        const platform::WorldPromptLayout layout =
            platform::world_prompt_layout(
                {width * 0.5F, height - 2.0F},
                {width * 0.36F, 30.0F}, width, height);
        const Rectangle screen{0.0F, 0.0F, width, height};
        ARPG_REQUIRE(layout.visible);
        ARPG_REQUIRE(layout.text_scale == 1.0F);
        ARPG_REQUIRE(layout.backing.x >= screen.x);
        ARPG_REQUIRE(layout.backing.y >= screen.y);
        ARPG_REQUIRE(layout.backing.x + layout.backing.width
            <= screen.x + screen.width);
        ARPG_REQUIRE(layout.backing.y + layout.backing.height
            <= screen.y + screen.height);
        ARPG_REQUIRE(layout.text_position.x >= layout.backing.x);
        ARPG_REQUIRE(layout.text_position.y >= layout.backing.y);
        ARPG_REQUIRE(layout.text_position.x + layout.measured.x
            <= layout.backing.x + layout.backing.width);
        ARPG_REQUIRE(layout.text_position.y + layout.measured.y
            <= layout.backing.y + layout.backing.height);

        constexpr std::array<Vector2, 4U> kShakeOffsets{{
            {-9.0F, -9.0F}, {-9.0F, 9.0F},
            {9.0F, -9.0F}, {9.0F, 9.0F},
        }};
        const platform::ActiveSkillHudLayout skill =
            platform::active_skill_hud_layout(viewport[0], viewport[1]);
        ARPG_REQUIRE(skill.valid);

        const Vector2 measured{48.0F * skill.scale, 16.0F * skill.scale};
        const Rectangle far_below{width * 0.5F - 92.0F,
            height + 40.0F, 184.0F, 184.0F};
        ARPG_REQUIRE(!platform::door_world_prompt_layout(
            {width * 0.5F, height + 15.0F}, measured,
            far_below, skill.visual_bounds, {}, width, height).visible);

        const Rectangle material_entering{width * 0.5F - 92.0F,
            height - 12.0F, 184.0F, 184.0F};
        const Rectangle fallback_still_offscreen{width * 0.5F - 41.0F,
            height + 2.0F, 82.0F, 70.0F};
        ARPG_REQUIRE(platform::door_world_prompt_layout(
            {width * 0.5F, height - 6.0F}, measured,
            material_entering, skill.visual_bounds, {}, width, height).visible);
        ARPG_REQUIRE(!platform::door_world_prompt_layout(
            {width * 0.5F, height - 6.0F}, measured,
            fallback_still_offscreen, skill.visual_bounds, {},
            width, height).visible);

        const Rectangle visible_door{width * 0.5F - 92.0F,
            height - 184.0F, 184.0F, 184.0F};
        for (const Vector2 offset : kShakeOffsets) {
            const platform::WorldPromptLayout logical =
                platform::door_world_prompt_layout(
                    {width * 0.5F, skill.visual_bounds.y + 4.0F},
                    measured, visible_door, skill.visual_bounds,
                    offset, width, height);
            ARPG_REQUIRE(logical.visible);
            const Rectangle final_backing = translated(logical.backing, offset);
            const Vector2 final_text{
                logical.text_position.x + offset.x,
                logical.text_position.y + offset.y};
            ARPG_REQUIRE(inside(
                final_backing, {0.0F, 0.0F, width, height}));
            ARPG_REQUIRE(!overlaps(final_backing, skill.visual_bounds));
            ARPG_REQUIRE(final_backing.y + final_backing.height
                <= skill.visual_bounds.y - 12.0F * skill.scale);
            ARPG_REQUIRE(centered(final_text, logical.measured, final_backing));
        }

        const Vector2 upper_offset{9.0F, -9.0F};
        const Vector2 upper_anchor{width * 0.25F, height * 0.18F};
        const Rectangle upper_door{
            width * 0.25F - 41.0F, height * 0.12F, 82.0F, 70.0F};
        const platform::WorldPromptLayout upper_logical =
            platform::door_world_prompt_layout(upper_anchor, measured,
                upper_door, skill.visual_bounds, upper_offset, width, height);
        const platform::WorldPromptLayout upper_expected =
            platform::world_prompt_layout(
                {upper_anchor.x + upper_offset.x,
                    upper_anchor.y + upper_offset.y},
                measured, width, height);
        ARPG_REQUIRE(upper_logical.visible);
        ARPG_REQUIRE(upper_expected.visible);
        ARPG_REQUIRE(arpg::test::near(
            upper_logical.text_scale, upper_expected.text_scale));
        ARPG_REQUIRE(same_rectangle(
            translated(upper_logical.backing, upper_offset),
            upper_expected.backing));
        ARPG_REQUIRE(arpg::test::near(
            upper_logical.text_position.x + upper_offset.x,
            upper_expected.text_position.x));
        ARPG_REQUIRE(arpg::test::near(
            upper_logical.text_position.y + upper_offset.y,
            upper_expected.text_position.y));
        ARPG_REQUIRE(arpg::test::near(
            upper_logical.measured.x, upper_expected.measured.x));
        ARPG_REQUIRE(arpg::test::near(
            upper_logical.measured.y, upper_expected.measured.y));

        const Vector2 entering_offset{0.0F, -9.0F};
        const Rectangle enters_after_offset{
            width * 0.5F - 41.0F, height + 1.0F, 82.0F, 8.0F};
        ARPG_REQUIRE(platform::door_world_prompt_layout(
            {width * 0.5F, height + 1.0F}, measured,
            enters_after_offset, {}, entering_offset, width, height).visible);

        const Rectangle finite_empty_blocker{
            17.0F, 23.0F, 0.0F, -1.0F};
        const platform::WorldPromptLayout empty_blocker_layout =
            platform::door_world_prompt_layout(upper_anchor, measured,
                upper_door, finite_empty_blocker, upper_offset,
                width, height);
        ARPG_REQUIRE(empty_blocker_layout.visible);
        ARPG_REQUIRE(same_rectangle(
            translated(empty_blocker_layout.backing, upper_offset),
            upper_expected.backing));
    }

    constexpr float kNan = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInf = std::numeric_limits<float>::infinity();
    constexpr std::array<float, 2U> kInvalidScalars{{kNan, kInf}};
    constexpr Vector2 kMeasured{48.0F, 16.0F};
    constexpr Rectangle kDoor{500.0F, 500.0F, 184.0F, 184.0F};
    constexpr Rectangle kBlocker{430.0F, 600.0F, 420.0F, 100.0F};
    for (const float invalid : kInvalidScalars) {
        ARPG_REQUIRE(!platform::door_world_prompt_layout(
            {invalid, 300.0F}, kMeasured, kDoor, kBlocker, {},
            1280.0F, 720.0F).visible);
        ARPG_REQUIRE(!platform::door_world_prompt_layout(
            {640.0F, 300.0F}, {48.0F, invalid}, kDoor, kBlocker, {},
            1280.0F, 720.0F).visible);
        ARPG_REQUIRE(!platform::door_world_prompt_layout(
            {640.0F, 300.0F}, kMeasured,
            {500.0F, invalid, 184.0F, 184.0F}, kBlocker, {},
            1280.0F, 720.0F).visible);
        ARPG_REQUIRE(!platform::door_world_prompt_layout(
            {640.0F, 300.0F}, kMeasured, kDoor,
            {430.0F, 600.0F, invalid, 100.0F}, {},
            1280.0F, 720.0F).visible);
        ARPG_REQUIRE(!platform::door_world_prompt_layout(
            {640.0F, 300.0F}, kMeasured, kDoor, kBlocker,
            {0.0F, invalid}, 1280.0F, 720.0F).visible);
        ARPG_REQUIRE(!platform::door_world_prompt_layout(
            {640.0F, 300.0F}, kMeasured, kDoor, kBlocker, {},
            invalid, 720.0F).visible);
        ARPG_REQUIRE(!platform::door_world_prompt_layout(
            {640.0F, 300.0F}, kMeasured, kDoor, kBlocker, {},
            1280.0F, invalid).visible);
    }
    ARPG_REQUIRE(!platform::door_world_prompt_layout(
        {640.0F, 300.0F}, kMeasured,
        {500.0F, 500.0F, 0.0F, 184.0F}, kBlocker, {},
        1280.0F, 720.0F).visible);
    ARPG_REQUIRE(!platform::door_world_prompt_layout(
        {640.0F, 300.0F}, kMeasured,
        {500.0F, 500.0F, -1.0F, 184.0F}, kBlocker, {},
        1280.0F, 720.0F).visible);

    constexpr std::array<Vector2, 4U> kProbeOffsets{{
        {-9.0F, -9.0F}, {-9.0F, 9.0F},
        {9.0F, -9.0F}, {9.0F, 9.0F},
    }};
    const platform::ActiveSkillHudLayout probe_skill =
        platform::active_skill_hud_layout(1280, 720);
    ARPG_REQUIRE(probe_skill.valid);
    for (const Vector2 offset : kProbeOffsets) {
        static_cast<void>(platform::door_world_prompt_layout(
            {640.0F, probe_skill.visual_bounds.y + 4.0F}, kMeasured,
            kDoor, probe_skill.visual_bounds, offset, 1280.0F, 720.0F));
    }
    std::uint64_t visible_probe_count{};
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 10'000U; ++iteration) {
        const platform::WorldPromptLayout probe =
            platform::door_world_prompt_layout(
                {640.0F, probe_skill.visual_bounds.y + 4.0F}, kMeasured,
                kDoor, probe_skill.visual_bounds,
                kProbeOffsets[iteration % kProbeOffsets.size()],
                1280.0F, 720.0F);
        visible_probe_count += probe.visible ? 1U : 0U;
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    ARPG_REQUIRE(visible_probe_count == 10'000U);

    constexpr std::array<Vector2, 2U> kOversizedMeasured{{
        {1600.0F, 30.0F},
        {120.0F, 900.0F},
    }};
    for (const Vector2 measured : kOversizedMeasured) {
        const platform::WorldPromptLayout layout =
            platform::world_prompt_layout(
                {400.0F, 448.0F}, measured, 800.0F, 450.0F);
        ARPG_REQUIRE(layout.visible);
        ARPG_REQUIRE(layout.text_scale > 0.0F);
        ARPG_REQUIRE(layout.text_scale < 1.0F);
        ARPG_REQUIRE(layout.measured.x <= layout.backing.width);
        ARPG_REQUIRE(layout.measured.y <= layout.backing.height);
        ARPG_REQUIRE(layout.text_position.x >= 0.0F);
        ARPG_REQUIRE(layout.text_position.y >= 0.0F);
        ARPG_REQUIRE(layout.text_position.x + layout.measured.x <= 800.0F);
        ARPG_REQUIRE(layout.text_position.y + layout.measured.y <= 450.0F);
        ARPG_REQUIRE(layout.backing.x >= 0.0F);
        ARPG_REQUIRE(layout.backing.y >= 0.0F);
        ARPG_REQUIRE(layout.backing.x + layout.backing.width <= 800.0F);
        ARPG_REQUIRE(layout.backing.y + layout.backing.height <= 450.0F);
    }
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
    {"world prompt supported viewport bounds",
        &world_prompt_backings_fit_supported_viewports},
};

}  // namespace

arpg::test::TestSuite hud_layout_suite() noexcept {
    return arpg::test::make_suite("hud_layout", kCases);
}
