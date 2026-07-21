#pragma once

#include "audio_scene.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

struct UiSoundApi final {
    Wave (*load_wave)(const char* path){};
    bool (*wave_valid)(Wave wave){};
    Sound (*load_sound)(Wave wave){};
    bool (*sound_valid)(Sound sound){};
    void (*unload_wave)(Wave wave){};
    void (*unload_sound)(Sound sound){};
    void (*play)(Sound sound){};
    void (*stop)(Sound sound){};
    void (*set_volume)(Sound sound, float volume){};
};

class UiAudioPack final {
public:
    UiAudioPack() noexcept;
    explicit UiAudioPack(UiSoundApi api) noexcept;
    ~UiAudioPack() noexcept;

    UiAudioPack(const UiAudioPack&) = delete;
    UiAudioPack& operator=(const UiAudioPack&) = delete;

    [[nodiscard]] bool load() noexcept;
    void play(UiAudioCue cue) noexcept;
    void set_volume(float volume) noexcept;
    void stop_all() noexcept;
    void unload() noexcept;
    [[nodiscard]] bool available(UiAudioCue cue) const noexcept;
    [[nodiscard]] bool using_fallback(UiAudioCue cue) const noexcept;

private:
    static constexpr std::size_t kCount = static_cast<std::size_t>(UiAudioCue::count);
    static constexpr std::size_t kFallbackFrames = 2646U;
    [[nodiscard]] Wave fallback_wave(UiAudioCue cue) noexcept;
    [[nodiscard]] bool load_fallback(std::size_t index, UiAudioCue cue) noexcept;

    UiSoundApi api_{};
    std::array<Sound, kCount> sounds_{};
    std::array<bool, kCount> available_{};
    std::array<bool, kCount> fallback_{};
    std::array<std::array<std::int16_t, kFallbackFrames>, kCount> samples_{};
};

}  // namespace arpg::platform
