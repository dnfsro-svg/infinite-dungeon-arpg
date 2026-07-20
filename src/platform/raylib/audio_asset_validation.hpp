#pragma once

#include "audio_manifest.hpp"

#include <cstdint>

namespace arpg::platform {

enum class AudioValidationError : std::uint8_t {
    none,
    invalid_manifest,
    invalid_id,
    duplicate_id,
    invalid_path,
    invalid_format,
    invalid_duration,
    invalid_peak,
    pcm_budget_exceeded,
};

struct AudioValidationResult final {
    bool valid{};
    AudioValidationError error{AudioValidationError::none};
};

[[nodiscard]] AudioValidationResult validate_audio_manifest(
    const AudioManifestDefinition& manifest) noexcept;

[[nodiscard]] AudioValidationResult validate_audio_metadata(
    const AudioManifestEntry& entry,
    const AudioDecodedMetadata& metadata) noexcept;

[[nodiscard]] AudioValidationResult validate_audio_pcm_budget(
    const AudioDecodedMetadata* metadata, std::size_t count) noexcept;

}  // namespace arpg::platform
