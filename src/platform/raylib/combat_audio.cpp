#include "combat_audio.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {
namespace {

constexpr float kPi = 3.14159265358979323846F;

template <std::size_t N>
Wave make_wave(std::array<std::int16_t, N>& samples) noexcept {
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(N);
    wave.sampleRate = 22050;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples.data();
    return wave;
}

bool has_cue(AudioCueMask mask, AudioCue cue) noexcept {
    return (mask & audio_cue_mask(cue)) != 0;
}

Sound* sound_for(AudioAssetId id, Sound& weapon,
    std::array<Sound, 3>& material, Sound& low, Sound& blink_warning,
    Sound& chain_warning, Sound& death_warning) noexcept {
    switch (id) {
    case AudioAssetId::swing_light_1:
    case AudioAssetId::swing_light_2:
    case AudioAssetId::swing_finisher:
    case AudioAssetId::swing_launcher:
        return &weapon;
    case AudioAssetId::impact_1: return &material[0];
    case AudioAssetId::impact_2: return &material[1];
    case AudioAssetId::impact_3: return &material[2];
    case AudioAssetId::impact_low:
    case AudioAssetId::player_hurt:
    case AudioAssetId::landing:
    case AudioAssetId::enemy_defeat:
        return &low;
    case AudioAssetId::warning_blink: return &blink_warning;
    case AudioAssetId::warning_chain: return &chain_warning;
    case AudioAssetId::warning_death: return &death_warning;
    case AudioAssetId::count: return nullptr;
    }
    return nullptr;
}

}  // namespace

CombatAudio::~CombatAudio() noexcept {
    shutdown();
}

bool CombatAudio::initialize() noexcept {
    if (ready_) {
        return true;
    }

    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        owns_device_ = IsAudioDeviceReady();
    }
    if (!IsAudioDeviceReady()) {
        return false;
    }

    float phase = 0.0F;
    for (std::size_t index = 0; index < weapon_samples_.size(); ++index) {
        const float progress = static_cast<float>(index)
            / static_cast<float>(weapon_samples_.size());
        const float frequency = 900.0F - 680.0F * progress;
        phase += 2.0F * kPi * frequency / static_cast<float>(kSampleRate);
        const float envelope = 1.0F - progress;
        weapon_samples_[index] = static_cast<std::int16_t>(
            std::sin(phase) * envelope * 11000.0F);
    }

    std::uint32_t noise = 0xA341316CU;
    for (std::size_t index = 0; index < material_samples_.size(); ++index) {
        noise = noise * 1664525U + 1013904223U;
        const float sample = static_cast<float>(
            static_cast<std::int32_t>(noise >> 16U) - 32768)
            / 32768.0F;
        const float envelope = 1.0F
            - static_cast<float>(index)
                / static_cast<float>(material_samples_.size());
        material_samples_[index] = static_cast<std::int16_t>(
            sample * envelope * 9000.0F);
    }

    for (std::size_t index = 0; index < low_samples_.size(); ++index) {
        const float time = static_cast<float>(index)
            / static_cast<float>(kSampleRate);
        const float envelope = 1.0F
            - static_cast<float>(index)
                / static_cast<float>(low_samples_.size());
        low_samples_[index] = static_cast<std::int16_t>(
            std::sin(2.0F * kPi * 80.0F * time)
            * envelope * 12000.0F);
    }

    for (std::size_t index = 0; index < blink_warning_samples_.size(); ++index) {
        const float progress = static_cast<float>(index)
            / static_cast<float>(blink_warning_samples_.size());
        blink_warning_samples_[index] = static_cast<std::int16_t>(
            std::sin(2.0F * kPi * 880.0F * static_cast<float>(index)
                / static_cast<float>(kSampleRate))
            * (1.0F - progress) * 9500.0F);
    }
    for (std::size_t index = 0; index < chain_warning_samples_.size(); ++index) {
        const float progress = static_cast<float>(index)
            / static_cast<float>(chain_warning_samples_.size());
        const float time = static_cast<float>(index) / static_cast<float>(kSampleRate);
        chain_warning_samples_[index] = static_cast<std::int16_t>(
            (std::sin(2.0F * kPi * 480.0F * time)
                + std::sin(2.0F * kPi * 960.0F * time) * 0.45F)
            * (1.0F - progress) * 7500.0F);
    }
    for (std::size_t index = 0; index < death_warning_samples_.size(); ++index) {
        const float progress = static_cast<float>(index)
            / static_cast<float>(death_warning_samples_.size());
        death_warning_samples_[index] = static_cast<std::int16_t>(
            std::sin(2.0F * kPi * 130.0F * static_cast<float>(index)
                / static_cast<float>(kSampleRate))
            * (1.0F - progress) * 11500.0F);
    }

    weapon_ = LoadSoundFromWave(make_wave(weapon_samples_));
    for (Sound& voice : material_) {
        voice = LoadSoundFromWave(make_wave(material_samples_));
    }
    low_ = LoadSoundFromWave(make_wave(low_samples_));
    blink_warning_ = LoadSoundFromWave(make_wave(blink_warning_samples_));
    chain_warning_ = LoadSoundFromWave(make_wave(chain_warning_samples_));
    death_warning_ = LoadSoundFromWave(make_wave(death_warning_samples_));
    ready_ = IsSoundValid(weapon_) && IsSoundValid(low_)
        && IsSoundValid(blink_warning_) && IsSoundValid(chain_warning_)
        && IsSoundValid(death_warning_);
    for (const Sound& voice : material_) {
        ready_ = ready_ && IsSoundValid(voice);
    }
    if (!ready_) {
        shutdown();
    }
    return ready_;
}

