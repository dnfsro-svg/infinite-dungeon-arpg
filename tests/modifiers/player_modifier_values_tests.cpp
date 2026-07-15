#include "test_framework.hpp"

#include "modifiers/player_modifier_values.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace {

using namespace arpg::modifiers;

static_assert(std::is_same_v<
    decltype(PlayerModifierValues::flat_damage),
    std::array<std::int64_t, 5>>);
static_assert(std::is_same_v<
    decltype(PlayerModifierValues::damage_increased),
    std::array<std::int32_t, 5>>);
static_assert(std::is_same_v<
    decltype(PlayerModifierValues::damage_reduction),
    std::array<std::int32_t, 4>>);
static_assert(std::is_same_v<
    decltype(PlayerModifierValues::damage_reduction_cap_bonus),
    std::array<std::int32_t, 4>>);

arpg::test::Failure equipment_values_are_independent() noexcept {
    const std::array<Modifier, 15> values{{
        {100U, StatId::physical_flat_damage, ModifierOperation::flat, 20000},
        {101U, StatId::fire_flat_damage, ModifierOperation::flat, 30000},
        {102U, StatId::fire_damage, ModifierOperation::increased, 800},
        {103U, StatId::fire_damage_reduction, ModifierOperation::flat, 700},
        {104U, StatId::water_damage_reduction, ModifierOperation::flat, 800},
        {105U, StatId::lightning_damage_reduction, ModifierOperation::flat, 900},
        {106U, StatId::chaos_damage_reduction, ModifierOperation::flat, 1000},
        {107U, StatId::fire_damage_reduction_cap, ModifierOperation::flat, 100},
        {108U, StatId::water_damage_reduction_cap, ModifierOperation::flat, 200},
        {109U, StatId::lightning_damage_reduction_cap, ModifierOperation::flat, 300},
        {110U, StatId::chaos_damage_reduction_cap, ModifierOperation::flat, 400},
        {111U, StatId::armor, ModifierOperation::flat, 12345},
        {112U, StatId::evasion, ModifierOperation::flat, 54321},
        {113U, StatId::move_speed, ModifierOperation::increased, 600},
        {114U, StatId::water_flat_damage, ModifierOperation::flat, 40000},
    }};
    const PlayerModifierValues result = evaluate_player_modifiers(values);
    ARPG_REQUIRE(result.valid);
    ARPG_REQUIRE(result.flat_damage[damage_index(DamageType::physical)] == 20000);
    ARPG_REQUIRE(result.flat_damage[damage_index(DamageType::fire)] == 30000);
    ARPG_REQUIRE(result.flat_damage[damage_index(DamageType::water)] == 40000);
    ARPG_REQUIRE(result.damage_increased[damage_index(DamageType::fire)] == 10800);
    ARPG_REQUIRE(result.damage_reduction[element_index(DamageType::fire)] == 700);
    ARPG_REQUIRE(result.damage_reduction[element_index(DamageType::water)] == 800);
    ARPG_REQUIRE(result.damage_reduction[element_index(DamageType::lightning)] == 900);
    ARPG_REQUIRE(result.damage_reduction[element_index(DamageType::chaos)] == 1000);
    ARPG_REQUIRE(result.damage_reduction_cap_bonus[
        element_index(DamageType::fire)] == 100);
    ARPG_REQUIRE(result.damage_reduction_cap_bonus[
        element_index(DamageType::water)] == 200);
    ARPG_REQUIRE(result.damage_reduction_cap_bonus[
        element_index(DamageType::lightning)] == 300);
    ARPG_REQUIRE(result.damage_reduction_cap_bonus[
        element_index(DamageType::chaos)] == 400);
    ARPG_REQUIRE(result.armor == 12345);
    ARPG_REQUIRE(result.evasion == 54321);
    ARPG_REQUIRE(result.movement_speed == 10600);
    return {};
}

arpg::test::Failure damage_reduction_and_damage_taken_are_clamped() noexcept {
    const std::array<Modifier, 3> values{{
        {201U, StatId::lightning_damage_reduction, ModifierOperation::flat, 10000},
        {202U, StatId::damage_taken, ModifierOperation::increased, -9000},
        {203U, StatId::water_damage_reduction, ModifierOperation::flat, -9000},
    }};
    const PlayerModifierValues result = evaluate_player_modifiers(values);
    ARPG_REQUIRE(result.damage_reduction[
        element_index(DamageType::lightning)] == 7500);
    ARPG_REQUIRE(result.damage_reduction[
        element_index(DamageType::water)] == -6000);
    ARPG_REQUIRE(result.damage_taken == 5000);
    return {};
}

arpg::test::Failure melee_damage_is_not_projected_to_physical_twice() noexcept {
    const std::array<Modifier, 1> values{{
        {301U, StatId::melee_damage, ModifierOperation::increased, 600},
    }};
    const PlayerModifierValues result = evaluate_player_modifiers(values);
    ARPG_REQUIRE(result.melee_damage == 10600);
    ARPG_REQUIRE(result.damage_increased[damage_index(DamageType::physical)]
        == kFixedOne);

    const PlayerModifierValues defaults = evaluate_player_modifiers({});
    ARPG_REQUIRE(defaults.damage_increased[damage_index(DamageType::physical)]
        == kFixedOne);
    ARPG_REQUIRE(defaults.damage_reduction[
        element_index(DamageType::fire)] == 0);
    return {};
}

arpg::test::Failure negative_armor_or_evasion_is_invalid() noexcept {
    const std::array<Modifier, 1> negative_armor{{
        {401U, StatId::armor, ModifierOperation::flat, -1},
    }};
    const std::array<Modifier, 1> negative_evasion{{
        {402U, StatId::evasion, ModifierOperation::flat, -1},
    }};
    ARPG_REQUIRE(!evaluate_player_modifiers(negative_armor).valid);
    ARPG_REQUIRE(!evaluate_player_modifiers(negative_evasion).valid);
    return {};
}

arpg::test::Failure addition_overflow_invalidates_player_values() noexcept {
    const std::array<Modifier, 2> values{{
        {501U, StatId::armor, ModifierOperation::flat,
            (std::numeric_limits<std::int64_t>::max)()},
        {502U, StatId::armor, ModifierOperation::flat, 1},
    }};
    ARPG_REQUIRE(!evaluate_player_modifiers(values).valid);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"independent equipment values", &equipment_values_are_independent},
    {"damage reduction and damage taken clamps",
        &damage_reduction_and_damage_taken_are_clamped},
    {"melee damage is applied once", &melee_damage_is_not_projected_to_physical_twice},
    {"negative armor and evasion", &negative_armor_or_evasion_is_invalid},
    {"addition overflow invalidates values",
        &addition_overflow_invalidates_player_values},
};

}  // namespace

arpg::test::TestSuite player_modifier_values_suite() noexcept {
    return arpg::test::make_suite("player_modifier_values", kCases);
}
