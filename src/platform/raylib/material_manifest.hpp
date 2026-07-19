#pragma once

#include "material_asset_types.hpp"

#include <cstddef>

namespace arpg::platform {

struct MaterialAtlasDefinition final {
    MaterialAtlasId id{MaterialAtlasId::actors};
    int width{};
    int height{};
    std::size_t rgba_bytes{};
};

struct MaterialManifestDefinition final {
    const MaterialAtlasDefinition* atlases{};
    std::size_t atlas_count{};
    const MaterialFrameDefinition* frames{};
    std::size_t frame_count{};
};

namespace detail {

inline constexpr MaterialAtlasDefinition kDefaultMaterialAtlases[] = {
    {MaterialAtlasId::environment, 1024, 1024, 4U * 1024U * 1024U},
    {MaterialAtlasId::actors, 2048, 2048, 4U * 2048U * 2048U},
    {MaterialAtlasId::effects_ui, 1024, 1024, 4U * 1024U * 1024U},
};

inline constexpr MaterialFrameDefinition kDefaultMaterialFrames[] = {
    {MaterialSpriteId::environment_floor_fire, MaterialAtlasId::environment,
        {0.0F, 0.0F, 1024.0F, 704.0F}, {512.0F, 704.0F}, 0U},
    {MaterialSpriteId::environment_floor_water, MaterialAtlasId::environment,
        {0.0F, 0.0F, 1024.0F, 704.0F}, {512.0F, 704.0F}, 0U},
    {MaterialSpriteId::environment_floor_lightning, MaterialAtlasId::environment,
        {0.0F, 0.0F, 1024.0F, 704.0F}, {512.0F, 704.0F}, 0U},
    {MaterialSpriteId::environment_floor_chaos, MaterialAtlasId::environment,
        {0.0F, 0.0F, 1024.0F, 704.0F}, {512.0F, 704.0F}, 0U},
    {MaterialSpriteId::environment_door_fire, MaterialAtlasId::environment,
        {0.0F, 704.0F, 112.0F, 112.0F}, {56.0F, 112.0F}, 0U},
    {MaterialSpriteId::environment_door_water, MaterialAtlasId::environment,
        {112.0F, 704.0F, 112.0F, 112.0F}, {56.0F, 112.0F}, 0U},
    {MaterialSpriteId::environment_door_lightning, MaterialAtlasId::environment,
        {224.0F, 704.0F, 112.0F, 112.0F}, {56.0F, 112.0F}, 0U},
    {MaterialSpriteId::environment_door_chaos, MaterialAtlasId::environment,
        {336.0F, 704.0F, 112.0F, 112.0F}, {56.0F, 112.0F}, 0U},
    {MaterialSpriteId::environment_hole, MaterialAtlasId::environment,
        {448.0F, 704.0F, 192.0F, 160.0F}, {96.0F, 80.0F}, 0U},
};

}  // namespace detail

[[nodiscard]] constexpr MaterialManifestDefinition
default_material_manifest() noexcept {
    return {detail::kDefaultMaterialAtlases,
        sizeof(detail::kDefaultMaterialAtlases)
            / sizeof(detail::kDefaultMaterialAtlases[0]),
        detail::kDefaultMaterialFrames,
        sizeof(detail::kDefaultMaterialFrames)
            / sizeof(detail::kDefaultMaterialFrames[0])};
}

}  // namespace arpg::platform
