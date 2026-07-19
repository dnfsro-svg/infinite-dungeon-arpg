#pragma once

#include "material_manifest.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {

class MaterialPackState final {
public:
    void set_available(MaterialAtlasId id, bool available) noexcept;
    [[nodiscard]] bool available(MaterialAtlasId id) const noexcept;
    [[nodiscard]] bool can_draw(MaterialSpriteId id) const noexcept;
    [[nodiscard]] bool any_available() const noexcept;
    void reset() noexcept;

private:
    std::array<bool, static_cast<std::size_t>(MaterialAtlasId::count)>
        available_{};
};

class MaterialPack final {
public:
    [[nodiscard]] bool load() noexcept;
    void unload() noexcept;
    [[nodiscard]] bool available(MaterialAtlasId id) const noexcept;
    [[nodiscard]] bool draw(
        MaterialSpriteId id, Vector2 foot_position, bool flip_x) const noexcept;

private:
    MaterialPackState state_{};
    std::array<Texture2D, static_cast<std::size_t>(MaterialAtlasId::count)>
        textures_{};
    std::array<bool, static_cast<std::size_t>(MaterialAtlasId::count)>
        warnings_emitted_{};
};

}  // namespace arpg::platform
