#include "death_overlay_font.hpp"

namespace arpg::platform {

DeathOverlayFontPlan death_overlay_font_plan() noexcept {
    return ui_glyph_plan();
}

bool death_overlay_font_has_codepoint(const DeathOverlayFontPlan& plan,
    int codepoint) noexcept {
    return ui_glyph_plan_has_codepoint(plan, codepoint);
}

bool death_overlay_font_covers_text(const DeathOverlayFontPlan& plan,
    const char* utf8_text) noexcept {
    return ui_glyph_plan_covers_text(plan, utf8_text);
}

}  // namespace arpg::platform
