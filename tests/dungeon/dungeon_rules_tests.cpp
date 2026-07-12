#include "test_framework.hpp"

#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace {

using arpg::dungeon::DungeonFault;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::compute_ecology_weights;
using arpg::dungeon::element_for_exit;
using arpg::dungeon::validate_rules;
namespace checkpoint = arpg::dungeon::checkpoint;
using checkpoint::DungeonElement;
using checkpoint::ExitDirection;

static_assert(static_cast<std::uint8_t>(ExitDirection::up) == 0U);
static_assert(static_cast<std::uint8_t>(ExitDirection::none) == 0xFFU);
static_assert(static_cast<std::uint8_t>(checkpoint::TransitionKind::descent) == 1U);

arpg::test::Failure exit_directions_map_to_elements() noexcept {
    ARPG_REQUIRE(element_for_exit(ExitDirection::up).value() == DungeonElement::fire);
    ARPG_REQUIRE(element_for_exit(ExitDirection::down).value() == DungeonElement::water);
    ARPG_REQUIRE(element_for_exit(ExitDirection::left).value() == DungeonElement::lightning);
    ARPG_REQUIRE(element_for_exit(ExitDirection::right).value() == DungeonElement::chaos);
    ARPG_REQUIRE(!element_for_exit(ExitDirection::none).has_value());
    return {};
}

arpg::test::Failure default_rules_are_valid() noexcept {
    DungeonRules defaults;
    ARPG_REQUIRE(validate_rules(defaults) == DungeonFault::none);
    ARPG_REQUIRE(defaults.base_weights[0] == 100U);
    ARPG_REQUIRE(defaults.bias_weight_increment == 25U);
    ARPG_REQUIRE(defaults.hole_threshold == 1000U);
    ARPG_REQUIRE(defaults.abyss_threshold == 100U);
    return {};
}

arpg::test::Failure default_biases_produce_checked_weights() noexcept {
    DungeonRules defaults;
    std::array<std::uint32_t, 4> biases{{5, 0, 0, 0}};
    std::array<std::uint64_t, 4> weights{};
    std::uint64_t total = 0;
    ARPG_REQUIRE(compute_ecology_weights(
        defaults, biases, weights, total) == DungeonFault::none);
    ARPG_REQUIRE(weights[0] == 225U && total == 525U);
    return {};
}

arpg::test::Failure threshold_boundaries_are_checked() noexcept {
    DungeonRules rules;
    rules.hole_threshold = 0U;
    rules.abyss_threshold = 0U;
    ARPG_REQUIRE(validate_rules(rules) == DungeonFault::none);
    rules.hole_threshold = 10000U;
    rules.abyss_threshold = 10000U;
    ARPG_REQUIRE(validate_rules(rules) == DungeonFault::none);
    rules.hole_threshold = 10001U;
    ARPG_REQUIRE(validate_rules(rules) == DungeonFault::invalid_rules);
    rules.hole_threshold = 10000U;
    rules.abyss_threshold = 10001U;
    ARPG_REQUIRE(validate_rules(rules) == DungeonFault::invalid_rules);
    return {};
}

arpg::test::Failure invalid_weights_leave_outputs_zeroed() noexcept {
    DungeonRules rules;
    rules.bias_weight_increment = (std::numeric_limits<std::uint32_t>::max)();
    std::array<std::uint32_t, 4> biases{{
        (std::numeric_limits<std::uint32_t>::max)(),
        (std::numeric_limits<std::uint32_t>::max)(),
        (std::numeric_limits<std::uint32_t>::max)(),
        (std::numeric_limits<std::uint32_t>::max)()}};
    std::array<std::uint64_t, 4> weights{};
    std::uint64_t total = 0;
    ARPG_REQUIRE(compute_ecology_weights(
        rules, biases, weights, total) == DungeonFault::weight_overflow);
    ARPG_REQUIRE((weights == std::array<std::uint64_t, 4>{}));
    ARPG_REQUIRE(total == 0U);

    rules.base_weights[0] = 0U;
    ARPG_REQUIRE(validate_rules(rules) == DungeonFault::invalid_rules);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"exit directions map to elements", &exit_directions_map_to_elements},
    {"default rules are valid", &default_rules_are_valid},
    {"default biases produce checked weights", &default_biases_produce_checked_weights},
    {"threshold boundaries are checked", &threshold_boundaries_are_checked},
    {"invalid weights leave outputs zeroed", &invalid_weights_leave_outputs_zeroed},
};

}  // namespace

arpg::test::TestSuite dungeon_rules_suite() noexcept {
    return arpg::test::make_suite("dungeon_rules", kCases);
}
