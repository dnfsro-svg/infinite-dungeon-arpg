#include "hud_font.hpp"

namespace arpg::platform {

HudFontPlan hud_font_plan() noexcept {
    HudFontPlan plan{};
    plan.shared = death_overlay_font_plan();
    constexpr const char* kRequiredText[] = {
        u8"生命", u8"护盾", u8"剩余", u8"出口已开放", u8"保存失败",
        u8"未分配点", u8"火焰", u8"水", u8"闪电", u8"混沌",
        u8"减速", u8"腐蚀", u8"无敌", u8"深度", u8"层房间", u8"生态",
        u8"第", u8"波", u8"下一波即将开始", u8"待领奖励", u8"未领取",
        u8"正在保存房间", u8"正在处理撤退", u8"房间状态异常", u8"深渊",
        u8"目标", u8"个敌人", u8"待结算经验",
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
