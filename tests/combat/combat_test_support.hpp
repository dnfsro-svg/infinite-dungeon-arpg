#pragma once

#include "combat/combat_world.hpp"

#include <cstddef>
#include <cstdint>

namespace arpg::test {

struct CombatWorldTestAccess final {
    static void fill_event_queue(
        combat::CombatWorld& world, std::size_t count) noexcept {
        combat::CombatEvent event{};
        event.kind = combat::CombatEventKind::reset;
        for (std::size_t index = 0U; index < count; ++index) {
            world.emit_event(event);
        }
    }
    static void apply_damage(
        combat::CombatWorld& world,
        int damage,
        combat::Vec3 source_position,
        combat::FeedbackLevel feedback) noexcept {
        world.apply_player_damage(damage, source_position, feedback);
    }

    static void apply_damage(
        combat::CombatWorld& world,
        combat::DamagePacket packet,
        combat::DamageDelivery delivery,
        combat::Vec3 source_position,
        combat::FeedbackLevel feedback) noexcept {
        world.apply_player_damage(packet, delivery, source_position, feedback);
    }

    static void apply_damage(
        combat::CombatWorld& world,
        combat::DamagePacket packet,
        combat::Vec3 source_position,
        combat::FeedbackLevel feedback) noexcept {
        apply_damage(world, packet, combat::DamageDelivery::direct,
                     source_position, feedback);
    }

    static void fill_projectiles(
        combat::CombatWorld& world,
        combat::MonsterHandle owner) noexcept {
        for (std::size_t index = 0; index < combat::kProjectileCapacity;
             ++index) {
            static_cast<void>(world.spawn_projectile(
                owner, combat::Vec3{10.0F, 5.0F, 0.0F}, combat::Vec3{},
                1000U, 1, 0.1F));
        }
        static_cast<void>(world.spawn_projectile(
            owner, combat::Vec3{10.0F, 5.0F, 0.0F}, combat::Vec3{},
            1000U, 1, 0.1F));
    }

    static bool spawn_projectile(
        combat::CombatWorld& world,
        combat::MonsterHandle owner) noexcept {
        return world.spawn_projectile(
            owner, combat::Vec3{}, combat::Vec3{}, 1000U, 1, 0.1F);
    }

    static bool spawn_projectile(
        combat::CombatWorld& world,
        combat::MonsterHandle owner,
        combat::Vec3 position,
        combat::Vec3 velocity,
        std::uint16_t lifetime_ticks,
        combat::DamagePacket damage,
        float radius,
        bool trigger_chain_on_end,
        combat::MonsterAffixSet owner_affixes) noexcept {
        return world.spawn_projectile(owner, position, velocity, lifetime_ticks,
            damage, radius, trigger_chain_on_end, owner_affixes);
    }

    static bool spawn_hazard(
        combat::CombatWorld& world,
        combat::MonsterHandle owner) noexcept {
        return world.spawn_hazard(owner, combat::HazardKind::native,
            combat::Vec3{}, 1.0F,
            1000U, 1000U, 30U, 1);
    }

    static void fill_hazards(
        combat::CombatWorld& world,
        combat::MonsterHandle owner) noexcept {
        for (std::size_t index = 0; index < combat::kHazardCapacity;
             ++index) {
            static_cast<void>(world.spawn_hazard(
                owner, combat::HazardKind::native, combat::Vec3{}, 1.0F,
                1000U, 1000U, 30U, 1));
        }
    }

    static void set_saturation_counts(
        combat::CombatWorld& world,
        std::uint32_t projectile,
        std::uint32_t hazard) noexcept {
        world.projectile_saturation_count_ = projectile;
        world.hazard_saturation_count_ = hazard;
    }

    static void set_invalid_owner_counts(
        combat::CombatWorld& world,
        std::uint32_t projectile,
        std::uint32_t hazard) noexcept {
        world.projectile_invalid_owner_count_ = projectile;
        world.hazard_invalid_owner_count_ = hazard;
    }

    static void set_monster_shield(
        combat::CombatWorld& world,
        std::size_t index,
        int shield) noexcept {
        if (index < world.monsters_.slots_.size()) {
            world.monsters_.slots_[index].shield = shield;
        }
    }

    static combat::MonsterAffixProfile monster_affix_profile(
        const combat::CombatWorld& world,
        std::size_t index) noexcept {
        return index < world.monsters_.slots_.size()
            ? world.monsters_.slots_[index].affix_profile
            : combat::MonsterAffixProfile{};
    }

    static void set_player_resources(
        combat::CombatWorld& world,
        int hp,
        int barrier) noexcept {
        world.player_.hp = hp;
        world.player_.barrier = barrier;
    }

    static const abyss::AbyssCombatConfig& abyss_config(
        const combat::CombatWorld& world) noexcept {
        return world.encounter_config_.abyss;
    }

    static void apply_monster_direct_hit(
        combat::CombatWorld& world,
        std::size_t slot,
        combat::DamagePacket packet,
        combat::Vec3 source_position,
        combat::FeedbackLevel feedback) noexcept {
        world.apply_monster_direct_hit(slot, packet, source_position, feedback);
    }

    static void defeat_monster(
        combat::CombatWorld& world,
        std::size_t slot,
        bool reward_eligible) noexcept {
        if (slot < world.monsters_.slots_.size()) {
            world.monsters_.slots_[slot].hp = 0;
            world.defeat_monster(slot, combat::AttackId::j1, reward_eligible);
        }
    }

    static void freeze_monster_ai(
        combat::CombatWorld& world,
        std::size_t slot,
        std::uint16_t ticks) noexcept {
        if (slot < world.monsters_.slots_.size()) {
            world.monsters_.slots_[slot].hit_stop_ticks = ticks;
        }
    }

    static void set_active_affix_ticks(
        combat::CombatWorld& world,
        std::size_t slot,
        std::uint16_t burning_ticks,
        std::uint16_t blink_ticks) noexcept {
        if (slot < world.monsters_.slots_.size()) {
            world.monsters_.slots_[slot].burning_ground_ticks = burning_ticks;
            world.monsters_.slots_[slot].blink_assault_ticks = blink_ticks;
        }
    }

    static void set_blink_empowered(
        combat::CombatWorld& world,
        std::size_t slot,
        bool empowered) noexcept {
        if (slot < world.monsters_.slots_.size()) {
            world.monsters_.slots_[slot].blink_empowered = empowered;
        }
    }

    static void tick_active_affixes(
        combat::CombatWorld& world,
        std::size_t slot) noexcept {
        if (slot < world.monsters_.slots_.size()) {
            world.tick_active_affixes(slot, world.monsters_.slots_[slot]);
        }
    }

    static void arm_monster_active_attack(
        combat::CombatWorld& world,
        std::size_t slot) noexcept {
        if (slot < world.monsters_.slots_.size()) {
            auto& monster = world.monsters_.slots_[slot];
            monster.ai_phase = combat::MonsterAiPhase::active;
            monster.ai_ticks = 1U;
            monster.contact_attack_resolved = false;
            monster.attack_target_position = world.player_.position;
        }
    }

    static void simulate_monster(
        combat::CombatWorld& world,
        std::size_t slot) noexcept {
        world.simulate_monster(slot);
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
