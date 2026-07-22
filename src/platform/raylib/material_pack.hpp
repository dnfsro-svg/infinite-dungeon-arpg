#pragma once

#include "material_manifest.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

struct MaterialCompositeParameters final {
    std::uint8_t roughness_channel{0U};
    std::uint8_t emissive_channel{1U};
    std::uint8_t metalness_channel{2U};
    float roughness_strength{0.32F};
    float emissive_strength{0.72F};
    float metalness_strength{0.28F};
    Color emissive_tint{72U, 214U, 255U, 255U};
};

struct MaterialTextureApi final {
    Texture2D (*load)(const char* path){};
    bool (*valid)(Texture2D texture){};
    void (*unload)(Texture2D texture){};
    bool (*initialize_material_pipeline)(){};
    void (*shutdown_material_pipeline)(){};
    void (*draw_material)(Texture2D color, Texture2D material,
        Rectangle source, Rectangle destination, Vector2 origin,
        float rotation, Color tint, MaterialCompositeParameters parameters){};
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
    [[nodiscard]] bool load(
        MaterialEcology ecology = MaterialEcology::common) noexcept;
    void unload() noexcept;
    [[nodiscard]] MaterialEcology current_ecology() const noexcept;
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
        color_textures_{};
    std::array<Texture2D, static_cast<std::size_t>(MaterialAtlasId::count)>
        material_textures_{};
    std::array<bool, static_cast<std::size_t>(MaterialAtlasId::count)>
        warnings_emitted_{};
    MaterialEcology current_ecology_{MaterialEcology::common};
    bool material_pipeline_ready_{};
};

}  // namespace arpg::platform
