#pragma once

#include <array>
#include <cstdint>

namespace arpg::platform {

enum class MusicTrack : std::uint8_t { explore, combat, count };
enum class AmbienceTrack : std::uint8_t { room, abyss, count };
enum class UiAudioCue : std::uint8_t { navigate, confirm, cancel, open, close, reward, count };
using UiAudioCueMask = std::uint8_t;

[[nodiscard]] constexpr UiAudioCueMask ui_audio_cue_mask(UiAudioCue cue) noexcept {
    return cue < UiAudioCue::count
        ? static_cast<UiAudioCueMask>(1U << static_cast<std::uint8_t>(cue))
        : 0U;
}

struct AudioSceneInput final {
    bool combat_active{};
    bool abyss_room{};
    bool paused{};
    bool death_overlay{};
};

struct AudioBusLevels final {
    std::uint8_t master_percent{100};
    std::uint8_t sfx_percent{100};
    std::uint8_t music_percent{45};
    std::uint8_t ambience_percent{35};
    std::uint8_t ui_percent{80};
};

struct AudioFramePlan final {
    MusicTrack music{MusicTrack::explore};
    AmbienceTrack ambience{AmbienceTrack::room};
    bool music_changed{};
    bool ambience_changed{};
    std::array<float, static_cast<std::size_t>(MusicTrack::count)> music_gain{};
    std::array<float, static_cast<std::size_t>(AmbienceTrack::count)> ambience_gain{};
    float sfx_gain{};
    float ui_gain{};
};

class AudioSceneState final {
public:
    [[nodiscard]] AudioFramePlan update(
        AudioSceneInput input,
        AudioBusLevels levels,
        float elapsed_seconds) noexcept;
    void reset() noexcept;

private:
    MusicTrack music_{MusicTrack::explore};
    AmbienceTrack ambience_{AmbienceTrack::room};
    std::array<float, static_cast<std::size_t>(MusicTrack::count)> music_mix_{};
    std::array<float, static_cast<std::size_t>(AmbienceTrack::count)> ambience_mix_{};
    bool initialized_{};
};

class UiAudioCueGate final {
public:
    [[nodiscard]] UiAudioCueMask rising(UiAudioCueMask active) noexcept;
    void reset() noexcept;

private:
    UiAudioCueMask previous_{};
};

}  // namespace arpg::platform
