#include "test_framework.hpp"

#include "hud_renderer.hpp"

#include <array>

namespace {

namespace platform = arpg::platform;

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

arpg::test::Failure monster_resource_bars_share_palette_ids_and_remain_world_space() noexcept {
    const platform::MonsterBarVisualPlan plan =
        platform::monster_bar_visual_plan();

    ARPG_REQUIRE(plan.world_space);
    ARPG_REQUIRE(plan.bar_count == 3U);
    ARPG_REQUIRE(plan.palette_ids[0] == platform::HudPaletteId::health);
    ARPG_REQUIRE(plan.palette_ids[1] == platform::HudPaletteId::barrier);
    ARPG_REQUIRE(plan.palette_ids[2] == platform::HudPaletteId::experience);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"health first", &health_is_always_the_first_visible_player_bar},
    {"conditional barrier", &barrier_bar_is_visible_only_with_a_positive_maximum},
    {"XP max", &max_level_experience_uses_the_max_presentation},
    {"low health presentation frequency", &low_health_emphasis_is_presentation_time_bounded_to_two_hz},
    {"three status tags", &player_plan_preserves_at_most_three_snapshot_status_tags},
    {"clamped stable bar bounds", &plan_clamps_ratios_and_keeps_stable_bounds},
    {"monster palette world space", &monster_resource_bars_share_palette_ids_and_remain_world_space},
};

}  // namespace

arpg::test::TestSuite hud_render_plan_suite() noexcept {
    return arpg::test::make_suite("hud_render_plan", kCases);
}
