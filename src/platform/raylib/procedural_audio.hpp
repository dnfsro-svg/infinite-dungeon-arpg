#pragma once

#include "audio_asset_types.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

class ProceduralAudio final {
public:
    [[nodiscard]] Wave wave(AudioAssetId id) noexcept;

private:
    static constexpr std::size_t kMaximumSampleCount = 2646U;
    static constexpr std::size_t kAssetCount =
        static_cast<std::size_t>(AudioAssetId::count);

    std::array<std::array<std::int16_t, kMaximumSampleCount>, kAssetCount>
        samples_{};
};

}  // namespace arpg::platform
