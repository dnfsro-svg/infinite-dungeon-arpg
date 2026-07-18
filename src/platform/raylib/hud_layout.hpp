#pragma once

namespace arpg::platform {

struct HudRect final {
    float x{};
    float y{};
    float width{};
    float height{};
};

struct HudLayout final {
    HudRect safe_area{};
    HudRect player_panel{};
    HudRect objective_panel{};
    HudRect navigation_panel{};
    HudRect primary_notice{};
    HudRect secondary_notice{};
    HudRect debug_panel{};
    HudRect combat_exclusion{};
    float scale{1.0F};
};

[[nodiscard]] HudLayout make_hud_layout(
    int screen_width, int screen_height, bool debug_visible) noexcept;
[[nodiscard]] bool hud_rects_overlap(HudRect lhs, HudRect rhs) noexcept;
[[nodiscard]] bool hud_rect_inside(HudRect inner, HudRect outer) noexcept;

}  // namespace arpg::platform
