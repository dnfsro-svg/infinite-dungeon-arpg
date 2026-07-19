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

}  // namespace arpg::platform
