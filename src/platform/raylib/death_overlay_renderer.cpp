#include "death_overlay_renderer.hpp"

#include "death_overlay_view.hpp"

#include <raylib.h>

namespace arpg::platform {
namespace {

Rectangle rectangle(DeathOverlayRect value) noexcept {
    return {value.x, value.y, value.width, value.height};
}

}  // namespace

void draw_death_overlay(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    const DeathOverlayView view = build_death_overlay_view(snapshot);
    if (!view.visible) return;
    const DeathOverlayLayout layout = death_overlay_layout(
        GetScreenWidth(), GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
        Color{2, 4, 8, 205});
    DrawRectangleRounded(rectangle(layout.panel), 0.04F, 8,
        Color{10, 13, 21, 247});
    DrawRectangleRoundedLines(rectangle(layout.panel), 0.04F, 8,
        Color{176, 63, 77, 255});

    const int title_width = MeasureText(view.title.data(), layout.title_font_size);
    DrawText(view.title.data(),
        static_cast<int>(layout.title.x
            + (layout.title.width - static_cast<float>(title_width)) * 0.5F),
        static_cast<int>(layout.title.y), layout.title_font_size,
        Color{255, 190, 176, 255});
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        const DeathOverlayRect bounds = layout.line_bounds[index];
        DrawText(view.lines[index].text.data(), static_cast<int>(bounds.x),
            static_cast<int>(bounds.y), layout.body_font_size,
            view.lines[index].heading ? Color{255, 190, 176, 255}
                                      : Color{222, 228, 239, 255});
    }
    DrawLine(static_cast<int>(layout.prompt.x),
        static_cast<int>(layout.prompt.y - 8.0F),
        static_cast<int>(layout.prompt.x + layout.prompt.width),
        static_cast<int>(layout.prompt.y - 8.0F),
        Color{87, 94, 112, 220});
    const int prompt_width = MeasureText(
        view.prompt.data(), layout.prompt_font_size);
    DrawText(view.prompt.data(),
        static_cast<int>(layout.prompt.x
            + (layout.prompt.width - static_cast<float>(prompt_width)) * 0.5F),
        static_cast<int>(layout.prompt.y), layout.prompt_font_size,
        snapshot.death->continue_failed
            ? Color{255, 116, 116, 255}
            : Color{255, 222, 146, 255});
}

}  // namespace arpg::platform
