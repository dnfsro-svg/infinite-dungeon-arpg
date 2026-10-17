#pragma once

#include "combat_view_math.hpp"
#include "hud_layout.hpp"

namespace arpg::platform {

struct RenderProjection final {
    float x{};
    float y{};
    float ground_y{};
    float scale{};
};

struct RenderLayout final {
    float hud_x{};
    int hud_first_line_y{};
    int hud_second_instruction_y{};
    int hud_status_y{};
    int hud_line_step{};
    int hud_debug_start_step{};
    float hud_panel_width{};
    float hud_panel_height{};
};

[[nodiscard]] inline RenderProjection project_render_world(
    float world_x,
    float world_y,
    float world_z,
    float viewport_width,
    float viewport_height) noexcept {
    const ScreenProjection projected = project_combat_position(
        {world_x, world_y, world_z}, viewport_width, viewport_height);
    return {
        projected.x,
        projected.y,
        projected.ground_y,
        projected.scale,
    };
}

[[nodiscard]] inline RenderLayout render_layout(bool draw_debug) noexcept {
    return {
        30.0F,
        28,
        53,
        81,
        23,
        25,
        570.0F,
        draw_debug ? 660.0F : 400.0F,
    };
}

[[nodiscard]] inline int debug_overlay_start_y(int hud_last_line_y) noexcept {
    return hud_last_line_y + render_layout(true).hud_debug_start_step;
}

}  // namespace arpg::platform
