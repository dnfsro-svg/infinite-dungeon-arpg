#include "audio_pack.hpp"

#include "audio_asset_validation.hpp"
#include "audio_manifest.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {
namespace {

constexpr std::size_t kAssetCount =
    static_cast<std::size_t>(AudioAssetId::count);
constexpr unsigned int kRequiredSampleRate = 44100U;
constexpr unsigned int kRequiredSampleSize = 16U;
constexpr unsigned int kRequiredChannels = 1U;
constexpr unsigned int kMaximumFrameCount = 66150U;

[[nodiscard]] constexpr bool is_known_id(AudioAssetId id) noexcept {
    return static_cast<std::size_t>(id) < kAssetCount;
}

[[nodiscard]] constexpr bool valid_api(AudioSoundApi api) noexcept {
    return api.load_wave != nullptr && api.wave_valid != nullptr
        && api.load_sound_from_wave != nullptr && api.sound_valid != nullptr
        && api.unload_wave != nullptr && api.unload_sound != nullptr
        && api.play_sound != nullptr && api.stop_sound != nullptr;
}

[[nodiscard]] AudioSoundApi default_audio_api() noexcept {
    return {&LoadWave, &IsWaveValid, &LoadSoundFromWave, &IsSoundValid,
        &UnloadWave, &UnloadSound, &PlaySound, &StopSound};
}

[[nodiscard]] const AudioManifestEntry* find_entry(
    const AudioManifestDefinition& manifest, AudioAssetId id) noexcept {
    for (std::size_t index{}; index < manifest.entry_count; ++index) {
        if (manifest.entries[index].id == id) return &manifest.entries[index];
    }
    return nullptr;
}

[[nodiscard]] float scan_pcm_peak(Wave wave) noexcept {
    const auto* const samples = static_cast<const std::int16_t*>(wave.data);
    std::uint32_t maximum{};
    for (std::size_t index{}; index < wave.frameCount; ++index) {
        const std::int32_t sample = samples[index];
        const std::uint32_t magnitude = static_cast<std::uint32_t>(
            sample < 0 ? -sample : sample);
        if (magnitude > maximum) maximum = magnitude;
    }
    return static_cast<float>(maximum) / 32768.0F;
}

[[nodiscard]] constexpr bool safe_to_scan_pcm(Wave wave) noexcept {
    return wave.data != nullptr && wave.frameCount != 0U
        && wave.frameCount <= kMaximumFrameCount
        && wave.sampleRate == kRequiredSampleRate
        && wave.sampleSize == kRequiredSampleSize
        && wave.channels == kRequiredChannels;
}

[[nodiscard]] AudioDecodedMetadata metadata_for(Wave wave) noexcept {
    return {wave.frameCount, wave.sampleRate, wave.sampleSize, wave.channels,
        scan_pcm_peak(wave)};
}

}  // namespace

AudioPack::AudioPack() noexcept
    : AudioPack(default_audio_api()) {}

AudioPack::AudioPack(AudioSoundApi api) noexcept
    : api_(api) {}

bool AudioPack::load_fallback(
    std::size_t index, AudioAssetId id) noexcept {
    const Wave fallback_wave = procedural_.wave(id);
    if (!api_.wave_valid(fallback_wave)) return false;

    Sound fallback_sound = api_.load_sound_from_wave(fallback_wave);
    if (!api_.sound_valid(fallback_sound)) return false;

    sounds_[index] = fallback_sound;
    available_[index] = true;
    using_fallback_[index] = true;
    return true;
}

bool AudioPack::load() noexcept {
    unload();
    if (!valid_api(api_)) return false;

    const AudioManifestDefinition manifest = default_audio_manifest();
    const bool manifest_valid = validate_audio_manifest(manifest).valid;
    std::array<AudioDecodedMetadata, kAssetCount> external_metadata{};
    std::size_t external_metadata_count{};
    bool all_available = true;

    for (std::size_t index{}; index < kAssetCount; ++index) {
        const auto id = static_cast<AudioAssetId>(index);
        const AudioManifestEntry* const entry = manifest_valid
            ? find_entry(manifest, id)
            : nullptr;
        bool external_loaded{};

        if (entry != nullptr) {
            Wave external_wave = api_.load_wave(entry->path);
            if (api_.wave_valid(external_wave)) {
                if (safe_to_scan_pcm(external_wave)) {
                    const AudioDecodedMetadata metadata = metadata_for(external_wave);
                    const bool metadata_valid =
                        validate_audio_metadata(*entry, metadata).valid;
                    if (metadata_valid) {
                        external_metadata[external_metadata_count++] = metadata;
                        Sound sound = api_.load_sound_from_wave(external_wave);
                        if (api_.sound_valid(sound)) {
                            sounds_[index] = sound;
                            available_[index] = true;
                            external_loaded = true;
                        }
                    }
                }
                api_.unload_wave(external_wave);
            }
        }

        if (!external_loaded) {
            if (!load_fallback(index, id)) {
                unload();
                return false;
            }
        }
        all_available = all_available && available_[index];
    }

    if (!validate_audio_pcm_budget(
            external_metadata.data(), external_metadata_count).valid) {
        unload();
        all_available = true;
        for (std::size_t index{}; index < kAssetCount; ++index) {
            const auto id = static_cast<AudioAssetId>(index);
            if (!load_fallback(index, id)) {
                unload();
                return false;
            }
            all_available = all_available && available_[index];
        }
    }
    return all_available;
}

void AudioPack::play(AudioAssetId id) noexcept {
    if (!is_known_id(id) || api_.play_sound == nullptr
        || api_.sound_valid == nullptr) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(id);
    if (available_[index] && api_.sound_valid(sounds_[index])) {
        api_.play_sound(sounds_[index]);
    }
}

void AudioPack::stop_all() noexcept {
    if (api_.stop_sound == nullptr || api_.sound_valid == nullptr) return;
    for (std::size_t index{}; index < kAssetCount; ++index) {
        if (available_[index] && api_.sound_valid(sounds_[index])) {
            api_.stop_sound(sounds_[index]);
        }
    }
}

void AudioPack::unload() noexcept {
    for (std::size_t index{}; index < kAssetCount; ++index) {
        if (available_[index] && api_.sound_valid != nullptr
            && api_.unload_sound != nullptr && api_.sound_valid(sounds_[index])) {
            api_.unload_sound(sounds_[index]);
        }
        sounds_[index] = Sound{};
        available_[index] = false;
        using_fallback_[index] = false;
    }
}

bool AudioPack::available(AudioAssetId id) const noexcept {
    return is_known_id(id) && available_[static_cast<std::size_t>(id)];
}

bool AudioPack::using_fallback(AudioAssetId id) const noexcept {
    return is_known_id(id) && using_fallback_[static_cast<std::size_t>(id)];
}

}  // namespace arpg::platform
