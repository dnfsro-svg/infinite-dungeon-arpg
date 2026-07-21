#include "audio_scene.hpp"

#include <algorithm>
#include <cstddef>

namespace arpg::platform {
namespace {

[[nodiscard]] float fraction(std::uint8_t percent) noexcept {
    return static_cast<float>(std::min<std::uint8_t>(percent, 100U)) * 0.01F;
}

void approach_targets(float elapsed_seconds, float duration_seconds,
    std::size_t target, float* values, std::size_t count) noexcept {
    const float step = duration_seconds > 0.0F
        ? std::clamp(elapsed_seconds / duration_seconds, 0.0F, 1.0F)
        : 1.0F;
    for (std::size_t index{}; index < count; ++index) {
        const float wanted = index == target ? 1.0F : 0.0F;
        if (values[index] < wanted) values[index] = std::min(wanted, values[index] + step);
        else if (values[index] > wanted) values[index] = std::max(wanted, values[index] - step);
    }
}

}  // namespace

AudioFramePlan AudioSceneState::update(AudioSceneInput input,
    AudioBusLevels levels, float elapsed_seconds) noexcept {
    const MusicTrack wanted_music = input.combat_active
        ? MusicTrack::combat : MusicTrack::explore;
    const AmbienceTrack wanted_ambience = input.abyss_room
        ? AmbienceTrack::abyss : AmbienceTrack::room;

    AudioFramePlan plan{};
    plan.music_changed = !initialized_ || wanted_music != music_;
    plan.ambience_changed = !initialized_ || wanted_ambience != ambience_;
    music_ = wanted_music;
    ambience_ = wanted_ambience;

    if (!initialized_) {
        music_mix_[static_cast<std::size_t>(music_)] = 1.0F;
        ambience_mix_[static_cast<std::size_t>(ambience_)] = 1.0F;
        initialized_ = true;
    } else {
        approach_targets(elapsed_seconds, 0.40F,
            static_cast<std::size_t>(music_), music_mix_.data(), music_mix_.size());
        approach_targets(elapsed_seconds, 0.25F,
            static_cast<std::size_t>(ambience_), ambience_mix_.data(), ambience_mix_.size());
    }

    const float master = fraction(levels.master_percent);
    const float duck = input.death_overlay ? 0.35F : (input.paused ? 0.55F : 1.0F);
    const float music_bus = master * fraction(levels.music_percent) * duck;
    const float ambience_bus = master * fraction(levels.ambience_percent) * duck;
    plan.music = music_;
    plan.ambience = ambience_;
    for (std::size_t index{}; index < music_mix_.size(); ++index) {
        plan.music_gain[index] = music_mix_[index] * music_bus;
    }
    for (std::size_t index{}; index < ambience_mix_.size(); ++index) {
        plan.ambience_gain[index] = ambience_mix_[index] * ambience_bus;
    }
    plan.sfx_gain = master * fraction(levels.sfx_percent);
    plan.ui_gain = master * fraction(levels.ui_percent);
    return plan;
}

void AudioSceneState::reset() noexcept {
    *this = {};
}

UiAudioCueMask UiAudioCueGate::rising(UiAudioCueMask active) noexcept {
    constexpr UiAudioCueMask kKnown = static_cast<UiAudioCueMask>(
        (1U << static_cast<std::uint8_t>(UiAudioCue::count)) - 1U);
    active = static_cast<UiAudioCueMask>(active & kKnown);
    const UiAudioCueMask result = static_cast<UiAudioCueMask>(active & ~previous_);
    previous_ = active;
    return result;
}

void UiAudioCueGate::reset() noexcept {
    previous_ = 0U;
}

}  // namespace arpg::platform
