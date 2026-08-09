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
    float font_size,
    Color color,
    UiTextAuditRole role,
    float minimum_font_size) noexcept {
    constexpr float kSpacing = 1.0F;
    const Vector2 measured = MeasureTextEx(
        font, text, font_size, kSpacing);
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
        position, font_size, kSpacing, bounds,
        minimum_font_size);
    draw_crisp_ui_text(font, text, position,
        font_size, kSpacing, color);
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
    push(PauseMenuRenderOpKind::footer);
    if (plan.has_message) push(PauseMenuRenderOpKind::message);
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
        screen_width, screen_height, view.row_count);

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
                draw_centered_text(font, view.title, layout.title,
                    layout.title_font_size,
                    ui_text_contrast_style().primary,
                    UiTextAuditRole::pause_title,
                    layout.title_font_size);
                break;
            case PauseMenuRenderOpKind::row: {
                const Rectangle bounds = layout.rows[op.row_index];
                const UiTextContrastStyle style = ui_text_contrast_style();
                DrawRectangleRounded(bounds, 0.18F, 5, style.backing);
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
                const float viewport_scale = ui_viewport_scale(
                    screen_width, screen_height);
                const float row_font_size = layout.row_font_size;
                constexpr float kRowTextSpacing = 0.5F;
                const PauseMenuRowView& row = view.rows[op.row_index];
                char fitted_primary[kPauseMenuPrimaryTextCapacity]{};
                const PauseMenuFittedTextLayout centered =
                    fit_centered_pause_text_layout(font, row.primary.data(),
                        fitted_primary, sizeof(fitted_primary), bounds,
                        row_font_size, kRowTextSpacing,
                        kPauseMenuMinimumRowFontSize,
                        12.0F * viewport_scale);
                const Vector2 row_position{
                    centered.text.x, centered.text.y};
                record_ui_text_bounds(UiTextAuditPage::pause,
                    UiTextAuditRole::pause_row, font,
                    fitted_primary, row_position,
                    centered.font_size, centered.spacing, centered.backing,
                    (std::min)(row_font_size,
                        kPauseMenuMinimumRowFontSize));
                draw_crisp_ui_text(font, fitted_primary,
                    row_position, centered.font_size, centered.spacing,
                    op.selected ? style.interaction : style.primary);
                if (row.saved[0] != '\0') {
                    const float saved_font_size = row_font_size * 0.68F;
                    const Vector2 saved_size = MeasureTextEx(font,
                        row.saved.data(), saved_font_size, kRowTextSpacing);
                    const float saved_x = bounds.x + bounds.width
                        - 12.0F * viewport_scale - saved_size.x;
                    const float safe_gap = 16.0F * viewport_scale;
                    if (saved_x >= centered.text.x
                            + centered.text.width + safe_gap) {
                        const Vector2 saved_position{
                            saved_x,
                            bounds.y + (bounds.height - saved_size.y) * 0.5F};
                        record_ui_text_bounds(UiTextAuditPage::pause,
                            UiTextAuditRole::pause_row, font,
                            row.saved.data(), saved_position,
                            saved_font_size, kRowTextSpacing,
                            centered.backing, saved_font_size);
                        draw_crisp_ui_text(font, row.saved.data(),
                            saved_position, saved_font_size, kRowTextSpacing,
                            style.secondary);
                    }
                }
                break;
            }
            case PauseMenuRenderOpKind::message:
                draw_centered_text(font, view.message, layout.footer,
                    layout.footer_font_size,
                    Color{255, 139, 139, 255},
                    UiTextAuditRole::pause_footer,
                    layout.footer_font_size);
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
                if (!plan.has_message && view.footer != nullptr) {
                    const UiTextContrastStyle style =
                        ui_text_contrast_style();
                    DrawRectangleRounded(
                        {layout.footer.x + 5.0F, layout.footer.y + 3.0F,
                         layout.footer.width - 10.0F,
                         layout.footer.height - 6.0F},
                        0.18F, 4, style.backing);
                    draw_centered_text(font, view.footer,
                        layout.footer,
                        layout.footer_font_size,
                        ui_text_contrast_style().muted,
                        UiTextAuditRole::pause_footer,
                        layout.footer_font_size);
                }
                break;
        }
    }
}

PauseMenuRenderer::~PauseMenuRenderer() noexcept {
    shutdown();
}

bool PauseMenuRenderer::initialize(
    const SharedUiFont& shared_ui_font) noexcept {
    shutdown();
    font_ = shared_ui_font.font();
    font_ready_ = shared_ui_font.ready();
    return font_ready_;
}

void PauseMenuRenderer::shutdown() noexcept {
    font_ = {};
    font_ready_ = false;
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
