#pragma once

#include "material_manifest.hpp"

namespace arpg::platform {

enum class MaterialValidationError : std::uint8_t {
    none,
    invalid_atlas,
    atlas_limit_exceeded,
    memory_budget_exceeded,
    invalid_frame,
    duplicate_atlas_id,
    duplicate_sprite_id,
    missing_frame_atlas,
    missing_atlas_color_map,
    missing_atlas_material_map,
    invalid_anchor,
    invalid_clip,
    repeated_frame_hash,
    ecology_not_loaded,
};

struct MaterialValidationResult final {
    bool valid{};
    MaterialValidationError error{MaterialValidationError::none};
};

[[nodiscard]] MaterialValidationResult validate_material_frame(
    const MaterialAtlasDefinition& atlas,
    const MaterialFrameDefinition& frame) noexcept;

[[nodiscard]] MaterialValidationResult validate_material_manifest(
    const MaterialManifestDefinition& manifest) noexcept;

[[nodiscard]] MaterialValidationResult validate_material_manifest_for_ecology(
    const MaterialManifestDefinition& manifest, MaterialEcology loaded_ecology) noexcept;

}  // namespace arpg::platform
