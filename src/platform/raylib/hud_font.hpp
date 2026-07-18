#pragma once

#include "death_overlay_font.hpp"

namespace arpg::platform {

struct HudFontPlan final {
    DeathOverlayFontPlan shared{};
    bool covers_required_text{};
};

[[nodiscard]] HudFontPlan hud_font_plan() noexcept;

}  // namespace arpg::platform
