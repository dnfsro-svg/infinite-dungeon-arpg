#include "test_framework.hpp"

#include "death_overlay_font.hpp"
#include "death_overlay_renderer.hpp"
#include "dungeon/dungeon_render_snapshot.hpp"
#include "hud_font.hpp"
#include "hud_palette.hpp"
#include "hud_renderer.hpp"
#include "hud_view_model.hpp"
#include "inventory_view_math.hpp"
#include "passive_tree_renderer.hpp"
#include "passive_tree_view_math.hpp"
#include "passives/passive_tree_catalog.hpp"
#include "pause_menu_renderer.hpp"
#include "shared_ui_font.hpp"
#include "ui_text_contrast.hpp"
#include "ui_typography.hpp"

#include <cstddef>
#include <cstring>
#include <cstdio>

namespace {

namespace platform = arpg::platform;
namespace settings = arpg::settings;

bool same_color(Color lhs, Color rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b
        && lhs.a == rhs.a;
}

[[nodiscard]] bool model_texts_are_covered(const platform::HudFontPlan& plan,
    const platform::HudViewModel& model) noexcept {
    const platform::HudText96* const texts[] = {
        &model.room.objective, &model.room.secondary,
        &model.room.movement, &model.room.controls[0],
        &model.room.controls[1], &model.room.controls[2],
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
    platform::refresh_control_hints(hints, settings::default_settings());
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
        u8"生命 1/1", u8"护盾 1/1", u8"经验已满",
        u8"等级 100 · 未分配点 0", u8"小键盘5", u8"30秒",
        u8"减速", u8"腐蚀", u8"无敌",
    };
    for (const char* text : kRendererLabels) {
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(plan.shared, text));
    }
    ARPG_REQUIRE(plan.shared.codepoint_count < plan.shared.codepoints.size());
    return {};
}

arpg::test::Failure visible_set_model_text_is_covered_by_shared_font_plan()
    noexcept {
    arpg::dungeon::DungeonSnapshot snapshot{};
    arpg::dungeon::DungeonRenderSnapshot presented{};
    presented.query.camera_version = 701U;
    presented.has_combat = true;
    presented.combat.monster_count = 105U;
    presented.environment.count = 73U;
    presented.environment.candidates_examined = 105U;
    presented.equipment_count = 19U;
    presented.material_count = 23U;
    presented.health_potion_count = 11U;
    presented.drop_candidates_examined = 331U;

    platform::HudViewModelProjector projector{};
    platform::HudViewModel model{};
    projector.build(model, snapshot, {}, {}, &presented);
    ARPG_REQUIRE(std::strcmp(model.room.visible_set.text.bytes.data(),
        u8"可见：怪 105/105 环境 73/105 掉落 53/331") == 0);

    const platform::HudFontPlan plan = platform::hud_font_plan();
    ARPG_REQUIRE(platform::death_overlay_font_covers_text(
        plan.shared, model.room.visible_set.text.bytes.data()));
    return {};
}

arpg::test::Failure required_hud_text_is_covered_by_shared_font_plan() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    constexpr const char* kRequiredText[] = {
        u8"生命", u8"护盾", u8"剩余", u8"出口已开放", u8"保存失败",
        u8"未分配点", u8"火焰", u8"水", u8"闪电", u8"混沌",
        u8"减速", u8"腐蚀", u8"无敌", u8"拔刀斩",
        u8"极·鬼剑术（暴风式）",
        u8"按键提示：右 Shift / ESC  ·  装备背包",
        u8"装备  背包  物品详情",
        u8"装备槽：全部  稀有度：稀有  三合一（3/3）",
        u8"过滤：无",
        u8"基础 [局部] 武器物理伤害 +16",
        u8"3级 前缀 锋利 [全局] 火焰伤害 +12% 提高",
        u8"最终近战伤害 ×1.12",
        u8"全部装备变化 预览不可用",
        u8"正在保存，已锁定",
        u8"空中移动 最终空中移动",
        u8"危险：三合一取消强化",
        u8"已选择：嬗变石 · 选择物品应用（右键取消）",
    };

    ARPG_REQUIRE(plan.covers_required_text);
    for (const char* text : kRequiredText) {
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(plan.shared, text));
    }
    for (std::uint8_t raw{};
         raw < static_cast<std::uint8_t>(arpg::modifiers::StatId::count);
         ++raw) {
        const auto stat = static_cast<arpg::modifiers::StatId>(raw);
        const platform::ItemAttributeLabel normal =
            platform::item_attribute_label(
                arpg::items::ItemEffectKind::global_modifier, stat,
                arpg::modifiers::ModifierOperation::flat,
                arpg::items::ItemSlot::weapon, 0xFFU);
        const platform::ItemAttributeLabel final =
            platform::item_attribute_label(
                arpg::items::ItemEffectKind::global_modifier, stat,
                arpg::modifiers::ModifierOperation::more,
                arpg::items::ItemSlot::weapon, 0xFFU);
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(
            plan.shared, normal.name));
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(
            plan.shared, final.name));
    }
    return {};
}

