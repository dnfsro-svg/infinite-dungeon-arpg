#include "progression/progression_rules.hpp"

#include <algorithm>
#include <limits>

namespace arpg::progression {

namespace {

std::uint64_t threshold_for_level(
    const ProgressionRules& rules,
    std::uint32_t level) noexcept {
    return rules.experience_to_next[static_cast<std::size_t>(level - 1U)];
}

}  // namespace

ProgressionRules default_progression_rules() noexcept {
    ProgressionRules rules{};
    rules.experience_to_next.fill(100U);
    rules.monster_experience = {{20U, 35U, 50U, 30U, 25U, 35U, 20U, 45U}};
    rules.room_clear_experience = 50U;
    return rules;
}

bool valid_progression_rules(const ProgressionRules& rules) noexcept {
    return rules.room_clear_experience != 0U
        && std::all_of(rules.experience_to_next.begin(),
            rules.experience_to_next.end(),
            [](std::uint64_t value) noexcept { return value != 0U; })
        && std::all_of(rules.monster_experience.begin(),
            rules.monster_experience.end(),
            [](std::uint32_t value) noexcept { return value != 0U; });
}

bool valid_progression_state(
    const ProgressionState& state,
    const ProgressionRules& rules) noexcept {
    if (!valid_progression_rules(rules)
        || state.level == 0U || state.level > kMaximumLevel
        || state.earned_passive_points != state.level - 1U
        || state.unspent_passive_points > state.earned_passive_points) {
        return false;
    }
    if (state.level == kMaximumLevel) {
        return state.experience == 0U;
    }
    return state.experience < threshold_for_level(rules, state.level);
}

ProgressionAward apply_experience(
    ProgressionState state,
    std::uint64_t award,
    const ProgressionRules& rules) noexcept {
    ProgressionAward result{state, 0U};
    if (!valid_progression_state(state, rules)
        || state.level == kMaximumLevel || award == 0U) {
        return result;
    }

    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    state.experience = award > maximum - state.experience
        ? maximum : state.experience + award;
    while (state.level < kMaximumLevel) {
        const std::uint64_t required = threshold_for_level(rules, state.level);
        if (state.experience < required) {
            break;
        }
        state.experience -= required;
        ++state.level;
        ++state.earned_passive_points;
        ++state.unspent_passive_points;
        ++result.levels_gained;
    }
    if (state.level == kMaximumLevel) {
        state.experience = 0U;
    }
    result.state = state;
    return result;
}

}  // namespace arpg::progression