void CombatAudio::consume_event(
    const combat::CombatEvent& event) noexcept {
    if (event.kind == combat::CombatEventKind::reset) {
        selection_.reset();
        playback_budget_.reset();
        return;
    }
    if (!ready_) {
        return;
    }
    const AudioPlan plan = route_audio_plan(event);
    constexpr std::array<AudioCue, 11> kCues{{
        AudioCue::swing_light, AudioCue::swing_finisher,
        AudioCue::swing_launcher, AudioCue::impact, AudioCue::impact_low,
        AudioCue::player_hurt, AudioCue::landing, AudioCue::enemy_defeat,
        AudioCue::warning_blink, AudioCue::warning_chain,
        AudioCue::warning_death,
    }};
    for (const AudioCue cue : kCues) {
        if (!has_cue(plan.cues, cue) || !playback_budget_.allow(cue, event.tick)) {
            continue;
        }
        Sound* const sound = sound_for(selection_.select(cue), weapon_, material_,
            low_, blink_warning_, chain_warning_, death_warning_);
        if (sound != nullptr) {
            PlaySound(*sound);
        }
    }
}

void CombatAudio::stop_all() noexcept {
    selection_.reset();
    playback_budget_.reset();
    if (!ready_) {
        return;
    }
    StopSound(weapon_);
    for (const Sound& voice : material_) {
        StopSound(voice);
    }
    StopSound(low_);
    StopSound(blink_warning_);
    StopSound(chain_warning_);
    StopSound(death_warning_);
}

void CombatAudio::shutdown() noexcept {
    if (IsSoundValid(weapon_)) {
        UnloadSound(weapon_);
    }
    for (Sound& voice : material_) {
        if (IsSoundValid(voice)) {
            UnloadSound(voice);
        }
        voice = Sound{};
    }
    if (IsSoundValid(low_)) {
        UnloadSound(low_);
    }
    if (IsSoundValid(blink_warning_)) {
        UnloadSound(blink_warning_);
    }
    if (IsSoundValid(chain_warning_)) {
        UnloadSound(chain_warning_);
    }
    if (IsSoundValid(death_warning_)) {
        UnloadSound(death_warning_);
    }
    weapon_ = Sound{};
    low_ = Sound{};
    blink_warning_ = Sound{};
    chain_warning_ = Sound{};
    death_warning_ = Sound{};
    selection_.reset();
    playback_budget_.reset();
    ready_ = false;
    if (owns_device_ && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    owns_device_ = false;
}

bool CombatAudio::ready() const noexcept {
    return ready_;
}

}  // namespace arpg::platform
