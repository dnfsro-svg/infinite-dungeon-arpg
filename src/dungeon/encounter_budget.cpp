#include "dungeon/dungeon_rules.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace arpg::dungeon::detail {

std::uint16_t compute_abyss_encounter_budget(
    std::uint8_t normal_budget) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(normal_budget) * 3U + 1U) / 2U);
}

std::uint8_t compute_encounter_budget(
    std::uint64_t depth,
    const EncounterDirectorConfig& config) noexcept {
    if (config.depth_step == 0U || config.max_budget == 0U) {
        return 0U;
    }
    if (depth <= 1U) {
        return config.base_budget;
    }
    const std::uint64_t steps =
        (depth - 1U) / static_cast<std::uint64_t>(config.depth_step);
    const std::uint64_t increment = config.budget_per_step;
    if (increment != 0U && steps >
            (std::numeric_limits<std::uint64_t>::max)() / increment) {
        return config.max_budget;
    }
    const std::uint64_t added = steps * increment;
    const std::uint64_t base = config.base_budget;
    if (base > (std::numeric_limits<std::uint64_t>::max)() - added) {
        return config.max_budget;
    }
    const std::uint64_t budget = base + added;
    return static_cast<std::uint8_t>(std::min<std::uint64_t>(
        budget, config.max_budget));
}

}  // namespace arpg::dungeon::detail
