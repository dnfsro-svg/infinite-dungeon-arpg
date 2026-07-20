#include "audio_asset_validation.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::platform {
namespace {

constexpr std::size_t kExpectedAudioAssetCount =
    static_cast<std::size_t>(AudioAssetId::count);
constexpr std::size_t kMaximumPcmBytes = 8U * 1024U * 1024U;
constexpr unsigned int kRequiredSampleRate = 44100U;
constexpr unsigned int kRequiredSampleSize = 16U;
constexpr unsigned int kRequiredChannels = 1U;
constexpr unsigned int kMaximumFrameCount = 66150U;
constexpr char kAudioPathPrefix[] = "assets/stage14/audio/";

[[nodiscard]] constexpr AudioValidationResult valid_result() noexcept {
    return {true, AudioValidationError::none};
}

[[nodiscard]] constexpr AudioValidationResult invalid_result(
    AudioValidationError error) noexcept {
    return {false, error};
}

[[nodiscard]] constexpr bool is_known_id(AudioAssetId id) noexcept {
    return static_cast<std::size_t>(id) < kExpectedAudioAssetCount;
}

[[nodiscard]] bool has_audio_path_prefix(const char* path) noexcept {
    if (path == nullptr) return false;
    for (std::size_t index = 0U; kAudioPathPrefix[index] != '\0'; ++index) {
        const char character = path[index];
        if (character == '\0' || character != kAudioPathPrefix[index]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_safe_audio_path(const char* path) noexcept {
    if (!has_audio_path_prefix(path)) return false;

    const std::size_t prefix_size = sizeof(kAudioPathPrefix) - 1U;
    const char* segment = path + prefix_size;
    if (*segment == '\0') return false;

    for (const char* current = segment;; ++current) {
        const char character = *current;
        if (character == '\\') return false;
        if (character != '/' && character != '\0') continue;

        const std::size_t segment_size =
            static_cast<std::size_t>(current - segment);
        if (segment_size == 0U
            || (segment_size == 1U && segment[0] == '.')
            || (segment_size == 2U && segment[0] == '.' && segment[1] == '.')) {
            return false;
        }
        if (character == '\0') return true;
        segment = current + 1;
    }
}

[[nodiscard]] AudioValidationResult validate_entry(
    const AudioManifestEntry& entry) noexcept {
    if (!is_known_id(entry.id)) return invalid_result(AudioValidationError::invalid_id);
    if (!is_safe_audio_path(entry.path)) {
        return invalid_result(AudioValidationError::invalid_path);
    }
    return valid_result();
}

[[nodiscard]] bool pcm_byte_count(const AudioDecodedMetadata& metadata,
    std::size_t& byte_count) noexcept {
    if (metadata.sample_size % 8U != 0U) return false;

    const std::size_t factors[] = {
        static_cast<std::size_t>(metadata.frame_count),
        static_cast<std::size_t>(metadata.channels),
        static_cast<std::size_t>(metadata.sample_size / 8U),
    };
    byte_count = 1U;
    for (const std::size_t factor : factors) {
        if (factor != 0U
            && byte_count > std::numeric_limits<std::size_t>::max() / factor) {
            return false;
        }
        byte_count *= factor;
    }
    return true;
}

[[nodiscard]] AudioValidationResult validate_metadata_basics(
    const AudioManifestEntry& entry,
    const AudioDecodedMetadata& metadata) noexcept {
    const AudioValidationResult entry_result = validate_entry(entry);
    if (!entry_result.valid) return entry_result;
    if (metadata.sample_rate != kRequiredSampleRate
        || metadata.sample_size != kRequiredSampleSize
        || metadata.channels != kRequiredChannels) {
        return invalid_result(AudioValidationError::invalid_format);
    }
    if (!(metadata.peak > 0.0F && metadata.peak < 1.0F)) {
        return invalid_result(AudioValidationError::invalid_peak);
    }
    std::size_t unused_byte_count{};
    if (!pcm_byte_count(metadata, unused_byte_count)) {
        return invalid_result(AudioValidationError::invalid_format);
    }
    return valid_result();
}

}  // namespace

AudioValidationResult validate_audio_metadata(const AudioManifestEntry& entry,
    const AudioDecodedMetadata& metadata) noexcept {
    const AudioValidationResult basic_result = validate_metadata_basics(entry, metadata);
    if (!basic_result.valid) return basic_result;
    if (metadata.frame_count == 0U || metadata.frame_count > kMaximumFrameCount) {
        return invalid_result(AudioValidationError::invalid_duration);
    }
    return valid_result();
}

AudioValidationResult validate_audio_pcm_budget(
    const AudioDecodedMetadata* metadata, std::size_t count) noexcept {
    if (count != 0U && metadata == nullptr) {
        return invalid_result(AudioValidationError::invalid_manifest);
    }

    std::size_t total_pcm_bytes{};
    for (std::size_t index = 0U; index < count; ++index) {
        std::size_t pcm_bytes{};
        if (!pcm_byte_count(metadata[index], pcm_bytes)
            || pcm_bytes > kMaximumPcmBytes - total_pcm_bytes) {
            return invalid_result(AudioValidationError::pcm_budget_exceeded);
        }
        total_pcm_bytes += pcm_bytes;
    }
    return valid_result();
}

AudioValidationResult validate_audio_manifest(
    const AudioManifestDefinition& manifest) noexcept {
    if (manifest.entries == nullptr || manifest.entry_count != kExpectedAudioAssetCount) {
        return invalid_result(AudioValidationError::invalid_manifest);
    }

    for (std::size_t index = 0U; index < manifest.entry_count; ++index) {
        const AudioValidationResult entry_result = validate_entry(manifest.entries[index]);
        if (!entry_result.valid) return entry_result;
        for (std::size_t prior = 0U; prior < index; ++prior) {
            if (manifest.entries[prior].id == manifest.entries[index].id) {
                return invalid_result(AudioValidationError::duplicate_id);
            }
        }
    }

    if (manifest.decoded_metadata == nullptr) return valid_result();

    for (std::size_t index = 0U; index < manifest.entry_count; ++index) {
        const AudioValidationResult metadata_result = validate_audio_metadata(
            manifest.entries[index], manifest.decoded_metadata[index]);
        if (!metadata_result.valid) return metadata_result;
    }
    return validate_audio_pcm_budget(
        manifest.decoded_metadata, manifest.entry_count);
}

}  // namespace arpg::platform
