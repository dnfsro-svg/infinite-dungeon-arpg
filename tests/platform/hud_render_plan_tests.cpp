#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat/combat_types.hpp"
#include "hud_palette.hpp"
#include "hud_renderer.hpp"

#include <array>
#include <limits>

namespace {

namespace platform = arpg::platform;
namespace combat = arpg::combat;

bool same_color(Color lhs, Color rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b
        && lhs.a == rhs.a;
}

[[nodiscard]] platform::PlayerHudModel player_model() noexcept {
    platform::PlayerHudModel model{};
    model.visible = true;
    model.hp = 75;
    model.max_hp = 100;
    model.hp_ratio = 0.75F;
    model.level = 12U;
    model.experience = 25U;
    model.required_experience = 100U;
    model.experience_ratio = 0.25F;
    return model;
}

[[nodiscard]] platform::HudLayout player_layout() noexcept {
    return platform::make_hud_layout(1280, 720, false);
}

[[nodiscard]] bool rect_inside(platform::HudRect inner,
    platform::HudRect outer) noexcept {
    return platform::hud_rect_inside(inner, outer);
}

arpg::test::Failure health_is_always_the_first_visible_player_bar() noexcept {
    const platform::PlayerPanelPlan plan = platform::make_player_panel_plan(
        player_model(), player_layout(), 0.0F);

    ARPG_REQUIRE(plan.bar_count == 2U);
    ARPG_REQUIRE(plan.bars[0].kind == platform::HudBarKind::health);
    ARPG_REQUIRE(arpg::test::near(plan.bars[0].ratio, 0.75F));
    ARPG_REQUIRE(rect_inside(plan.bars[0].bounds, player_layout().player_panel));
    return {};
}

arpg::test::Failure barrier_bar_is_visible_only_with_a_positive_maximum() noexcept {
    platform::PlayerHudModel without_barrier = player_model();
    without_barrier.max_barrier = 0;
    const platform::PlayerPanelPlan hidden = platform::make_player_panel_plan(
        without_barrier, player_layout(), 0.0F);
    ARPG_REQUIRE(hidden.bar_count == 2U);

    platform::PlayerHudModel with_barrier = player_model();
    with_barrier.barrier = 30;
    with_barrier.max_barrier = 60;
    with_barrier.barrier_ratio = 0.5F;
    const platform::PlayerPanelPlan visible = platform::make_player_panel_plan(
        with_barrier, player_layout(), 0.0F);
    ARPG_REQUIRE(visible.bar_count == 3U);
    ARPG_REQUIRE(visible.bars[1].kind == platform::HudBarKind::barrier);
    ARPG_REQUIRE(arpg::test::near(visible.bars[1].ratio, 0.5F));
    ARPG_REQUIRE(rect_inside(visible.bars[1].bounds, player_layout().player_panel));
    return {};
}

arpg::test::Failure max_level_experience_uses_the_max_presentation() noexcept {
    platform::PlayerHudModel model = player_model();
    model.level = 100U;
    model.experience = 99999U;
    model.required_experience = 0U;
    model.experience_ratio = -1.0F;
    const platform::PlayerPanelPlan plan = platform::make_player_panel_plan(
        model, player_layout(), 0.0F);

    ARPG_REQUIRE(plan.experience_maxed);
    ARPG_REQUIRE(plan.bar_count == 2U);
    ARPG_REQUIRE(plan.bars[1].kind == platform::HudBarKind::experience);
    ARPG_REQUIRE(arpg::test::near(plan.bars[1].ratio, 1.0F));
    return {};
}

arpg::test::Failure low_health_emphasis_is_presentation_time_bounded_to_two_hz() noexcept {
    platform::PlayerHudModel model = player_model();
    model.hp_ratio = 0.24F;
    const platform::HudLayout layout = player_layout();
    const platform::PlayerPanelPlan first = platform::make_player_panel_plan(
        model, layout, 0.0F);
    const platform::PlayerPanelPlan before_edge = platform::make_player_panel_plan(
        model, layout, 0.249F);
    const platform::PlayerPanelPlan after_edge = platform::make_player_panel_plan(
        model, layout, 0.251F);
    const platform::PlayerPanelPlan next_pulse = platform::make_player_panel_plan(
        model, layout, 0.501F);

    ARPG_REQUIRE(first.low_health_emphasis);
    ARPG_REQUIRE(before_edge.low_health_emphasis == first.low_health_emphasis);
    ARPG_REQUIRE(after_edge.low_health_emphasis != first.low_health_emphasis);
    ARPG_REQUIRE(next_pulse.low_health_emphasis == first.low_health_emphasis);

    model.hp_ratio = 0.25F;
    ARPG_REQUIRE(!platform::make_player_panel_plan(model, layout, 0.0F)
        .low_health_emphasis);
    return {};
}

arpg::test::Failure player_plan_preserves_at_most_three_snapshot_status_tags() noexcept {
    platform::PlayerHudModel model = player_model();
    model.status_tags = {{
        platform::HudStatusTagKind::slow,
        platform::HudStatusTagKind::corrosion,
        platform::HudStatusTagKind::invulnerable,
    }};
    model.status_tag_count = 3U;
    const platform::PlayerPanelPlan plan = platform::make_player_panel_plan(
        model, player_layout(), 1.0F);

    ARPG_REQUIRE(plan.tag_count == 3U);
    for (std::size_t index = 0U; index < plan.tag_count; ++index) {
        ARPG_REQUIRE(plan.tags[index] == model.status_tags[index]);
    }
    return {};
}

arpg::test::Failure plan_clamps_ratios_and_keeps_stable_bounds() noexcept {
    platform::PlayerHudModel model = player_model();
    model.hp_ratio = 2.0F;
    model.barrier_ratio = -1.0F;
    model.experience_ratio = 7.0F;
    model.barrier = 5;
    model.max_barrier = 10;
    const platform::HudLayout layout = player_layout();
    const platform::PlayerPanelPlan first = platform::make_player_panel_plan(
        model, layout, 0.0F);
    const platform::PlayerPanelPlan later = platform::make_player_panel_plan(
        model, layout, 123.0F);

    ARPG_REQUIRE(first.bar_count == 3U);
    ARPG_REQUIRE(arpg::test::near(first.bars[0].ratio, 1.0F));
    ARPG_REQUIRE(arpg::test::near(first.bars[1].ratio, 0.0F));
    ARPG_REQUIRE(arpg::test::near(first.bars[2].ratio, 1.0F));
    for (std::size_t index = 0U; index < first.bar_count; ++index) {
        ARPG_REQUIRE(rect_inside(first.bars[index].bounds, layout.player_panel));
        ARPG_REQUIRE(arpg::test::near(first.bars[index].bounds.x,
            later.bars[index].bounds.x));
        ARPG_REQUIRE(arpg::test::near(first.bars[index].bounds.y,
            later.bars[index].bounds.y));
        ARPG_REQUIRE(arpg::test::near(first.bars[index].bounds.width,
            later.bars[index].bounds.width));
        ARPG_REQUIRE(arpg::test::near(first.bars[index].bounds.height,
            later.bars[index].bounds.height));
    }
    return {};
}

arpg::test::Failure nonfinite_player_ratios_fall_back_to_zero_without_low_health_pulse() noexcept {
    constexpr std::array<float, 3> kNonfinite{{
        (std::numeric_limits<float>::quiet_NaN)(),
        (std::numeric_limits<float>::infinity)(),
        -(std::numeric_limits<float>::infinity)(),
    }};
    for (const float nonfinite : kNonfinite) {
        platform::PlayerHudModel model = player_model();
        model.hp_ratio = nonfinite;
        model.barrier_ratio = nonfinite;
        model.experience_ratio = nonfinite;
        model.barrier = 5;
        model.max_barrier = 10;
        const platform::PlayerPanelPlan plan = platform::make_player_panel_plan(
            model, player_layout(), 0.0F);

        ARPG_REQUIRE(plan.bar_count == 3U);
        ARPG_REQUIRE(arpg::test::near(plan.bars[0].ratio, 0.0F));
        ARPG_REQUIRE(arpg::test::near(plan.bars[1].ratio, 0.0F));
        ARPG_REQUIRE(arpg::test::near(plan.bars[2].ratio, 0.0F));
        ARPG_REQUIRE(!plan.low_health_emphasis);
    }
    return {};
}

arpg::test::Failure monster_resource_plan_consumes_snapshot_values_and_palette_ids() noexcept {
    combat::MonsterSnapshot without_resources{};
    without_resources.hp = 150;
    without_resources.max_hp = 100;
    const platform::MonsterBarVisualPlan bare =
        platform::make_monster_bar_visual_plan(without_resources);

    ARPG_REQUIRE(bare.world_space);
    ARPG_REQUIRE(bare.bars[0].visible);
    ARPG_REQUIRE(arpg::test::near(bare.bars[0].ratio, 1.0F));
    ARPG_REQUIRE(bare.bars[0].palette_id == platform::HudPaletteId::health);
    ARPG_REQUIRE(!bare.bars[1].visible);
    ARPG_REQUIRE(!bare.bars[2].visible);

    combat::MonsterSnapshot with_resources{};
    with_resources.hp = 25;
    with_resources.max_hp = 100;
    with_resources.shield = -5;
    with_resources.max_shield = 20;
    with_resources.break_value = 30;
    with_resources.max_break = 60;
    const platform::MonsterBarVisualPlan full =
        platform::make_monster_bar_visual_plan(with_resources);

    ARPG_REQUIRE(full.world_space);
    ARPG_REQUIRE(full.bars[0].visible);
    ARPG_REQUIRE(arpg::test::near(full.bars[0].ratio, 0.25F));
    ARPG_REQUIRE(full.bars[1].visible);
    ARPG_REQUIRE(arpg::test::near(full.bars[1].ratio, 0.0F));
    ARPG_REQUIRE(full.bars[1].palette_id == platform::HudPaletteId::barrier);
    ARPG_REQUIRE(full.bars[2].visible);
    ARPG_REQUIRE(arpg::test::near(full.bars[2].ratio, 0.5F));
    ARPG_REQUIRE(full.bars[2].palette_id == platform::HudPaletteId::experience);
    return {};
}

arpg::test::Failure objective_navigation_and_context_plans_stay_in_their_layout_panels() noexcept {
    const platform::HudLayout layout = player_layout();
    platform::RoomHudModel room{};
    static_cast<void>(std::snprintf(room.objective.bytes.data(), room.objective.bytes.size(),
        u8"第 1/2 波 · 剩余 3"));
    platform::NavigationHudModel navigation{};
    static_cast<void>(std::snprintf(navigation.primary.bytes.data(), navigation.primary.bytes.size(),
        u8"深度 1 · 层房间 2"));
    platform::ContextHudModel context{};
    context.primary_kind = platform::HudNoticeKind::exit_ready;
    static_cast<void>(std::snprintf(context.primary.bytes.data(), context.primary.bytes.size(),
        "E to enter exit"));

    const platform::ObjectivePanelPlan objective =
        platform::make_objective_panel_plan(room, layout);
    const platform::NavigationPanelPlan navigation_plan =
        platform::make_navigation_panel_plan(navigation, layout);
    const platform::ContextPanelPlan context_plan =
        platform::make_context_panel_plan(context, layout);

    ARPG_REQUIRE(objective.visible);
    ARPG_REQUIRE(objective.primary.bytes == room.objective.bytes);
    ARPG_REQUIRE(rect_inside(objective.bounds, layout.objective_panel));
    ARPG_REQUIRE(navigation_plan.visible);
    ARPG_REQUIRE(navigation_plan.primary.bytes == navigation.primary.bytes);
    ARPG_REQUIRE(rect_inside(navigation_plan.bounds, layout.navigation_panel));
    ARPG_REQUIRE(context_plan.primary_visible);
    ARPG_REQUIRE(context_plan.primary_kind == platform::HudNoticeKind::exit_ready);
    ARPG_REQUIRE(rect_inside(context_plan.primary_bounds, layout.primary_notice));
    return {};
}

float monospace_measure(const char* text, float font_size, void*) noexcept {
    std::size_t count{};
    while (text != nullptr && text[count] != '\0') ++count;
    return static_cast<float>(count) * font_size * 0.6F;
}

bool is_valid_utf8(const char* text) noexcept {
    if (text == nullptr) return false;
    const auto* bytes = reinterpret_cast<const unsigned char*>(text);
    for (std::size_t index{}; bytes[index] != 0U;) {
        const unsigned char first = bytes[index++];
        if (first < 0x80U) continue;
        std::size_t continuation{};
        std::uint32_t codepoint{};
        std::uint32_t minimum{};
        if ((first & 0xE0U) == 0xC0U) {
            continuation = 1U; codepoint = first & 0x1FU; minimum = 0x80U;
        } else if ((first & 0xF0U) == 0xE0U) {
            continuation = 2U; codepoint = first & 0x0FU; minimum = 0x800U;
        } else if ((first & 0xF8U) == 0xF0U) {
            continuation = 3U; codepoint = first & 0x07U; minimum = 0x10000U;
        } else {
            return false;
        }
        for (std::size_t offset{}; offset < continuation; ++offset) {
            const unsigned char next = bytes[index++];
            if ((next & 0xC0U) != 0x80U) return false;
            codepoint = (codepoint << 6U) | (next & 0x3FU);
        }
        if (codepoint < minimum || codepoint > 0x10FFFFU
            || (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) return false;
    }
    return true;
}

arpg::test::Failure navigation_element_colors_use_the_authoritative_hud_palette() noexcept {
    platform::NavigationHudModel navigation{};
    navigation.primary.bytes[0] = 'x';
    navigation.element_count = 4U;
    navigation.elements[0].color_id = platform::HudPaletteId::fire;
    navigation.elements[1].color_id = platform::HudPaletteId::water;
    navigation.elements[2].color_id = platform::HudPaletteId::lightning;
    navigation.elements[3].color_id = platform::HudPaletteId::chaos;
    const platform::NavigationPanelPlan plan =
        platform::make_navigation_panel_plan(navigation, player_layout());
    const platform::HudPalette palette = platform::hud_palette();

    ARPG_REQUIRE(same_color(platform::hud_palette_color(plan.elements[0].color_id),
        palette.fire));
    ARPG_REQUIRE(same_color(platform::hud_palette_color(plan.elements[1].color_id),
        palette.water));
    ARPG_REQUIRE(same_color(platform::hud_palette_color(plan.elements[2].color_id),
        palette.lightning));
    ARPG_REQUIRE(same_color(platform::hud_palette_color(plan.elements[3].color_id),
        palette.chaos));
    return {};
}

arpg::test::Failure maximum_navigation_text_has_a_measured_bounded_draw_plan() noexcept {
    platform::HudText96 text{};
    static_cast<void>(std::snprintf(text.bytes.data(), text.bytes.size(),
        u8"深度 18446744073709551615 · 层房间 18446744073709551615"));
    const std::uint64_t before = arpg::test::allocation_count();
    const platform::HudTextDrawPlan plan = platform::make_hud_text_draw_plan(
        text, 270.0F, 16.0F, 11.0F, &monospace_measure, nullptr);
    ARPG_REQUIRE(arpg::test::allocation_count() == before);

    ARPG_REQUIRE(plan.visible);
    ARPG_REQUIRE(plan.font_size >= 11.0F);
    ARPG_REQUIRE(plan.font_size <= 16.0F);
    ARPG_REQUIRE(monospace_measure(plan.text.bytes.data(), plan.font_size, nullptr)
        <= 270.0F);
    ARPG_REQUIRE(plan.truncated || plan.font_size < 16.0F);
    ARPG_REQUIRE(plan.text.bytes.back() == '\0');
    ARPG_REQUIRE(is_valid_utf8(plan.text.bytes.data()));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"health first", &health_is_always_the_first_visible_player_bar},
    {"conditional barrier", &barrier_bar_is_visible_only_with_a_positive_maximum},
    {"XP max", &max_level_experience_uses_the_max_presentation},
    {"low health presentation frequency", &low_health_emphasis_is_presentation_time_bounded_to_two_hz},
    {"three status tags", &player_plan_preserves_at_most_three_snapshot_status_tags},
    {"clamped stable bar bounds", &plan_clamps_ratios_and_keeps_stable_bounds},
    {"nonfinite ratios", &nonfinite_player_ratios_fall_back_to_zero_without_low_health_pulse},
    {"monster snapshot palette world space", &monster_resource_plan_consumes_snapshot_values_and_palette_ids},
    {"objective navigation context plans", &objective_navigation_and_context_plans_stay_in_their_layout_panels},
    {"navigation colors use shared palette", &navigation_element_colors_use_the_authoritative_hud_palette},
    {"navigation maximum text fit", &maximum_navigation_text_has_a_measured_bounded_draw_plan},
};

}  // namespace

arpg::test::TestSuite hud_render_plan_suite() noexcept {
    return arpg::test::make_suite("hud_render_plan", kCases);
}
