#include "hud_font.hpp"

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
        0x77E5, 0x88C5, 0x5907,
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

}  // namespace arpg::platform