arpg::test::Failure passive_tree_chinese_text_is_covered_by_shared_font_plan()
    noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    constexpr const char* kPassiveTreeText[] = {
        u8"被动星图", u8"未分配点：7", u8"P 关闭",
        u8"起始 / 状态：锁定", u8"连接 / 状态：可分配",
        u8"小节点 / 状态：已分配", u8"重要节点 / 状态：保存中",
        u8"核心 / 状态：锁定",
        u8"收益：最终火焰伤害 ×1.30",
        u8"收益：最终水流伤害 ×1.25，+24 最大护盾",
        u8"收益：最终闪电伤害 ×1.25，移动速度/攻击速度 +12%",
        u8"收益：最终混沌伤害 ×1.30",
        u8"代价：-20% 水流减伤",
        u8"代价：最终近战伤害 ×0.85",
        u8"代价：最终最大生命 ×0.80",
        u8"代价：最终承受伤害 ×1.15",
        u8"点击分配并保存", u8"点击退款并保存",
        u8"请分配相邻节点", u8"退款错误：将断链",
        u8"保存状态：保存中", u8"保存状态：就绪",
        u8"保存状态：错误",
    };
    for (const char* text : kPassiveTreeText) {
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(
            plan.shared, text));
    }
    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = arpg::dungeon::RoomPhase::awaiting_exit;
    snapshot.progression.unspent_passive_points = 1U;
    snapshot.passive_tree.allocated_bits = 1ULL;
    for (const arpg::passives::PassiveNode& node
        : arpg::passives::passive_nodes()) {
        const auto text = platform::passive_node_text_view(
            snapshot, node.id);
        ARPG_REQUIRE(text.valid);
        for (const char* value : {text.name.data(), text.type_state.data(),
                 text.benefit.data(), text.cost.data(), text.action.data()}) {
            ARPG_REQUIRE(platform::death_overlay_font_covers_text(
                plan.shared, value));
        }
    }
    ARPG_REQUIRE(plan.shared.codepoint_count == 644U);
    ARPG_REQUIRE(plan.shared.codepoint_count < platform::kUiGlyphCapacity);
    return {};
}

arpg::test::Failure passive_tree_text_selects_shared_font_when_ready() noexcept {
    ARPG_REQUIRE(platform::passive_tree_text_draw_mode(true)
        == platform::PassiveTreeTextDrawMode::hud_font);
    ARPG_REQUIRE(platform::passive_tree_text_draw_mode(false)
        == platform::PassiveTreeTextDrawMode::unavailable);
    return {};
}

arpg::test::Failure ground_loot_labels_are_covered_by_the_hud_owned_font() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    constexpr const char* kGroundLootText =
        u8"普通魔法稀有已拾取未知装备生命药恢复物品等级"
        u8"嬗变石增幅石富豪石混沌石崇高石剥离石神圣石重铸石"
        u8"定向核心强化石强化券材料袋伤害核心防御核心速度核心元素核心"
        u8"危险：强化失败，装备丢失确认强化石？确认取消";

    ARPG_REQUIRE(plan.covers_required_text);
    ARPG_REQUIRE(platform::death_overlay_font_has_codepoint(
        plan.shared, 0x836F));
    ARPG_REQUIRE(platform::death_overlay_font_covers_text(
        plan.shared, kGroundLootText));
    ARPG_REQUIRE(!platform::ui_glyph_plan_has_codepoint(
        plan.shared, 0x8715));
    ARPG_REQUIRE(platform::ui_glyph_plan_has_codepoint(
        plan.shared, 0x888B));
    ARPG_REQUIRE(plan.shared.codepoint_count < platform::kUiGlyphCapacity);
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

arpg::test::Failure bundled_noto_sans_sc_is_the_required_runtime_font() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    ARPG_REQUIRE(plan.shared.candidate_count == 1U);
    ARPG_REQUIRE(plan.shared.candidate_paths[0] != nullptr);
    ARPG_REQUIRE(std::strcmp(plan.shared.candidate_paths[0],
        "assets/fonts/NotoSansCJKsc-Medium.otf") == 0);
    ARPG_REQUIRE(plan.shared.codepoint_count < plan.shared.codepoints.size());
    constexpr const char* kReadableUiCorpus[] = {
        u8"装备背包 技能石 暂停 继续 设置 退出游戏",
        u8"生命 护盾 目标 深度 生态 火焰 水 闪电 混沌",
        u8"装备 背包 物品详情 材料",
        u8"方向键导航 Enter 选择 Esc 返回",
    };
    for (const char* text : kReadableUiCorpus) {
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(
            plan.shared, text));
    }
    return {};
}

