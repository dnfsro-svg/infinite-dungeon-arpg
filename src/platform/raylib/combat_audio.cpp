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

std::size_t warning_cue_index(AudioCue cue) noexcept {
    switch (cue) {
    case AudioCue::blink_warning: return 0U;
    case AudioCue::chain_warning: return 1U;
    case AudioCue::death_warning: return 2U;
    default: return 3U;
    }
}

}  // namespace

AudioCueMask route_audio_cues(const combat::CombatEvent& event) noexcept {
    switch (event.kind) {
    case combat::CombatEventKind::swing:
        return audio_cue_mask(AudioCue::weapon);
    case combat::CombatEventKind::hit:
        return audio_cue_mask(AudioCue::material);
    case combat::CombatEventKind::impact_summary:
        return event.feedback == combat::FeedbackLevel::heavy
            ? audio_cue_mask(AudioCue::low)
            : 0;
    case combat::CombatEventKind::player_hit:
        return audio_cue_mask(AudioCue::low);
    case combat::CombatEventKind::affix_blink_warning:
        return audio_cue_mask(AudioCue::blink_warning);
    case combat::CombatEventKind::affix_chain_warning:
        return audio_cue_mask(AudioCue::chain_warning);
    case combat::CombatEventKind::affix_death_warning:
        return audio_cue_mask(AudioCue::death_warning);
    default:
        return 0;
    }
}

bool WarningAudioThrottle::allow(AudioCue cue, std::uint64_t tick) noexcept {
    const std::size_t index = warning_cue_index(cue);
    if (index >= last_ticks_.size()) {
        return false;
    }
    if (!has_last_tick_[index] || tick >= last_ticks_[index] + 12U
        || tick < last_ticks_[index]) {
        last_ticks_[index] = tick;
        has_last_tick_[index] = true;
        return true;
    }
    return false;
}

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
    if (!ready_) {
        return;
    }
    const AudioCueMask cues = route_audio_cues(event);
    if (has_cue(cues, AudioCue::weapon)) {
        PlaySound(weapon_);
    }
    if (has_cue(cues, AudioCue::material)) {
        PlaySound(material_[material_voice_]);
        material_voice_ = (material_voice_ + 1) % material_.size();
    }
    if (has_cue(cues, AudioCue::low)) {
        PlaySound(low_);
    }
    if (has_cue(cues, AudioCue::blink_warning)
        && warning_throttle_.allow(AudioCue::blink_warning, event.tick)) {
        PlaySound(blink_warning_);
    }
    if (has_cue(cues, AudioCue::chain_warning)
        && warning_throttle_.allow(AudioCue::chain_warning, event.tick)) {
        PlaySound(chain_warning_);
    }
    if (has_cue(cues, AudioCue::death_warning)
        && warning_throttle_.allow(AudioCue::death_warning, event.tick)) {
        PlaySound(death_warning_);
    }
}

void CombatAudio::stop_all() noexcept {
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
    material_voice_ = 0;
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
    material_voice_ = 0;
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
