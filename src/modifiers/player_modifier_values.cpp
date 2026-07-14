#include "modifiers/player_modifier_values.hpp"

#include <limits>

namespace arpg::modifiers {

namespace {

constexpr StatBounds kUnbounded{
    (std::numeric_limits<FixedValue>::min)(),
    (std::numeric_limits<FixedValue>::max)(),
};
constexpr StatBounds kNonNegative{
    0,
    (std::numeric_limits<FixedValue>::max)(),
};
constexpr StatBounds kResistance{-6000, 7500};
constexpr StatBounds kDamageTaken{5000, 20000};

FixedValue evaluate(
    FixedValue base,
    StatId stat,
    ModifierSpan modifiers,
    StatBounds bounds,
    bool& valid) noexcept {
    const StatEvaluation result = evaluate_stat(
        base, stat, modifiers, {}, bounds);
    valid = valid && result.valid;
    return result.value;
}

}  // namespace

PlayerModifierValues evaluate_player_modifiers(
    ModifierSpan modifiers) noexcept {
    PlayerModifierValues result{};

    result.flat_damage[damage_index(DamageType::fire)] = evaluate(
        0, StatId::fire_flat_damage, modifiers, kUnbounded, result.valid);
    result.flat_damage[damage_index(DamageType::water)] = evaluate(
        0, StatId::water_flat_damage, modifiers, kUnbounded, result.valid);
    result.flat_damage[damage_index(DamageType::lightning)] = evaluate(
        0, StatId::lightning_flat_damage, modifiers, kUnbounded, result.valid);
    result.flat_damage[damage_index(DamageType::chaos)] = evaluate(
        0, StatId::chaos_flat_damage, modifiers, kUnbounded, result.valid);

    result.damage_increased[damage_index(DamageType::physical)] = evaluate(
        kFixedOne, StatId::melee_damage, modifiers, kNonNegative, result.valid);
    result.damage_increased[damage_index(DamageType::fire)] = evaluate(
        kFixedOne, StatId::fire_damage, modifiers, kNonNegative, result.valid);
    result.damage_increased[damage_index(DamageType::water)] = evaluate(
        kFixedOne, StatId::water_damage, modifiers, kNonNegative, result.valid);
    result.damage_increased[damage_index(DamageType::lightning)] = evaluate(
        kFixedOne, StatId::lightning_damage, modifiers, kNonNegative, result.valid);
    result.damage_increased[damage_index(DamageType::chaos)] = evaluate(
        kFixedOne, StatId::chaos_damage, modifiers, kNonNegative, result.valid);

    result.resistance[element_index(DamageType::fire)] = evaluate(
        0, StatId::fire_resistance, modifiers, kResistance, result.valid);
    result.resistance[element_index(DamageType::water)] = evaluate(
        0, StatId::water_resistance, modifiers, kResistance, result.valid);
    result.resistance[element_index(DamageType::lightning)] = evaluate(
        0, StatId::lightning_resistance, modifiers, kResistance, result.valid);
    result.resistance[element_index(DamageType::chaos)] = evaluate(
        0, StatId::chaos_resistance, modifiers, kResistance, result.valid);

    result.melee_damage = evaluate(
        kFixedOne, StatId::melee_damage, modifiers, kNonNegative, result.valid);
    result.max_health = evaluate(
        0, StatId::max_health, modifiers, kNonNegative, result.valid);
    result.max_barrier = evaluate(
        0, StatId::max_barrier, modifiers, kNonNegative, result.valid);
    result.damage_taken = evaluate(
        kFixedOne, StatId::damage_taken, modifiers, kDamageTaken, result.valid);
    result.movement_speed = evaluate(
        kFixedOne, StatId::move_speed, modifiers, kNonNegative, result.valid);
    result.attack_speed = evaluate(
        kFixedOne, StatId::attack_speed, modifiers, kNonNegative, result.valid);
    result.impulse_scale = evaluate(
        kFixedOne, StatId::impulse_scale, modifiers, kNonNegative, result.valid);
    result.jump_speed = evaluate(
        kFixedOne, StatId::jump_speed, modifiers, kNonNegative, result.valid);
    result.air_control = evaluate(
        kFixedOne, StatId::air_control, modifiers, kNonNegative, result.valid);

    return result;
}

}  // namespace arpg::modifiers
