#include "test_framework.hpp"

#include "modifiers/player_modifier_values.hpp"

#include <array>

namespace {

using namespace arpg::modifiers;

arpg::test::Failure fire_and_generic_values_are_independent() noexcept {
    const std::array<Modifier, 4> values{{
        {101U, StatId::fire_flat_damage, ModifierOperation::flat, 30000},
        {102U, StatId::fire_damage, ModifierOperation::increased, 800},
        {103U, StatId::fire_resistance, ModifierOperation::flat, 700},
        {104U, StatId::move_speed, ModifierOperation::increased, 600},
    }};
    const PlayerModifierValues result = evaluate_player_modifiers(values);
    ARPG_REQUIRE(result.flat_damage[damage_index(DamageType::fire)] == 30000);
    ARPG_REQUIRE(result.damage_increased[damage_index(DamageType::fire)] == 10800);
    ARPG_REQUIRE(result.resistance[element_index(DamageType::fire)] == 700);
    ARPG_REQUIRE(result.movement_speed == 10600);
    ARPG_REQUIRE(result.resistance[element_index(DamageType::water)] == 0);
    return {};
}

arpg::test::Failure resistance_and_reduction_are_clamped() noexcept {
    const std::array<Modifier, 2> values{{
        {201U, StatId::lightning_resistance, ModifierOperation::flat, 10000},
        {202U, StatId::damage_taken, ModifierOperation::increased, -9000},
    }};
    const PlayerModifierValues result = evaluate_player_modifiers(values);
    ARPG_REQUIRE(result.resistance[element_index(DamageType::lightning)] == 7500);
    ARPG_REQUIRE(result.damage_taken == 5000);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"independent element and generic values", &fire_and_generic_values_are_independent},
    {"resistance and damage taken clamps", &resistance_and_reduction_are_clamped},
};

}  // namespace

arpg::test::TestSuite player_modifier_values_suite() noexcept {
    return arpg::test::make_suite("player_modifier_values", kCases);
}

