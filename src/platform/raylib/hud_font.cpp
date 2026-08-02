#include "hud_font.hpp"

#include "ui_typography.hpp"

namespace arpg::platform {
namespace {

void add_hud_codepoint(DeathOverlayFontPlan& plan, int codepoint) noexcept {
    if (death_overlay_font_has_codepoint(plan, codepoint)
            || plan.codepoint_count == plan.codepoints.size()) {
        return;
    }
    plan.codepoints[plan.codepoint_count++] = codepoint;
}

void add_ground_loot_codepoints(DeathOverlayFontPlan& plan) noexcept {
    constexpr int kCodepoints[] = {
        0x666E, 0x901A, 0x9B54, 0x6CD5, 0x7A00,
        0x6709, 0x5DF2, 0x62FE, 0x53D6, 0x672A,
        0x77E5, 0x88C5, 0x5907, 0x836F,
    };
    for (const int codepoint : kCodepoints) {
        add_hud_codepoint(plan, codepoint);
    }
}

void add_active_skill_codepoints(DeathOverlayFontPlan& plan) noexcept {
    constexpr int kCodepoints[] = {
        0x4E3B, 0x52A8, 0x6280, 0x80FD, 0x77F3, 0x69FD,
        0x88C5, 0x5907, 0x6750, 0x6599, 0x8F85, 0x52A9,
        0xFF08, 0x53EA, 0x8BFB, 0xFF09, 0x7A7A, 0x672A,
        0x65E0, 0x53D6, 0x51FA, 0x6B63, 0x5728, 0x4FDD,
        0x5B58, 0x5931, 0x8D25,
        0x80CC, 0x5305, 0x5173, 0x95ED,
        0x62D4, 0x5200, 0x65A9,
        0x6781, 0x00B7, 0x9B3C, 0x5251, 0x672F,
        0x66B4, 0x98CE, 0x5F0F,
    };
    for (const int codepoint : kCodepoints) {
        add_hud_codepoint(plan, codepoint);
    }
}

void add_large_room_hud_codepoints(DeathOverlayFontPlan& plan) noexcept {
    constexpr int kCodepoints[] = {
        0x602A, 0x7FA4, 0x89C4, 0x6A21, 0xFF1A,
        0x62E5, 0x6324, 0x5BC6, 0x96C6, 0x517D, 0x6F6E,
        0x6D88, 0x706D, 0x603B, 0x8BA1, 0xFF08, 0xFF09,
        0x5C1A, 0x5F00, 0x653E, 0xFF0C, 0x6218, 0x6597,
        0x4ECD, 0x53EF, 0x7EE7, 0x7EED, 0x79BB, 0x5C06,
        0x5F03, 0x5269, 0x4F59, 0x5956, 0x52B1,
    };
    for (const int codepoint : kCodepoints) {
        add_hud_codepoint(plan, codepoint);
    }
}

}  // namespace

HudFontPlan hud_font_plan() noexcept {
    HudFontPlan plan{};
    plan.shared = death_overlay_font_plan();
    add_ground_loot_codepoints(plan.shared);
    add_active_skill_codepoints(plan.shared);
    add_large_room_hud_codepoints(plan.shared);
    constexpr const char* kRequiredText[] = {
        u8"生命", u8"护盾", u8"剩余", u8"出口已开放", u8"保存失败",
        u8"未分配点", u8"火焰", u8"水", u8"闪电", u8"混沌",
        u8"减速", u8"腐蚀", u8"无敌", u8"深度", u8"层房间", u8"生态",
        u8"第", u8"波", u8"下一波即将开始", u8"待领奖励", u8"未领取",
        u8"正在保存房间", u8"正在处理撤退", u8"房间状态异常", u8"深渊",
        u8"目标", u8"个敌人", u8"待结算经验",
        u8"需要恢复存档", u8"离开后再次触碰同一出口以放弃全部剩余奖励",
        u8"再次交互，放弃剩余奖励并下降", u8"进入下一层", u8"进入出口",
        u8"房间已清理", u8"奖励", u8"升级至级", u8"有未分配被动点",
        u8"打开背包", u8"打开被动树",
        u8"普通魔法稀有已拾取未知装备",
        u8"装备 / 材料", u8"技能石", u8"主动技能石槽",
        u8"空主技能槽", u8"辅助技能石（只读）", u8"未装备技能石",
        u8"无", u8"取出", u8"正在保存",
        u8"技能石背包关闭",
        u8"拔刀斩", u8"极·鬼剑术（暴风式）",
        u8"怪群规模：拥挤密集兽潮",
        u8"消灭（总计）剩余",
        u8"出口尚未开放，战斗仍可继续",
        u8"离开将放弃剩余奖励",
    };
    plan.covers_required_text = true;
    for (const char* text : kRequiredText) {
        if (!death_overlay_font_covers_text(plan.shared, text)) {
            plan.covers_required_text = false;
            break;
        }
    }
    return plan;
}

HudFontDrawMode hud_font_draw_mode(bool cjk_font_ready) noexcept {
    return cjk_font_ready ? HudFontDrawMode::cjk_ready
                          : HudFontDrawMode::fallback;
}

HudFontSelectionPlan make_hud_font_selection_plan(bool cjk_font_ready) noexcept {
    const HudFontDrawMode mode = hud_font_draw_mode(cjk_font_ready);
    return {mode, mode == HudFontDrawMode::fallback,
        mode == HudFontDrawMode::cjk_ready};
}

CombatTextStyle combat_text_style(
    int screen_width, int screen_height) noexcept {
    const float scale = ui_viewport_scale(screen_width, screen_height);
    return {20.0F * scale, 24.0F * scale, 1.0F};
}

}  // namespace arpg::platform
