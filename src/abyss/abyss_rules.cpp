#include "abyss/abyss_rules.hpp"

#include "core/deterministic_rng.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::abyss {
namespace {

constexpr std::uint64_t kAbyssRollDomain = 0x41425953535F3031ULL;
constexpr std::uint64_t kRulesVersionDomain = 0x4142595352563031ULL;
constexpr std::uint64_t kDangerDomain = 0x414259535F444E47ULL;
constexpr std::uint64_t kRuleDomain = 0x414259535F52554CULL;

struct DangerWeights final {
    std::uint8_t low{};
    std::uint8_t medium{};
    std::uint8_t high{};
};

DangerWeights weights_for_depth(std::uint64_t depth) noexcept {
    if (depth < 10U) return {70U, 25U, 5U};
    if (depth < 20U) return {45U, 40U, 15U};
    if (depth < 40U) return {25U, 45U, 30U};
    return {10U, 35U, 55U};
}

AbyssDanger danger_for_roll(
    std::uint64_t roll,
    DangerWeights weights) noexcept {
    if (roll < weights.low) return AbyssDanger::low;
    if (roll < static_cast<std::uint64_t>(weights.low) + weights.medium)
        return AbyssDanger::medium;
    return AbyssDanger::high;
}

AbyssRuleId rule_for_slot(
    AbyssDanger danger,
    std::size_t slot) noexcept {
    constexpr std::array<std::array<AbyssRuleId, 3>, 3> rules{{
        {{AbyssRuleId::thunderstorm,
          AbyssRuleId::swift_pursuit,
          AbyssRuleId::heavy_steps}},
        {{AbyssRuleId::hunting_flames,
          AbyssRuleId::abyss_bulwark,
          AbyssRuleId::exhausted_recovery}},
        {{AbyssRuleId::chaos_expansion,
          AbyssRuleId::abyss_fury,
          AbyssRuleId::life_sacrifice}},
    }};
    return rules[static_cast<std::size_t>(danger)][slot];
}

bool valid_rule(AbyssRuleId rule) noexcept {
    return static_cast<std::uint8_t>(rule)
        <= static_cast<std::uint8_t>(AbyssRuleId::life_sacrifice);
}

AbyssDanger danger_for_rule(AbyssRuleId rule) noexcept {
    switch (rule) {
    case AbyssRuleId::thunderstorm:
    case AbyssRuleId::swift_pursuit:
    case AbyssRuleId::heavy_steps:
        return AbyssDanger::low;
    case AbyssRuleId::hunting_flames:
    case AbyssRuleId::abyss_bulwark:
    case AbyssRuleId::exhausted_recovery:
        return AbyssDanger::medium;
    case AbyssRuleId::chaos_expansion:
    case AbyssRuleId::abyss_fury:
    case AbyssRuleId::life_sacrifice:
        return AbyssDanger::high;
    default:
        return AbyssDanger::low;
    }
}

}  // namespace

bool is_abyss_roll(std::uint64_t room_seed) noexcept {
    auto stream = core::DeterministicRng::derive_stream(
        room_seed, kAbyssRollDomain);
    return stream.next_bounded(10000U).value_or(10000U) < 100U;
}

std::optional<AbyssSelection> select_abyss_rule(
    std::uint64_t room_seed,
    std::uint64_t depth) noexcept {
    if (depth == 0U) return std::nullopt;

    auto version = core::DeterministicRng::derive_stream(
        room_seed, kRulesVersionDomain ^ kAbyssRulesVersion);
    const std::uint64_t root = version.next_u64();
    auto danger_stream = core::DeterministicRng::derive_stream(
        root, kDangerDomain);
    auto rule_stream = core::DeterministicRng::derive_stream(
        root, kRuleDomain);
    const std::uint64_t danger_roll =
        danger_stream.next_bounded(100U).value_or(0U);
    const std::size_t rule_slot = static_cast<std::size_t>(
        rule_stream.next_bounded(3U).value_or(0U));
    const AbyssDanger danger = danger_for_roll(
        danger_roll, weights_for_depth(depth));
    return AbyssSelection{
        danger, rule_for_slot(danger, rule_slot), kAbyssRulesVersion};
}

