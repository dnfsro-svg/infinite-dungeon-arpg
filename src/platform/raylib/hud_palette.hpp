#pragma once

#include "hud_color.hpp"

#include <raylib.h>

namespace arpg::platform {

[[nodiscard]] constexpr Color hud_palette_color(HudPaletteId id) noexcept {
    const HudRgba8 color = hud_palette_rgba(id);
    return {color.r, color.g, color.b, color.a};
}

struct HudPalette final {
    Color health{};
    Color barrier{};
    Color experience{};
    Color fire{};
    Color water{};
    Color lightning{};
    Color chaos{};
    Color text{};
    Color error{};
};

[[nodiscard]] constexpr HudPalette hud_palette() noexcept {
    return {
        hud_palette_color(HudPaletteId::health),
        hud_palette_color(HudPaletteId::barrier),
        hud_palette_color(HudPaletteId::experience),
        hud_palette_color(HudPaletteId::fire),
        hud_palette_color(HudPaletteId::water),
        hud_palette_color(HudPaletteId::lightning),
        hud_palette_color(HudPaletteId::chaos),
        hud_palette_color(HudPaletteId::text),
        hud_palette_color(HudPaletteId::error),
    };
}

}  // namespace arpg::platform
