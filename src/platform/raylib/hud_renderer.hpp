#pragma once

#include "hud_layout.hpp"
#include "hud_view_model.hpp"

#include <raylib.h>

namespace arpg::platform {

class HudRenderer final {
public:
    HudRenderer() noexcept = default;
    ~HudRenderer() noexcept;
    HudRenderer(const HudRenderer&) = delete;
    HudRenderer& operator=(const HudRenderer&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool font_ready() const noexcept;
    void draw(const HudViewModel&, const HudLayout&) const noexcept;

private:
    Font font_{};
    bool font_ready_{};
};

}  // namespace arpg::platform