std::uint8_t abyss_encounter_budget(
    std::uint8_t normal_budget) noexcept {
    const std::uint64_t scaled =
        (static_cast<std::uint64_t>(normal_budget) * 3U + 1U) / 2U;
    return static_cast<std::uint8_t>((std::min)(
        scaled,
        static_cast<std::uint64_t>((std::numeric_limits<std::uint8_t>::max)())));
}

std::uint8_t minimum_abyss_affixes(std::uint64_t depth) noexcept {
    if (depth == 0U) return 0U;
    if (depth < 20U) return 1U;
    if (depth < 40U) return 2U;
    return 3U;
}

std::optional<int> map_resource_ratio(
    int current,
    int old_max,
    int new_max,
    bool alive) noexcept {
    if (old_max <= 0 || new_max <= 0 || current < 0 || current > old_max)
        return std::nullopt;
    const std::uint64_t numerator = static_cast<std::uint64_t>(current)
            * static_cast<std::uint64_t>(new_max)
        + static_cast<std::uint64_t>(old_max / 2);
    const std::uint64_t mapped = numerator / static_cast<std::uint64_t>(old_max);
    const std::uint64_t minimum = alive ? 1U : 0U;
    const std::uint64_t clamped = (std::min)(
        (std::max)(mapped, minimum),
        static_cast<std::uint64_t>(new_max));
    return static_cast<int>(clamped);
}

std::optional<int> percent_of_actual_max_hp(
    int actual_max_hp,
    std::uint16_t basis_points) noexcept {
    if (actual_max_hp <= 0 || basis_points == 0U || basis_points > 10000U)
        return std::nullopt;
    const std::uint64_t numerator = static_cast<std::uint64_t>(actual_max_hp)
            * static_cast<std::uint64_t>(basis_points)
        + 9999U;
    const std::uint64_t result = numerator / 10000U;
    return static_cast<int>((std::max)(std::uint64_t{1U}, result));
}

AbyssCombatConfig combat_config_for(AbyssRuleId rule) noexcept {
    AbyssCombatConfig config{};
    if (!valid_rule(rule)) return config;
    config.rule = rule;
    config.danger = danger_for_rule(rule);
    switch (rule) {
    case AbyssRuleId::thunderstorm:
        config.environment.damage_type = AbyssDamageType::lightning;
        config.environment.damage_bp = 1500U;
        config.environment.cycle_ticks = 180U;
        config.environment.warning_ticks = 45U;
        config.environment.radius_milliunits[0] = 800U;
        config.environment.radius_count = 1U;
        break;
    case AbyssRuleId::hunting_flames:
        config.environment.damage_type = AbyssDamageType::fire;
        config.environment.damage_bp = 1000U;
        config.environment.cycle_ticks = 240U;
        config.environment.warning_ticks = 45U;
        config.environment.duration_ticks = 180U;
        config.environment.damage_interval_ticks = 60U;
        config.environment.radius_milliunits[0] = 1000U;
        config.environment.radius_count = 1U;
        break;
    case AbyssRuleId::chaos_expansion:
        config.environment.damage_type = AbyssDamageType::chaos;
        config.environment.damage_bp = 800U;
        config.environment.damage_interval_ticks = 60U;
        config.environment.expansion_interval_ticks = 180U;
        config.environment.radius_milliunits = {
            1000U, 2300U, 3600U, 4900U, 6200U};
        config.environment.radius_count = 5U;
        break;
    case AbyssRuleId::swift_pursuit:
        config.monster_move_bp = 11500U;
        config.monster_cooldown_bp = 8500U;
        break;
    case AbyssRuleId::abyss_bulwark:
        config.monster_armor_bp = 13000U;
        config.monster_extra_shield_bp = 3000U;
        break;
    case AbyssRuleId::abyss_fury:
        config.monster_damage_bp = 14500U;
        config.monster_attack_speed_bp = 14500U;
        break;
    case AbyssRuleId::heavy_steps:
        config.player_ground_move_bp = 8500U;
        break;
    case AbyssRuleId::exhausted_recovery:
        config.player_resource_restore_bp = 7000U;
        break;
    case AbyssRuleId::life_sacrifice:
        config.player_max_health_bp = 5500U;
        break;
    default:
        break;
    }
    return config;
}

}  // namespace arpg::abyss
