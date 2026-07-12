#pragma once

#include "combat/combat_world.hpp"

#include <cstddef>

namespace arpg::test {

struct CombatWorldTestAccess final {
    static void apply_damage(
        combat::CombatWorld& world,
        int damage,
        combat::Vec3 source_position,
        combat::FeedbackLevel feedback) noexcept {
        world.apply_player_damage(damage, source_position, feedback);
    }

    static void fill_projectiles(
        combat::CombatWorld& world,
        combat::MonsterHandle owner) noexcept {
        for (std::size_t index = 0; index < combat::kProjectileCapacity;
             ++index) {
            static_cast<void>(world.spawn_projectile(
                owner, combat::Vec3{}, combat::Vec3{0.01F, 0.0F, 0.0F},
                1000U, 1, 0.1F));
        }
        static_cast<void>(world.spawn_projectile(
            owner, combat::Vec3{}, combat::Vec3{}, 1000U, 1, 0.1F));
    }
};

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
