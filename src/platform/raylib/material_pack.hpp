#pragma once

#include "material_manifest.hpp"
#include "material_residency.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

enum class MaterialCompositeMode : std::uint8_t {
    surface,
    emissive_only,
};

struct MaterialCompositeParameters final {
    std::uint8_t roughness_channel{0U};
    std::uint8_t emissive_channel{1U};
    std::uint8_t metalness_channel{2U};
    float roughness_strength{0.32F};
    float emissive_strength{0.72F};
    float metalness_strength{0.28F};
    Color emissive_tint{72U, 214U, 255U, 255U};
    MaterialCompositeMode mode{MaterialCompositeMode::surface};
    float emissive_mask_start{0.08F};
    float emissive_mask_end{0.24F};
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
    void (*draw_material_quad)(Texture2D color, Texture2D material,
        Rectangle source, MaterialScreenQuad destination, Color tint,
        MaterialCompositeParameters parameters){};
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
    [[nodiscard]] bool synchronize_residency(
        MaterialResidencyRequest request) noexcept;
    [[nodiscard]] bool residency_satisfied(
        MaterialResidencyRequest request) const noexcept;
    [[nodiscard]] MaterialResidencyRequest requested_residency() const noexcept;
    [[nodiscard]] MaterialAtlasMask resident_atlases() const noexcept;
    [[nodiscard]] std::size_t resident_bytes() const noexcept;
    [[nodiscard]] std::uint64_t texture_load_call_count() const noexcept;
    [[nodiscard]] std::uint64_t texture_unload_call_count() const noexcept;
    void unload() noexcept;
    [[nodiscard]] MaterialEcology current_ecology() const noexcept;
    [[nodiscard]] bool material_pipeline_ready() const noexcept;
    [[nodiscard]] bool ecology_ready(MaterialEcology ecology) const noexcept;
    [[nodiscard]] bool available(MaterialAtlasId id) const noexcept;
    [[nodiscard]] bool can_draw(MaterialSpriteId id) const noexcept;
    [[nodiscard]] std::uint64_t sprite_draw_count(
        MaterialSpriteId id) const noexcept;
    [[nodiscard]] std::uint64_t direct_stretch_draw_count(
        MaterialSpriteId id) const noexcept;
    [[nodiscard]] bool draw(
        MaterialSpriteId id, Vector2 foot_position, bool flip_x,
        float scale = 1.0F, Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_transformed(
        MaterialSpriteId id, Vector2 foot_position, bool flip_x,
        float scale, float rotation_degrees,
        Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_to(MaterialSpriteId id, Rectangle destination,
        Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_nine_slice(MaterialSpriteId id,
        Rectangle destination, float border_pixels = 32.0F,
        Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_horizontal_slice(MaterialSpriteId id,
        Rectangle source_within_frame, float cap_source_width,
        Rectangle destination, Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_region_fit(MaterialSpriteId id,
        Rectangle source_within_frame, Rectangle destination_bounds,
        Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_frame(
        MaterialAtlasId atlas, Rectangle source, Vector2 foot_anchor,
        Vector2 foot_position, bool flip_x, float scale = 1.0F,
        Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_frame_emissive(
        MaterialAtlasId atlas, Rectangle source, Vector2 foot_anchor,
        Vector2 foot_position, bool flip_x, float scale = 1.0F,
        Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_frame_to(
        MaterialAtlasId atlas, Rectangle source, Rectangle destination,
        Color tint = WHITE) const noexcept;
    [[nodiscard]] bool draw_frame_quad(
        MaterialAtlasId atlas, Rectangle source,
        MaterialScreenQuad destination, Color tint = WHITE) const noexcept;

private:
    [[nodiscard]] bool draw_frame_composited(
        MaterialAtlasId atlas, Rectangle source, Vector2 foot_anchor,
        Vector2 foot_position, bool flip_x, float scale, Color tint,
        MaterialCompositeParameters parameters) const noexcept;
    MaterialTextureApi texture_api_{};
    MaterialPackState state_{};
    std::array<Texture2D, static_cast<std::size_t>(MaterialAtlasId::count)>
        color_textures_{};
    std::array<Texture2D, static_cast<std::size_t>(MaterialAtlasId::count)>
        material_textures_{};
    std::array<bool, static_cast<std::size_t>(MaterialAtlasId::count)>
        warnings_emitted_{};
    mutable std::array<std::uint64_t,
        static_cast<std::size_t>(MaterialSpriteId::count)> sprite_draw_counts_{};
    mutable std::array<std::uint64_t,
        static_cast<std::size_t>(MaterialSpriteId::count)>
        direct_stretch_draw_counts_{};
    MaterialEcology current_ecology_{MaterialEcology::common};
    MaterialResidencyRequest requested_residency_{};
    MaterialAtlasMask resident_atlases_{};
    std::uint64_t texture_load_call_count_{};
    std::uint64_t texture_unload_call_count_{};
    bool material_pipeline_ready_{};
};

}  // namespace arpg::platform
