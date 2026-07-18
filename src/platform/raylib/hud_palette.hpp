#pragma once

#include <raylib.h>

namespace arpg::platform {

struct HudPalette final {
    Color health{218, 78, 88, 255};
    Color barrier{86, 164, 240, 255};
    Color experience{231, 190, 78, 255};
    Color fire{227, 91, 62, 255};
    Color water{64, 169, 222, 255};
    Color lightning{240, 211, 73, 255};
    Color chaos{166, 91, 205, 255};
    Color text{230, 235, 242, 255};
    Color error{255, 127, 110, 255};
};

[[nodiscard]] constexpr HudPalette hud_palette() noexcept {
    return {};
}

}  // namespace arpg::platform
