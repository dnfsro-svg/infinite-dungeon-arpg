#pragma once

#include <raylib.h>

namespace arpg::platform {

[[nodiscard]] Vector2 snap_ui_text_position(Vector2 position) noexcept;

void draw_crisp_ui_text(Font font, const char* text, Vector2 position,
    float font_size, float spacing, Color color) noexcept;

}  // namespace arpg::platform
