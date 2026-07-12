#include "dungeon/dungeon_rules.hpp"

#include <limits>

namespace arpg::dungeon {

std::optional<checkpoint::DungeonElement> element_for_exit(
    checkpoint::ExitDirection direction) noexcept {
    switch (direction) {
    case checkpoint::ExitDirection::up:
        return checkpoint::DungeonElement::fire;
    case checkpoint::ExitDirection::down:
        return checkpoint::DungeonElement::water;
    case checkpoint::ExitDirection::left:
        return checkpoint::DungeonElement::lightning;
    case checkpoint::ExitDirection::right:
        return checkpoint::DungeonElement::chaos;
    case checkpoint::ExitDirection::none:
        return std::nullopt;
    }
    return std::nullopt;
}

DungeonFault validate_rules(const DungeonRules& rules) noexcept {
    if (rules.rules_version != 1U
            || rules.hole_threshold > kProbabilityScale
            || rules.abyss_threshold > kProbabilityScale) {
        return DungeonFault::invalid_rules;
    }

    for (const std::uint32_t weight : rules.base_weights) {
        if (weight == 0U) {
            return DungeonFault::invalid_rules;
        }
    }
    return DungeonFault::none;
}

DungeonFault compute_ecology_weights(
    const DungeonRules& rules,
    const std::array<std::uint32_t, 4>& biases,
    std::array<std::uint64_t, 4>& weights,
    std::uint64_t& total) noexcept {
    const DungeonFault rules_fault = validate_rules(rules);
    if (rules_fault != DungeonFault::none) {
        return rules_fault;
    }

    std::array<std::uint64_t, 4> checked_weights{};
    std::uint64_t checked_total = 0U;
    for (std::size_t index = 0; index < checked_weights.size(); ++index) {
        const std::uint64_t base = rules.base_weights[index];
        const std::uint64_t bias = biases[index];
        const std::uint64_t increment = rules.bias_weight_increment;
        if (increment != 0U
                && bias > ((std::numeric_limits<std::uint64_t>::max)()
                    - base) / increment) {
            return DungeonFault::weight_overflow;
        }
        const std::uint64_t weight = base + bias * increment;
        if (checked_total > (std::numeric_limits<std::uint64_t>::max)()
                - weight) {
            return DungeonFault::weight_overflow;
        }
        checked_weights[index] = weight;
        checked_total += weight;
    }

    weights = checked_weights;
    total = checked_total;
    return DungeonFault::none;
}

}  // namespace arpg::dungeon
