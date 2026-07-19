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

}  // namespace arpg::platform
