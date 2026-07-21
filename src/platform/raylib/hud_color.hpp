#pragma once

#include <cstdint>

namespace arpg::platform {

struct HudRgba8 final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255U};
};

enum class HudPaletteId : std::uint8_t {
    health, barrier, experience, fire, water, lightning, chaos, text, error
};

[[nodiscard]] constexpr HudRgba8 hud_palette_rgba(HudPaletteId id) noexcept {
    switch (id) {
    case HudPaletteId::health: return {218U, 78U, 88U, 255U};
    case HudPaletteId::barrier: return {86U, 164U, 240U, 255U};
    case HudPaletteId::experience: return {231U, 190U, 78U, 255U};
    case HudPaletteId::fire: return {227U, 91U, 62U, 255U};
    case HudPaletteId::water: return {64U, 169U, 222U, 255U};
    case HudPaletteId::lightning: return {240U, 211U, 73U, 255U};
    case HudPaletteId::chaos: return {166U, 91U, 205U, 255U};
    case HudPaletteId::text: return {230U, 235U, 242U, 255U};
    case HudPaletteId::error: return {255U, 127U, 110U, 255U};
    }
    return {230U, 235U, 242U, 255U};
}

}  // namespace arpg::platform
