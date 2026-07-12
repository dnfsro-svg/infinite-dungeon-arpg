#pragma once

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
    constexpr float kMinimumY = -5.5F;
    constexpr float kRoomDepth = 11.0F;
    constexpr float kMaximumX = 12.0F;
    float depth = (world_y - kMinimumY) / kRoomDepth;
    if (depth < 0.0F) {
        depth = 0.0F;
    } else if (depth > 1.0F) {
        depth = 1.0F;
    }
    const float scale = 0.70F + 0.30F * depth;
    const float ground_y = viewport_height * (0.38F + 0.50F * depth);
    return {
        viewport_width * 0.50F
            + world_x * (viewport_width * 0.46F / kMaximumX) * scale,
        ground_y - world_z * 70.0F * scale,
        ground_y,
        scale,
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
