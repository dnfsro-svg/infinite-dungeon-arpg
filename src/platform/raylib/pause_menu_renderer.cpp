#include "pause_menu_renderer.hpp"

#include "death_overlay_font.hpp"
#include "pause_menu_view.hpp"
#include "ui_material.hpp"
#include "ui_text_bounds_audit.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_renderer.hpp"
#include "ui_typography.hpp"

#include <raylib.h>

#include <cstddef>
#include <algorithm>

namespace arpg::platform {
namespace {

void draw_centered_text(
    Font font,
    const char* text,
    Rectangle bounds,
    int font_size,
    Color color,
    UiTextAuditRole role,
    float minimum_font_size) noexcept {
    constexpr float kSpacing = 1.0F;
    const float scaled_font_size = scaled_ui_font_size(
        static_cast<float>(font_size), GetScreenWidth(), GetScreenHeight());
    minimum_font_size = scaled_ui_font_size(
        minimum_font_size, GetScreenWidth(), GetScreenHeight());
    const Vector2 measured = MeasureTextEx(
        font, text, scaled_font_size, kSpacing);
    const int text_width = static_cast<int>(measured.x);
    const int x = static_cast<int>(
        bounds.x + (bounds.width - static_cast<float>(text_width)) * 0.5F);
    const int y = static_cast<int>(
        bounds.y + (bounds.height - measured.y) * 0.5F);
    const Vector2 position{static_cast<float>(x), static_cast<float>(y)};
    const UiTextContrastStyle style = ui_text_contrast_style();
    color.a = 255U;
    if (ui_luma_contrast_ratio(color, style.backing) < 4.5F) {
        color = style.muted;
    }
    record_ui_text_bounds(UiTextAuditPage::pause, role, font, text,
        position, scaled_font_size, kSpacing, bounds,
        minimum_font_size);
    draw_crisp_ui_text(font, text, position,
        scaled_font_size, kSpacing, color);
}

}  // namespace

PauseMenuRenderPlan make_pause_menu_render_plan(
    const PauseMenuView& view) noexcept {
    PauseMenuRenderPlan plan{};
    if (view.row_count == 0U || view.title == nullptr) return plan;

    const auto push = [&plan](
        PauseMenuRenderOpKind kind,
        std::size_t row_index = 0U,
        bool selected = false) noexcept {
        if (plan.op_count >= plan.ops.size()) return;
        plan.ops[plan.op_count++] = {kind, row_index, selected};
    };

    push(PauseMenuRenderOpKind::dim);
    push(PauseMenuRenderOpKind::panel);
    push(PauseMenuRenderOpKind::title);
    const std::size_t row_count = view.row_count < kPauseMenuRowCapacity
        ? view.row_count
        : kPauseMenuRowCapacity;
    for (std::size_t row = 0U; row < row_count; ++row) {
        push(PauseMenuRenderOpKind::row, row, row == view.selected_row);
    }
    plan.has_message = view.message != nullptr && view.message[0] != '\0';
    if (plan.has_message) push(PauseMenuRenderOpKind::message);
    push(PauseMenuRenderOpKind::footer);
    return plan;
}

void draw_pause_menu_with_font(
    const PauseMenuState& state, Font font,
    const MaterialPack* assets) noexcept {
    const PauseMenuView view = make_pause_menu_view(state);
    const PauseMenuRenderPlan plan = make_pause_menu_render_plan(view);
    if (plan.op_count == 0U) return;

    const int screen_width = GetScreenWidth();
    const int screen_height = GetScreenHeight();
    const PauseMenuLayout layout = pause_menu_layout(
        screen_width, screen_height);

    for (std::size_t index = 0U; index < plan.op_count; ++index) {
        const PauseMenuRenderOp op = plan.ops[index];
        switch (op.kind) {
            case PauseMenuRenderOpKind::dim:
                DrawRectangle(0, 0, screen_width, screen_height,
                    Color{2, 4, 8, 255});
                break;
            case PauseMenuRenderOpKind::panel:
                if (assets == nullptr || !assets->draw_nine_slice(ui_material_sprite(
                        UiMaterialElement::pause_panel), layout.panel)) {
                    DrawRectangleRounded(layout.panel, 0.04F, 8,
                        Color{10, 14, 23, 248});
                    DrawRectangleRoundedLinesEx(
                        layout.panel, 0.04F, 8, 2.0F,
                        Color{94, 159, 206, 255});
                }
                break;
            case PauseMenuRenderOpKind::title:
                draw_centered_text(font, view.title, layout.title, 26,
                    ui_text_contrast_style().primary,
                    UiTextAuditRole::pause_title, 20.0F);
                break;
            case PauseMenuRenderOpKind::row: {
                const Rectangle bounds = layout.rows[op.row_index];
                const bool row_drawn = assets != nullptr
                    && assets->draw_horizontal_slice(
                        ui_material_sprite(op.selected
                            ? UiMaterialElement::pause_row_selected
                            : UiMaterialElement::pause_row_idle),
                        op.selected
                            ? Rectangle{4.0F, 41.0F, 120.0F, 46.0F}
                            : Rectangle{4.0F, 40.0F, 120.0F, 47.0F},
                        24.0F, bounds);
                if (!row_drawn && op.selected) {
                    DrawRectangleRounded(bounds, 0.18F, 5,
                        Color{42, 91, 126, 235});
                    DrawRectangleRoundedLinesEx(
                        bounds, 0.18F, 5, 1.0F,
                        Color{121, 197, 244, 255});
                }
                const UiTextContrastStyle style = ui_text_contrast_style();
                const float viewport_scale = ui_viewport_scale(
                    screen_width, screen_height);
                const float row_font_size = scaled_ui_font_size(
                    ui_typography().kPauseRowFontSize,
                    screen_width, screen_height);
                const Vector2 row_position{
                    bounds.x + 14.0F * viewport_scale,
                    bounds.y + 2.0F * viewport_scale};
                const float row_text_width = MeasureTextEx(font,
                    view.rows[op.row_index].data(), row_font_size, 0.5F).x;
                DrawRectangleRounded(
                    {bounds.x + 8.0F * viewport_scale,
                     bounds.y + 1.0F * viewport_scale,
                     (std::min)(row_text_width + 16.0F * viewport_scale,
                         bounds.width - 16.0F * viewport_scale),
                     bounds.height - 2.0F * viewport_scale},
                    0.20F, 4, style.backing);
                record_ui_text_bounds(UiTextAuditPage::pause,
                    UiTextAuditRole::pause_row, font,
                    view.rows[op.row_index].data(), row_position,
                    row_font_size, 0.5F,
                    {bounds.x + 8.0F * viewport_scale,
                        bounds.y + 1.0F * viewport_scale,
                        bounds.width - 16.0F * viewport_scale,
                        bounds.height - 2.0F * viewport_scale},
                    row_font_size);
                draw_crisp_ui_text(font, view.rows[op.row_index].data(),
                    row_position, row_font_size, 0.5F,
                    op.selected ? style.interaction : style.primary);
                break;
            }
            case PauseMenuRenderOpKind::message:
                draw_centered_text(font, view.message, layout.footer, 16,
                    Color{255, 139, 139, 255},
                    UiTextAuditRole::pause_footer, 16.0F);
                break;
            case PauseMenuRenderOpKind::footer:
                if (assets == nullptr || !assets->draw_horizontal_slice(
                        ui_material_sprite(UiMaterialElement::pause_footer),
                        {4.0F, 52.0F, 120.0F, 24.0F}, 24.0F,
                        layout.footer)) {
                    DrawLine(
                        static_cast<int>(layout.footer.x),
                        static_cast<int>(layout.footer.y - 5.0F),
                        static_cast<int>(layout.footer.x
                            + layout.footer.width),
                        static_cast<int>(layout.footer.y - 5.0F),
                        Color{75, 86, 104, 220});
                }
                if (!plan.has_message) {
                    const UiTextContrastStyle style =
                        ui_text_contrast_style();
                    DrawRectangleRounded(
                        {layout.footer.x + 5.0F, layout.footer.y + 3.0F,
                         layout.footer.width - 10.0F,
                         layout.footer.height - 6.0F},
                        0.18F, 4, style.backing);
                    draw_centered_text(font,
                        "Arrow keys navigate | Enter select | Esc back",
                        layout.footer,
                        static_cast<int>(ui_typography().pause_footer_font_size),
                        ui_text_contrast_style().muted,
                        UiTextAuditRole::pause_footer,
                        ui_typography().pause_footer_font_size);
                }
                break;
        }
    }
}

PauseMenuRenderer::~PauseMenuRenderer() noexcept {
    shutdown();
}

bool PauseMenuRenderer::initialize() noexcept {
    shutdown();
    const DeathOverlayFontPlan plan = death_overlay_font_plan();
    for (std::size_t index = 0U; index < plan.candidate_count; ++index) {
        const char* path = plan.candidate_paths[index];
        if (path == nullptr || !FileExists(path)) continue;
        Font candidate = LoadFontEx(path, kUiFontSourceBaseSize,
            plan.codepoints.data(),
            static_cast<int>(plan.codepoint_count));
        if (IsFontValid(candidate)
                && candidate.glyphCount == static_cast<int>(plan.codepoint_count)) {
            SetTextureFilter(candidate.texture, TEXTURE_FILTER_BILINEAR);
            font_ = candidate;
            owns_font_ = true;
            return true;
        }
        if (IsFontValid(candidate)) UnloadFont(candidate);
    }
    font_ = GetFontDefault();
    return false;
}

void PauseMenuRenderer::shutdown() noexcept {
    if (owns_font_ && IsWindowReady()) UnloadFont(font_);
    font_ = {};
    owns_font_ = false;
}

void PauseMenuRenderer::draw(const PauseMenuState& state,
    const MaterialPack& material_pack) const noexcept {
    draw_pause_menu_with_font(state,
        IsFontValid(font_) ? font_ : GetFontDefault(), &material_pack);
}

void draw_pause_menu(const PauseMenuState& state) noexcept {
    draw_pause_menu_with_font(state, GetFontDefault(), nullptr);
}

}  // namespace arpg::platform
