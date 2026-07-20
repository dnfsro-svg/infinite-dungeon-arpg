#pragma once

#include "audio_asset_types.hpp"
#include "procedural_audio.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arpg::platform {

struct AudioSoundApi final {
    Wave (*load_wave)(const char* path){};
    bool (*wave_valid)(Wave wave){};
    Sound (*load_sound_from_wave)(Wave wave){};
    bool (*sound_valid)(Sound sound){};
    void (*unload_wave)(Wave wave){};
    void (*unload_sound)(Sound sound){};
    void (*play_sound)(Sound sound){};
    void (*stop_sound)(Sound sound){};
};

class AudioPack final {
public:
    AudioPack() noexcept;
    explicit AudioPack(AudioSoundApi api) noexcept;
    [[nodiscard]] bool load() noexcept;
    void play(AudioAssetId id) noexcept;
    void stop_all() noexcept;
    void unload() noexcept;
    [[nodiscard]] bool available(AudioAssetId id) const noexcept;
    [[nodiscard]] bool using_fallback(AudioAssetId id) const noexcept;

private:
    static constexpr std::size_t kAssetCount =
        static_cast<std::size_t>(AudioAssetId::count);

    AudioSoundApi api_{};
    ProceduralAudio procedural_{};
    std::array<Sound, kAssetCount> sounds_{};
    std::array<bool, kAssetCount> available_{};
    std::array<bool, kAssetCount> using_fallback_{};
};

}  // namespace arpg::platform
