#include "death_overlay_renderer.hpp"

#include "death_overlay_view.hpp"
#include "material_pack.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_layout.hpp"
#include "ui_text_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

Rectangle rectangle(DeathOverlayRect value) noexcept {
    return {value.x, value.y, value.width, value.height};
}

struct DeathTextMeasureContext final {
    Font font{};
};

UiTextExtent measure_death_text(const char* value, float size, float spacing,
    void* opaque) noexcept {
    const auto* context = static_cast<const DeathTextMeasureContext*>(opaque);
    const Vector2 measured = MeasureTextEx(context->font, value, size, spacing);
    return {measured.x, measured.y};
}

void draw_text(Font font, const char* text, DeathOverlayRect bounds,
    int font_size, Color color, bool centered = false) noexcept {
    constexpr float kSpacing = 1.0F;
    const DeathOverlayTextStyle style = death_overlay_text_style();
    color.a = 255U;
    if (style.backing.a != 255U || color.a != 255U
            || ui_luma_contrast_ratio(color, style.backing) < 4.5F) {
        return;
    }
    DrawRectangleRec(rectangle(bounds), style.backing);

    DeathTextMeasureContext context{font};
    std::array<char, kDeathOverlayTextCapacity> fitted{};
    const UiCenteredTextLayout layout = fit_centered_ui_text(
        fitted.data(), fitted.size(), text, rectangle(bounds),
        static_cast<float>(font_size),
        (std::max)(9.0F, static_cast<float>(font_size) * 0.7F),
        kSpacing, &measure_death_text, &context);
    if (!layout.drawable) return;
    Vector2 position = layout.position;
    if (!centered) {
        position.x = bounds.x;
    }
    draw_crisp_ui_text(font, layout.text, position,
        layout.font_size, layout.spacing, color);
}

void draw_screen_dimmer(std::uint8_t alpha) noexcept {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
        Color{2, 4, 8, alpha});
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

DeathOverlayTextDrawMode death_overlay_text_draw_mode(
    bool shared_font_ready) noexcept {
    return shared_font_ready ? DeathOverlayTextDrawMode::shared_ui_font
                             : DeathOverlayTextDrawMode::unavailable;
}

DeathOverlayRenderer::~DeathOverlayRenderer() noexcept { shutdown(); }

bool DeathOverlayRenderer::initialize(
    const SharedUiFont& shared_ui_font) noexcept {
    shutdown();
    font_ = shared_ui_font.font();
    font_ready_ = shared_ui_font.ready();
    return font_ready_;
}

void DeathOverlayRenderer::shutdown() noexcept {
    font_ = {};
    font_ready_ = false;
}

void DeathOverlayRenderer::draw(
    const dungeon::DungeonSnapshot& snapshot,
    const MaterialPack& material_pack,
    const char* continue_binding_label) const noexcept {
    if (death_overlay_text_draw_mode(font_ready_)
            != DeathOverlayTextDrawMode::shared_ui_font
            || !IsFontValid(font_)) {
        return;
    }
    const DeathOverlayView view =
        build_death_overlay_view(snapshot, continue_binding_label);
    const DeathOverlayMaterialPlan plan =
        death_overlay_material_plan(view.visible);
    if (!plan.visible) return;
    const DeathOverlayLayout layout = death_overlay_layout(
        view, GetScreenWidth(), GetScreenHeight());
    draw_screen_dimmer(plan.dimmer_alpha);
    if (!material_pack.draw_nine_slice(
            plan.panel, rectangle(layout.panel), plan.panel_border_pixels)) {
        draw_panel_fallback(layout);
    }
    if (!material_pack.draw_region_fit(plan.title_plate,
            {4.0F, 52.0F, 120.0F, 24.0F}, rectangle(layout.title))) {
        draw_title_plate_fallback(layout);
    }
    const DeathOverlayTextStyle style = death_overlay_text_style();
    draw_text(font_, view.title.data(), layout.title,
        layout.title_font_size, style.danger, true);
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        const DeathOverlayRect bounds = layout.line_bounds[index];
        draw_text(font_, view.lines[index].text.data(), bounds,
            view.lines[index].heading
                ? layout.heading_font_size : layout.body_font_size,
            view.lines[index].heading ? style.danger : style.primary);
    }
    DrawLine(static_cast<int>(layout.prompt.x),
        static_cast<int>(layout.prompt.y - 8.0F),
        static_cast<int>(layout.prompt.x + layout.prompt.width),
        static_cast<int>(layout.prompt.y - 8.0F),
        Color{87, 94, 112, 220});
    draw_text(font_, view.prompt.data(), layout.prompt,
        layout.prompt_font_size,
        snapshot.death->continue_failed ? style.error : style.warning, true);
}

}  // namespace arpg::platform
