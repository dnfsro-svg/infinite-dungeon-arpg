#pragma once

#include "combat/combat_scaling.hpp"
#include "combat/monster_pool.hpp"

#include <cmath>
#include <cstdint>

namespace arpg::combat {

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
