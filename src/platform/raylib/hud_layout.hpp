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

struct HudTextSafeLayout final {
    HudRect objective_title{};
    HudRect objective_hint{};
    HudRect navigation_title{};
    HudRect navigation_ecology{};
};

[[nodiscard]] HudLayout make_hud_layout(
    int screen_width, int screen_height, bool debug_visible) noexcept;
[[nodiscard]] HudTextSafeLayout make_hud_text_safe_layout(
    const HudLayout& layout) noexcept;
[[nodiscard]] bool hud_rects_overlap(HudRect lhs, HudRect rhs) noexcept;
[[nodiscard]] bool hud_rect_inside(HudRect inner, HudRect outer) noexcept;

}  // namespace arpg::platform
