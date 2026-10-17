#pragma once

namespace arpg::platform {

inline constexpr float kUiReferenceWidth = 1280.0F;
inline constexpr float kUiReferenceHeight = 720.0F;
inline constexpr float kUiMaximumViewportScale = 1.5F;

[[nodiscard]] constexpr float ui_viewport_scale(
    int width, int height) noexcept {
    if (width <= 0 || height <= 0) return 1.0F;
    const float width_scale = static_cast<float>(width) / kUiReferenceWidth;
    const float height_scale = static_cast<float>(height) / kUiReferenceHeight;
    const float scale = width_scale < height_scale ? width_scale : height_scale;
    if (scale < 1.0F) return 1.0F;
    return scale > kUiMaximumViewportScale
        ? kUiMaximumViewportScale : scale;
}

[[nodiscard]] constexpr float scaled_ui_font_size(
    float base_size, int width, int height) noexcept {
    return base_size * ui_viewport_scale(width, height);
}

struct UiTypography final {
    float kHudSkillNameFontSize{16.0F};
    float kInventoryBodyFontSize{16.0F};
    float kInventoryDetailFontSize{16.0F};
    float kSkillDescriptionFontSize{18.0F};
    float kPauseRowFontSize{18.0F};
    float page_title_font_size{24.0F};
    float panel_title_font_size{20.0F};
    float inventory_tab_font_size{18.0F};
    float pause_footer_font_size{18.0F};
};

[[nodiscard]] constexpr UiTypography ui_typography() noexcept {
    return {};
}

}  // namespace arpg::platform
