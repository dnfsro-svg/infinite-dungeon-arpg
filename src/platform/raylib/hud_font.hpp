#pragma once

#include "death_overlay_font.hpp"

#include <cstdint>

namespace arpg::platform {

enum class HudFontDrawMode : std::uint8_t { fallback, cjk_ready };

struct HudFontSelectionPlan final {
    HudFontDrawMode draw_mode{HudFontDrawMode::fallback};
    bool use_default_font{true};
    bool owns_loaded_font{};
};

struct HudFontPlan final {
    DeathOverlayFontPlan shared{};
    bool covers_required_text{};
};

[[nodiscard]] HudFontPlan hud_font_plan() noexcept;
[[nodiscard]] HudFontDrawMode hud_font_draw_mode(bool cjk_font_ready) noexcept;
[[nodiscard]] HudFontSelectionPlan make_hud_font_selection_plan(
    bool cjk_font_ready) noexcept;

}  // namespace arpg::platform
