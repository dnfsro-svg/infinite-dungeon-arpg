#include "game_audio.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

[[nodiscard]] bool has_cue(AudioCueMask mask, AudioCue cue) noexcept {
    return (mask & audio_cue_mask(cue)) != 0U;
}

[[nodiscard]] constexpr StreamAssetId stream_id(MusicTrack track) noexcept {
    return track == MusicTrack::combat
        ? StreamAssetId::music_combat : StreamAssetId::music_explore;
}

[[nodiscard]] constexpr StreamAssetId stream_id(AmbienceTrack track) noexcept {
    return track == AmbienceTrack::abyss
        ? StreamAssetId::ambience_abyss : StreamAssetId::ambience_room;
}

}  // namespace

GameAudio::~GameAudio() noexcept { shutdown(); }

bool GameAudio::initialize() noexcept {
    if (ready_) return true;
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        owns_device_ = IsAudioDeviceReady();
    }
    if (!IsAudioDeviceReady()) return false;

    const bool sfx_ready = sfx_.load();
    const bool streams_ready = streams_.load();
    const bool ui_ready = ui_.load();
    ready_ = sfx_ready && streams_ready && ui_ready;
    if (!ready_) shutdown();
    return ready_;
}

void GameAudio::consume_event(const combat::CombatEvent& event) noexcept {
    if (event.kind == combat::CombatEventKind::reset) {
        clear_combat_transients();
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
            sfx_.play(selection_.select(cue));
        }
    }
}

void GameAudio::update(AudioSceneInput input, AudioBusLevels levels,
    UiAudioCueMask ui_cues, float elapsed_seconds) noexcept {
    if (!ready_) return;
    const AudioFramePlan plan = scene_.update(input, levels, elapsed_seconds);
    if (plan.music_changed) streams_.play(stream_id(plan.music));
    if (plan.ambience_changed) streams_.play(stream_id(plan.ambience));

    for (std::size_t index{}; index < plan.music_gain.size(); ++index) {
        streams_.set_volume(stream_id(static_cast<MusicTrack>(index)),
            plan.music_gain[index]);
    }
    for (std::size_t index{}; index < plan.ambience_gain.size(); ++index) {
        streams_.set_volume(stream_id(static_cast<AmbienceTrack>(index)),
            plan.ambience_gain[index]);
    }
    sfx_.set_volume(plan.sfx_gain);
    ui_.set_volume(plan.ui_gain);

    const UiAudioCueMask rising = ui_gate_.rising(ui_cues);
    for (std::size_t index{};
         index < static_cast<std::size_t>(UiAudioCue::count); ++index) {
        const auto cue = static_cast<UiAudioCue>(index);
        if ((rising & ui_audio_cue_mask(cue)) != 0U) ui_.play(cue);
    }
    streams_.update();
}

void GameAudio::clear_combat_transients() noexcept {
    sfx_.stop_all();
    selection_.reset();
    playback_budget_.reset();
}

void GameAudio::stop_all() noexcept {
    clear_combat_transients();
    streams_.stop_all();
    ui_.stop_all();
    scene_.reset();
    ui_gate_.reset();
}

void GameAudio::shutdown() noexcept {
    stop_all();
    ui_.unload();
    streams_.unload();
    sfx_.unload();
    ready_ = false;
    if (owns_device_ && IsAudioDeviceReady()) CloseAudioDevice();
    owns_device_ = false;
}

bool GameAudio::ready() const noexcept { return ready_; }

}  // namespace arpg::platform
