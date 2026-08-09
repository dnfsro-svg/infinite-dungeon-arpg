#include "hud_font.hpp"

#include "ui_typography.hpp"

namespace arpg::platform {
HudFontPlan hud_font_plan() noexcept {
    HudFontPlan plan{};
    plan.shared = ui_glyph_plan();
    plan.covers_required_text = !plan.shared.capacity_exceeded;
    return plan;
}

HudFontDrawMode hud_font_draw_mode(bool cjk_font_ready) noexcept {
    return cjk_font_ready ? HudFontDrawMode::cjk_ready
                          : HudFontDrawMode::fallback;
}

HudFontSelectionPlan make_hud_font_selection_plan(bool cjk_font_ready) noexcept {
    const HudFontDrawMode mode = hud_font_draw_mode(cjk_font_ready);
    return {mode, mode == HudFontDrawMode::fallback, false};
}

CombatTextStyle combat_text_style(
    int screen_width, int screen_height) noexcept {
    const float scale = ui_viewport_scale(screen_width, screen_height);
    return {20.0F * scale, 24.0F * scale, 1.0F};
}

}  // namespace arpg::platform
