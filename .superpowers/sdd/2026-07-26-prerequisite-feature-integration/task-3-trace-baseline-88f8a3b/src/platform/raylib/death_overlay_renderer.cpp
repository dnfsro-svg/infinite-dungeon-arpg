#include "death_overlay_renderer.hpp"

#include "death_overlay_font.hpp"
#include "death_overlay_view.hpp"
#include "material_pack.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_renderer.hpp"

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
    const UiTextContrastStyle style = ui_text_contrast_style();
    color.a = 255U;
    if (ui_luma_contrast_ratio(color, style.backing) < 4.5F) {
        color = style.muted;
    }
    draw_crisp_ui_text(font, text, {x, bounds.y},
        static_cast<float>(font_size), kSpacing, color);
}

void draw_screen_dimmer() noexcept {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
        Color{2, 4, 8, 205});
}

void draw_panel_fallback(const DeathOverlayLayout& layout) noexcept {
    DrawRectangleRounded(rectangle(layout.panel), 0.04F, 8,
        Color{10, 13, 21, 255});
    DrawRectangleRoundedLines(rectangle(layout.panel), 0.04F, 8,
        Color{176, 63, 77, 255});
}

void draw_title_plate_fallback(const DeathOverlayLayout& layout) noexcept {
    DrawRectangleRounded(rectangle(layout.title), 0.12F, 6,
        Color{22, 19, 25, 255});
    DrawRectangleRoundedLines(rectangle(layout.title), 0.12F, 6,
        Color{132, 101, 68, 255});
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
        Font candidate = LoadFontEx(path, kUiFontSourceBaseSize,
            plan.codepoints.data(),
            static_cast<int>(plan.codepoint_count));
        if (IsFontValid(candidate)
                && candidate.glyphCount
                    == static_cast<int>(plan.codepoint_count)) {
            SetTextureFilter(candidate.texture, TEXTURE_FILTER_BILINEAR);
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
    const dungeon::DungeonSnapshot& snapshot,
    const MaterialPack& material_pack) const noexcept {
    const DeathOverlayView view = owns_font_
        ? build_death_overlay_view(snapshot)
        : build_death_overlay_ascii_view(snapshot);
    const DeathOverlayMaterialPlan plan =
        death_overlay_material_plan(view.visible);
    if (!plan.visible) return;
    const DeathOverlayLayout layout = death_overlay_layout(
        GetScreenWidth(), GetScreenHeight());
    draw_screen_dimmer();
    if (!material_pack.draw_nine_slice(
            plan.panel, rectangle(layout.panel), plan.panel_border_pixels)) {
        draw_panel_fallback(layout);
    }
    if (!material_pack.draw_region_fit(plan.title_plate,
            {4.0F, 52.0F, 120.0F, 24.0F}, rectangle(layout.title))) {
        draw_title_plate_fallback(layout);
    }
    const Font draw_font = IsFontValid(font_) ? font_ : GetFontDefault();
    draw_text(draw_font, view.title.data(), layout.title,
        layout.title_font_size, ui_text_contrast_style().danger, true);
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        const DeathOverlayRect bounds = layout.line_bounds[index];
        draw_text(draw_font, view.lines[index].text.data(), bounds,
            layout.body_font_size,
            view.lines[index].heading ? ui_text_contrast_style().danger
                                      : ui_text_contrast_style().primary);
    }
    DrawLine(static_cast<int>(layout.prompt.x),
        static_cast<int>(layout.prompt.y - 8.0F),
        static_cast<int>(layout.prompt.x + layout.prompt.width),
        static_cast<int>(layout.prompt.y - 8.0F),
        Color{87, 94, 112, 220});
    draw_text(draw_font, view.prompt.data(), layout.prompt,
        layout.prompt_font_size,
        snapshot.death->continue_failed
            ? ui_text_contrast_style().danger
            : ui_text_contrast_style().warning, true);
}

}  // namespace arpg::platform
