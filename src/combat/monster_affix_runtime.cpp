#include "combat/monster_affix_runtime.hpp"

#include "combat/monster_affix_catalog.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::combat {
namespace {

constexpr std::int32_t kBasisPoints = 10000;

[[nodiscard]] int scale_value(int value, std::int32_t basis_points) noexcept {
    const std::int64_t scaled = static_cast<std::int64_t>(value)
        * static_cast<std::int64_t>(basis_points) / kBasisPoints;
    return static_cast<int>(std::clamp(scaled, std::int64_t{0},
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

[[nodiscard]] std::int32_t multiply_basis_points(
    std::int32_t left, std::int32_t right) noexcept {
    const std::int64_t product = static_cast<std::int64_t>(left)
        * static_cast<std::int64_t>(right) / kBasisPoints;
    return static_cast<std::int32_t>(std::clamp(product, std::int64_t{0},
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())));
}

}  // namespace

MonsterAffixProfile evaluate_monster_affixes(
    const MonsterDefinition& monster,
    const MonsterAffixSet& affixes) noexcept {
    MonsterAffixProfile result{};
    result.max_hp = monster.max_hp;
    std::int32_t shield_percent_bp{};

    const std::size_t count = std::min<std::size_t>(
        affixes.count, affixes.values.size());
    for (std::size_t index = 0U; index < count; ++index) {
        const MonsterAffixInstance& instance = affixes.values[index];
        const MonsterAffixDefinition* const definition =
            monster_affix_definition(instance.id);
        const std::size_t tier = static_cast<std::size_t>(instance.tier);
        if (definition == nullptr || tier >= definition->tiers.size()) {
            continue;
        }

        const MonsterAffixTierValues& values = definition->tiers[tier];
        switch (instance.id) {
        case MonsterAffixId::mighty:
            result.max_hp = scale_value(result.max_hp, values.primary_bp);
            result.horizontal_impulse_bp = multiply_basis_points(
                result.horizontal_impulse_bp, values.secondary_bp);
            break;
        case MonsterAffixId::frenzy:
            result.damage_bp = multiply_basis_points(
                result.damage_bp, values.primary_bp);
            result.attack_timing_bp = multiply_basis_points(
                result.attack_timing_bp, values.secondary_bp);
            break;
        case MonsterAffixId::swift:
            result.move_bp = multiply_basis_points(
                result.move_bp, values.primary_bp);
            result.cooldown_bp = multiply_basis_points(
                result.cooldown_bp, values.secondary_bp);
            break;
        case MonsterAffixId::armored:
            result.armor_rating += values.primary_bp;
            break;
        case MonsterAffixId::shielding:
            shield_percent_bp = values.primary_bp;
            result.shield_recharge_delay_ticks = values.duration_ticks;
            break;
        case MonsterAffixId::multishot:
        case MonsterAffixId::burning_ground:
        case MonsterAffixId::chilling:
        case MonsterAffixId::chain_lightning:
        case MonsterAffixId::chaos_corrosion:
        case MonsterAffixId::blink_assault:
        case MonsterAffixId::death_blast:
        case MonsterAffixId::count:
            break;
        }
    }

    if (shield_percent_bp > 0) {
        result.max_shield = scale_value(result.max_hp, shield_percent_bp);
    }
    return result;
}

}  // namespace arpg::combat
