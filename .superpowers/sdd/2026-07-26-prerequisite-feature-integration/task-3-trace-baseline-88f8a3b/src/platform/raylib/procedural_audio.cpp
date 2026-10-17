#include "procedural_audio.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {
namespace {

constexpr unsigned int kSampleRate = 22050U;
constexpr float kPi = 3.14159265358979323846F;
constexpr std::size_t kAssetCount =
    static_cast<std::size_t>(AudioAssetId::count);

constexpr std::array<unsigned int, kAssetCount> kFrameCounts{{
    1764U, 1543U, 2205U, 1984U,
    1323U, 1323U, 1323U, 2646U,
    1984U, 1764U, 2205U,
    882U, 1102U, 1764U,
}};

[[nodiscard]] constexpr bool is_known_id(AudioAssetId id) noexcept {
    return static_cast<std::size_t>(id) < kAssetCount;
}

[[nodiscard]] float envelope(std::size_t index, unsigned int count) noexcept {
    return 1.0F - static_cast<float>(index) / static_cast<float>(count);
}

void synthesize_swing(std::int16_t* samples, unsigned int count,
    float start_frequency, float end_frequency, float amplitude) noexcept {
    float phase{};
    for (std::size_t index{}; index < count; ++index) {
        const float progress = static_cast<float>(index)
            / static_cast<float>(count);
        const float frequency = start_frequency
            + (end_frequency - start_frequency) * progress;
        phase += 2.0F * kPi * frequency / static_cast<float>(kSampleRate);
        samples[index] = static_cast<std::int16_t>(
            std::sin(phase) * envelope(index, count) * amplitude);
    }
}

void synthesize_impact(std::int16_t* samples, unsigned int count,
    std::uint32_t seed, float tone_frequency, float amplitude) noexcept {
    std::uint32_t noise = seed;
    for (std::size_t index{}; index < count; ++index) {
        noise = noise * 1664525U + 1013904223U;
        const float random_sample = static_cast<float>(
            static_cast<std::int32_t>(noise >> 16U) - 32768)
            / 32768.0F;
        const float time = static_cast<float>(index)
            / static_cast<float>(kSampleRate);
        const float tone = std::sin(2.0F * kPi * tone_frequency * time);
        samples[index] = static_cast<std::int16_t>(
            (random_sample * 0.72F + tone * 0.28F)
            * envelope(index, count) * amplitude);
    }
}

void synthesize_low(std::int16_t* samples, unsigned int count,
    float frequency, float overtone, float amplitude) noexcept {
    for (std::size_t index{}; index < count; ++index) {
        const float time = static_cast<float>(index)
            / static_cast<float>(kSampleRate);
        const float body = std::sin(2.0F * kPi * frequency * time);
        const float harmonic = std::sin(
            2.0F * kPi * frequency * 2.0F * time) * overtone;
        samples[index] = static_cast<std::int16_t>(
            (body + harmonic) * envelope(index, count) * amplitude);
    }
}

void synthesize_warning(std::int16_t* samples, unsigned int count,
    float frequency, float harmonic, float amplitude) noexcept {
    for (std::size_t index{}; index < count; ++index) {
        const float time = static_cast<float>(index)
            / static_cast<float>(kSampleRate);
        const float primary = std::sin(2.0F * kPi * frequency * time);
        const float secondary = std::sin(
            2.0F * kPi * frequency * 2.0F * time) * harmonic;
        samples[index] = static_cast<std::int16_t>(
            (primary + secondary) * envelope(index, count) * amplitude);
    }
}

}  // namespace

Wave ProceduralAudio::wave(AudioAssetId id) noexcept {
    if (!is_known_id(id)) return {};

    const std::size_t asset_index = static_cast<std::size_t>(id);
    const unsigned int frame_count = kFrameCounts[asset_index];
    std::int16_t* const samples = samples_[asset_index].data();
    switch (id) {
    case AudioAssetId::swing_light_1:
        synthesize_swing(samples, frame_count, 940.0F, 240.0F, 10500.0F);
        break;
    case AudioAssetId::swing_light_2:
        synthesize_swing(samples, frame_count, 1080.0F, 310.0F, 9800.0F);
        break;
    case AudioAssetId::swing_finisher:
        synthesize_swing(samples, frame_count, 760.0F, 95.0F, 12500.0F);
        break;
    case AudioAssetId::swing_launcher:
        synthesize_swing(samples, frame_count, 260.0F, 1040.0F, 11200.0F);
        break;
    case AudioAssetId::impact_1:
        synthesize_impact(samples, frame_count, 0xA341316CU, 150.0F, 9000.0F);
        break;
    case AudioAssetId::impact_2:
        synthesize_impact(samples, frame_count, 0xC8013EA4U, 185.0F, 8700.0F);
        break;
    case AudioAssetId::impact_3:
        synthesize_impact(samples, frame_count, 0xAD90777DU, 120.0F, 9400.0F);
        break;
    case AudioAssetId::impact_low:
        synthesize_low(samples, frame_count, 72.0F, 0.22F, 10500.0F);
        break;
    case AudioAssetId::player_hurt:
        synthesize_low(samples, frame_count, 118.0F, 0.34F, 9000.0F);
        break;
    case AudioAssetId::landing:
        synthesize_low(samples, frame_count, 86.0F, 0.12F, 8000.0F);
        break;
    case AudioAssetId::enemy_defeat:
        synthesize_low(samples, frame_count, 56.0F, 0.28F, 11000.0F);
        break;
    case AudioAssetId::warning_blink:
        synthesize_warning(samples, frame_count, 880.0F, 0.0F, 9500.0F);
        break;
    case AudioAssetId::warning_chain:
        synthesize_warning(samples, frame_count, 480.0F, 0.45F, 7500.0F);
        break;
    case AudioAssetId::warning_death:
        synthesize_warning(samples, frame_count, 130.0F, 0.16F, 11000.0F);
        break;
    case AudioAssetId::count:
        return {};
    }

    Wave result{};
    result.frameCount = frame_count;
    result.sampleRate = kSampleRate;
    result.sampleSize = 16U;
    result.channels = 1U;
    result.data = samples;
    return result;
}

}  // namespace arpg::platform
