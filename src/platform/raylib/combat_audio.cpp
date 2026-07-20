#include "combat_audio.hpp"

#include <raylib.h>

#include <array>

namespace arpg::platform {
namespace {

[[nodiscard]] bool has_cue(AudioCueMask mask, AudioCue cue) noexcept {
    return (mask & audio_cue_mask(cue)) != 0U;
}

}  // namespace

CombatAudio::~CombatAudio() noexcept {
    shutdown();
}

bool CombatAudio::initialize() noexcept {
    if (ready_) return true;

    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        owns_device_ = IsAudioDeviceReady();
    }
    if (!IsAudioDeviceReady()) return false;

    ready_ = pack_.load();
    if (!ready_) shutdown();
    return ready_;
}

void CombatAudio::consume_event(const combat::CombatEvent& event) noexcept {
    if (event.kind == combat::CombatEventKind::reset) {
        pack_.stop_all();
        selection_.reset();
        playback_budget_.reset();
        return;
    }
    if (!ready_) return;

    const AudioPlan plan = route_audio_plan(event);
    constexpr std::array<AudioCue, 11> kCues{{
        AudioCue::swing_light, AudioCue::swing_finisher,
        AudioCue::swing_launcher, AudioCue::impact, AudioCue::impact_low,
        AudioCue::player_hurt, AudioCue::landing, AudioCue::enemy_defeat,
        AudioCue::warning_blink, AudioCue::warning_chain,
        AudioCue::warning_death,
    }};
    for (const AudioCue cue : kCues) {
        if (has_cue(plan.cues, cue)
            && playback_budget_.allow(cue, event.tick)) {
            pack_.play(selection_.select(cue));
        }
    }
}

void CombatAudio::stop_all() noexcept {
    pack_.stop_all();
    selection_.reset();
    playback_budget_.reset();
}

void CombatAudio::shutdown() noexcept {
    pack_.unload();
    selection_.reset();
    playback_budget_.reset();
    ready_ = false;
    if (owns_device_ && IsAudioDeviceReady()) CloseAudioDevice();
    owns_device_ = false;
}

bool CombatAudio::ready() const noexcept {
    return ready_;
}

}  // namespace arpg::platform
