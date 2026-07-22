#include "hud_layout.hpp"

#include <algorithm>

namespace arpg::platform {
namespace {

constexpr float kLogicalWidth = 1280.0F;
constexpr float kLogicalHeight = 720.0F;
constexpr float kMinimumScale = 0.8F;
constexpr float kMaximumScale = 1.5F;
constexpr float kLogicalMargin = 16.0F;
constexpr float kPlayerWidth = 300.0F;
constexpr float kPlayerHeight = 144.0F;
constexpr float kObjectiveWidth = 340.0F;
constexpr float kObjectiveHeight = 72.0F;
constexpr float kNavigationWidth = 270.0F;
constexpr float kNavigationHeight = 116.0F;
constexpr float kPrimaryNoticeWidth = 600.0F;
constexpr float kPrimaryNoticeHeight = 36.0F;
constexpr float kSecondaryNoticeHeight = 28.0F;
constexpr float kNoticeGap = 6.0F;
constexpr float kAbyssConfirmationHeight = 56.0F;
constexpr float kAbyssConfirmationGap = 8.0F;
constexpr float kCombatExclusionX = 360.0F;
constexpr float kCombatExclusionY = 148.0F;
constexpr float kCombatExclusionWidth = 560.0F;
constexpr float kCombatExclusionHeight = 310.0F;
constexpr float kDebugWidth = 300.0F;
constexpr float kDebugY = 172.0F;
constexpr float kDebugHeight = 228.0F;

[[nodiscard]] HudRect scaled_rect(float origin_x,
    float origin_y,
    float width,
    float height,
    float scale) noexcept {
    return {origin_x, origin_y, width * scale, height * scale};
}

[[nodiscard]] bool has_area(HudRect rect) noexcept {
    return rect.width > 0.0F && rect.height > 0.0F;
}

}  // namespace

HudLayout make_hud_layout(
    int screen_width, int screen_height, bool debug_visible) noexcept {
    if (screen_width <= 0 || screen_height <= 0) {
        HudLayout empty{};
        empty.scale = 0.0F;
        return empty;
    }

    const float width = static_cast<float>(screen_width);
    const float height = static_cast<float>(screen_height);
    const float scale = std::clamp(std::min(width / kLogicalWidth,
        height / kLogicalHeight), kMinimumScale, kMaximumScale);
    const float canvas_x = (width - (kLogicalWidth * scale)) * 0.5F;
    const float canvas_y = (height - (kLogicalHeight * scale)) * 0.5F;
    const float margin = kLogicalMargin * scale;
    const HudRect safe_area{
        canvas_x + margin,
        canvas_y + margin,
        (kLogicalWidth * scale) - (margin * 2.0F),
        (kLogicalHeight * scale) - (margin * 2.0F),
    };
    const float confirmation_y = safe_area.y + safe_area.height
        - (kAbyssConfirmationHeight * scale);
    const float primary_notice_y = confirmation_y
        - (kAbyssConfirmationGap * scale) - (kPrimaryNoticeHeight * scale);
    const float primary_notice_x = canvas_x
        + ((kLogicalWidth - kPrimaryNoticeWidth) * 0.5F * scale);

    HudLayout layout{};
    layout.scale = scale;
    layout.safe_area = safe_area;
    layout.player_panel = scaled_rect(safe_area.x,
        safe_area.y + safe_area.height - (kPlayerHeight * scale),
        kPlayerWidth, kPlayerHeight, scale);
    layout.objective_panel = scaled_rect(canvas_x
            + ((kLogicalWidth - kObjectiveWidth) * 0.5F * scale),
        safe_area.y, kObjectiveWidth, kObjectiveHeight, scale);
    layout.navigation_panel = scaled_rect(safe_area.x + safe_area.width
            - (kNavigationWidth * scale),
        safe_area.y, kNavigationWidth, kNavigationHeight, scale);
    layout.primary_notice = scaled_rect(primary_notice_x, primary_notice_y,
        kPrimaryNoticeWidth, kPrimaryNoticeHeight, scale);
    layout.secondary_notice = scaled_rect(primary_notice_x,
        primary_notice_y - ((kNoticeGap + kSecondaryNoticeHeight) * scale),
        kPrimaryNoticeWidth, kSecondaryNoticeHeight, scale);
    layout.combat_exclusion = scaled_rect(canvas_x + (kCombatExclusionX * scale),
        canvas_y + (kCombatExclusionY * scale), kCombatExclusionWidth,
        kCombatExclusionHeight, scale);
    if (debug_visible) {
        layout.debug_panel = scaled_rect(safe_area.x,
            canvas_y + (kDebugY * scale), kDebugWidth, kDebugHeight, scale);
    }
    return layout;
}

HudTextSafeLayout make_hud_text_safe_layout(
    const HudLayout& layout) noexcept {
    if (layout.scale <= 0.0F) return {};
    const float scale = layout.scale;
    const auto inset = [scale](HudRect panel, float x, float y,
                               float height) noexcept {
        return HudRect{panel.x + x * scale, panel.y + y * scale,
            std::max(0.0F, panel.width - x * 2.0F * scale),
            height * scale};
    };
    return {
        inset(layout.objective_panel, 18.0F, 8.0F, 23.0F),
        inset(layout.objective_panel, 18.0F, 39.0F, 20.0F),
        inset(layout.navigation_panel, 18.0F, 9.0F, 21.0F),
        inset(layout.navigation_panel, 18.0F, 37.0F, 19.0F),
    };
}

bool hud_rects_overlap(HudRect lhs, HudRect rhs) noexcept {
    if (!has_area(lhs) || !has_area(rhs)) return false;
    return lhs.x < rhs.x + rhs.width && rhs.x < lhs.x + lhs.width
        && lhs.y < rhs.y + rhs.height && rhs.y < lhs.y + lhs.height;
}

bool hud_rect_inside(HudRect inner, HudRect outer) noexcept {
    if (!has_area(inner) || !has_area(outer)) return false;
    return inner.x >= outer.x && inner.y >= outer.y
        && inner.x + inner.width <= outer.x + outer.width
        && inner.y + inner.height <= outer.y + outer.height;
}

}  // namespace arpg::platform
