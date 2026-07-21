#include "ui_audio_pack.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace arpg::platform {
namespace {

constexpr std::array<const char*, static_cast<std::size_t>(UiAudioCue::count)> kPaths{{
    "assets/stage15/audio/ui-navigate.wav",
    "assets/stage15/audio/ui-confirm.wav",
    "assets/stage15/audio/ui-cancel.wav",
    "assets/stage15/audio/ui-open.wav",
    "assets/stage15/audio/ui-close.wav",
    "assets/stage15/audio/ui-reward.wav",
}};
constexpr unsigned int kRate = 22050U;
constexpr float kPi = 3.14159265358979323846F;

[[nodiscard]] constexpr bool known(UiAudioCue cue) noexcept {
    return static_cast<std::size_t>(cue) < kPaths.size();
}

[[nodiscard]] constexpr bool valid_api(UiSoundApi api) noexcept {
    return api.load_wave && api.wave_valid && api.load_sound && api.sound_valid
        && api.unload_wave && api.unload_sound && api.play && api.stop && api.set_volume;
}

[[nodiscard]] UiSoundApi native_api() noexcept {
    return {&LoadWave, &IsWaveValid, &LoadSoundFromWave, &IsSoundValid,
        &UnloadWave, &UnloadSound, &PlaySound, &StopSound, &SetSoundVolume};
}

[[nodiscard]] bool accepted_external(Wave wave) noexcept {
    return wave.data && wave.frameCount > 0U && wave.frameCount <= 33075U
        && wave.sampleRate == 44100U && wave.sampleSize == 16U && wave.channels == 1U;
}

}  // namespace

UiAudioPack::UiAudioPack() noexcept : UiAudioPack(native_api()) {}
UiAudioPack::UiAudioPack(UiSoundApi api) noexcept : api_(api) {}
UiAudioPack::~UiAudioPack() noexcept { unload(); }

Wave UiAudioPack::fallback_wave(UiAudioCue cue) noexcept {
    if (!known(cue)) return {};
    const auto index = static_cast<std::size_t>(cue);
    const unsigned int frames = 882U + static_cast<unsigned int>(index) * 220U;
    const float base = 330.0F + static_cast<float>(index) * 95.0F;
    auto& samples = samples_[index];
    for (unsigned int frame{}; frame < frames; ++frame) {
        const float t = static_cast<float>(frame) / static_cast<float>(kRate);
        const float envelope = 1.0F - static_cast<float>(frame) / static_cast<float>(frames);
        samples[frame] = static_cast<std::int16_t>(
            std::sin(2.0F * kPi * base * t) * envelope * 8500.0F);
    }
    return {frames, kRate, 16U, 1U, samples.data()};
}

bool UiAudioPack::load_fallback(std::size_t index, UiAudioCue cue) noexcept {
    const Wave wave = fallback_wave(cue);
    if (!api_.wave_valid(wave)) return false;
    const Sound sound = api_.load_sound(wave);
    if (!api_.sound_valid(sound)) return false;
    sounds_[index] = sound;
    available_[index] = true;
    fallback_[index] = true;
    return true;
}

bool UiAudioPack::load() noexcept {
    unload();
    if (!valid_api(api_)) return false;
    for (std::size_t index{}; index < kCount; ++index) {
        const auto cue = static_cast<UiAudioCue>(index);
        Wave wave = api_.load_wave(kPaths[index]);
        bool loaded{};
        if (api_.wave_valid(wave)) {
            if (accepted_external(wave)) {
                const Sound sound = api_.load_sound(wave);
                if (api_.sound_valid(sound)) {
                    sounds_[index] = sound;
                    available_[index] = true;
                    loaded = true;
                }
            }
            api_.unload_wave(wave);
        }
        if (!loaded && !load_fallback(index, cue)) {
            unload();
            return false;
        }
    }
    return true;
}

void UiAudioPack::play(UiAudioCue cue) noexcept {
    if (!known(cue) || !valid_api(api_)) return;
    const auto index = static_cast<std::size_t>(cue);
    if (available_[index] && api_.sound_valid(sounds_[index])) api_.play(sounds_[index]);
}

void UiAudioPack::set_volume(float volume) noexcept {
    if (!valid_api(api_)) return;
    volume = std::clamp(volume, 0.0F, 1.0F);
    for (std::size_t index{}; index < kCount; ++index) {
        if (available_[index] && api_.sound_valid(sounds_[index])) api_.set_volume(sounds_[index], volume);
    }
}

void UiAudioPack::stop_all() noexcept {
    if (!valid_api(api_)) return;
    for (std::size_t index{}; index < kCount; ++index) {
        if (available_[index] && api_.sound_valid(sounds_[index])) api_.stop(sounds_[index]);
    }
}

void UiAudioPack::unload() noexcept {
    for (std::size_t index{}; index < kCount; ++index) {
        if (available_[index] && api_.sound_valid && api_.unload_sound
            && api_.sound_valid(sounds_[index])) api_.unload_sound(sounds_[index]);
        sounds_[index] = {};
        available_[index] = false;
        fallback_[index] = false;
    }
}

bool UiAudioPack::available(UiAudioCue cue) const noexcept {
    return known(cue) && available_[static_cast<std::size_t>(cue)];
}

bool UiAudioPack::using_fallback(UiAudioCue cue) const noexcept {
    return known(cue) && fallback_[static_cast<std::size_t>(cue)];
}

}  // namespace arpg::platform
