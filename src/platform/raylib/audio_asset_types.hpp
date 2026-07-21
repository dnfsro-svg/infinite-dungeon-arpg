#pragma once

#include <cstdint>

namespace arpg::platform {

enum class AudioAssetId : std::uint8_t {
    swing_light_1,
    swing_light_2,
    swing_finisher,
    swing_launcher,
    impact_1,
    impact_2,
    impact_3,
    impact_low,
    player_hurt,
    landing,
    enemy_defeat,
    warning_blink,
    warning_chain,
    warning_death,
    count,
};

struct AudioDecodedMetadata final {
    unsigned int frame_count{};
    unsigned int sample_rate{};
    unsigned int sample_size{};
    unsigned int channels{};
    float peak{};
};

}  // namespace arpg::platform
