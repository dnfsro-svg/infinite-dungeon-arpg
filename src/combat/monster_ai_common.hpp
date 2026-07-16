#pragma once

#include "combat/monster_pool.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace arpg::combat {

enum class BasisPointRounding : std::uint8_t {
    floor,
    ceil,
};

inline int scale_basis_points(
    int value,
    std::uint32_t basis_points,
    BasisPointRounding rounding = BasisPointRounding::floor) noexcept {
    if (value <= 0 || basis_points == 0U) return 0;
    const std::uint64_t product = static_cast<std::uint64_t>(value)
        * static_cast<std::uint64_t>(basis_points);
    const std::uint64_t scaled = rounding == BasisPointRounding::ceil
        ? (product + 9999U) / 10000U
        : product / 10000U;
    return static_cast<int>(std::min<std::uint64_t>(
        scaled,
        static_cast<std::uint64_t>((std::numeric_limits<int>::max)())));
}

inline float scale_basis_points(float value, std::uint32_t basis_points) noexcept {
    return value * static_cast<float>(basis_points) / 10000.0F;
}

inline std::uint16_t scale_ticks_ratio(
    std::uint16_t base,
    std::uint32_t numerator,
    std::uint32_t denominator = 10000U) noexcept {
    if (base == 0U) return 0U;
    if (numerator == 0U || denominator == 0U) return 1U;
    const std::uint64_t product = static_cast<std::uint64_t>(base)
        * static_cast<std::uint64_t>(numerator);
    const std::uint64_t scaled = (product + denominator - 1U) / denominator;
    return static_cast<std::uint16_t>(std::clamp<std::uint64_t>(
        scaled,
        1U,
        (std::numeric_limits<std::uint16_t>::max)()));
}

inline DamagePacket scale_monster_affix_damage(
    DamagePacket packet,
    const MonsterAffixProfile& profile) noexcept {
    for (int& amount : packet.amount) {
        amount = scaled_monster_damage(amount, profile);
    }
    return packet;
}

inline DamagePacket scale_monster_outgoing_damage(
    DamagePacket packet,
    std::uint32_t basis_points) noexcept {
    for (int& amount : packet.amount) {
        amount = scale_basis_points(amount, basis_points);
    }
    return packet;
}

inline float abyss_monster_move_step(
    float base,
    const MonsterAffixProfile& profile,
    const abyss::AbyssCombatConfig& config) noexcept {
    return scale_basis_points(
        monster_move_step(base, profile), config.monster_move_bp);
}

inline std::uint16_t abyss_monster_attack_ticks(
    std::uint16_t base,
    const MonsterAffixProfile& profile,
    const abyss::AbyssCombatConfig& config) noexcept {
    const std::uint16_t stage9 = scaled_monster_ticks(
        base, profile.attack_timing_bp);
    return scale_ticks_ratio(stage9, 10000U, config.monster_attack_speed_bp);
}

inline std::uint16_t abyss_monster_cooldown_ticks(
    std::uint16_t base,
    const MonsterAffixProfile& profile,
    const abyss::AbyssCombatConfig& config) noexcept {
    const std::uint16_t stage9_attack = scaled_monster_ticks(
        base, profile.attack_timing_bp);
    const std::uint16_t stage9_cooldown = scaled_monster_ticks(
        stage9_attack, profile.cooldown_bp);
    const std::uint16_t abyss_cooldown = scale_ticks_ratio(
        stage9_cooldown, config.monster_cooldown_bp);
    return scale_ticks_ratio(
        abyss_cooldown, 10000U, config.monster_attack_speed_bp);
}

inline float target_distance(Vec3 from, Vec3 to) noexcept {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    return std::sqrt(dx * dx + dy * dy);
}

inline void face_toward(MonsterRuntime& monster, Vec3 target) noexcept {
    if (target.x < monster.position.x) {
        monster.facing = Facing::left;
    } else if (target.x > monster.position.x) {
        monster.facing = Facing::right;
    }
}

inline bool tick_down(std::uint16_t& ticks) noexcept {
    if (ticks != 0) {
        --ticks;
    }
    return ticks == 0;
}

}  // namespace arpg::combat
