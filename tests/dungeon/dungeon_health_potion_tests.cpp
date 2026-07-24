#include "test_framework.hpp"

#include "dungeon/health_potion_loot.hpp"

#include <cstdint>

namespace {

arpg::test::Failure health_potion_rules_are_frozen() noexcept {
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionDropChanceBp == 3000U);
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionRestoreBp == 2500U);
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionAutoUseThresholdBp == 7500U);
    ARPG_REQUIRE(arpg::dungeon::kGroundHealthPotionCapacity == 192U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(0U) == 1U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(191U) == 383U);
    return {};
}

arpg::test::Failure health_potion_roll_is_deterministic_and_has_both_outcomes()
    noexcept {
    bool found_drop = false;
    bool found_miss = false;
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const bool first = arpg::dungeon::roll_health_potion_drop(seed, 37U);
        const bool second = arpg::dungeon::roll_health_potion_drop(seed, 37U);
        ARPG_REQUIRE(first == second);
        found_drop = found_drop || first;
        found_miss = found_miss || !first;
    }
    ARPG_REQUIRE(found_drop);
    ARPG_REQUIRE(found_miss);
    return {};
}

arpg::test::Failure health_potion_threshold_is_integer_exact() noexcept {
    using arpg::dungeon::health_potion_auto_use_eligible;
    ARPG_REQUIRE(!health_potion_auto_use_eligible(751, 1000));
    ARPG_REQUIRE(health_potion_auto_use_eligible(750, 1000));
    ARPG_REQUIRE(health_potion_auto_use_eligible(749, 1000));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(0, 1000));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(1, 0));
    ARPG_REQUIRE(health_potion_auto_use_eligible(3, 4));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(4, 4));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"health potion rules frozen", &health_potion_rules_are_frozen},
    {"health potion deterministic roll",
        &health_potion_roll_is_deterministic_and_has_both_outcomes},
    {"health potion exact threshold", &health_potion_threshold_is_integer_exact},
};

}  // namespace

arpg::test::TestSuite dungeon_health_potion_suite() noexcept {
    return arpg::test::make_suite("dungeon_health_potion", kCases);
}
