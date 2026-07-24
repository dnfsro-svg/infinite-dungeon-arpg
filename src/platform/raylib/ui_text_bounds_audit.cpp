#include "ui_text_bounds_audit.hpp"

#include "ui_text_contrast.hpp"
#include "ui_text_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace arpg::platform {
namespace {

UiTextBoundsAuditStatus g_status{};

[[nodiscard]] Rectangle measured_bounds(Font font, const char* text,
    Vector2 position, float font_size, float spacing) noexcept {
    position = snap_ui_text_position(position);
    const Vector2 measured = MeasureTextEx(font, text, font_size, spacing);
    const UiTextContrastStyle style = ui_text_contrast_style();
    const float left = static_cast<float>(style.outline_pixels);
    const float top = static_cast<float>(style.outline_pixels);
    const float right = static_cast<float>((std::max)(
        style.outline_pixels, style.shadow_pixels));
    const float bottom = right;
    return {position.x - left, position.y - top,
        measured.x + left + right, measured.y + top + bottom};
}

}  // namespace

void reset_ui_text_bounds_audit() noexcept {
    g_status = {};
}

bool ui_text_bounds_inside(Rectangle inner, Rectangle outer) noexcept {
    constexpr float kTolerance = 0.01F;
    return inner.width > 0.0F && inner.height > 0.0F
        && outer.width > 0.0F && outer.height > 0.0F
        && inner.x + kTolerance >= outer.x
        && inner.y + kTolerance >= outer.y
        && inner.x + inner.width <= outer.x + outer.width + kTolerance
        && inner.y + inner.height <= outer.y + outer.height + kTolerance;
}

bool ui_text_bounds_separated(Rectangle first, Rectangle second) noexcept {
    if (first.width <= 0.0F || first.height <= 0.0F
            || second.width <= 0.0F || second.height <= 0.0F) {
        return true;
    }
    return first.x + first.width <= second.x
        || second.x + second.width <= first.x
        || first.y + first.height <= second.y
        || second.y + second.height <= first.y;
}

void record_ui_text_bounds(UiTextAuditPage page, UiTextAuditRole role,
    Font font, const char* text, Vector2 position, float font_size,
    float spacing, Rectangle container, float minimum_font_size,
    const Rectangle* blockers, std::size_t blocker_count) noexcept {
    const std::size_t page_index = static_cast<std::size_t>(page);
    const std::size_t role_index = static_cast<std::size_t>(role);
    if (page_index >= g_status.pages.size() || role_index >= 64U
            || text == nullptr || text[0] == '\0' || !IsFontValid(font)) {
        return;
    }
    UiTextAuditPageStatus& status = g_status.pages[page_index];
    status.observed_roles |= 1ULL << role_index;
    ++status.measured_text_count;
    status.minimum_display_font_size = status.measured_text_count == 1U
        ? font_size : (std::min)(status.minimum_display_font_size, font_size);
    const std::uint64_t role_bit = 1ULL << role_index;
    const bool size_readable = std::isfinite(font_size)
        && font_size >= minimum_font_size;
    status.sizes_readable = status.sizes_readable && size_readable;
    if (!size_readable) status.failed_size_roles |= role_bit;
    const Rectangle bounds = measured_bounds(
        font, text, position, font_size, spacing);
    const bool inside_container = ui_text_bounds_inside(bounds, container);
    bool bounds_safe = inside_container;
    for (std::size_t index{}; index < blocker_count; ++index) {
        bounds_safe = bounds_safe
            && ui_text_bounds_separated(bounds, blockers[index]);
    }
    if (!bounds_safe) {
        TraceLog(LOG_WARNING,
            "UI TEXT BOUNDS role=%u inside=%u bounds=%.1f,%.1f,%.1f,%.1f container=%.1f,%.1f,%.1f,%.1f blockers=%u",
            static_cast<unsigned>(role), inside_container ? 1U : 0U,
            bounds.x, bounds.y, bounds.width, bounds.height,
            container.x, container.y, container.width, container.height,
            static_cast<unsigned>(blocker_count));
        for (std::size_t index{}; index < blocker_count; ++index) {
            TraceLog(LOG_WARNING,
                "UI TEXT BLOCKER role=%u index=%u rect=%.1f,%.1f,%.1f,%.1f separated=%u",
                static_cast<unsigned>(role), static_cast<unsigned>(index),
                blockers[index].x, blockers[index].y,
                blockers[index].width, blockers[index].height,
                ui_text_bounds_separated(bounds, blockers[index]) ? 1U : 0U);
        }
    }
    status.bounds_safe = status.bounds_safe && bounds_safe;
    if (!bounds_safe) status.failed_bounds_roles |= role_bit;
}

UiTextBoundsAuditStatus ui_text_bounds_audit_status() noexcept {
    return g_status;
}

}  // namespace arpg::platform
