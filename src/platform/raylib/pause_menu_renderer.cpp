#include "pause_menu_renderer.hpp"

#include "death_overlay_font.hpp"
#include "pause_menu_view.hpp"

#include <raylib.h>

#include <cstddef>

namespace arpg::platform {
namespace {

void draw_centered_text(
    Font font,
    const char* text,
    Rectangle bounds,
    int font_size,
    Color color) noexcept {
    constexpr float kSpacing = 1.0F;
    const int text_width = static_cast<int>(MeasureTextEx(
        font, text, static_cast<float>(font_size), kSpacing).x);
    const int x = static_cast<int>(
        bounds.x + (bounds.width - static_cast<float>(text_width)) * 0.5F);
    const int y = static_cast<int>(
        bounds.y + (bounds.height - static_cast<float>(font_size)) * 0.5F);
    DrawTextEx(font, text, {static_cast<float>(x), static_cast<float>(y)},
        static_cast<float>(font_size), kSpacing, color);
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
    const PauseMenuState& state, Font font) noexcept {
    const PauseMenuView view = make_pause_menu_view(state);
    const PauseMenuRenderPlan plan = make_pause_menu_render_plan(view);
    if (plan.op_count == 0U) return;

    const int screen_width = GetScreenWidth();
    const int screen_height = GetScreenHeight();
    const PauseMenuLayout layout = pause_menu_layout(
        screen_width, screen_height);

    const Rectangle title_bounds{
        layout.panel.x + 24.0F,
        layout.panel.y + 14.0F,
        layout.panel.width - 48.0F,
        32.0F,
    };
    for (std::size_t index = 0U; index < plan.op_count; ++index) {
        const PauseMenuRenderOp op = plan.ops[index];
        switch (op.kind) {
            case PauseMenuRenderOpKind::dim:
                DrawRectangle(0, 0, screen_width, screen_height,
                    Color{2, 4, 8, 190});
                break;
            case PauseMenuRenderOpKind::panel:
                DrawRectangleRounded(layout.panel, 0.04F, 8,
                    Color{10, 14, 23, 248});
                DrawRectangleRoundedLinesEx(
                    layout.panel, 0.04F, 8, 2.0F,
                    Color{94, 159, 206, 255});
                break;
            case PauseMenuRenderOpKind::title:
                draw_centered_text(font, view.title, title_bounds, 28,
                    Color{191, 225, 255, 255});
                break;
            case PauseMenuRenderOpKind::row: {
                const Rectangle bounds = layout.rows[op.row_index];
                if (op.selected) {
                    DrawRectangleRounded(bounds, 0.18F, 5,
                        Color{42, 91, 126, 235});
                    DrawRectangleRoundedLinesEx(
                        bounds, 0.18F, 5, 1.0F,
                        Color{121, 197, 244, 255});
                }
                DrawTextEx(font, view.rows[op.row_index].data(),
                    {bounds.x + 10.0F, bounds.y + 3.0F}, 16.0F, 1.0F,
                    op.selected
                        ? Color{244, 249, 255, 255}
                        : Color{207, 218, 231, 255});
                break;
            }
            case PauseMenuRenderOpKind::message:
                draw_centered_text(font, view.message, layout.footer, 16,
                    Color{255, 139, 139, 255});
                break;
            case PauseMenuRenderOpKind::footer:
                DrawLine(
                    static_cast<int>(layout.footer.x),
                    static_cast<int>(layout.footer.y - 5.0F),
                    static_cast<int>(layout.footer.x
                        + layout.footer.width),
                    static_cast<int>(layout.footer.y - 5.0F),
                    Color{75, 86, 104, 220});
                if (!plan.has_message) {
                    draw_centered_text(font,
                        "Arrow keys navigate | Enter select | Esc back",
                        layout.footer,
                        16,
                        Color{165, 178, 196, 255});
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
        Font candidate = LoadFontEx(path, 32, plan.codepoints.data(),
            static_cast<int>(plan.codepoint_count));
        if (IsFontValid(candidate)
                && candidate.glyphCount == static_cast<int>(plan.codepoint_count)) {
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

void PauseMenuRenderer::draw(const PauseMenuState& state) const noexcept {
    draw_pause_menu_with_font(state,
        IsFontValid(font_) ? font_ : GetFontDefault());
}

void draw_pause_menu(const PauseMenuState& state) noexcept {
    draw_pause_menu_with_font(state, GetFontDefault());
}

}  // namespace arpg::platform
