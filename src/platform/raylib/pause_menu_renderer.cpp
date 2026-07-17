#include "pause_menu_renderer.hpp"

#include "pause_menu_view.hpp"

#include <raylib.h>

#include <cstddef>

namespace arpg::platform {
namespace {

void draw_centered_text(
    const char* text,
    Rectangle bounds,
    int font_size,
    Color color) noexcept {
    const int text_width = MeasureText(text, font_size);
    const int x = static_cast<int>(
        bounds.x + (bounds.width - static_cast<float>(text_width)) * 0.5F);
    const int y = static_cast<int>(
        bounds.y + (bounds.height - static_cast<float>(font_size)) * 0.5F);
    DrawText(text, x, y, font_size, color);
}

}  // namespace

void draw_pause_menu(const PauseMenuState& state) noexcept {
    const PauseMenuView view = make_pause_menu_view(state);
    if (view.row_count == 0U || view.title == nullptr) return;

    const int screen_width = GetScreenWidth();
    const int screen_height = GetScreenHeight();
    const PauseMenuLayout layout = pause_menu_layout(
        screen_width, screen_height);

    DrawRectangle(0, 0, screen_width, screen_height,
        Color{2, 4, 8, 190});
    DrawRectangleRounded(layout.panel, 0.04F, 8,
        Color{10, 14, 23, 248});
    DrawRectangleRoundedLinesEx(layout.panel, 0.04F, 8, 2.0F,
        Color{94, 159, 206, 255});

    const Rectangle title_bounds{
        layout.panel.x + 24.0F,
        layout.panel.y + 14.0F,
        layout.panel.width - 48.0F,
        32.0F,
    };
    draw_centered_text(view.title, title_bounds, 28,
        Color{191, 225, 255, 255});

    for (std::size_t row = 0U; row < view.row_count; ++row) {
        const Rectangle bounds = layout.rows[row];
        if (row == view.selected_row) {
            DrawRectangleRounded(bounds, 0.18F, 5,
                Color{42, 91, 126, 235});
            DrawRectangleRoundedLinesEx(bounds, 0.18F, 5, 1.0F,
                Color{121, 197, 244, 255});
        }
        DrawText(view.rows[row].data(),
            static_cast<int>(bounds.x + 10.0F),
            static_cast<int>(bounds.y + 3.0F),
            16,
            row == view.selected_row
                ? Color{244, 249, 255, 255}
                : Color{207, 218, 231, 255});
    }

    DrawLine(
        static_cast<int>(layout.footer.x),
        static_cast<int>(layout.footer.y - 5.0F),
        static_cast<int>(layout.footer.x + layout.footer.width),
        static_cast<int>(layout.footer.y - 5.0F),
        Color{75, 86, 104, 220});
    const bool has_message = view.message != nullptr && view.message[0] != '\0';
    draw_centered_text(
        has_message ? view.message : "Arrow keys navigate | Enter select | Esc back",
        layout.footer,
        16,
        has_message ? Color{255, 139, 139, 255}
                    : Color{165, 178, 196, 255});
}

}  // namespace arpg::platform
