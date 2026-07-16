#include "test_framework.hpp"

#include "abyss/abyss_rules.hpp"
#include "core/deterministic_rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using arpg::abyss::AbyssDanger;
using arpg::abyss::AbyssRuleId;

constexpr std::uint64_t kAbyssRollDomain = 0x41425953535F3031ULL;
constexpr std::uint64_t kRulesVersionDomain = 0x4142595352563031ULL;
constexpr std::uint64_t kDangerDomain = 0x414259535F444E47ULL;
constexpr std::uint64_t kRuleDomain = 0x414259535F52554CULL;

std::uint64_t selection_root(std::uint64_t room_seed) noexcept {
    auto version = arpg::core::DeterministicRng::derive_stream(
        room_seed, kRulesVersionDomain ^ arpg::abyss::kAbyssRulesVersion);
    return version.next_u64();
}

std::uint64_t expected_danger_roll(std::uint64_t room_seed) noexcept {
    auto danger = arpg::core::DeterministicRng::derive_stream(
        selection_root(room_seed), kDangerDomain);
    return danger.next_bounded(100U).value();
}

std::uint64_t expected_rule_slot(std::uint64_t room_seed) noexcept {
    auto rule = arpg::core::DeterministicRng::derive_stream(
        selection_root(room_seed), kRuleDomain);
    return rule.next_bounded(3U).value();
}

AbyssDanger expected_danger(
    std::uint64_t roll,
    std::uint8_t low,
    std::uint8_t medium) noexcept {
    if (roll < low) return AbyssDanger::low;
    if (roll < static_cast<std::uint64_t>(low) + medium)
        return AbyssDanger::medium;
    return AbyssDanger::high;
}

arpg::test::Failure verify_danger_band(
    std::uint64_t depth,
    std::uint8_t low,
    std::uint8_t medium) noexcept {
    std::array<bool, 100> observed{};
    for (std::uint64_t seed = 0U; seed < 200000U; ++seed) {
        const std::uint64_t roll = expected_danger_roll(seed);
        observed[static_cast<std::size_t>(roll)] = true;
        const auto selection = arpg::abyss::select_abyss_rule(seed, depth);
        ARPG_REQUIRE(selection.has_value());
        ARPG_REQUIRE(selection->danger == expected_danger(roll, low, medium));
        ARPG_REQUIRE(selection->rules_version == arpg::abyss::kAbyssRulesVersion);
    }
    for (const bool value : observed) ARPG_REQUIRE(value);
    return {};
}

std::uint8_t rule_slot(AbyssRuleId rule) noexcept {
    return static_cast<std::uint8_t>(rule) / 3U;
}

arpg::test::Failure stable_public_values_and_defaults() noexcept {
    using arpg::abyss::AbyssLifecycle;
    static_assert(arpg::abyss::kAbyssRulesVersion == 1U);
    static_assert(static_cast<std::uint8_t>(AbyssDanger::low) == 0U);
    static_assert(static_cast<std::uint8_t>(AbyssDanger::medium) == 1U);
    static_assert(static_cast<std::uint8_t>(AbyssDanger::high) == 2U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::thunderstorm) == 0U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::hunting_flames) == 1U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::chaos_expansion) == 2U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::swift_pursuit) == 3U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::abyss_bulwark) == 4U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::abyss_fury) == 5U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::heavy_steps) == 6U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::exhausted_recovery) == 7U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::life_sacrifice) == 8U);
    static_assert(static_cast<std::uint8_t>(AbyssRuleId::none) == 0xFFU);
    static_assert(static_cast<std::uint8_t>(AbyssLifecycle::none) == 0U);
    static_assert(static_cast<std::uint8_t>(AbyssLifecycle::available) == 1U);
    static_assert(static_cast<std::uint8_t>(AbyssLifecycle::started) == 2U);
    static_assert(static_cast<std::uint8_t>(AbyssLifecycle::cleared) == 3U);
    static_assert(static_cast<std::uint8_t>(AbyssLifecycle::failed) == 4U);
    const arpg::abyss::AbyssCombatConfig config{};
    ARPG_REQUIRE(config.rule == AbyssRuleId::none);
    ARPG_REQUIRE(config.player_ground_move_bp == 10000U);
    ARPG_REQUIRE(config.monster_extra_shield_bp == 0U);
    return {};
}