arpg::test::Failure ui_text_uses_one_solid_foreground_without_fake_bold() noexcept {
    const platform::UiTextContrastStyle contrast =
        platform::ui_text_contrast_style();
    const platform::HudReadabilityStyle readability =
        platform::hud_readability_style();
    ARPG_REQUIRE(contrast.outline_pixels == 0);
    ARPG_REQUIRE(readability.outline_pixels == 0);
    ARPG_REQUIRE(readability.embolden_pixels == 0);
    ARPG_REQUIRE(contrast.primary.a == 255U);
    ARPG_REQUIRE(contrast.secondary.a == 255U);
    const auto inventory = platform::inventory_control_text_style();
    const Color kInventoryTextColors[] = {
        contrast.primary,
        contrast.secondary,
        contrast.interaction,
        contrast.muted,
        contrast.warning,
        contrast.danger,
    };
    ARPG_REQUIRE(inventory.content_backing.a == 255U);
    for (const Color color : kInventoryTextColors) {
        ARPG_REQUIRE(color.a == 255U);
        ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
            color, inventory.content_backing) >= 4.5F);
    }
    struct ControlState final {
        Color text{};
        Color backing{};
    };
    const ControlState states[] = {
        {contrast.primary, inventory.idle_backing},
        {contrast.interaction, inventory.active_backing},
        {contrast.muted, inventory.disabled_backing},
        {contrast.interaction, inventory.remove_backing},
    };
    for (const ControlState state : states) {
        ARPG_REQUIRE(state.text.a == 255U);
        ARPG_REQUIRE(state.backing.a == 255U);
        ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
            state.text, state.backing) >= 4.5F);
    }
    return {};
}

arpg::test::Failure ui_typography_scales_to_physical_full_hd_pixels() noexcept {
    ARPG_REQUIRE(arpg::test::near(platform::ui_viewport_scale(1280, 720), 1.0F));
    ARPG_REQUIRE(arpg::test::near(platform::ui_viewport_scale(1920, 1080), 1.5F));
    ARPG_REQUIRE(arpg::test::near(platform::ui_viewport_scale(3840, 2160), 1.5F));
    ARPG_REQUIRE(arpg::test::near(platform::scaled_ui_font_size(
        platform::ui_typography().kInventoryBodyFontSize, 1920, 1080),
        24.0F));
    ARPG_REQUIRE(arpg::test::near(platform::scaled_ui_font_size(
        platform::ui_typography().kPauseRowFontSize, 1920, 1080),
        27.0F));
    return {};
}

arpg::test::Failure bundled_font_uses_bounded_high_resolution_glyph_atlas() noexcept {
    static_assert(platform::kUiFontAtlasInstanceCount == 1U);
    static_assert(platform::kUiFontSourceBaseSize >= 96);
    static_assert(platform::kUiFontMaximumDisplaySize >= 39);
    static_assert(platform::kUiFontSourceBaseSize
        >= platform::kUiFontMaximumDisplaySize * 2);
    static_assert(platform::kUiFontAtlasByteBudget > 0U);
    static_assert(platform::kUiFontAtlasByteBudget <= 32U * 1024U * 1024U);
    static_assert(platform::kDeathOverlayCodepointCapacity
        == platform::kUiGlyphCapacity);
    const platform::DeathOverlayFontPlan plan =
        platform::death_overlay_font_plan();
    ARPG_REQUIRE(plan.codepoint_count <= platform::kDeathOverlayCodepointCapacity);
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
    ARPG_REQUIRE(platform::death_overlay_text_draw_mode(true)
        == platform::DeathOverlayTextDrawMode::shared_ui_font);
    ARPG_REQUIRE(platform::death_overlay_text_draw_mode(false)
        == platform::DeathOverlayTextDrawMode::unavailable);

    platform::SharedUiFont shared{};
    platform::HudRenderer renderer{};
    ARPG_REQUIRE(!renderer.font_ready());
    ARPG_REQUIRE(!shared.ready());
    ARPG_REQUIRE(shared.atlas_bytes() == 0U);
    ARPG_REQUIRE(!renderer.initialize(shared));
    renderer.shutdown();
    renderer.shutdown();
    ARPG_REQUIRE(!renderer.font_ready());
    ARPG_REQUIRE(!shared.ready());
    renderer.draw({}, {});

    platform::DeathOverlayRenderer death{};
    platform::PauseMenuRenderer pause{};
    ARPG_REQUIRE(!death.initialize(shared));
    ARPG_REQUIRE(!pause.initialize(shared));
    death.shutdown();
    pause.shutdown();
    ARPG_REQUIRE(!shared.ready());
    shared.shutdown();
    shared.shutdown();
    return {};
}

