#pragma once

#include "audio_pack.hpp"
#include "audio_routing.hpp"
#include "audio_scene.hpp"
#include "stream_pack.hpp"
#include "ui_audio_pack.hpp"

namespace arpg::platform {

class GameAudio final {
public:
    GameAudio() noexcept = default;
    ~GameAudio() noexcept;

    GameAudio(const GameAudio&) = delete;
    GameAudio& operator=(const GameAudio&) = delete;
    GameAudio(GameAudio&&) = delete;
    GameAudio& operator=(GameAudio&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void consume_event(const combat::CombatEvent& event) noexcept;
    void update(AudioSceneInput scene, AudioBusLevels levels,
        UiAudioCueMask ui_cues, float elapsed_seconds) noexcept;
    void clear_combat_transients() noexcept;
    void stop_all() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool ready() const noexcept;

private:
    AudioPack sfx_{};
    StreamPack streams_{};
    UiAudioPack ui_{};
    AudioSelectionState selection_{};
    AudioPlaybackBudget playback_budget_{};
    AudioSceneState scene_{};
    UiAudioCueGate ui_gate_{};
    bool ready_{};
    bool owns_device_{};
};

}  // namespace arpg::platform
