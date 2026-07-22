#include "material_asset_validation.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::platform {
namespace {

constexpr int kMaximumAtlasDimension = 2048;
constexpr std::size_t kMaximumManifestRgbaBytes = 64U * 1024U * 1024U;
constexpr std::uint16_t kMaximumClipFrames = 256U;

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
    if (!std::isfinite(frame.source.x) || !std::isfinite(frame.source.y)
        || !std::isfinite(frame.source.width)
        || !std::isfinite(frame.source.height)
        || !std::isfinite(frame.foot_anchor.x)
        || !std::isfinite(frame.foot_anchor.y)
        || !std::isfinite(frame.weapon_anchor.x)
        || !std::isfinite(frame.weapon_anchor.y)) {
        return invalid_result(MaterialValidationError::invalid_frame);
    }
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
        return invalid_result(MaterialValidationError::invalid_anchor);
    }
    if (frame.weapon_anchor.x < 0.0F || frame.weapon_anchor.y < 0.0F
        || frame.weapon_anchor.x > frame.source.width
        || frame.weapon_anchor.y > frame.source.height) {
        return invalid_result(MaterialValidationError::invalid_anchor);
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
        if (atlas.color_path == nullptr || atlas.color_path[0] == '\0') {
            return invalid_result(MaterialValidationError::missing_atlas_color_map);
        }
        if (atlas.material_path == nullptr || atlas.material_path[0] == '\0') {
            return invalid_result(MaterialValidationError::missing_atlas_material_map);
        }
        const std::size_t budget = manifest.memory_budget_bytes == 0U
            ? kMaximumManifestRgbaBytes
            : manifest.memory_budget_bytes;
        if (rgba_bytes > budget || atlas.rgba_bytes > budget - rgba_bytes) {
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

    for (std::size_t index = 0U; index < manifest.clip_count; ++index) {
        const AnimationClipDefinition& clip = manifest.clips[index];
        if (clip.id >= AnimationClipId::count || clip.resource_id >= MaterialSpriteId::count
            || clip.frame_count == 0U || clip.minimum_frames == 0U
            || clip.frame_count < clip.minimum_frames
            || clip.frame_count > kMaximumClipFrames
            || static_cast<std::size_t>(clip.first_frame) + clip.frame_count > manifest.frame_count
            || static_cast<std::size_t>(clip.first_event) + clip.event_count > manifest.event_count) {
            return invalid_result(MaterialValidationError::invalid_clip);
        }
        std::uint16_t repeated{};
        for (std::uint16_t offset = 1U; offset < clip.frame_count; ++offset) {
            const MaterialFrameDefinition& previous = manifest.frames[clip.first_frame + offset - 1U];
            const MaterialFrameDefinition& current = manifest.frames[clip.first_frame + offset];
            if (previous.perceptual_hash != 0U
                && previous.perceptual_hash == current.perceptual_hash) {
                ++repeated;
            }
        }
        if (clip.frame_count > 1U && repeated * 4U > clip.frame_count) {
            return invalid_result(MaterialValidationError::repeated_frame_hash);
        }
    }
    return valid_result();
}

MaterialValidationResult validate_material_manifest_for_ecology(
    const MaterialManifestDefinition& manifest, MaterialEcology loaded_ecology) noexcept {
    const MaterialValidationResult base = validate_material_manifest(manifest);
    if (!base.valid) return base;
    for (std::size_t index = 0U; index < manifest.clip_count; ++index) {
        const MaterialEcology ecology = manifest.clips[index].ecology;
        if (ecology != MaterialEcology::common && ecology != loaded_ecology) {
            return invalid_result(MaterialValidationError::ecology_not_loaded);
        }
    }
    return valid_result();
}

}  // namespace arpg::platform
