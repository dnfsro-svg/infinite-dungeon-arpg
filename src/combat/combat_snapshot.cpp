#include "combat/combat_world.hpp"

#include "combat/attack_catalog.hpp"

namespace arpg::combat {

CombatSnapshot CombatWorld::snapshot() const noexcept {
    CombatSnapshot result{};
    result.tick = tick_;
    result.player = PlayerSnapshot{
        player_.position, player_.velocity, player_.facing, player_.state,
        attack_.id,
        attack_.id == AttackId::none
            ? AttackPhase::finished
            : attack_phase_at(
                  *find_attack_definition(attack_.id), attack_.elapsed_ticks,
                  attack_.startup_ticks, attack_.recovery_ticks),
        attack_.elapsed_ticks, player_.combo_stage, player_.hit_stop_ticks,
        player_.air_attack_available, player_.hp, player_.max_hp,
        player_.barrier, player_.max_barrier, player_.damage_reduction,
        player_.damage_reduction_cap, player_.armor, player_.evasion,
        player_.armor_reduction_bp, player_.evasion_rate_bp,
        player_.hurt_ticks, player_.invulnerability_ticks,
        player_.status.slow_bp, player_.status.slow_ticks,
        player_.status.corrosion_damage_per_second,
        player_.status.corrosion_ticks, player_.status.corrosion_tick_phase,
    };
    for (std::size_t index = 0; index < monsters_.slots().size(); ++index) {
        const MonsterRuntime& monster = monsters_.slots()[index];
        result.monsters[index] = MonsterSnapshot{
            monster.active, monster.generation, monster.id, monster.affixes,
            monster.spawn_ordinal, monster.spawn,
            monster.position, monster.velocity, monster.kind, monster.facing,
            monster.reaction, monster.armor, monster.hp, monster.max_hp,
            monster.break_value, monster.max_break, monster.shield,
            monster.max_shield, monster.shield_ticks, monster.max_shield_ticks,
            monster.break_window_ticks, monster.hit_stop_ticks, monster.ai_phase,
            monster.attack_target_position, monster.attack_vector,
            monster.affix_warning, monster.affix_warning_ticks,
            monster.blink_empowered,
        };
    }
    result.monster_count = monsters_.active_count();
    if (projectiles_.active_count() != 0U) {
        for (std::size_t index = 0; index < projectiles_.slots().size(); ++index) {
            const ProjectileRuntime& projectile = projectiles_.slots()[index];
            if (!projectile.active) continue;
            result.projectiles[index] = ProjectileSnapshot{
                projectile.active, projectile.generation, projectile.owner,
                projectile.position, projectile.velocity, projectile.lifetime_ticks,
                projectile.damage, projectile.radius,
                projectile.trigger_chain_on_end,
            };
        }
    }
    result.projectile_count = projectiles_.active_count();
    if (hazards_.active_count() != 0U) {
        for (std::size_t index = 0; index < hazards_.slots().size(); ++index) {
            const HazardRuntime& hazard = hazards_.slots()[index];
            if (!hazard.active) continue;
            result.hazards[index] = HazardSnapshot{
                hazard.active, hazard.generation, hazard.owner, hazard.kind, hazard.center,
                hazard.radius, hazard.telegraph_ticks, hazard.active_ticks,
                hazard.lifetime_ticks, hazard.damage_interval_ticks,
                hazard.player_latched, hazard.persists_after_owner_death, hazard.damage,
            };
        }
    }
    result.hazard_count = hazards_.active_count();
    std::size_t compatibility_index = 0U;
    for (std::size_t index = 0;
         index < monsters_.slots().size() && compatibility_index < kDummyCount;
         ++index) {
        if (!result.monsters[index].active) continue;
        result.dummies[compatibility_index] = result.monsters[index];
        ++compatibility_index;
    }
    std::size_t effect_owner_count = 0;
    std::size_t active_effect_count = 0;
    std::uint32_t effect_overflow_count = 0;
    std::uint32_t effect_command_overflow_count = 0;
    for (const auto& owner : effect_owners_) {
        if (!owner.occupied) continue;
        ++effect_owner_count;
        active_effect_count += owner.effects.active_count();
        effect_overflow_count += owner.effects.diagnostics().effect_overflows;
        effect_command_overflow_count += owner.effects.diagnostics().command_overflows;
    }
    result.diagnostics = CombatDiagnostics{
        input_buffer_.size(), input_buffer_.expired_count(),
        input_buffer_.overflow_count(), event_overflow_count_,
        projectile_saturation_count_, projectile_invalid_owner_count_,
        hazard_saturation_count_, hazard_invalid_owner_count_, effect_owner_count,
        active_effect_count, effect_overflow_count, effect_command_overflow_count,
    };
    return result;
}

}  // namespace arpg::combat
