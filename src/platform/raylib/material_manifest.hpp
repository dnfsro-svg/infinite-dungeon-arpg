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

}  // namespace detail

[[nodiscard]] constexpr MaterialManifestDefinition
default_material_manifest() noexcept {
    return {detail::kDefaultMaterialAtlases,
        sizeof(detail::kDefaultMaterialAtlases)
            / sizeof(detail::kDefaultMaterialAtlases[0]),
        nullptr, 0U};
}

}  // namespace arpg::platform