arpg::test::Failure abyss_roll_uses_frozen_one_percent_domain() noexcept {
    bool saw_hit = false;
    bool saw_miss = false;
    for (std::uint64_t seed = 0U; seed < 10000U; ++seed) {
        auto stream = arpg::core::DeterministicRng::derive_stream(
            seed, kAbyssRollDomain);
        const bool expected = stream.next_bounded(10000U).value() < 100U;
        ARPG_REQUIRE(arpg::abyss::is_abyss_roll(seed) == expected);
        saw_hit = saw_hit || expected;
        saw_miss = saw_miss || !expected;
    }
    ARPG_REQUIRE(saw_hit);
    ARPG_REQUIRE(saw_miss);
    return {};
}

arpg::test::Failure depth_zero_is_not_selectable() noexcept {
    ARPG_REQUIRE(!arpg::abyss::select_abyss_rule(1U, 0U).has_value());
    return {};
}

arpg::test::Failure weights_depth_1() noexcept {
    return verify_danger_band(1U, 70U, 25U);
}
arpg::test::Failure weights_depth_9() noexcept {
    return verify_danger_band(9U, 70U, 25U);
}
arpg::test::Failure weights_depth_10() noexcept {
    return verify_danger_band(10U, 45U, 40U);
}
arpg::test::Failure weights_depth_19() noexcept {
    return verify_danger_band(19U, 45U, 40U);
}
arpg::test::Failure weights_depth_20() noexcept {
    return verify_danger_band(20U, 25U, 45U);
}
arpg::test::Failure weights_depth_39() noexcept {
    return verify_danger_band(39U, 25U, 45U);
}
arpg::test::Failure weights_depth_40() noexcept {
    return verify_danger_band(40U, 10U, 35U);
}
arpg::test::Failure weights_depth_max() noexcept {
    return verify_danger_band(
        (std::numeric_limits<std::uint64_t>::max)(), 10U, 35U);
}

arpg::test::Failure rules_for_danger_are_reachable(
    AbyssDanger wanted,
    std::uint64_t depth) noexcept {
    std::array<bool, 3> slots{};
    for (std::uint64_t seed = 0U; seed < 10000U; ++seed) {
        const auto selected = arpg::abyss::select_abyss_rule(seed, depth);
        ARPG_REQUIRE(selected.has_value());
        if (selected->danger == wanted)
            slots[rule_slot(selected->rule)] = true;
    }
    ARPG_REQUIRE(slots[0]);
    ARPG_REQUIRE(slots[1]);
    ARPG_REQUIRE(slots[2]);
    return {};
}

arpg::test::Failure low_rules_are_reachable() noexcept {
    return rules_for_danger_are_reachable(AbyssDanger::low, 1U);
}
arpg::test::Failure medium_rules_are_reachable() noexcept {
    return rules_for_danger_are_reachable(AbyssDanger::medium, 20U);
}
arpg::test::Failure high_rules_are_reachable() noexcept {
    return rules_for_danger_are_reachable(AbyssDanger::high, 40U);
}

arpg::test::Failure selection_is_stable_for_identical_inputs() noexcept {
    const auto first = arpg::abyss::select_abyss_rule(0x123456789ABCDEF0ULL, 27U);
    const auto second = arpg::abyss::select_abyss_rule(0x123456789ABCDEF0ULL, 27U);
    ARPG_REQUIRE(first.has_value());
    ARPG_REQUIRE(second.has_value());
    ARPG_REQUIRE(first->danger == second->danger);
    ARPG_REQUIRE(first->rule == second->rule);
    ARPG_REQUIRE(first->rules_version == second->rules_version);
    return {};
}

arpg::test::Failure named_domains_are_independent() noexcept {
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const auto selected = arpg::abyss::select_abyss_rule(seed, 20U);
        ARPG_REQUIRE(selected.has_value());
        ARPG_REQUIRE(rule_slot(selected->rule) == expected_rule_slot(seed));
        ARPG_REQUIRE(selected->danger == expected_danger(
            expected_danger_roll(seed), 25U, 45U));
    }
    return {};
}

