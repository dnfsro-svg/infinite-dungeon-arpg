#include "test_framework.hpp"

#include "abyss/abyss_rewards.hpp"

namespace {

using arpg::abyss::AbyssDanger;
using arpg::abyss::AbyssRarityWeights;

arpg::test::Failure low_reward_profile_is_fixed() noexcept {
    const auto profile = arpg::abyss::reward_profile_for(AbyssDanger::low, 20U);
    ARPG_REQUIRE(profile.item_count == 1U);
    ARPG_REQUIRE(profile.item_level == 21U);
    return {};
}

arpg::test::Failure medium_reward_profile_is_fixed() noexcept {
    const auto profile = arpg::abyss::reward_profile_for(AbyssDanger::medium, 20U);
    ARPG_REQUIRE(profile.item_count == 2U);
    ARPG_REQUIRE(profile.item_level == 23U);
    return {};
}

arpg::test::Failure high_reward_profile_is_fixed_and_capped() noexcept {
    const auto profile = arpg::abyss::reward_profile_for(AbyssDanger::high, 98U);
    ARPG_REQUIRE(profile.item_count == 3U);
    ARPG_REQUIRE(profile.item_level == 100U);
    const auto invalid = arpg::abyss::reward_profile_for(
        static_cast<AbyssDanger>(99U), 20U);
    ARPG_REQUIRE(invalid.item_count == 0U);
    ARPG_REQUIRE(invalid.item_level == 0U);
    return {};
}

arpg::test::Failure rarity_shifts_match_all_three_profiles() noexcept {
    constexpr AbyssRarityWeights base{40U, 40U, 20U};
    const auto low = arpg::abyss::shift_abyss_rarity_weights(
        base, AbyssDanger::low);
    const auto medium = arpg::abyss::shift_abyss_rarity_weights(
        base, AbyssDanger::medium);
    const auto high = arpg::abyss::shift_abyss_rarity_weights(
        base, AbyssDanger::high);
    ARPG_REQUIRE(low.has_value());
    ARPG_REQUIRE(low->normal == 30U && low->magic == 45U && low->rare == 25U);
    ARPG_REQUIRE(medium.has_value());
    ARPG_REQUIRE(medium->normal == 20U
        && medium->magic == 50U && medium->rare == 30U);
    ARPG_REQUIRE(high.has_value());
    ARPG_REQUIRE(high->normal == 10U
        && high->magic == 50U && high->rare == 40U);
    return {};
}

arpg::test::Failure rarity_shortfall_uses_largest_remainder() noexcept {
    const auto one = arpg::abyss::shift_abyss_rarity_weights(
        {1U, 40U, 59U}, AbyssDanger::high);
    ARPG_REQUIRE(one.has_value());
    ARPG_REQUIRE(one->normal == 0U && one->magic == 40U && one->rare == 60U);
    const auto tied = arpg::abyss::shift_abyss_rarity_weights(
        {5U, 45U, 50U}, AbyssDanger::low);
    ARPG_REQUIRE(tied.has_value());
    ARPG_REQUIRE(tied->normal == 0U
        && tied->magic == 48U && tied->rare == 52U);
    ARPG_REQUIRE(!arpg::abyss::shift_abyss_rarity_weights(
        {40U, 40U, 19U}, AbyssDanger::low).has_value());
    ARPG_REQUIRE(!arpg::abyss::shift_abyss_rarity_weights(
        {40U, 40U, 20U}, static_cast<AbyssDanger>(99U)).has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"low reward profile", &low_reward_profile_is_fixed},
    {"medium reward profile", &medium_reward_profile_is_fixed},
    {"high reward profile", &high_reward_profile_is_fixed_and_capped},
    {"rarity profile shifts", &rarity_shifts_match_all_three_profiles},
    {"rarity largest remainder", &rarity_shortfall_uses_largest_remainder},
};

}  // namespace

arpg::test::TestSuite abyss_rewards_suite() noexcept {
    return arpg::test::make_suite("abyss_rewards", kCases);
}
