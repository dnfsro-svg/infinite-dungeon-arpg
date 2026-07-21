#include "test_framework.hpp"

#include "death_overlay_font.hpp"
#include "hud_font.hpp"
#include "hud_palette.hpp"
#include "hud_renderer.hpp"
#include "hud_view_model.hpp"

#include <cstddef>
#include <cstdio>

namespace {

namespace platform = arpg::platform;

bool same_color(Color lhs, Color rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b
        && lhs.a == rhs.a;
}

[[nodiscard]] bool model_texts_are_covered(const platform::HudFontPlan& plan,
    const platform::HudViewModel& model) noexcept {
    const platform::HudText96* const texts[] = {
        &model.room.objective, &model.room.secondary,
        &model.room.abyss_effect, &model.room.abyss_rewards,
        &model.navigation.primary, &model.navigation.ecology_label,
        &model.navigation.elements[0].label, &model.navigation.elements[1].label,
        &model.navigation.elements[2].label, &model.navigation.elements[3].label,
    };
    for (const platform::HudText96* const text : texts) {
        if (text->bytes[0] != '\0'
            && !platform::death_overlay_font_covers_text(plan.shared,
                text->bytes.data())) {
            return false;
        }
    }
    return true;
}

arpg::test::Failure production_view_model_texts_and_player_labels_are_covered() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    platform::ControlHints hints{};
    static_cast<void>(std::snprintf(hints.primary.data(), hints.primary.size(),
        "W Move Up"));
    static_cast<void>(std::snprintf(hints.secondary.data(), hints.secondary.size(),
        "F Interact"));
    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.depth = 9U;
    snapshot.floor_room_index = 7U;
    snapshot.ecology = arpg::dungeon::DungeonElement::chaos;
    snapshot.biases = {{1U, 2U, 3U, 4U}};
    snapshot.remaining_targets = 6U;
    snapshot.pending_room_experience = 99U;
    snapshot.wave_count = 3U;
    snapshot.wave_index = 1U;
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 1;
    snapshot.combat->player.max_hp = 1;
    snapshot.combat->player.barrier = 1;
    snapshot.combat->player.max_barrier = 1;
    snapshot.combat->player.slow_bp = 1;
    snapshot.combat->player.slow_ticks = 1U;
    snapshot.combat->player.corrosion_damage_per_second = 1;
    snapshot.combat->player.corrosion_ticks = 1U;
    snapshot.combat->player.invulnerability_ticks = 1U;

    const arpg::dungeon::RoomPhase phases[] = {
        arpg::dungeon::RoomPhase::locked,
        arpg::dungeon::RoomPhase::combat,
        arpg::dungeon::RoomPhase::wave_delay,
        arpg::dungeon::RoomPhase::cleared,
        arpg::dungeon::RoomPhase::committing,
        arpg::dungeon::RoomPhase::death_pending,
        arpg::dungeon::RoomPhase::faulted,
    };
    for (const arpg::dungeon::RoomPhase phase : phases) {
        snapshot.phase = phase;
        platform::HudViewModel model{};
        platform::build_hud_view_model(model, snapshot, {}, hints);
        ARPG_REQUIRE(model_texts_are_covered(plan, model));
    }

    snapshot.is_abyss = true;
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::abyss_fury;
    platform::HudViewModel abyss{};
    platform::build_hud_view_model(abyss, snapshot, {}, hints);
    ARPG_REQUIRE(model_texts_are_covered(plan, abyss));

    constexpr const char* kRendererLabels[] = {
        u8"生命 HP 1/1", u8"护盾 1/1", "XP MAX",
        u8"减速", u8"腐蚀", u8"无敌",
    };
    for (const char* text : kRendererLabels) {
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(plan.shared, text));
    }
    ARPG_REQUIRE(plan.shared.codepoint_count < plan.shared.codepoints.size());
    return {};
}

arpg::test::Failure required_hud_text_is_covered_by_shared_font_plan() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    constexpr const char* kRequiredText[] = {
        u8"生命", u8"护盾", u8"剩余", u8"出口已开放", u8"保存失败",
        u8"未分配点", u8"火焰", u8"水", u8"闪电", u8"混沌",
        u8"减速", u8"腐蚀", u8"无敌", u8"拔刀斩",
        u8"极·鬼剑术（暴风式）",
    };

    ARPG_REQUIRE(plan.covers_required_text);
    for (const char* text : kRequiredText) {
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(plan.shared, text));
    }
    return {};
}

arpg::test::Failure ground_loot_labels_are_covered_by_the_hud_owned_font() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    constexpr const char* kGroundLootText =
        u8"普通魔法稀有已拾取未知装备";

    ARPG_REQUIRE(plan.covers_required_text);
    ARPG_REQUIRE(platform::death_overlay_font_covers_text(
        plan.shared, kGroundLootText));
    return {};
}

arpg::test::Failure task6_visible_chinese_text_is_covered_without_exhausting_shared_capacity() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    constexpr const char* kTask6Text[] = {
        u8"深度", u8"层房间", u8"生态", u8"第", u8"波", u8"下一波即将开始",
        u8"待领奖励", u8"未领取", u8"规则", u8"正在保存房间", u8"正在处理撤退",
        u8"房间状态异常", u8"深渊", u8"火", u8"水", u8"电", u8"混沌",
        u8"拥挤", u8"密集", u8"兽潮", u8"怪物",
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
    {"ground loot Chinese coverage",
        &ground_loot_labels_are_covered_by_the_hud_owned_font},
    {"Task6 Chinese coverage has capacity", &task6_visible_chinese_text_is_covered_without_exhausting_shared_capacity},
    {"production ViewModel text coverage", &production_view_model_texts_and_player_labels_are_covered},
    {"fixed unique shared codepoints", &shared_codepoints_are_unique_and_fixed_capacity},
    {"opaque distinct HUD palette", &hud_palette_key_colors_are_opaque_and_distinct},
    {"safe uninitialized renderer shutdown", &renderer_shutdown_is_safe_before_initialization},
};

}  // namespace

arpg::test::TestSuite hud_font_suite() noexcept {
    return arpg::test::make_suite("hud_font", kCases);
}
