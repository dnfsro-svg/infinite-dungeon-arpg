#include "death_overlay_renderer.hpp"

#include "death_overlay_font.hpp"
#include "death_overlay_view.hpp"

#include <cstddef>

namespace arpg::platform {
namespace {

Rectangle rectangle(DeathOverlayRect value) noexcept {
    return {value.x, value.y, value.width, value.height};
}

void draw_text(Font font, const char* text, DeathOverlayRect bounds,
    int font_size, Color color, bool centered = false) noexcept {
    constexpr float kSpacing = 1.0F;
    float x = bounds.x;
    if (centered) {
        const Vector2 measured = MeasureTextEx(
            font, text, static_cast<float>(font_size), kSpacing);
        x += (bounds.width - measured.x) * 0.5F;
    }
    constexpr int kOutlinePixels = 2;
    const Color outline{3, 5, 10, 245};
    for (int offset_y = -kOutlinePixels; offset_y <= kOutlinePixels; ++offset_y) {
        for (int offset_x = -kOutlinePixels; offset_x <= kOutlinePixels; ++offset_x) {
            if (offset_x == 0 && offset_y == 0) continue;
            DrawTextEx(font, text, {x + static_cast<float>(offset_x),
                bounds.y + static_cast<float>(offset_y)},
                static_cast<float>(font_size), kSpacing, outline);
        }
    }
    DrawTextEx(font, text, {x, bounds.y}, static_cast<float>(font_size),
        kSpacing, color);
}

void draw_panel(const DeathOverlayLayout& layout) noexcept {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
        Color{2, 4, 8, 205});
    DrawRectangleRounded(rectangle(layout.panel), 0.04F, 8,
        Color{10, 13, 21, 247});
    DrawRectangleRoundedLines(rectangle(layout.panel), 0.04F, 8,
        Color{176, 63, 77, 255});
}

}  // namespace

DeathOverlayRenderer::~DeathOverlayRenderer() noexcept {
    if (owns_font_ && IsWindowReady()) UnloadFont(font_);
}

bool DeathOverlayRenderer::initialize() noexcept {
    shutdown();
    const DeathOverlayFontPlan plan = death_overlay_font_plan();
    for (std::size_t index = 0U; index < plan.candidate_count; ++index) {
        const char* path = plan.candidate_paths[index];
        if (path == nullptr || !FileExists(path)) continue;
        Font candidate = LoadFontEx(path, 32, plan.codepoints.data(),
            static_cast<int>(plan.codepoint_count));
        if (IsFontValid(candidate)
                && candidate.glyphCount
                    == static_cast<int>(plan.codepoint_count)) {
            font_ = candidate;
            owns_font_ = true;
            TraceLog(LOG_INFO, "DEATH OVERLAY: Loaded font %s (%i glyphs)",
                path, candidate.glyphCount);
            return true;
        }
        if (IsFontValid(candidate)) UnloadFont(candidate);
    }
    font_ = GetFontDefault();
    TraceLog(LOG_WARNING,
        "DEATH OVERLAY: No complete CJK font found; using readable ASCII fallback");
    return false;
}

void DeathOverlayRenderer::shutdown() noexcept {
    if (owns_font_ && IsWindowReady()) UnloadFont(font_);
    font_ = {};
    owns_font_ = false;
}

void DeathOverlayRenderer::draw(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    const DeathOverlayView view = owns_font_
        ? build_death_overlay_view(snapshot)
        : build_death_overlay_ascii_view(snapshot);
    if (!view.visible) return;
    const DeathOverlayLayout layout = death_overlay_layout(
        GetScreenWidth(), GetScreenHeight());
    draw_panel(layout);
    const Font draw_font = IsFontValid(font_) ? font_ : GetFontDefault();
    draw_text(draw_font, view.title.data(), layout.title,
        layout.title_font_size, Color{255, 190, 176, 255}, true);
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        const DeathOverlayRect bounds = layout.line_bounds[index];
        draw_text(draw_font, view.lines[index].text.data(), bounds,
            layout.body_font_size,
            view.lines[index].heading ? Color{255, 190, 176, 255}
                                      : Color{222, 228, 239, 255});
    }
    DrawLine(static_cast<int>(layout.prompt.x),
        static_cast<int>(layout.prompt.y - 8.0F),
        static_cast<int>(layout.prompt.x + layout.prompt.width),
        static_cast<int>(layout.prompt.y - 8.0F),
        Color{87, 94, 112, 220});
    draw_text(draw_font, view.prompt.data(), layout.prompt,
        layout.prompt_font_size,
        snapshot.death->continue_failed
            ? Color{255, 116, 116, 255}
            : Color{255, 222, 146, 255}, true);
}

}  // namespace arpg::platform