arpg::test::Failure encounter_budget_uses_ceiling_three_halves() noexcept {
    ARPG_REQUIRE(arpg::abyss::abyss_encounter_budget(0U) == 0U);
    ARPG_REQUIRE(arpg::abyss::abyss_encounter_budget(1U) == 2U);
    ARPG_REQUIRE(arpg::abyss::abyss_encounter_budget(2U) == 3U);
    ARPG_REQUIRE(arpg::abyss::abyss_encounter_budget(169U) == 254U);
    ARPG_REQUIRE(arpg::abyss::abyss_encounter_budget(170U) == 255U);
    ARPG_REQUIRE(arpg::abyss::abyss_encounter_budget(255U) == 255U);
    return {};
}

arpg::test::Failure affix_minimum_follows_depth_bands() noexcept {
    ARPG_REQUIRE(arpg::abyss::minimum_abyss_affixes(0U) == 0U);
    ARPG_REQUIRE(arpg::abyss::minimum_abyss_affixes(1U) == 1U);
    ARPG_REQUIRE(arpg::abyss::minimum_abyss_affixes(19U) == 1U);
    ARPG_REQUIRE(arpg::abyss::minimum_abyss_affixes(20U) == 2U);
    ARPG_REQUIRE(arpg::abyss::minimum_abyss_affixes(39U) == 2U);
    ARPG_REQUIRE(arpg::abyss::minimum_abyss_affixes(40U) == 3U);
    ARPG_REQUIRE(arpg::abyss::minimum_abyss_affixes(
        (std::numeric_limits<std::uint64_t>::max)()) == 3U);
    return {};
}

arpg::test::Failure resource_ratio_rejects_invalid_ranges() noexcept {
    ARPG_REQUIRE(!arpg::abyss::map_resource_ratio(1, 0, 10, true).has_value());
    ARPG_REQUIRE(!arpg::abyss::map_resource_ratio(1, 10, 0, true).has_value());
    ARPG_REQUIRE(!arpg::abyss::map_resource_ratio(-1, 10, 10, false).has_value());
    ARPG_REQUIRE(!arpg::abyss::map_resource_ratio(11, 10, 10, true).has_value());
    return {};
}

arpg::test::Failure resource_ratio_rounds_and_clamps_exactly() noexcept {
    ARPG_REQUIRE(arpg::abyss::map_resource_ratio(1, 2, 3, true).value() == 2);
    ARPG_REQUIRE(arpg::abyss::map_resource_ratio(1, 3, 2, true).value() == 1);
    ARPG_REQUIRE(arpg::abyss::map_resource_ratio(0, 100, 55, false).value() == 0);
    ARPG_REQUIRE(arpg::abyss::map_resource_ratio(0, 100, 55, true).value() == 1);
    ARPG_REQUIRE(arpg::abyss::map_resource_ratio(100, 100, 55, true).value() == 55);
    return {};
}

arpg::test::Failure percent_damage_rejects_invalid_values() noexcept {
    ARPG_REQUIRE(!arpg::abyss::percent_of_actual_max_hp(0, 1500U).has_value());
    ARPG_REQUIRE(!arpg::abyss::percent_of_actual_max_hp(-1, 1500U).has_value());
    ARPG_REQUIRE(!arpg::abyss::percent_of_actual_max_hp(100, 0U).has_value());
    ARPG_REQUIRE(!arpg::abyss::percent_of_actual_max_hp(100, 10001U).has_value());
    return {};
}

arpg::test::Failure percent_damage_uses_ceiling_and_minimum_one() noexcept {
    ARPG_REQUIRE(arpg::abyss::percent_of_actual_max_hp(1, 800U).value() == 1);
    ARPG_REQUIRE(arpg::abyss::percent_of_actual_max_hp(101, 1500U).value() == 16);
    ARPG_REQUIRE(arpg::abyss::percent_of_actual_max_hp(100, 1000U).value() == 10);
    ARPG_REQUIRE(arpg::abyss::percent_of_actual_max_hp(
        (std::numeric_limits<int>::max)(), 10000U).value()
        == (std::numeric_limits<int>::max)());
    return {};
}

