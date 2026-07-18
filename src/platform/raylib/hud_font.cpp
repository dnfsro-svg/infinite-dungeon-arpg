#include "hud_font.hpp"

namespace arpg::platform {

HudFontPlan hud_font_plan() noexcept {
    HudFontPlan plan{};
    plan.shared = death_overlay_font_plan();
    constexpr const char* kRequiredText[] = {
        u8"生命", u8"护盾", u8"剩余", u8"出口已开放", u8"保存失败",
        u8"未分配点", u8"火焰", u8"水", u8"闪电", u8"混沌",
        u8"减速", u8"腐蚀", u8"无敌",
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

}  // namespace arpg::platform
