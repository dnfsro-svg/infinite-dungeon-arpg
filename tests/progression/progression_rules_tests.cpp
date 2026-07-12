#include "test_framework.hpp"

#include "progression/progression_rules.hpp"

#include <cstdint>
#include <limits>

namespace {

using namespace arpg::progression;

ProgressionRules flat_rules() noexcept {
    ProgressionRules rules{};
    rules.experience_to_next.fill(100U);
    rules.monster_experience.fill(10U);
    rules.room_clear_experience = 50U;
    return rules;
}

arpg::test::Failure new_character_is_level_one() noexcept {
    const ProgressionState state{};
    ARPG_REQUIRE(state.level == 1U);
    ARPG_REQUIRE(state.experience == 0U);
    ARPG_REQUIRE(state.earned_passive_points == 0U);
    ARPG_REQUIRE(state.unspent_passive_points == 0U);
    ARPG_REQUIRE(valid_progression_state(state, flat_rules()));
    return {};
}

arpg::test::Failure subthreshold_award_stays_in_level() noexcept {
    const ProgressionAward result = apply_experience(
        ProgressionState{}, 99U, flat_rules());
    ARPG_REQUIRE(result.state.level == 1U);
    ARPG_REQUIRE(result.state.experience == 99U);
    ARPG_REQUIRE(result.levels_gained == 0U);
    return {};
}

arpg::test::Failure exact_threshold_awards_one_point() noexcept {
    const ProgressionAward result = apply_experience(
        ProgressionState{}, 100U, flat_rules());
    ARPG_REQUIRE(result.state.level == 2U);
    ARPG_REQUIRE(result.state.experience == 0U);
    ARPG_REQUIRE(result.state.earned_passive_points == 1U);
    ARPG_REQUIRE(result.state.unspent_passive_points == 1U);
    ARPG_REQUIRE(result.levels_gained == 1U);
    return {};
}

arpg::test::Failure one_award_can_cross_multiple_levels() noexcept {
    const ProgressionAward result = apply_experience(
        ProgressionState{}, 250U, flat_rules());
    ARPG_REQUIRE(result.state.level == 3U);
    ARPG_REQUIRE(result.state.experience == 50U);
    ARPG_REQUIRE(result.state.earned_passive_points == 2U);
    ARPG_REQUIRE(result.state.unspent_passive_points == 2U);
    ARPG_REQUIRE(result.levels_gained == 2U);
    return {};
}

arpg::test::Failure huge_award_saturates_at_level_one_hundred() noexcept {
    const ProgressionAward result = apply_experience(
        ProgressionState{},
        (std::numeric_limits<std::uint64_t>::max)(), flat_rules());
    ARPG_REQUIRE(result.state.level == 100U);
    ARPG_REQUIRE(result.state.experience == 0U);
    ARPG_REQUIRE(result.state.earned_passive_points == 99U);
    ARPG_REQUIRE(result.state.unspent_passive_points == 99U);
    ARPG_REQUIRE(result.levels_gained == 99U);
    return {};
}

arpg::test::Failure invalid_states_and_rules_are_rejected() noexcept {
    ProgressionRules rules = flat_rules();
    ARPG_REQUIRE(valid_progression_rules(rules));
    rules.experience_to_next[4] = 0U;
    ARPG_REQUIRE(!valid_progression_rules(rules));

    rules = flat_rules();
    ProgressionState invalid{};
    invalid.level = 2U;
    ARPG_REQUIRE(!valid_progression_state(invalid, rules));
    invalid.earned_passive_points = 1U;
    invalid.unspent_passive_points = 2U;
    ARPG_REQUIRE(!valid_progression_state(invalid, rules));
    invalid.unspent_passive_points = 1U;
    invalid.experience = 100U;
    ARPG_REQUIRE(!valid_progression_state(invalid, rules));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"new character", &new_character_is_level_one},
    {"subthreshold award", &subthreshold_award_stays_in_level},
    {"exact threshold", &exact_threshold_awards_one_point},
    {"multi-level award", &one_award_can_cross_multiple_levels},
    {"level one hundred saturation", &huge_award_saturates_at_level_one_hundred},
    {"invalid state and rules", &invalid_states_and_rules_are_rejected},
};

}  // namespace

arpg::test::TestSuite progression_rules_suite() noexcept {
    return arpg::test::make_suite("progression_rules", kCases);
}
