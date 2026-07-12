#pragma once

#include "combat/monster_pool.hpp"

#include <cmath>
#include <cstdint>

namespace arpg::combat {

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
