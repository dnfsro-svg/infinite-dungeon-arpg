#include "modifiers/player_modifier_values.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
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
constexpr StatBounds kDamageReduction{-6000, 7500};
constexpr StatBounds kNonNegativeBasisPoints{
    0,
    (std::numeric_limits<std::int32_t>::max)(),
};
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

    result.flat_damage[damage_index(DamageType::physical)] = evaluate(
        0, StatId::physical_flat_damage, modifiers, kUnbounded, result.valid);
    result.flat_damage[damage_index(DamageType::fire)] = evaluate(
        0, StatId::fire_flat_damage, modifiers, kUnbounded, result.valid);
    result.flat_damage[damage_index(DamageType::water)] = evaluate(
        0, StatId::water_flat_damage, modifiers, kUnbounded, result.valid);
    result.flat_damage[damage_index(DamageType::lightning)] = evaluate(
        0, StatId::lightning_flat_damage, modifiers, kUnbounded, result.valid);
    result.flat_damage[damage_index(DamageType::chaos)] = evaluate(
        0, StatId::chaos_flat_damage, modifiers, kUnbounded, result.valid);

    result.damage_increased[damage_index(DamageType::fire)] =
        static_cast<std::int32_t>(evaluate(kFixedOne, StatId::fire_damage,
            modifiers, kNonNegativeBasisPoints, result.valid));
    result.damage_increased[damage_index(DamageType::water)] =
        static_cast<std::int32_t>(evaluate(kFixedOne, StatId::water_damage,
            modifiers, kNonNegativeBasisPoints, result.valid));
    result.damage_increased[damage_index(DamageType::lightning)] =
        static_cast<std::int32_t>(evaluate(kFixedOne, StatId::lightning_damage,
            modifiers, kNonNegativeBasisPoints, result.valid));
    result.damage_increased[damage_index(DamageType::chaos)] =
        static_cast<std::int32_t>(evaluate(kFixedOne, StatId::chaos_damage,
            modifiers, kNonNegativeBasisPoints, result.valid));

    result.damage_reduction[element_index(DamageType::fire)] =
        static_cast<std::int32_t>(evaluate(0, StatId::fire_damage_reduction,
            modifiers, kDamageReduction, result.valid));
    result.damage_reduction[element_index(DamageType::water)] =
        static_cast<std::int32_t>(evaluate(0, StatId::water_damage_reduction,
            modifiers, kDamageReduction, result.valid));
    result.damage_reduction[element_index(DamageType::lightning)] =
        static_cast<std::int32_t>(evaluate(0, StatId::lightning_damage_reduction,
            modifiers, kDamageReduction, result.valid));
    result.damage_reduction[element_index(DamageType::chaos)] =
        static_cast<std::int32_t>(evaluate(0, StatId::chaos_damage_reduction,
            modifiers, kDamageReduction, result.valid));

    result.damage_reduction_cap_bonus[element_index(DamageType::fire)] =
        static_cast<std::int32_t>(evaluate(0,
            StatId::fire_damage_reduction_cap, modifiers,
            kNonNegativeBasisPoints, result.valid));
    result.damage_reduction_cap_bonus[element_index(DamageType::water)] =
        static_cast<std::int32_t>(evaluate(0,
            StatId::water_damage_reduction_cap, modifiers,
            kNonNegativeBasisPoints, result.valid));
    result.damage_reduction_cap_bonus[element_index(DamageType::lightning)] =
        static_cast<std::int32_t>(evaluate(0,
            StatId::lightning_damage_reduction_cap, modifiers,
            kNonNegativeBasisPoints, result.valid));
    result.damage_reduction_cap_bonus[element_index(DamageType::chaos)] =
        static_cast<std::int32_t>(evaluate(0,
            StatId::chaos_damage_reduction_cap, modifiers,
            kNonNegativeBasisPoints, result.valid));

    result.armor = evaluate(
        0, StatId::armor, modifiers, kUnbounded, result.valid);
    result.evasion = evaluate(
        0, StatId::evasion, modifiers, kUnbounded, result.valid);
    result.valid = result.valid && result.armor >= 0 && result.evasion >= 0;

    result.melee_damage = evaluate(
        kFixedOne, StatId::melee_damage, modifiers, kNonNegative, result.valid);
    result.max_health = evaluate(
        0, StatId::max_health, modifiers, kNonNegative, result.valid);
    result.max_health_more = evaluate(
        kFixedOne, StatId::max_health_more, modifiers, kNonNegative, result.valid);
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

std::int32_t rating_to_basis_points(std::int64_t value) noexcept {
    if (value <= 0) return 0;

    constexpr std::array<std::int64_t, 7> kRatings{{
        0, 100, 1000, 10000, 100000, 1000000, 10000000}};
    constexpr std::array<std::int32_t, 7> kBasisPoints{{
        0, 2000, 4000, 6000, 8000, 9900, 9990}};

    for (std::size_t index = 1U; index < kRatings.size(); ++index) {
        if (value > kRatings[index]) continue;
        const std::int64_t rating_offset = value - kRatings[index - 1U];
        const std::int64_t rating_span =
            kRatings[index] - kRatings[index - 1U];
        const std::int64_t basis_point_span =
            static_cast<std::int64_t>(kBasisPoints[index])
            - kBasisPoints[index - 1U];
        if (basis_point_span != 0
            && rating_offset
                > (std::numeric_limits<std::int64_t>::max)()
                    / basis_point_span) {
            return kBasisPoints[index];
        }
        const std::int64_t interpolated =
            rating_offset * basis_point_span / rating_span;
        return static_cast<std::int32_t>(
            kBasisPoints[index - 1U] + interpolated);
    }

    constexpr std::int64_t kTailNumerator = 100000000;
    const std::int64_t gap = kTailNumerator / value
        + (kTailNumerator % value != 0 ? 1 : 0);
    return static_cast<std::int32_t>(10000 - (std::max)(std::int64_t{1}, gap));
}

}  // namespace arpg::modifiers
