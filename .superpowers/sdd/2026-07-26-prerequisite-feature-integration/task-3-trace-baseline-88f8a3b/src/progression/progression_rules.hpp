#pragma once

#include "progression/progression_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::progression {

inline constexpr std::size_t kMaximumLevel = 100U;
inline constexpr std::size_t kLevelThresholdCount = 99U;
inline constexpr std::size_t kMonsterExperienceCount = 8U;

struct ProgressionRules final {
    std::array<std::uint64_t, kLevelThresholdCount> experience_to_next{};
    std::array<std::uint32_t, kMonsterExperienceCount> monster_experience{};
    std::uint32_t room_clear_experience{};
};

[[nodiscard]] ProgressionRules default_progression_rules() noexcept;
[[nodiscard]] bool valid_progression_rules(
    const ProgressionRules& rules) noexcept;
[[nodiscard]] bool valid_progression_state(
    const ProgressionState& state,
    const ProgressionRules& rules) noexcept;
[[nodiscard]] ProgressionAward apply_experience(
    ProgressionState state,
    std::uint64_t award,
    const ProgressionRules& rules) noexcept;

}  // namespace arpg::progression
