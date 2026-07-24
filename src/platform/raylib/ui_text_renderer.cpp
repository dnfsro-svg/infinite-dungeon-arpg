#include "ui_text_renderer.hpp"

#include "ui_text_contrast.hpp"

#include <cmath>

namespace arpg::platform {

Vector2 snap_ui_text_position(Vector2 position) noexcept {
    return {std::round(position.x), std::round(position.y)};
}

void draw_crisp_ui_text(Font font, const char* text, Vector2 position,
    float font_size, float spacing, Color color) noexcept {
    if (!IsFontValid(font) || text == nullptr || text[0] == '\0'
            || !(font_size > 0.0F)) {
        return;
    }
    const UiTextContrastStyle style = ui_text_contrast_style();
    color.a = 255U;
    if (ui_luma_contrast_ratio(color, style.backing) < 4.5F) {
        color = style.muted;
    }
    const Vector2 foreground = snap_ui_text_position(position);
    const Vector2 shadow{
        foreground.x + static_cast<float>(style.shadow_pixels),
        foreground.y + static_cast<float>(style.shadow_pixels),
    };
    DrawTextEx(font, text, shadow, font_size, spacing, style.shadow);
    DrawTextEx(font, text, foreground, font_size, spacing, color);
}

}  // namespace arpg::platform