arpg::test::Failure dungeon_interaction_chinese_text_is_covered_by_shared_font_plan() noexcept {
    const platform::HudFontPlan plan = platform::hud_font_plan();
    constexpr const char* kTask5Text[] = {
        u8"火焰水流闪电混沌未知",
        u8"低危中危高危",
        u8"雷暴猎杀之焰混沌扩张迅捷追猎深渊壁垒深渊狂怒沉重脚步疲惫恢复生命献祭无规则",
        u8"每180帧：预警45帧/半径0.8，造成15%最大生命闪电伤害",
        u8"每240帧：预警45帧/半径1.0，灼烧180帧，每60帧造成10%最大生命伤害",
        u8"每180帧扩张至半径1.0/2.3/3.6/4.9/6.2，每60帧造成8%最大生命混沌伤害",
        u8"怪物：移动速度+15%，冷却时间×85%",
        u8"怪物：护甲×130%，护盾+30%最大生命",
        u8"怪物：伤害与攻击速度×145%",
        u8"玩家：地面移动速度×85%",
        u8"玩家：入场资源及后续恢复×70%",
        u8"玩家：最大生命×55%",
        u8"封闭可进入保存中异常",
        u8"已保存已恢复存档错误",
        u8"锁定战斗中等待下一波已清理等待离开保存中切换房间处理撤退中异常",
        u8"上方下方左方右方无方向",
        u8"Q：下潜",
        u8"Q：进入下一层",
        u8"离开后再次触碰同一出口以放弃全部剩余奖励",
        u8"再次交互，放弃剩余奖励并下潜",
    };
    ARPG_REQUIRE(plan.shared.codepoint_count < plan.shared.codepoints.size());
    for (const char* text : kTask5Text) {
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(plan.shared, text));
    }
    return {};
}

arpg::test::Failure combat_text_has_shared_font_coverage_and_scaled_sizes() noexcept {
    const platform::HudFontPlan font = platform::hud_font_plan();
    constexpr const char* kCombatText = "0123456789-+%DEFEATED";
    ARPG_REQUIRE(platform::death_overlay_font_covers_text(
        font.shared, kCombatText));

    const platform::CombatTextStyle compact =
        platform::combat_text_style(800, 450);
    const platform::CombatTextStyle full_hd =
        platform::combat_text_style(1920, 1080);
    ARPG_REQUIRE(compact.damage_font_size > 0.0F);
    ARPG_REQUIRE(compact.defeated_font_size > compact.damage_font_size);
    ARPG_REQUIRE(arpg::test::near(compact.spacing, 1.0F));
    ARPG_REQUIRE(full_hd.damage_font_size > compact.damage_font_size);
    ARPG_REQUIRE(full_hd.defeated_font_size > compact.defeated_font_size);
    ARPG_REQUIRE(arpg::test::near(full_hd.spacing, 1.0F));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"required Chinese coverage", &required_hud_text_is_covered_by_shared_font_plan},
    {"passive tree Chinese coverage",
        &passive_tree_chinese_text_is_covered_by_shared_font_plan},
    {"passive tree shared font selection",
        &passive_tree_text_selects_shared_font_when_ready},
    {"ground loot Chinese coverage",
        &ground_loot_labels_are_covered_by_the_hud_owned_font},
    {"Task6 Chinese coverage has capacity", &task6_visible_chinese_text_is_covered_without_exhausting_shared_capacity},
    {"dungeon interaction Chinese coverage",
        &dungeon_interaction_chinese_text_is_covered_by_shared_font_plan},
    {"production ViewModel text coverage", &production_view_model_texts_and_player_labels_are_covered},
    {"visible-set ViewModel shared font coverage",
        &visible_set_model_text_is_covered_by_shared_font_plan},
    {"fixed unique shared codepoints", &shared_codepoints_are_unique_and_fixed_capacity},
    {"bundled Noto Sans SC runtime font",
        &bundled_noto_sans_sc_is_the_required_runtime_font},
    {"bounded high-resolution glyph atlas",
        &bundled_font_uses_bounded_high_resolution_glyph_atlas},
    {"single solid text foreground",
        &ui_text_uses_one_solid_foreground_without_fake_bold},
    {"full-HD physical typography scaling",
        &ui_typography_scales_to_physical_full_hd_pixels},
    {"opaque distinct HUD palette", &hud_palette_key_colors_are_opaque_and_distinct},
    {"safe uninitialized renderer shutdown", &renderer_shutdown_is_safe_before_initialization},
    {"combat text font coverage and scaling",
        &combat_text_has_shared_font_coverage_and_scaled_sizes},
};

}  // namespace

arpg::test::TestSuite hud_font_suite() noexcept {
    return arpg::test::make_suite("hud_font", kCases);
}
