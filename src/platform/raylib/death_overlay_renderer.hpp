#pragma once

#include "dungeon/dungeon_types.hpp"
#include "shared_ui_font.hpp"

#include <raylib.h>

namespace arpg::platform {

class MaterialPack;

enum class DeathOverlayTextDrawMode {
    unavailable,
    shared_ui_font,
};

struct DeathOverlayTextStyle final {
    Color backing{5, 9, 16, 255};
    Color primary{248, 246, 238, 255};
    Color danger{255, 152, 152, 255};
    Color warning{255, 210, 118, 255};
    Color error{255, 152, 152, 255};
};

[[nodiscard]] constexpr DeathOverlayTextStyle
death_overlay_text_style() noexcept {
    return {};
}

[[nodiscard]] DeathOverlayTextDrawMode death_overlay_text_draw_mode(
    bool shared_font_ready) noexcept;

class DeathOverlayRenderer final {
public:
    DeathOverlayRenderer() noexcept = default;
    ~DeathOverlayRenderer() noexcept;
    DeathOverlayRenderer(const DeathOverlayRenderer&) = delete;
    DeathOverlayRenderer& operator=(const DeathOverlayRenderer&) = delete;

    [[nodiscard]] bool initialize(const SharedUiFont&) noexcept;
    void shutdown() noexcept;
    void draw(const dungeon::DungeonSnapshot& snapshot,
        const MaterialPack& material_pack,
        const char* continue_binding_label) const noexcept;

private:
    Font font_{};
    bool font_ready_{};
};

}  // namespace arpg::platform
