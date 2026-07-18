#pragma once

#include "death_overlay_font.hpp"

#include <cstdint>

namespace arpg::platform {

enum class HudFontDrawMode : std::uint8_t { fallback, cjk_ready };

struct HudFontPlan final {
    DeathOverlayFontPlan shared{};
    bool covers_required_text{};
};

[[nodiscard]] HudFontPlan hud_font_plan() noexcept;
[[nodiscard]] HudFontDrawMode hud_font_draw_mode(bool cjk_font_ready) noexcept;

}  // namespace arpg::platform