arpg::test::Failure combat_config_catalog_matches_nine_rules() noexcept {
    const auto thunder = arpg::abyss::combat_config_for(AbyssRuleId::thunderstorm);
    ARPG_REQUIRE(thunder.rule == AbyssRuleId::thunderstorm);
    ARPG_REQUIRE(thunder.player_ground_move_bp == 10000U);
    ARPG_REQUIRE(thunder.monster_damage_bp == 10000U);

    const auto swift = arpg::abyss::combat_config_for(AbyssRuleId::swift_pursuit);
    ARPG_REQUIRE(swift.rule == AbyssRuleId::swift_pursuit);
    ARPG_REQUIRE(swift.monster_move_bp == 11500U);
    ARPG_REQUIRE(swift.monster_cooldown_bp == 8500U);

    const auto bulwark = arpg::abyss::combat_config_for(AbyssRuleId::abyss_bulwark);
    ARPG_REQUIRE(bulwark.monster_armor_bp == 13000U);
    ARPG_REQUIRE(bulwark.monster_extra_shield_bp == 3000U);

    const auto fury = arpg::abyss::combat_config_for(AbyssRuleId::abyss_fury);
    ARPG_REQUIRE(fury.monster_damage_bp == 14500U);
    ARPG_REQUIRE(fury.monster_attack_speed_bp == 14500U);

    const auto heavy = arpg::abyss::combat_config_for(AbyssRuleId::heavy_steps);
    ARPG_REQUIRE(heavy.player_ground_move_bp == 8500U);

    const auto exhausted = arpg::abyss::combat_config_for(
        AbyssRuleId::exhausted_recovery);
    ARPG_REQUIRE(exhausted.player_resource_restore_bp == 7000U);

    const auto sacrifice = arpg::abyss::combat_config_for(AbyssRuleId::life_sacrifice);
    ARPG_REQUIRE(sacrifice.player_max_health_bp == 5500U);

    const auto flames = arpg::abyss::combat_config_for(AbyssRuleId::hunting_flames);
    const auto chaos = arpg::abyss::combat_config_for(AbyssRuleId::chaos_expansion);
    ARPG_REQUIRE(flames.rule == AbyssRuleId::hunting_flames);
    ARPG_REQUIRE(chaos.rule == AbyssRuleId::chaos_expansion);
    ARPG_REQUIRE(arpg::abyss::combat_config_for(AbyssRuleId::none).rule
        == AbyssRuleId::none);
    ARPG_REQUIRE(arpg::abyss::combat_config_for(
        static_cast<AbyssRuleId>(99U)).rule == AbyssRuleId::none);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"stable public values", &stable_public_values_and_defaults},
    {"frozen one percent roll", &abyss_roll_uses_frozen_one_percent_domain},
    {"depth zero rejected", &depth_zero_is_not_selectable},
    {"weights depth 1", &weights_depth_1},
    {"weights depth 9", &weights_depth_9},
    {"weights depth 10", &weights_depth_10},
    {"weights depth 19", &weights_depth_19},
    {"weights depth 20", &weights_depth_20},
    {"weights depth 39", &weights_depth_39},
    {"weights depth 40", &weights_depth_40},
    {"weights depth max", &weights_depth_max},
    {"low rules reachable", &low_rules_are_reachable},
    {"medium rules reachable", &medium_rules_are_reachable},
    {"high rules reachable", &high_rules_are_reachable},
    {"selection stable", &selection_is_stable_for_identical_inputs},
    {"named domains independent", &named_domains_are_independent},
    {"encounter budget", &encounter_budget_uses_ceiling_three_halves},
    {"affix minimum", &affix_minimum_follows_depth_bands},
    {"resource ratio invalid", &resource_ratio_rejects_invalid_ranges},
    {"resource ratio exact", &resource_ratio_rounds_and_clamps_exactly},
    {"percent damage invalid", &percent_damage_rejects_invalid_values},
    {"percent damage exact", &percent_damage_uses_ceiling_and_minimum_one},
    {"combat config catalog", &combat_config_catalog_matches_nine_rules},
};

}  // namespace

arpg::test::TestSuite abyss_rules_suite() noexcept {
    return arpg::test::make_suite("abyss_rules", kCases);
}
