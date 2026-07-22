#pragma once

#include <raylib.h>

#include <algorithm>

namespace arpg::platform {

struct UiTextContrastStyle final {
    Color primary{248, 246, 238, 255};
    Color secondary{194, 229, 255, 255};
    Color interaction{194, 229, 255, 255};
    Color muted{184, 204, 220, 255};
    Color warning{255, 210, 118, 255};
    Color danger{255, 152, 152, 255};
    Color backing{5, 9, 16, 232};
    Color shadow{1, 3, 7, 248};
    int outline_pixels{1};
    int shadow_pixels{2};
};

[[nodiscard]] constexpr UiTextContrastStyle
ui_text_contrast_style() noexcept {
    return {};
}

[[nodiscard]] constexpr float ui_perceived_luma(Color color) noexcept {
    return (0.2126F * static_cast<float>(color.r))
        + (0.7152F * static_cast<float>(color.g))
        + (0.0722F * static_cast<float>(color.b));
}

// A deterministic display-luma contrast guard for the raster UI.  The 12.75
// offset is the 8-bit form of the contrast-ratio 0.05 flare term.
[[nodiscard]] constexpr float ui_luma_contrast_ratio(
    Color first, Color second) noexcept {
    const float first_luma = ui_perceived_luma(first);
    const float second_luma = ui_perceived_luma(second);
    const float lighter = first_luma > second_luma ? first_luma : second_luma;
    const float darker = first_luma > second_luma ? second_luma : first_luma;
    return (lighter + 12.75F) / (darker + 12.75F);
}

}  // namespace arpg::platform
