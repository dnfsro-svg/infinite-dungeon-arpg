#pragma once

#include "material_manifest.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {

struct MaterialTextureApi final {
    Texture2D (*load)(const char* path){};
    bool (*valid)(Texture2D texture){};
    void (*unload)(Texture2D texture){};
};

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
    MaterialPack() noexcept;
    explicit MaterialPack(MaterialTextureApi texture_api) noexcept;
    [[nodiscard]] bool load() noexcept;
    void unload() noexcept;
    [[nodiscard]] bool available(MaterialAtlasId id) const noexcept;
    [[nodiscard]] bool can_draw(MaterialSpriteId id) const noexcept;
    [[nodiscard]] bool draw(
        MaterialSpriteId id, Vector2 foot_position, bool flip_x,
        float scale = 1.0F, Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_frame(
        MaterialAtlasId atlas, Rectangle source, Vector2 foot_anchor,
        Vector2 foot_position, bool flip_x, float scale = 1.0F,
        Color tint = WHITE) const noexcept;

private:
    MaterialTextureApi texture_api_{};
    MaterialPackState state_{};
    std::array<Texture2D, static_cast<std::size_t>(MaterialAtlasId::count)>
        textures_{};
    std::array<bool, static_cast<std::size_t>(MaterialAtlasId::count)>
        warnings_emitted_{};
};

}  // namespace arpg::platform
