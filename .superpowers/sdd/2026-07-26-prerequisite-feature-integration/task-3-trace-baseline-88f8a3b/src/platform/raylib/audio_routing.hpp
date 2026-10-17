#pragma once

#include "audio_asset_types.hpp"
#include "combat/combat_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::platform {

enum class AudioCue : std::uint16_t {
    swing_light = 1U << 0U,
    swing_finisher = 1U << 1U,
    swing_launcher = 1U << 2U,
    impact = 1U << 3U,
    impact_low = 1U << 4U,
    player_hurt = 1U << 5U,
    landing = 1U << 6U,
    enemy_defeat = 1U << 7U,
    warning_blink = 1U << 8U,
    warning_chain = 1U << 9U,
    warning_death = 1U << 10U,
};

using AudioCueMask = std::uint16_t;

[[nodiscard]] constexpr AudioCueMask audio_cue_mask(AudioCue cue) noexcept {
    return static_cast<AudioCueMask>(cue);
}

struct AudioPlan final {
    AudioCueMask cues{};
};

[[nodiscard]] AudioPlan route_audio_plan(
    const combat::CombatEvent& event) noexcept;

class AudioSelectionState final {
public:
    [[nodiscard]] AudioAssetId select(AudioCue cue) noexcept;
    void reset() noexcept;

private:
    std::uint8_t swing_light_variant_{};
    std::uint8_t impact_variant_{};
};

class AudioPlaybackBudget final {
public:
    [[nodiscard]] bool allow(AudioCue cue, std::uint64_t tick) noexcept;
    void reset() noexcept;

private:
    static constexpr std::size_t kCueCategoryCount = 9U;
    static constexpr std::size_t kWarningCount = 3U;

    std::array<std::uint8_t, kCueCategoryCount> counts_{};
    std::array<std::uint64_t, kWarningCount> last_warning_ticks_{};
    std::array<bool, kWarningCount> has_last_warning_tick_{};
    std::uint64_t counted_tick_{};
    bool has_counted_tick_{};
};

}  // namespace arpg::platform
