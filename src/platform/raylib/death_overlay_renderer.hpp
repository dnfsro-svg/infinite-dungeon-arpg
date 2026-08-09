#pragma once

#include "dungeon/dungeon_types.hpp"
#include "shared_ui_font.hpp"

#include <raylib.h>

namespace arpg::platform {

class MaterialPack;

class DeathOverlayRenderer final {
public:
    DeathOverlayRenderer() noexcept = default;
    ~DeathOverlayRenderer() noexcept;
    DeathOverlayRenderer(const DeathOverlayRenderer&) = delete;
    DeathOverlayRenderer& operator=(const DeathOverlayRenderer&) = delete;

    [[nodiscard]] bool initialize(const SharedUiFont&) noexcept;
    void shutdown() noexcept;
    void draw(const dungeon::DungeonSnapshot& snapshot,
        const MaterialPack& material_pack) const noexcept;

private:
    Font font_{};
    bool font_ready_{};
};

}  // namespace arpg::platform
