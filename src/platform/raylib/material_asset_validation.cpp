#include "material_asset_validation.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::platform {
namespace {

constexpr int kMaximumAtlasDimension = 2048;
constexpr std::size_t kMaximumManifestRgbaBytes = 64U * 1024U * 1024U;

[[nodiscard]] constexpr bool is_known_atlas(MaterialAtlasId id) noexcept {
    return id < MaterialAtlasId::count;
}

[[nodiscard]] constexpr bool is_known_sprite(MaterialSpriteId id) noexcept {
    return id < MaterialSpriteId::count;
}

[[nodiscard]] constexpr MaterialValidationResult valid_result() noexcept {
    return {true, MaterialValidationError::none};
}

[[nodiscard]] constexpr MaterialValidationResult invalid_result(
    MaterialValidationError error) noexcept {
    return {false, error};
}

[[nodiscard]] const MaterialAtlasDefinition* find_atlas(
    const MaterialManifestDefinition& manifest,
    MaterialAtlasId id) noexcept {
    for (std::size_t index = 0U; index < manifest.atlas_count; ++index) {
        if (manifest.atlases[index].id == id) return &manifest.atlases[index];
    }
    return nullptr;
}

}  // namespace

MaterialValidationResult validate_material_frame(
    const MaterialAtlasDefinition& atlas,
    const MaterialFrameDefinition& frame) noexcept {
    if (!is_known_atlas(atlas.id) || !is_known_atlas(frame.atlas)
        || !is_known_sprite(frame.id) || atlas.id != frame.atlas
        || atlas.width <= 0 || atlas.height <= 0) {
        return invalid_result(MaterialValidationError::invalid_frame);
    }
    if (frame.source.width <= 0.0F || frame.source.height <= 0.0F
        || frame.source.x < 0.0F || frame.source.y < 0.0F
        || frame.source.x + frame.source.width > static_cast<float>(atlas.width)
        || frame.source.y + frame.source.height > static_cast<float>(atlas.height)
        || frame.foot_anchor.x < 0.0F || frame.foot_anchor.y < 0.0F
        || frame.foot_anchor.x > frame.source.width
        || frame.foot_anchor.y > frame.source.height) {
        return invalid_result(MaterialValidationError::invalid_frame);
    }
    return valid_result();
}

MaterialValidationResult validate_material_manifest(
    const MaterialManifestDefinition& manifest) noexcept {
    if ((manifest.atlas_count != 0U && manifest.atlases == nullptr)
        || (manifest.frame_count != 0U && manifest.frames == nullptr)) {
        return invalid_result(MaterialValidationError::invalid_atlas);
    }

    std::size_t rgba_bytes{};
    for (std::size_t index = 0U; index < manifest.atlas_count; ++index) {
        const MaterialAtlasDefinition& atlas = manifest.atlases[index];
        if (!is_known_atlas(atlas.id) || atlas.width <= 0 || atlas.height <= 0) {
            return invalid_result(MaterialValidationError::invalid_atlas);
        }
        if (atlas.width > kMaximumAtlasDimension
            || atlas.height > kMaximumAtlasDimension) {
            return invalid_result(MaterialValidationError::atlas_limit_exceeded);
        }
        if (atlas.rgba_bytes > kMaximumManifestRgbaBytes - rgba_bytes) {
            return invalid_result(MaterialValidationError::memory_budget_exceeded);
        }
        rgba_bytes += atlas.rgba_bytes;
        for (std::size_t prior = 0U; prior < index; ++prior) {
            if (manifest.atlases[prior].id == atlas.id) {
                return invalid_result(MaterialValidationError::duplicate_atlas_id);
            }
        }
    }

    for (std::size_t index = 0U; index < manifest.frame_count; ++index) {
        const MaterialFrameDefinition& frame = manifest.frames[index];
        const MaterialAtlasDefinition* atlas = find_atlas(manifest, frame.atlas);
        if (atlas == nullptr) {
            return invalid_result(MaterialValidationError::missing_frame_atlas);
        }
        const MaterialValidationResult frame_result =
            validate_material_frame(*atlas, frame);
        if (!frame_result.valid) return frame_result;
        for (std::size_t prior = 0U; prior < index; ++prior) {
            if (manifest.frames[prior].id == frame.id) {
                return invalid_result(MaterialValidationError::duplicate_sprite_id);
            }
        }
    }
    return valid_result();
}

}  // namespace arpg::platform
