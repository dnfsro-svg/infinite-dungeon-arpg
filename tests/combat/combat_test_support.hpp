#pragma once

#include "combat/combat_world.hpp"

namespace arpg::test {

inline void tick_n(
    combat::CombatWorld& world,
    int count,
    combat::MovementInput movement = {}) noexcept {
    for (int tick = 0; tick < count; ++tick) {
        world.tick(movement);
    }
}

[[nodiscard]] inline bool finish_attack(
    combat::CombatWorld& world,
    int max_ticks) noexcept {
    for (int tick = 0; tick < max_ticks; ++tick) {
        if (world.snapshot().player.active_attack == combat::AttackId::none) {
            return true;
        }
        world.tick(combat::MovementInput{});
    }
    return world.snapshot().player.active_attack == combat::AttackId::none;
}

inline void drain_events(combat::CombatWorld& world) noexcept {
    while (world.try_pop_event().has_value()) {
    }
}

}  // namespace arpg::test
