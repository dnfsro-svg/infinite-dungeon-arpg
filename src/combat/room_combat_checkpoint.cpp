#include "combat/room_combat_checkpoint.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/combat_world.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/monster_persistent_state.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/room_bounds.hpp"
#include "skills/active_skill_catalog.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace arpg::combat {

static_assert(limits::kRoomMonsterCapacity == 1152U);
static_assert(kRoomEnvironmentCellCount == 400U);

namespace {

[[nodiscard]] bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z);
}

[[nodiscard]] bool valid_position(Vec3 value) noexcept {
    return finite(value) && value.x >= room_bounds::min_x
        && value.x <= room_bounds::max_x && value.y >= room_bounds::min_y
        && value.y <= room_bounds::max_y && value.z >= 0.0F
        && value.z <= 32.0F;
}

[[nodiscard]] bool valid_affixes(const MonsterAffixSet& affixes) noexcept {
    if (affixes.count > affixes.values.size()) return false;
    for (std::size_t index = 0U; index < affixes.count; ++index) {
        if (affixes.values[index].id >= MonsterAffixId::count
                || affixes.values[index].tier >= MonsterAffixTier::count) {
            return false;
        }
        for (std::size_t prior = 0U; prior < index; ++prior) {
            if (affixes.values[prior].id == affixes.values[index].id) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool same_source(const PlayerDamageSource& left,
    const PlayerDamageSource& right) noexcept {
    return left.kind == right.kind && left.monster == right.monster
        && left.detail_id == right.detail_id;
}

[[nodiscard]] PlayerDamageSource canonical_source(
    PlayerDamageSource source) noexcept {
    const auto valid_monster = [](MonsterId value) noexcept {
        return value < MonsterId::count;
    };
    switch (source.kind) {
    case PlayerDamageSourceKind::monster_attack:
    case PlayerDamageSourceKind::projectile:
        if (!valid_monster(source.monster)) return {};
        source.detail_id = 0U;
        return source;
    case PlayerDamageSourceKind::ground_hazard:
        return valid_monster(source.monster)
                && source.detail_id
                    <= static_cast<std::uint16_t>(HazardKind::chaos_expansion)
            ? source : PlayerDamageSource{};
    case PlayerDamageSourceKind::monster_affix:
        return valid_monster(source.monster)
                && source.detail_id
                    < static_cast<std::uint16_t>(MonsterAffixId::count)
            ? source : PlayerDamageSource{};
    case PlayerDamageSourceKind::abyss_environment:
        if (source.detail_id > static_cast<std::uint16_t>(
                abyss::AbyssRuleId::life_sacrifice)) return {};
        source.monster = MonsterId::count;
        return source;
    case PlayerDamageSourceKind::unknown:
    default:
        return {};
    }
}

[[nodiscard]] bool same_profile(const MonsterAffixProfile& left,
    const MonsterAffixProfile& right) noexcept {
    return left.max_hp == right.max_hp
        && left.armor_rating == right.armor_rating
        && left.max_shield == right.max_shield
        && left.shield_recharge_delay_ticks
            == right.shield_recharge_delay_ticks
        && left.damage_bp == right.damage_bp
        && left.attack_timing_bp == right.attack_timing_bp
        && left.move_bp == right.move_bp
        && left.cooldown_bp == right.cooldown_bp
        && left.horizontal_impulse_bp == right.horizontal_impulse_bp;
}

[[nodiscard]] const MonsterAffixTierValues* affix_values(
    const MonsterAffixSet& affixes, MonsterAffixId id) noexcept {
    for (std::size_t index = 0U;
            index < affixes.count && index < affixes.values.size(); ++index) {
        const MonsterAffixInstance& instance = affixes.values[index];
        if (instance.id != id) continue;
        const MonsterAffixDefinition* definition =
            monster_affix_definition(instance.id);
        const std::size_t tier = static_cast<std::size_t>(instance.tier);
        return definition != nullptr && tier < definition->tiers.size()
            ? &definition->tiers[tier] : nullptr;
    }
    return nullptr;
}

[[nodiscard]] bool same_crate(const FireRoomCrateSnapshot& left,
    const FireRoomCrateSnapshot& right) noexcept {
    return left.position.x == right.position.x
        && left.position.y == right.position.y
        && left.position.z == right.position.z
        && left.broken_tick == right.broken_tick
        && left.intact == right.intact;
}

void capture_monster(const MonsterRuntime& source,
    MonsterCombatCheckpoint& out) noexcept {
    out = {};
    out.ordinal = source.monster_ordinal;
    out.id = source.id;
    out.affixes = source.affixes;
    out.affix_profile = source.affix_profile;
    out.kind = source.kind;
    out.spawn = source.spawn;
    out.position = source.position;
    out.velocity = source.velocity;
    out.facing = source.facing;
    out.reaction = source.reaction;
    out.armor = source.armor;
    out.reaction_ticks = source.reaction_ticks;
    out.ai_phase = source.ai_phase;
    out.ai_ticks = source.ai_ticks;
    out.attack_serial = source.attack_serial;
    out.contact_attack_resolved = source.contact_attack_resolved;
    out.hp = source.hp;
    out.max_hp = source.max_hp;
    out.break_value = source.break_value;
    out.max_break = source.max_break;
    out.shield = source.shield;
    out.max_shield = source.max_shield;
    out.shield_ticks = source.shield_ticks;
    out.max_shield_ticks = source.max_shield_ticks;
    out.shield_recharge_ticks = source.shield_recharge_ticks;
    out.break_window_ticks = source.break_window_ticks;
    out.owner_transient_counter = source.owner_transient_counter;
    out.burning_ground_ticks = source.burning_ground_ticks;
    out.blink_assault_ticks = source.blink_assault_ticks;
    out.blink_empowered = source.blink_empowered;
    out.attack_target_position = source.attack_target_position;
    out.attack_vector = source.attack_vector;
    out.affix_warning = source.affix_warning;
    out.affix_warning_ticks = source.affix_warning_ticks;
    source.effects.capture_checkpoint(out.effects);
    out.effects_touched = source.effects_touched;
}

void capture_monster(const MonsterPersistentState& source,
    MonsterCombatCheckpoint& out) noexcept {
    out = {};
    out.ordinal = source.spawn_ordinal;
    out.id = source.id;
    out.affixes = source.affixes;
    out.affix_profile = source.affix_profile;
    out.kind = source.kind;
    out.spawn = source.spawn;
    out.position = source.position;
    out.velocity = source.velocity;
    out.facing = source.facing;
    out.reaction = source.reaction;
    out.armor = source.armor;
    out.reaction_ticks = source.reaction_ticks;
    out.ai_phase = source.ai_phase;
    out.ai_ticks = source.ai_ticks;
    out.attack_serial = source.attack_serial;
    out.contact_attack_resolved = source.contact_attack_resolved;
    out.hp = source.hp;
    out.max_hp = source.max_hp;
    out.break_value = source.break_value;
    out.max_break = source.max_break;
    out.shield = source.shield;
    out.max_shield = source.max_shield;
    out.shield_ticks = source.shield_ticks;
    out.max_shield_ticks = source.max_shield_ticks;
    out.shield_recharge_ticks = source.shield_recharge_ticks;
    out.break_window_ticks = source.break_window_ticks;
    out.owner_transient_counter = source.owner_transient_counter;
    out.burning_ground_ticks = source.burning_ground_ticks;
    out.blink_assault_ticks = source.blink_assault_ticks;
    out.blink_empowered = source.blink_empowered;
    out.attack_target_position = source.attack_target_position;
    out.attack_vector = source.attack_vector;
    out.affix_warning = source.affix_warning;
    out.affix_warning_ticks = source.affix_warning_ticks;
    source.effects.capture_checkpoint(out.effects);
    out.effects_touched = source.effects_touched;
}

[[nodiscard]] bool valid_player(
    const PlayerCombatCheckpoint& player) noexcept {
    if (!valid_position(player.position) || !finite(player.velocity)
            || (player.facing != Facing::left
                && player.facing != Facing::right)
            || player.state > PlayerState::landing
            || player.hp < 0 || player.max_hp <= 0
            || player.hp > player.max_hp || player.barrier < 0
            || player.max_barrier < 0 || player.barrier > player.max_barrier
            || player.armor < 0 || player.evasion < 0
            || player.armor_reduction_bp < 0
            || player.armor_reduction_bp > 10000
            || player.evasion_rate_bp < 0
            || player.evasion_rate_bp > 10000
            || player.hurt_ticks > 12U
            || player.invulnerability_ticks > 30U
            || player.status.slow_bp < 0
            || player.status.slow_bp > 10000
            || player.status.corrosion_damage_per_second < 0
            || player.status.corrosion_tick_phase >= 60U
            || player.status.corrosion_source.kind
                > PlayerDamageSourceKind::unknown
            || (player.status.corrosion_source.monster >= MonsterId::count
                && player.status.corrosion_source.monster
                    != MonsterId::count)
            || !same_source(player.status.corrosion_source,
                canonical_source(player.status.corrosion_source))
            || (player.status.slow_ticks == 0U
                && player.status.slow_bp != 0)
            || (player.status.corrosion_ticks == 0U
                && (player.status.corrosion_damage_per_second != 0
                    || player.status.corrosion_tick_phase != 0U
                    || !same_source(player.status.corrosion_source,
                        PlayerDamageSource{})))
            || (player.status.corrosion_ticks != 0U
                && (player.status.corrosion_damage_per_second <= 0
                    || player.status.corrosion_source.kind
                        != PlayerDamageSourceKind::monster_affix
                    || player.status.corrosion_source.detail_id
                        != static_cast<std::uint16_t>(
                            MonsterAffixId::chaos_corrosion)))) {
        return false;
    }
    for (std::size_t index = 0U;
            index < player.damage_reduction.size(); ++index) {
        if (player.damage_reduction_cap[index] < 0
                || player.damage_reduction_cap[index] > 10000
                || player.damage_reduction[index] < -10000
                || player.damage_reduction[index]
                    > player.damage_reduction_cap[index]) return false;
    }
    return true;
}

[[nodiscard]] bool valid_attack(const AttackCheckpoint& attack,
    std::uint32_t generated) noexcept {
    bool any_hit_target = false;
    for (const std::uint64_t word : attack.hit_targets.words) {
        any_hit_target = any_hit_target || word != 0U;
    }
    if (attack.id == AttackId::none) {
        if (attack.elapsed_ticks != 0U || attack.startup_ticks != 0U
                || attack.recovery_ticks != 0U || attack.connected
                || attack.impact_event_emitted) return false;
        for (const std::uint64_t word : attack.hit_targets.words) {
            if (word != 0U) return false;
        }
    } else {
        const AttackDefinition* definition = find_attack_definition(attack.id);
        if (definition == nullptr || attack.startup_ticks == 0U
                || attack.recovery_ticks == 0U
                || attack.elapsed_ticks
                    >= static_cast<std::uint32_t>(attack.startup_ticks)
                        + definition->active_ticks + attack.recovery_ticks
                || attack.connected != any_hit_target
                || attack.impact_event_emitted != any_hit_target
                || (attack.elapsed_ticks < attack.startup_ticks
                    && any_hit_target)) {
            return false;
        }
    }
    for (std::uint32_t ordinal = generated;
            ordinal < limits::kRoomMonsterCapacity; ++ordinal) {
        if (attack.hit_targets.contains(
                static_cast<MonsterOrdinal>(ordinal))) return false;
    }
    return true;
}

[[nodiscard]] bool valid_monster(
    const MonsterCombatCheckpoint& monster) noexcept {
    if (monster.ordinal >= limits::kRoomMonsterCapacity
            || monster.id >= MonsterId::count
            || !valid_affixes(monster.affixes)
            || !valid_position(monster.spawn)
            || !valid_position(monster.position)
            || !finite(monster.velocity)
            || !finite(monster.attack_target_position)
            || !finite(monster.attack_vector)
            || (monster.facing != Facing::left
                && monster.facing != Facing::right)
            || monster.kind > DummyKind::heavy
            || monster.reaction >= ReactionState::defeated
            || monster.armor > ArmorState::broken
            || monster.ai_phase >= MonsterAiPhase::defeated
            || monster.affix_warning > MonsterAffixWarning::death_blast
            || monster.hp <= 0 || monster.max_hp <= 0
            || monster.hp > monster.max_hp || monster.max_break < 0
            || monster.break_value < 0
            || monster.break_value > monster.max_break
            || monster.max_shield < 0 || monster.shield < 0
            || monster.shield > monster.max_shield
            || monster.shield_ticks > monster.max_shield_ticks) {
        return false;
    }
    if ((monster.max_break == 0
            && (monster.break_value != 0
                || monster.armor != ArmorState::none
                || monster.break_window_ticks != 0U))
            || (monster.max_break > 0
                && ((monster.armor == ArmorState::broken
                        && (monster.break_value != 0
                            || monster.break_window_ticks == 0U
                            || monster.break_window_ticks > 180U))
                    || (monster.armor == ArmorState::armored
                        && (monster.break_value == 0
                            || monster.break_window_ticks != 0U))
                    || monster.armor == ArmorState::none))) return false;
    const MonsterAffixTierValues* burning = affix_values(
        monster.affixes, MonsterAffixId::burning_ground);
    if ((burning == nullptr && monster.burning_ground_ticks != 0U)
            || (burning != nullptr
                && (burning->interval_ticks == 0U
                    ? monster.burning_ground_ticks != 0U
                    : monster.burning_ground_ticks
                        >= burning->interval_ticks))) return false;
    const MonsterAffixTierValues* blink = affix_values(
        monster.affixes, MonsterAffixId::blink_assault);
    if ((blink == nullptr && (monster.blink_assault_ticks != 0U
                || monster.blink_empowered
                || monster.affix_warning != MonsterAffixWarning::none
                || monster.affix_warning_ticks != 0U))
            || (blink != nullptr
                && ((blink->interval_ticks == 0U
                        ? monster.blink_assault_ticks != 0U
                        : monster.blink_assault_ticks
                            >= blink->interval_ticks)
                    || (monster.affix_warning == MonsterAffixWarning::blink
                        ? (monster.affix_warning_ticks == 0U
                            || monster.affix_warning_ticks
                                > blink->duration_ticks)
                        : (monster.affix_warning
                                != MonsterAffixWarning::none
                            || monster.affix_warning_ticks != 0U))))) {
        return false;
    }
    modifiers::EffectSet effects{};
    if (!effects.restore_checkpoint(monster.effects)) return false;
    return monster.shield_recharge_ticks
            <= monster.affix_profile.shield_recharge_delay_ticks
        && (monster.effects_touched || (effects.active_count() == 0U
            && effects.queued_command_count() == 0U
            && effects.diagnostics().effect_overflows == 0U
            && effects.diagnostics().command_overflows == 0U));
}

[[nodiscard]] bool inside(const Aabb& bounds, Vec3 position) noexcept {
    return position.x >= bounds.minimum.x && position.x <= bounds.maximum.x
        && position.y >= bounds.minimum.y && position.y <= bounds.maximum.y
        && position.z >= bounds.minimum.z && position.z <= bounds.maximum.z;
}

void to_persistent(const MonsterCombatCheckpoint& source,
    MonsterPersistentState& out) noexcept {
    out = {};
    out.id = source.id;
    out.affixes = source.affixes;
    out.spawn_ordinal = source.ordinal;
    out.affix_profile = source.affix_profile;
    out.kind = source.kind;
    out.spawn = source.spawn;
    out.position = source.position;
    out.velocity = source.velocity;
    out.facing = source.facing;
    out.reaction = source.reaction;
    out.armor = source.armor;
    out.reaction_ticks = source.reaction_ticks;
    out.ai_phase = source.ai_phase;
    out.ai_ticks = source.ai_ticks;
    out.attack_serial = source.attack_serial;
    out.contact_attack_resolved = source.contact_attack_resolved;
    out.hp = source.hp;
    out.max_hp = source.max_hp;
    out.break_value = source.break_value;
    out.max_break = source.max_break;
    out.shield = source.shield;
    out.max_shield = source.max_shield;
    out.shield_ticks = source.shield_ticks;
    out.max_shield_ticks = source.max_shield_ticks;
    out.shield_recharge_ticks = source.shield_recharge_ticks;
    out.break_window_ticks = source.break_window_ticks;
    out.hit_stop_ticks = 0U;
    out.owner_transient_counter = source.owner_transient_counter;
    out.burning_ground_ticks = source.burning_ground_ticks;
    out.blink_assault_ticks = source.blink_assault_ticks;
    out.blink_empowered = source.blink_empowered;
    out.attack_target_position = source.attack_target_position;
    out.attack_vector = source.attack_vector;
    out.affix_warning = source.affix_warning;
    out.affix_warning_ticks = source.affix_warning_ticks;
    static_cast<void>(out.effects.restore_checkpoint(source.effects));
    out.effects_touched = source.effects_touched;
    out.touched = true;
    out.defeated = false;
}

}  // namespace

bool valid_room_combat_checkpoint_structural(
    const RoomCombatCheckpoint& checkpoint,
    const std::uint32_t generated_monsters) noexcept {
    bool rng_nonzero = false;
    for (const std::uint64_t word : checkpoint.evasion_rng_state) {
        rng_nonzero = rng_nonzero || word != 0U;
    }
    PlayerDamageHistory checked_history{};
    if (!rng_nonzero || generated_monsters > limits::kRoomMonsterCapacity
            || !valid_player(checkpoint.player)
            || !valid_attack(checkpoint.attack, generated_monsters)
            || !checked_history.restore_checkpoint(
                checkpoint.player_damage_history)
            || checkpoint.monster_count > checkpoint.monsters.size()
            || checkpoint.obstacle_count > checkpoint.obstacles.size()
            || checkpoint.fire_crate_count > checkpoint.fire_crates.size()) {
        return false;
    }
    if (checkpoint.player_damage_history.initialized) {
        if (checkpoint.player_damage_history.active_tick > checkpoint.tick
                || (!checkpoint.has_death_snapshot
                    && checkpoint.tick
                        - checkpoint.player_damage_history.active_tick > 1U)) {
            return false;
        }
    } else if (checkpoint.tick != 0U) {
        return false;
    }
    if (checkpoint.player.skill_cooldowns[
                static_cast<std::size_t>(skills::ActiveSkillId::draw_slash)]
            > skills::kDrawSlashCooldownTicks
            || checkpoint.player.skill_cooldowns[
                static_cast<std::size_t>(skills::ActiveSkillId::storm_swords)]
                > skills::kStormSwordsCooldownTicks
            || (checkpoint.attack.id == AttackId::none
                && checkpoint.player.state >= PlayerState::attack_startup
                && checkpoint.player.state <= PlayerState::attack_recovery)
            || (checkpoint.attack.id == AttackId::none
                && checkpoint.player.combo_stage != 0U)
            || (checkpoint.attack.id == AttackId::none
                && (checkpoint.player.state == PlayerState::jump_rise
                    || checkpoint.player.state == PlayerState::jump_fall)
                && checkpoint.player.position.z == 0.0F
                && checkpoint.player.velocity.z == 0.0F)
            || (checkpoint.abyss_environment.rule
                    > abyss::AbyssRuleId::life_sacrifice
                && checkpoint.abyss_environment.rule
                    != abyss::AbyssRuleId::none)
            || !finite(checkpoint.abyss_environment.locked_center)) {
        return false;
    }

    MonsterOrdinal previous_monster = kInvalidMonsterOrdinal;
    for (std::uint16_t index = 0U; index < checkpoint.monster_count; ++index) {
        const MonsterCombatCheckpoint& monster = checkpoint.monsters[index];
        if (!valid_monster(monster) || monster.ordinal >= generated_monsters
                || (index != 0U && monster.ordinal <= previous_monster)) {
            return false;
        }
        previous_monster = monster.ordinal;
    }
    std::uint16_t previous_obstacle{};
    for (std::uint16_t index = 0U; index < checkpoint.obstacle_count; ++index) {
        const RoomObstacleCheckpoint& obstacle = checkpoint.obstacles[index];
        modifiers::EffectSet effects{};
        if ((index != 0U && obstacle.ordinal <= previous_obstacle)
                || (obstacle.intact
                    ? (obstacle.max_hp == 0U
                        ? (obstacle.hp != 0U
                            || obstacle.broken_tick != 0U)
                        : (obstacle.hp == 0U
                            || obstacle.hp > obstacle.max_hp
                            || obstacle.broken_tick != 0U))
                    : (obstacle.max_hp == 0U || obstacle.hp != 0U
                        || obstacle.broken_tick > checkpoint.tick))
                || !effects.restore_checkpoint(obstacle.effects)) {
            return false;
        }
        previous_obstacle = obstacle.ordinal;
    }
    for (std::uint8_t index = 0U;
            index < checkpoint.fire_crate_count; ++index) {
        const FireRoomCrateSnapshot& crate = checkpoint.fire_crates[index];
        if (!finite(crate.position)
                || (crate.intact && crate.broken_tick != 0U)
                || (!crate.intact && crate.broken_tick > checkpoint.tick)) {
            return false;
        }
    }

    if (!checkpoint.has_death_snapshot) return checkpoint.player.hp != 0;
    const CombatDeathSnapshot& death = checkpoint.death_snapshot;
    std::uint64_t recent_total{};
    for (const std::uint64_t value : death.recent_damage) {
        if (recent_total > (std::numeric_limits<std::uint64_t>::max)() - value) {
            return false;
        }
        recent_total += value;
    }
    return death.tick <= checkpoint.tick && death.final_damage != 0U
        && death.health_loss != 0U && death.defense.hp == 0
        && death.raw_damage >= death.final_damage
        && death.barrier_loss <= static_cast<std::uint64_t>(
            checkpoint.player.max_barrier - checkpoint.player.barrier)
        && death.health_loss <= static_cast<std::uint64_t>(
            checkpoint.player.max_hp)
        && death.barrier_loss
            <= (std::numeric_limits<std::uint64_t>::max)() - death.health_loss
        && death.final_damage == death.barrier_loss + death.health_loss
        && death.source.kind <= PlayerDamageSourceKind::unknown
        && death.primary_type <= modifiers::DamageType::chaos
        && recent_total >= death.final_damage
        && death.recent_damage[modifiers::damage_index(death.primary_type)] != 0U
        && checkpoint.player.hp == 0
        && death.defense.max_hp == checkpoint.player.max_hp
        && death.defense.barrier == checkpoint.player.barrier
        && death.defense.max_barrier == checkpoint.player.max_barrier
        && death.defense.armor == checkpoint.player.armor
        && death.defense.evasion == checkpoint.player.evasion
        && death.defense.armor_reduction_bp
            == checkpoint.player.armor_reduction_bp
        && death.defense.evasion_rate_bp
            == checkpoint.player.evasion_rate_bp
        && death.defense.damage_reduction == checkpoint.player.damage_reduction
        && death.defense.damage_reduction_cap
            == checkpoint.player.damage_reduction_cap
        && death.recent_damage == checked_history.totals()
        && checkpoint.player_damage_history.initialized
        && checkpoint.player_damage_history.active_tick == death.tick
        && same_source(death.source, canonical_source(death.source));
}

bool CombatWorld::capture_room_checkpoint(
    RoomCombatCheckpoint& out) const noexcept {
    out.tick = tick_;
    out.evasion_rng_state = evasion_rng_.export_state();
    out.player = {
        player_.position, player_.velocity, player_.facing, player_.state,
        player_.combo_stage, player_.air_attack_available, player_.hp,
        player_.max_hp, player_.barrier, player_.max_barrier,
        player_.damage_reduction, player_.damage_reduction_cap,
        player_.armor, player_.evasion, player_.armor_reduction_bp,
        player_.evasion_rate_bp, player_.hurt_ticks,
        player_.invulnerability_ticks, player_.status,
        active_skill_.cooldowns,
    };
    if (active_skill_.snapshot.id != skills::ActiveSkillId::none
            || (attack_.id == AttackId::none
                && out.player.state >= PlayerState::attack_startup
                && out.player.state <= PlayerState::attack_recovery)) {
        if (out.player.position.z > 0.0F) {
            out.player.state = out.player.velocity.z > 0.0F
                ? PlayerState::jump_rise : PlayerState::jump_fall;
        } else {
            out.player.state = PlayerState::idle;
        }
    }
    out.abyss_environment = abyss_environment_;
    out.attack = {attack_.id, attack_.elapsed_ticks, attack_.startup_ticks,
        attack_.recovery_ticks, attack_.connected,
        attack_.impact_event_emitted, attack_.hit_targets};
    if (out.attack.id == AttackId::none) out.player.combo_stage = 0U;
    for (MonsterCombatCheckpoint& monster : out.monsters) monster = {};
    out.monster_count = 0U;
    if (room_monster_field_ != nullptr) {
        for (std::uint32_t ordinal = 0U;
                ordinal < room_monster_field_->total_count(); ++ordinal) {
            const auto value = static_cast<MonsterOrdinal>(ordinal);
            const MonsterRuntime* active =
                room_monster_field_->active_runtime(value);
            const MonsterPersistentState* persistent =
                room_monster_field_->persistent_state(value);
            if (persistent == nullptr || persistent->defeated
                    || (active == nullptr && !persistent->touched)) continue;
            if (out.monster_count >= out.monsters.size()) return false;
            if (active != nullptr) {
                capture_monster(*active, out.monsters[out.monster_count]);
            } else {
                capture_monster(*persistent,
                    out.monsters[out.monster_count]);
            }
            ++out.monster_count;
        }
    } else {
        for (const MonsterRuntime& monster : monsters_.slots()) {
            if (!monster.active || monster.hp <= 0) continue;
            if (out.monster_count >= out.monsters.size()) return false;
            capture_monster(monster, out.monsters[out.monster_count++]);
        }
        std::sort(out.monsters.begin(),
            out.monsters.begin() + out.monster_count,
            [](const MonsterCombatCheckpoint& left,
                    const MonsterCombatCheckpoint& right) noexcept {
                return left.ordinal < right.ordinal;
            });
    }
    for (RoomObstacleCheckpoint& obstacle : out.obstacles) obstacle = {};
    out.obstacle_count = 0U;
    if (room_obstacles_ != nullptr) {
        for (std::uint16_t ordinal = 0U;
                ordinal < kRoomEnvironmentRecordCapacity; ++ordinal) {
            const RoomObstacleState* obstacle = room_obstacles_->state(ordinal);
            if (obstacle == nullptr) continue;
            if (out.obstacle_count >= out.obstacles.size()) return false;
            RoomObstacleCheckpoint& target = out.obstacles[out.obstacle_count++];
            target = {obstacle->ordinal, obstacle->hp, obstacle->max_hp,
                obstacle->broken_tick, obstacle->intact, {}};
            obstacle->effects.capture_checkpoint(target.effects);
        }
    }
    out.fire_crates = fire_crates_;
    out.fire_crate_count = room_obstacles_ == nullptr
        && encounter_config_.fire_room_obstacles
        ? static_cast<std::uint8_t>(fire_crates_.size()) : 0U;
    player_damage_history_.capture_checkpoint(out.player_damage_history);
    out.has_death_snapshot = death_snapshot_.has_value();
    out.death_snapshot = death_snapshot_.value_or(CombatDeathSnapshot{});
    return valid_player(out.player)
        && valid_attack(out.attack, room_monster_field_ != nullptr
            ? room_monster_field_->total_count()
            : static_cast<std::uint32_t>(limits::kRoomMonsterCapacity));
}

bool CombatWorld::restore_room_checkpoint(
    const RoomCombatCheckpoint& checkpoint) noexcept {
    core::DeterministicRng checked_rng{1U};
    PlayerDamageHistory checked_history{};
    const std::uint32_t generated = room_monster_field_ != nullptr
        ? room_monster_field_->total_count()
        : static_cast<std::uint32_t>(limits::kRoomMonsterCapacity);
    if (!valid_room_combat_checkpoint_structural(checkpoint, generated)
            || !checked_rng.import_state(checkpoint.evasion_rng_state)
            || !checked_history.restore_checkpoint(
                checkpoint.player_damage_history)) {
        return false;
    }
    if (checkpoint.player.max_hp != player_.max_hp
            || checkpoint.player.max_barrier != player_.max_barrier
            || checkpoint.player.damage_reduction
                != player_.damage_reduction
            || checkpoint.player.damage_reduction_cap
                != player_.damage_reduction_cap
            || checkpoint.player.armor != player_.armor
            || checkpoint.player.evasion != player_.evasion
            || checkpoint.player.armor_reduction_bp
                != player_.armor_reduction_bp
            || checkpoint.player.evasion_rate_bp
                != player_.evasion_rate_bp) return false;
    if (checkpoint.player.skill_cooldowns[
                static_cast<std::size_t>(skills::ActiveSkillId::draw_slash)]
            > skills::kDrawSlashCooldownTicks
            || checkpoint.player.skill_cooldowns[
                static_cast<std::size_t>(skills::ActiveSkillId::storm_swords)]
            > skills::kStormSwordsCooldownTicks) return false;
    if (checkpoint.attack.id == AttackId::none
            && checkpoint.player.state >= PlayerState::attack_startup
            && checkpoint.player.state <= PlayerState::attack_recovery) {
        return false;
    }
    if (checkpoint.attack.id == AttackId::none
            && checkpoint.player.combo_stage != 0U) return false;
    if (checkpoint.attack.id == AttackId::none
            && (checkpoint.player.state == PlayerState::jump_rise
                || checkpoint.player.state == PlayerState::jump_fall)
            && checkpoint.player.position.z == 0.0F
            && checkpoint.player.velocity.z == 0.0F) return false;
    if (checkpoint.attack.id != AttackId::none) {
        const AttackDefinition* definition =
            find_attack_definition(checkpoint.attack.id);
        const auto& build = encounter_config_.player_build;
        const std::int64_t attack_speed = build.values.attack_speed
            * (modifiers::kFixedOne
                + static_cast<std::int64_t>(build.local_attack_speed_bp))
            / modifiers::kFixedOne;
        const std::uint8_t expected_combo = checkpoint.attack.id == AttackId::j1
            ? 1U : checkpoint.attack.id == AttackId::j2 ? 2U
                : checkpoint.attack.id == AttackId::j3 ? 3U : 0U;
        const AttackPhase phase = attack_phase_at(*definition,
            checkpoint.attack.elapsed_ticks,
            checkpoint.attack.startup_ticks,
            checkpoint.attack.recovery_ticks);
        const PlayerState expected_state = phase == AttackPhase::startup
            ? PlayerState::attack_startup : phase == AttackPhase::active
                ? PlayerState::attack_active : PlayerState::attack_recovery;
        if (checkpoint.attack.startup_ticks != scaled_phase_ticks(
                    definition->startup_ticks, attack_speed)
                || checkpoint.attack.recovery_ticks != scaled_phase_ticks(
                    definition->recovery_ticks, attack_speed)
                || checkpoint.player.combo_stage != expected_combo
                || (checkpoint.player.state != expected_state
                    && !(checkpoint.attack.id == AttackId::air_j
                        && checkpoint.player.state == PlayerState::landing
                        && checkpoint.player.position.z == 0.0F
                        && checkpoint.player.velocity.z == 0.0F))) return false;
    }
    if (room_monster_field_ == nullptr) {
        for (std::uint32_t ordinal = 0U;
                ordinal < limits::kRoomMonsterCapacity; ++ordinal) {
            if (checkpoint.attack.hit_targets.contains(
                    static_cast<MonsterOrdinal>(ordinal))
                    && active_monster_by_ordinal(
                        static_cast<MonsterOrdinal>(ordinal)) == nullptr) {
                return false;
            }
        }
    }
    if (checkpoint.abyss_environment.rule > abyss::AbyssRuleId::life_sacrifice
            && checkpoint.abyss_environment.rule
                != abyss::AbyssRuleId::none) return false;
    const auto& abyss_config = encounter_config_.abyss;
    const auto& environment_config = abyss_config.environment;
    if (checkpoint.abyss_environment.rule != abyss_config.rule
            || checkpoint.abyss_environment.active
                != environment_config.active
            || !finite(checkpoint.abyss_environment.locked_center)
            || (environment_config.radius_count == 0U
                && checkpoint.abyss_environment.expansion_stage != 0xFFU)
            || (environment_config.radius_count != 0U
                && checkpoint.abyss_environment.expansion_stage != 0xFFU
                && checkpoint.abyss_environment.expansion_stage
                    >= environment_config.radius_count)
            || (environment_config.expansion_interval_ticks != 0U
                && checkpoint.abyss_environment.stage_tick
                    >= environment_config.expansion_interval_ticks)
            || (environment_config.cycle_ticks != 0U
                && checkpoint.abyss_environment.cycle_tick
                    >= environment_config.cycle_ticks)) return false;
    const bool zero_center = checkpoint.abyss_environment.locked_center.x == 0.0F
        && checkpoint.abyss_environment.locked_center.y == 0.0F
        && checkpoint.abyss_environment.locked_center.z == 0.0F;
    if (!environment_config.active) {
        if (checkpoint.abyss_environment.cycle_tick != 0U
                || checkpoint.abyss_environment.stage_tick != 0U
                || !zero_center || checkpoint.abyss_environment.warning
                || checkpoint.abyss_environment.expansion_stage != 0xFFU) {
            return false;
        }
    } else if (environment_config.expansion_interval_ticks != 0U) {
        const std::uint64_t alive_tick = checkpoint.tick == 0U
            ? 0U : checkpoint.tick - 1U;
        const std::uint64_t death_tick = checkpoint.has_death_snapshot
            ? checkpoint.death_snapshot.tick : alive_tick;
        const std::uint64_t before_death_tick = death_tick == 0U
            ? 0U : death_tick - 1U;
        const auto matches_expansion_tick = [&](std::uint64_t tick) noexcept {
            const std::uint8_t expected_stage = static_cast<std::uint8_t>(
                (std::min)(tick / environment_config.expansion_interval_ticks,
                    static_cast<std::uint64_t>(
                        environment_config.radius_count - 1U)));
            return checkpoint.abyss_environment.stage_tick
                    == tick % environment_config.expansion_interval_ticks
                && checkpoint.abyss_environment.expansion_stage
                    == expected_stage;
        };
        const bool valid_simulated_tick = checkpoint.has_death_snapshot
            ? matches_expansion_tick(death_tick)
                || matches_expansion_tick(before_death_tick)
            : matches_expansion_tick(alive_tick);
        if (checkpoint.abyss_environment.cycle_tick != 0U
                || !zero_center || checkpoint.abyss_environment.warning
                || checkpoint.abyss_environment.expansion_stage == 0xFFU
                || !valid_simulated_tick) {
            return false;
        }
    } else if (environment_config.cycle_ticks != 0U) {
        const std::uint64_t alive_tick = checkpoint.tick == 0U
            ? 0U : checkpoint.tick - 1U;
        const std::uint64_t death_tick = checkpoint.has_death_snapshot
            ? checkpoint.death_snapshot.tick : alive_tick;
        const std::uint64_t before_death_tick = death_tick == 0U
            ? 0U : death_tick - 1U;
        const auto matches_cycle_tick = [&](std::uint64_t tick) noexcept {
            return checkpoint.abyss_environment.cycle_tick
                == tick % environment_config.cycle_ticks;
        };
        const bool valid_simulated_tick = checkpoint.has_death_snapshot
            ? matches_cycle_tick(death_tick)
                || matches_cycle_tick(before_death_tick)
            : matches_cycle_tick(alive_tick);
        if (checkpoint.abyss_environment.expansion_stage != 0xFFU
                || checkpoint.abyss_environment.stage_tick != 0U
                || !valid_position(
                    checkpoint.abyss_environment.locked_center)
                || !valid_simulated_tick) {
            return false;
        }
    }

    RoomResidentOrdinals desired{};
    if (room_monster_field_ != nullptr) {
        desired = room_monster_field_->required_residents(
            make_room_streaming_region(checkpoint.player.position));
        if (desired.fault != RoomMonsterFieldFault::none
                || desired.count > kMonsterCapacity) return false;
    }

    MonsterOrdinal previous = kInvalidMonsterOrdinal;
    for (std::uint16_t index = 0U; index < checkpoint.monster_count; ++index) {
        const MonsterCombatCheckpoint& monster = checkpoint.monsters[index];
        if (!valid_monster(monster)
                || (index != 0U && monster.ordinal <= previous)) return false;
        previous = monster.ordinal;
        if (room_monster_field_ != nullptr) {
            const MonsterPersistentState* current =
                room_monster_field_->persistent_state(monster.ordinal);
            const RoomMonsterBlueprint& blueprint =
                room_monster_field_->plan().monsters[monster.ordinal];
            if (current == nullptr || current->defeated
                    || monster.id != blueprint.id
                    || !(monster.affixes == blueprint.affixes)
                    || monster.kind != current->kind
                    || !same_profile(monster.affix_profile,
                        current->affix_profile)
                    || monster.max_hp != current->max_hp
                    || monster.max_break != current->max_break
                    || monster.max_shield != current->max_shield
                    || monster.max_shield_ticks
                        != current->max_shield_ticks
                    || monster.spawn.x != blueprint.initial_position.x
                    || monster.spawn.y != blueprint.initial_position.y
                    || monster.spawn.z != blueprint.initial_position.z) {
                return false;
            }
        } else {
            const MonsterRuntime* current =
                active_monster_by_ordinal(monster.ordinal);
            if (current == nullptr || monster.id != current->id
                    || !(monster.affixes == current->affixes)
                    || monster.kind != current->kind
                    || !same_profile(monster.affix_profile,
                        current->affix_profile)
                    || monster.max_hp != current->max_hp
                    || monster.max_break != current->max_break
                    || monster.max_shield != current->max_shield
                    || monster.max_shield_ticks
                        != current->max_shield_ticks
                    || monster.spawn.x != current->spawn.x
                    || monster.spawn.y != current->spawn.y
                    || monster.spawn.z != current->spawn.z) return false;
        }
    }

    std::uint16_t previous_obstacle = 0U;
    for (std::uint16_t index = 0U; index < checkpoint.obstacle_count; ++index) {
        const RoomObstacleCheckpoint& value = checkpoint.obstacles[index];
        const RoomObstacleState* original = room_obstacles_ == nullptr
            ? nullptr : room_obstacles_->state(value.ordinal);
        modifiers::EffectSet effects{};
        if (original == nullptr || (index != 0U
                && value.ordinal <= previous_obstacle)
                || value.max_hp != original->max_hp
                || (original->kind != RoomObstacleKind::breakable
                    && (value.hp != 0U || value.max_hp != 0U
                        || value.broken_tick != 0U || !value.intact))
                || (value.intact
                    ? (original->kind == RoomObstacleKind::breakable
                        && (value.hp == 0U || value.hp > value.max_hp
                            || value.broken_tick != 0U))
                    : (original->kind != RoomObstacleKind::breakable
                        || value.hp != 0U || value.broken_tick > checkpoint.tick))
                || !effects.restore_checkpoint(value.effects)) {
            return false;
        }
        previous_obstacle = value.ordinal;
    }
    if (room_obstacles_ == nullptr && checkpoint.obstacle_count != 0U) {
        return false;
    }
    if (room_obstacles_ != nullptr
            && checkpoint.obstacle_count
                != room_obstacles_->obstacle_count()) return false;

    const std::uint8_t expected_crate_count = room_obstacles_ == nullptr
            && encounter_config_.fire_room_obstacles
        ? static_cast<std::uint8_t>(kFireRoomCrateCapacity) : 0U;
    if (checkpoint.fire_crate_count != expected_crate_count) return false;
    const std::array<Vec3, kFireRoomCrateCapacity> expected_positions{{
        {-3.20F, 0.0F, 0.0F}, {3.20F, 0.0F, 0.0F}}};
    for (std::size_t index = 0U; index < checkpoint.fire_crates.size();
            ++index) {
        const FireRoomCrateSnapshot& crate = checkpoint.fire_crates[index];
        if (index >= checkpoint.fire_crate_count) {
            if (!same_crate(crate, FireRoomCrateSnapshot{})) return false;
            continue;
        }
        if (!finite(crate.position)
                || crate.position.x != expected_positions[index].x
                || crate.position.y != expected_positions[index].y
                || crate.position.z != expected_positions[index].z
                || (crate.intact && crate.broken_tick != 0U)
                || (!crate.intact && crate.broken_tick > checkpoint.tick)) {
            return false;
        }
    }

    for (std::uint16_t obstacle_index = 0U;
            obstacle_index < checkpoint.obstacle_count; ++obstacle_index) {
        const RoomObstacleCheckpoint& obstacle =
            checkpoint.obstacles[obstacle_index];
        if (!obstacle.intact) continue;
        const RoomObstacleState* original =
            room_obstacles_->state(obstacle.ordinal);
        if (original == nullptr) return false;
        if (inside(original->bounds, checkpoint.player.position)) return false;
        for (std::uint16_t monster_index = 0U;
                monster_index < checkpoint.monster_count; ++monster_index) {
            if (inside(original->bounds,
                    checkpoint.monsters[monster_index].position)) return false;
        }
    }
    if (checkpoint.has_death_snapshot) {
        const CombatDeathSnapshot& death = checkpoint.death_snapshot;
        std::uint64_t recent_total{};
        for (const std::uint64_t value : death.recent_damage) {
            const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
            recent_total = recent_total > maximum - value
                ? maximum : recent_total + value;
        }
        if (death.tick > checkpoint.tick || death.final_damage == 0U
                || death.health_loss == 0U || death.defense.hp != 0
                || death.raw_damage < death.final_damage
                || death.barrier_loss > static_cast<std::uint64_t>(
                    checkpoint.player.max_barrier
                        - checkpoint.player.barrier)
                || death.health_loss > static_cast<std::uint64_t>(
                    checkpoint.player.max_hp)
                || death.barrier_loss
                    > (std::numeric_limits<std::uint64_t>::max)()
                        - death.health_loss
                || death.final_damage
                    != death.barrier_loss + death.health_loss
                || death.source.kind > PlayerDamageSourceKind::unknown
                || death.primary_type > modifiers::DamageType::chaos
                || recent_total < death.final_damage
                || death.recent_damage[modifiers::damage_index(
                    death.primary_type)] == 0U
                || checkpoint.player.hp != 0
                || death.defense.max_hp != checkpoint.player.max_hp
                || death.defense.barrier != checkpoint.player.barrier
                || death.defense.max_barrier != checkpoint.player.max_barrier
                || death.defense.armor != checkpoint.player.armor
                || death.defense.evasion != checkpoint.player.evasion
                || death.defense.armor_reduction_bp
                    != checkpoint.player.armor_reduction_bp
                || death.defense.evasion_rate_bp
                    != checkpoint.player.evasion_rate_bp
                || death.defense.damage_reduction
                    != checkpoint.player.damage_reduction
                || death.defense.damage_reduction_cap
                    != checkpoint.player.damage_reduction_cap
                || death.recent_damage != checked_history.totals()
                || !checkpoint.player_damage_history.initialized
                || checkpoint.player_damage_history.active_tick != death.tick
                || !same_source(death.source,
                    canonical_source(death.source))) {
            return false;
        }
    } else if (checkpoint.player.hp == 0) return false;

    tick_ = checkpoint.tick;
    static_cast<void>(evasion_rng_.import_state(checkpoint.evasion_rng_state));
    player_.position = checkpoint.player.position;
    player_.velocity = checkpoint.player.velocity;
    player_.facing = checkpoint.player.facing;
    player_.state = checkpoint.player.state;
    player_.combo_stage = checkpoint.player.combo_stage;
    player_.hit_stop_ticks = 0U;
    player_.air_attack_available = checkpoint.player.air_attack_available;
    player_.hp = checkpoint.player.hp;
    player_.max_hp = checkpoint.player.max_hp;
    player_.barrier = checkpoint.player.barrier;
    player_.max_barrier = checkpoint.player.max_barrier;
    player_.damage_reduction = checkpoint.player.damage_reduction;
    player_.damage_reduction_cap = checkpoint.player.damage_reduction_cap;
    player_.armor = checkpoint.player.armor;
    player_.evasion = checkpoint.player.evasion;
    player_.armor_reduction_bp = checkpoint.player.armor_reduction_bp;
    player_.evasion_rate_bp = checkpoint.player.evasion_rate_bp;
    player_.hurt_ticks = checkpoint.player.hurt_ticks;
    player_.invulnerability_ticks = checkpoint.player.invulnerability_ticks;
    player_.status = checkpoint.player.status;
    attack_ = {checkpoint.attack.id, checkpoint.attack.elapsed_ticks,
        checkpoint.attack.startup_ticks, checkpoint.attack.recovery_ticks,
        checkpoint.attack.connected, checkpoint.attack.impact_event_emitted,
        checkpoint.attack.hit_targets};
    active_skill_ = {};
    active_skill_.cooldowns = checkpoint.player.skill_cooldowns;
    abyss_environment_ = checkpoint.abyss_environment;
    player_damage_history_ = checked_history;

    if (room_monster_field_ != nullptr) {
        room_monster_field_->discard_active_residency();
        for (std::uint16_t index = 0U;
                index < checkpoint.monster_count; ++index) {
            MonsterPersistentState* state =
                room_monster_field_->persistent_state(
                    checkpoint.monsters[index].ordinal);
            assert(state != nullptr);
            to_persistent(checkpoint.monsters[index], *state);
        }
        room_monster_field_->rebind_active_pool(monsters_);
        const bool synchronized = room_monster_field_->synchronize_active_region(
            make_room_streaming_region(player_.position));
        assert(synchronized);
        static_cast<void>(synchronized);
    } else {
        for (std::uint16_t index = 0U;
                index < checkpoint.monster_count; ++index) {
            MonsterRuntime* runtime = active_monster_by_ordinal(
                checkpoint.monsters[index].ordinal);
            assert(runtime != nullptr);
            MonsterPersistentState state{};
            to_persistent(checkpoint.monsters[index], state);
            restore_monster_persistent_state(state, *runtime);
        }
    }
    for (std::uint16_t index = 0U; index < checkpoint.obstacle_count; ++index) {
        const RoomObstacleCheckpoint& source = checkpoint.obstacles[index];
        RoomObstacleState* obstacle = room_obstacles_->state(source.ordinal);
        assert(obstacle != nullptr);
        obstacle->hp = source.hp;
        obstacle->broken_tick = source.broken_tick;
        obstacle->intact = source.intact;
        static_cast<void>(obstacle->effects.restore_checkpoint(source.effects));
    }
    fire_crates_ = checkpoint.fire_crates;
    projectiles_.clear();
    hazards_.clear();
    input_buffer_.clear();
    input_buffer_.reset_diagnostics();
    defeat_ledger_.clear();
    while (events_.try_pop().has_value()) {
    }
    event_overflow_count_ = 0U;
    projectile_saturation_count_ = 0U;
    projectile_invalid_owner_count_ = 0U;
    hazard_saturation_count_ = 0U;
    hazard_invalid_owner_count_ = 0U;
    death_snapshot_ = checkpoint.has_death_snapshot
        ? std::optional<CombatDeathSnapshot>{checkpoint.death_snapshot}
        : std::nullopt;
    fault_ = CombatFault::none;
    return true;
}

}  // namespace arpg::combat
