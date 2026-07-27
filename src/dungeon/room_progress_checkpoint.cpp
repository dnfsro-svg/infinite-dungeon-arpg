#include "dungeon/room_progress_checkpoint.hpp"
#include "dungeon/dungeon_types.hpp"
#include "dungeon/material_loot.hpp"

#include "items/item_catalog.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace arpg::dungeon::checkpoint {
namespace {

[[nodiscard]] bool finite(combat::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z);
}

[[nodiscard]] std::uint32_t popcount(std::uint64_t value) noexcept {
    std::uint32_t result{};
    while (value != 0U) {
        value &= value - 1U;
        ++result;
    }
    return result;
}

template <std::size_t Count>
[[nodiscard]] std::uint32_t popcount(
    const std::array<std::uint64_t, Count>& values) noexcept {
    std::uint32_t result{};
    for (const std::uint64_t value : values) result += popcount(value);
    return result;
}

template <std::size_t Count>
[[nodiscard]] bool no_bits_at_or_above(
    const std::array<std::uint64_t, Count>& values,
    std::uint32_t limit) noexcept {
    if (limit >= Count * 64U) return true;
    const std::size_t word = limit / 64U;
    const std::size_t bit = limit % 64U;
    if (bit != 0U && (values[word] >> bit) != 0U) return false;
    const std::size_t begin = bit == 0U ? word : word + 1U;
    for (std::size_t index = begin; index < Count; ++index) {
        if (values[index] != 0U) return false;
    }
    return true;
}

[[nodiscard]] bool bit_is_set(
    const std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>& bits,
    std::uint16_t ordinal) noexcept {
    return ordinal < limits::kRoomMonsterCapacity
        && (bits[ordinal / 64U]
            & (std::uint64_t{1U} << (ordinal % 64U))) != 0U;
}

[[nodiscard]] bool canonical_combat_none(
    const combat::RoomCombatCheckpoint& combat) noexcept {
    const combat::PlayerCombatCheckpoint& player = combat.player;
    const combat::PlayerCombatCheckpoint canonical_player{};
    const combat::AbyssEnvironmentRuntime& abyss = combat.abyss_environment;
    const combat::AbyssEnvironmentRuntime canonical_abyss{};
    const combat::AttackCheckpoint& attack = combat.attack;
    const combat::AttackCheckpoint canonical_attack{};
    if (combat.tick != 0U
            || combat.evasion_rng_state != core::DeterministicRng::State{}
            || player.position.x != canonical_player.position.x
            || player.position.y != canonical_player.position.y
            || player.position.z != canonical_player.position.z
            || player.velocity.x != canonical_player.velocity.x
            || player.velocity.y != canonical_player.velocity.y
            || player.velocity.z != canonical_player.velocity.z
            || player.facing != canonical_player.facing
            || player.state != canonical_player.state
            || player.combo_stage != canonical_player.combo_stage
            || player.air_attack_available
                != canonical_player.air_attack_available
            || player.hp != 0 || player.max_hp != 0
            || player.barrier != 0 || player.max_barrier != 0
            || player.damage_reduction
                != canonical_player.damage_reduction
            || player.damage_reduction_cap
                != canonical_player.damage_reduction_cap
            || player.armor != 0 || player.evasion != 0
            || player.armor_reduction_bp != 0
            || player.evasion_rate_bp != 0
            || player.hurt_ticks != 0U
            || player.invulnerability_ticks != 0U
            || player.status.slow_bp != 0
            || player.status.slow_ticks != 0U
            || player.status.corrosion_damage_per_second != 0
            || player.status.corrosion_ticks != 0U
            || player.status.corrosion_tick_phase != 0U
            || player.status.corrosion_source.kind
                != canonical_player.status.corrosion_source.kind
            || player.status.corrosion_source.monster
                != canonical_player.status.corrosion_source.monster
            || player.status.corrosion_source.detail_id != 0U
            || player.skill_cooldowns
                != canonical_player.skill_cooldowns
            || abyss.rule != canonical_abyss.rule
            || abyss.cycle_tick != 0U || abyss.stage_tick != 0U
            || abyss.locked_center.x != 0.0F
            || abyss.locked_center.y != 0.0F
            || abyss.locked_center.z != 0.0F
            || abyss.expansion_stage != canonical_abyss.expansion_stage
            || abyss.warning || abyss.active
            || attack.id != canonical_attack.id
            || attack.elapsed_ticks != 0U
            || attack.startup_ticks != 0U
            || attack.recovery_ticks != 0U
            || attack.connected || attack.impact_event_emitted
            || attack.hit_targets.words
                != canonical_attack.hit_targets.words
            || combat.monster_count != 0U
            || combat.obstacle_count != 0U
            || combat.fire_crate_count != 0U
            || combat.player_damage_history.initialized
            || combat.player_damage_history.active_tick != 0U
            || combat.has_death_snapshot) {
        return false;
    }
    for (const auto& bucket : combat.player_damage_history.buckets) {
        for (const std::uint64_t amount : bucket) {
            if (amount != 0U) return false;
        }
    }
    return true;
}

[[nodiscard]] bool canonical_inactive_abyss_environment(
    const combat::AbyssEnvironmentRuntime& value) noexcept {
    return value.rule == abyss::AbyssRuleId::none
        && value.cycle_tick == 0U && value.stage_tick == 0U
        && value.locked_center.x == 0.0F
        && value.locked_center.y == 0.0F
        && value.locked_center.z == 0.0F
        && value.expansion_stage == 0xFFU
        && !value.warning && !value.active;
}

[[nodiscard]] bool same_death_owner(
    const combat::CombatDeathSnapshot& combat_death,
    const DeathCheckpoint& durable_death) noexcept {
    const bool source_has_monster = combat_death.source.kind
            != combat::PlayerDamageSourceKind::abyss_environment
        && combat_death.source.kind
            != combat::PlayerDamageSourceKind::unknown;
    const std::uint8_t source_monster = source_has_monster
        ? static_cast<std::uint8_t>(combat_death.source.monster) : 0xFFU;
    return durable_death.source_kind == static_cast<DeathSourceKind>(
            combat_death.source.kind)
        && durable_death.source_monster_id == source_monster
        && durable_death.source_detail_id == combat_death.source.detail_id
        && durable_death.damage_type == static_cast<DeathDamageType>(
            combat_death.primary_type)
        && durable_death.raw_damage == combat_death.raw_damage
        && durable_death.barrier_loss == combat_death.barrier_loss
        && durable_death.health_loss == combat_death.health_loss
        && durable_death.final_damage == combat_death.final_damage
        && durable_death.recent_damage == combat_death.recent_damage
        && durable_death.hp == combat_death.defense.hp
        && durable_death.max_hp == combat_death.defense.max_hp
        && durable_death.barrier == combat_death.defense.barrier
        && durable_death.max_barrier == combat_death.defense.max_barrier
        && durable_death.armor == combat_death.defense.armor
        && durable_death.evasion == combat_death.defense.evasion
        && durable_death.armor_reduction_bp
            == combat_death.defense.armor_reduction_bp
        && durable_death.evasion_rate_bp
            == combat_death.defense.evasion_rate_bp
        && durable_death.damage_reduction
            == combat_death.defense.damage_reduction
        && durable_death.damage_reduction_cap
            == combat_death.defense.damage_reduction_cap;
}

[[nodiscard]] bool valid_owner_cross_invariants(
    const RoomProgressCheckpoint& room,
    const DungeonRunState& state) noexcept {
    if (room.full_clear
            && (!room.exits_unlocked || !room.reward_committed)) {
        return false;
    }
    const bool durable_death = state.death.lifecycle
        == DeathLifecycle::pending_continue;
    if (durable_death
            || room.lifecycle == RoomProgressLifecycle::death_pending) {
        if (!durable_death
                || room.lifecycle != RoomProgressLifecycle::death_pending
                || !room.combat.has_death_snapshot
                || room.full_clear || room.reward_committed
                || state.current_room.is_abyss
                || state.death.death_depth != state.current_room.depth
                || state.death.death_floor_room_index
                    != state.current_room.floor_room_index
                || state.death.death_ecology != state.current_room.ecology
                || !same_death_owner(
                    room.combat.death_snapshot, state.death)) {
            return false;
        }
        if (state.death.death_was_abyss) {
            return state.abyss.lifecycle == abyss::AbyssLifecycle::failed
                && room.combat.abyss_environment.rule == state.abyss.rule;
        }
        return canonical_inactive_abyss_environment(
            room.combat.abyss_environment);
    }
    const auto lifecycle = state.abyss.lifecycle;
    if (lifecycle == abyss::AbyssLifecycle::started) {
        return state.current_room.is_abyss && !room.full_clear
            && !room.reward_committed
            && room.combat.abyss_environment.rule == state.abyss.rule;
    }
    if (lifecycle == abyss::AbyssLifecycle::cleared) {
        return state.current_room.is_abyss && room.full_clear
            && room.exits_unlocked && room.reward_committed
            && canonical_inactive_abyss_environment(
                room.combat.abyss_environment);
    }
    if (lifecycle == abyss::AbyssLifecycle::available) return false;
    return !state.current_room.is_abyss
        && canonical_inactive_abyss_environment(
            room.combat.abyss_environment);
}

[[nodiscard]] bool valid_abyss_reward_ground_ownership(
    const RoomProgressCheckpoint& room,
    const DungeonRunState& state) noexcept {
    const bool cleared = state.abyss.lifecycle
        == abyss::AbyssLifecycle::cleared;
    std::uint8_t visible_rewards{};
    for (std::uint16_t index = 0U;
            index < room.equipment_ground_count; ++index) {
        const EquipmentGroundCheckpoint& ground = room.equipment_ground[index];
        if (ground.source
                > static_cast<std::uint8_t>(GroundItemSource::abyss_chest)) {
            return false;
        }
        if (ground.source
                == static_cast<std::uint8_t>(GroundItemSource::monster_drop)) {
            if (ground.reward_ordinal != 0xFFU) return false;
            continue;
        }
        if (!cleared || ground.reward_ordinal >= state.abyss.reward_total
                || ground.reward_ordinal >= 3U) {
            return false;
        }
        const auto bit = static_cast<std::uint8_t>(
            1U << ground.reward_ordinal);
        if ((state.abyss.generated_mask & bit) == 0U
                || (state.abyss.claimed_mask & bit) != 0U
                || (visible_rewards & bit) != 0U) {
            return false;
        }
        visible_rewards |= bit;
    }
    if (!cleared) {
        return visible_rewards == 0U
            && state.abyss.generated_mask == 0U
            && state.abyss.claimed_mask == 0U
            && state.abyss.abandoned_mask == 0U;
    }
    if (state.abyss.reward_total == 0U || state.abyss.reward_total > 3U) {
        return false;
    }
    const auto valid = static_cast<std::uint8_t>(
        (1U << state.abyss.reward_total) - 1U);
    if (((state.abyss.generated_mask | state.abyss.claimed_mask
                | state.abyss.abandoned_mask)
            & static_cast<std::uint8_t>(~valid)) != 0U
            || (state.abyss.claimed_mask
                & static_cast<std::uint8_t>(
                    ~state.abyss.generated_mask)) != 0U
            || (state.abyss.abandoned_mask
                & state.abyss.generated_mask) != 0U) {
        return false;
    }
    const auto expected_visible = static_cast<std::uint8_t>(
        state.abyss.generated_mask
        & static_cast<std::uint8_t>(~state.abyss.claimed_mask)
        & valid);
    return visible_rewards == expected_visible;
}

[[nodiscard]] bool canonical_none(
    const RoomProgressCheckpoint& room) noexcept {
    return room.room_index == 0U && room.room_seed == 0U
        && room.monster_generator_version == 0U
        && room.monster_blueprint_hash == 0U
        && room.environment_generator_version == 0U
        && room.environment_blueprint_hash == 0U
        && room.generated_monsters == 0U && room.defeated_monsters == 0U
        && room.required_kills == 0U && !room.exits_unlocked
        && !room.full_clear && !room.reward_committed
        && popcount(room.defeat_bits) == 0U
        && popcount(room.equipment_claim_bits) == 0U
        && popcount(room.secondary_claim_bits) == 0U
        && room.equipment_ground_count == 0U
        && room.secondary_ground_count == 0U
        && canonical_combat_none(room.combat);
}

[[nodiscard]] bool same_vec(
    combat::Vec3 left, combat::Vec3 right) noexcept {
    const auto same_float = [](const float a, const float b) noexcept {
        std::uint32_t left_bits{};
        std::uint32_t right_bits{};
        std::memcpy(&left_bits, &a, sizeof(left_bits));
        std::memcpy(&right_bits, &b, sizeof(right_bits));
        return left_bits == right_bits;
    };
    return same_float(left.x, right.x) && same_float(left.y, right.y)
        && same_float(left.z, right.z);
}

[[nodiscard]] bool same_item(
    const items::ItemInstance& left,
    const items::ItemInstance& right) noexcept {
    if (left.id != right.id || left.base_id != right.base_id
            || left.rarity != right.rarity
            || left.item_level != right.item_level
            || left.required_level != right.required_level
            || left.affix_count != right.affix_count
            || left.reserved != right.reserved
            || left.reinforcement != right.reinforcement
            || left.extension_reserved != right.extension_reserved) {
        return false;
    }
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        const auto& a = left.affixes[index];
        const auto& b = right.affixes[index];
        if (a.affix_id != b.affix_id || a.tier != b.tier
                || a.variant != b.variant
                || a.value_roll_bp != b.value_roll_bp) return false;
    }
    return true;
}

[[nodiscard]] bool same_source(const combat::PlayerDamageSource& left,
    const combat::PlayerDamageSource& right) noexcept {
    return left.kind == right.kind && left.monster == right.monster
        && left.detail_id == right.detail_id;
}

[[nodiscard]] bool same_modifier(const modifiers::Modifier& left,
    const modifiers::Modifier& right) noexcept {
    return left.id == right.id && left.stat == right.stat
        && left.operation == right.operation && left.value == right.value
        && left.required_tags == right.required_tags
        && left.forbidden_tags == right.forbidden_tags
        && left.required_conditions == right.required_conditions
        && left.priority == right.priority
        && left.conversion_target == right.conversion_target;
}

[[nodiscard]] bool same_effects(
    const modifiers::EffectSetCheckpoint& left,
    const modifiers::EffectSetCheckpoint& right) noexcept {
    if (left.command_head != right.command_head
            || left.command_count != right.command_count
            || left.diagnostics.effect_overflows
                != right.diagnostics.effect_overflows
            || left.diagnostics.command_overflows
                != right.diagnostics.command_overflows) {
        return false;
    }
    for (std::size_t index = 0U; index < left.effects.size(); ++index) {
        const auto& a = left.effects[index];
        const auto& b = right.effects[index];
        if (a.id != b.id || a.remaining_ticks != b.remaining_ticks
                || a.stacks != b.stacks || a.max_stacks != b.max_stacks
                || a.refresh_rule != b.refresh_rule
                || a.strength != b.strength
                || !same_modifier(a.modifier, b.modifier)
                || a.has_modifier != b.has_modifier
                || a.on_expire.kind != b.on_expire.kind
                || a.on_expire.value != b.on_expire.value
                || a.occupied != b.occupied) {
            return false;
        }
    }
    for (std::size_t index = 0U; index < left.commands.size(); ++index) {
        const auto& a = left.commands[index];
        const auto& b = right.commands[index];
        if (a.kind != b.kind || a.value != b.value
                || a.effect_id != b.effect_id) return false;
    }
    return true;
}

[[nodiscard]] bool same_player(
    const combat::PlayerCombatCheckpoint& left,
    const combat::PlayerCombatCheckpoint& right) noexcept {
    return same_vec(left.position, right.position)
        && same_vec(left.velocity, right.velocity)
        && left.facing == right.facing && left.state == right.state
        && left.combo_stage == right.combo_stage
        && left.air_attack_available == right.air_attack_available
        && left.hp == right.hp && left.max_hp == right.max_hp
        && left.barrier == right.barrier
        && left.max_barrier == right.max_barrier
        && left.damage_reduction == right.damage_reduction
        && left.damage_reduction_cap == right.damage_reduction_cap
        && left.armor == right.armor && left.evasion == right.evasion
        && left.armor_reduction_bp == right.armor_reduction_bp
        && left.evasion_rate_bp == right.evasion_rate_bp
        && left.hurt_ticks == right.hurt_ticks
        && left.invulnerability_ticks == right.invulnerability_ticks
        && left.status.slow_bp == right.status.slow_bp
        && left.status.slow_ticks == right.status.slow_ticks
        && left.status.corrosion_damage_per_second
            == right.status.corrosion_damage_per_second
        && left.status.corrosion_ticks == right.status.corrosion_ticks
        && left.status.corrosion_tick_phase
            == right.status.corrosion_tick_phase
        && same_source(left.status.corrosion_source,
            right.status.corrosion_source)
        && left.skill_cooldowns == right.skill_cooldowns;
}

[[nodiscard]] bool same_abyss(
    const combat::AbyssEnvironmentRuntime& left,
    const combat::AbyssEnvironmentRuntime& right) noexcept {
    return left.rule == right.rule && left.cycle_tick == right.cycle_tick
        && left.stage_tick == right.stage_tick
        && same_vec(left.locked_center, right.locked_center)
        && left.expansion_stage == right.expansion_stage
        && left.warning == right.warning && left.active == right.active;
}

[[nodiscard]] bool same_attack(const combat::AttackCheckpoint& left,
    const combat::AttackCheckpoint& right) noexcept {
    return left.id == right.id && left.elapsed_ticks == right.elapsed_ticks
        && left.startup_ticks == right.startup_ticks
        && left.recovery_ticks == right.recovery_ticks
        && left.connected == right.connected
        && left.impact_event_emitted == right.impact_event_emitted
        && left.hit_targets.words == right.hit_targets.words;
}

[[nodiscard]] bool same_profile(const combat::MonsterAffixProfile& left,
    const combat::MonsterAffixProfile& right) noexcept {
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

[[nodiscard]] bool same_monster(
    const combat::MonsterCombatCheckpoint& left,
    const combat::MonsterCombatCheckpoint& right) noexcept {
    return left.ordinal == right.ordinal && left.id == right.id
        && left.affixes == right.affixes
        && same_profile(left.affix_profile, right.affix_profile)
        && left.kind == right.kind && same_vec(left.spawn, right.spawn)
        && same_vec(left.position, right.position)
        && same_vec(left.velocity, right.velocity)
        && left.facing == right.facing && left.reaction == right.reaction
        && left.armor == right.armor
        && left.reaction_ticks == right.reaction_ticks
        && left.ai_phase == right.ai_phase && left.ai_ticks == right.ai_ticks
        && left.attack_serial == right.attack_serial
        && left.contact_attack_resolved == right.contact_attack_resolved
        && left.hp == right.hp && left.max_hp == right.max_hp
        && left.break_value == right.break_value
        && left.max_break == right.max_break
        && left.shield == right.shield && left.max_shield == right.max_shield
        && left.shield_ticks == right.shield_ticks
        && left.max_shield_ticks == right.max_shield_ticks
        && left.shield_recharge_ticks == right.shield_recharge_ticks
        && left.break_window_ticks == right.break_window_ticks
        && left.owner_transient_counter == right.owner_transient_counter
        && left.burning_ground_ticks == right.burning_ground_ticks
        && left.blink_assault_ticks == right.blink_assault_ticks
        && left.blink_empowered == right.blink_empowered
        && same_vec(left.attack_target_position,
            right.attack_target_position)
        && same_vec(left.attack_vector, right.attack_vector)
        && left.affix_warning == right.affix_warning
        && left.affix_warning_ticks == right.affix_warning_ticks
        && same_effects(left.effects, right.effects)
        && left.effects_touched == right.effects_touched;
}

[[nodiscard]] bool same_obstacle(
    const combat::RoomObstacleCheckpoint& left,
    const combat::RoomObstacleCheckpoint& right) noexcept {
    return left.ordinal == right.ordinal && left.hp == right.hp
        && left.max_hp == right.max_hp
        && left.broken_tick == right.broken_tick
        && left.intact == right.intact
        && same_effects(left.effects, right.effects);
}

[[nodiscard]] bool same_death(const combat::CombatDeathSnapshot& left,
    const combat::CombatDeathSnapshot& right) noexcept {
    return left.tick == right.tick && same_source(left.source, right.source)
        && left.primary_type == right.primary_type
        && left.raw_damage == right.raw_damage
        && left.barrier_loss == right.barrier_loss
        && left.health_loss == right.health_loss
        && left.final_damage == right.final_damage
        && left.recent_damage == right.recent_damage
        && left.defense.hp == right.defense.hp
        && left.defense.max_hp == right.defense.max_hp
        && left.defense.barrier == right.defense.barrier
        && left.defense.max_barrier == right.defense.max_barrier
        && left.defense.armor == right.defense.armor
        && left.defense.evasion == right.defense.evasion
        && left.defense.armor_reduction_bp
            == right.defense.armor_reduction_bp
        && left.defense.evasion_rate_bp
            == right.defense.evasion_rate_bp
        && left.defense.damage_reduction
            == right.defense.damage_reduction
        && left.defense.damage_reduction_cap
            == right.defense.damage_reduction_cap;
}

[[nodiscard]] bool same_combat(
    const combat::RoomCombatCheckpoint& left,
    const combat::RoomCombatCheckpoint& right) noexcept {
    if (left.tick != right.tick
            || left.evasion_rng_state != right.evasion_rng_state
            || !same_player(left.player, right.player)
            || !same_abyss(left.abyss_environment,
                right.abyss_environment)
            || !same_attack(left.attack, right.attack)
            || left.monster_count != right.monster_count
            || left.obstacle_count != right.obstacle_count
            || left.fire_crate_count != right.fire_crate_count
            || left.player_damage_history.initialized
                != right.player_damage_history.initialized
            || left.player_damage_history.active_tick
                != right.player_damage_history.active_tick
            || left.player_damage_history.buckets
                != right.player_damage_history.buckets
            || left.has_death_snapshot != right.has_death_snapshot) {
        return false;
    }
    for (std::uint16_t index = 0U; index < left.monster_count; ++index) {
        if (!same_monster(left.monsters[index], right.monsters[index])) {
            return false;
        }
    }
    for (std::uint16_t index = 0U; index < left.obstacle_count; ++index) {
        if (!same_obstacle(left.obstacles[index], right.obstacles[index])) {
            return false;
        }
    }
    for (std::uint8_t index = 0U; index < left.fire_crate_count; ++index) {
        const auto& a = left.fire_crates[index];
        const auto& b = right.fire_crates[index];
        if (!same_vec(a.position, b.position)
                || a.broken_tick != b.broken_tick
                || a.intact != b.intact) return false;
    }
    return !left.has_death_snapshot
        || same_death(left.death_snapshot, right.death_snapshot);
}

}  // namespace

void clear_room_progress_checkpoint(RoomProgressCheckpoint& out) noexcept {
    out.lifecycle = RoomProgressLifecycle::none;
    out.room_index = 0U;
    out.room_seed = 0U;
    out.monster_generator_version = 0U;
    out.monster_blueprint_hash = 0U;
    out.environment_generator_version = 0U;
    out.environment_blueprint_hash = 0U;
    out.generated_monsters = 0U;
    out.defeated_monsters = 0U;
    out.required_kills = 0U;
    out.exits_unlocked = false;
    out.full_clear = false;
    out.reward_committed = false;
    out.defeat_bits = {};
    out.equipment_claim_bits = {};
    out.secondary_claim_bits = {};
    out.combat.tick = 0U;
    out.combat.evasion_rng_state = {};
    out.combat.player = {};
    out.combat.abyss_environment = {};
    out.combat.attack = {};
    for (auto& monster : out.combat.monsters) monster = {};
    out.combat.monster_count = 0U;
    for (auto& obstacle : out.combat.obstacles) obstacle = {};
    out.combat.obstacle_count = 0U;
    out.combat.fire_crates = {};
    out.combat.fire_crate_count = 0U;
    out.combat.player_damage_history = {};
    out.combat.has_death_snapshot = false;
    out.combat.death_snapshot = {};
    for (auto& ground : out.equipment_ground) ground = {};
    out.equipment_ground_count = 0U;
    for (auto& ground : out.secondary_ground) ground = {};
    out.secondary_ground_count = 0U;
}

void clear_save_checkpoint_slot(SaveCheckpointSlot& out) noexcept {
    auto item_backing = std::move(out.state.item_ownership.items);
    out.state = {};
    out.state.item_ownership.items = std::move(item_backing);
    out.state.item_ownership.items.clear();
    out.persistence_revision = 0U;
    clear_room_progress_checkpoint(out.room_progress);
}

bool valid_room_progress_checkpoint_structural(
    const RoomProgressCheckpoint& room,
    const DungeonRunState& state) noexcept {
    if (room.lifecycle == RoomProgressLifecycle::none) {
        return canonical_none(room);
    }
    if (room.lifecycle != RoomProgressLifecycle::active
            && room.lifecycle != RoomProgressLifecycle::death_pending) {
        return false;
    }
    if (room.room_seed == 0U || room.room_index != state.current_room.index
            || room.room_seed != state.current_room.seed
            || room.monster_generator_version == 0U
            || room.monster_blueprint_hash == 0U
            || room.environment_generator_version == 0U
            || room.environment_blueprint_hash == 0U
            || room.generated_monsters == 0U
            || room.generated_monsters > limits::kRoomMonsterCapacity
            || room.defeated_monsters > room.generated_monsters
            || room.required_kills != (room.generated_monsters + 3U) / 4U
            || popcount(room.defeat_bits) != room.defeated_monsters
            || !no_bits_at_or_above(
                room.defeat_bits, room.generated_monsters)
            || !no_bits_at_or_above(
                room.equipment_claim_bits, room.generated_monsters)
            || (room.exits_unlocked
                && room.defeated_monsters < room.required_kills)
            || (room.full_clear
                && room.defeated_monsters != room.generated_monsters)
            || (room.reward_committed && !room.full_clear)
            || room.equipment_ground_count > room.equipment_ground.size()
            || room.secondary_ground_count > room.secondary_ground.size()
            || (room.lifecycle == RoomProgressLifecycle::death_pending)
                != room.combat.has_death_snapshot
            || !valid_owner_cross_invariants(room, state)) {
        return false;
    }
    const auto rng = room.combat.evasion_rng_state;
    if ((rng[0] | rng[1] | rng[2] | rng[3]) == 0U
            || room.combat.monster_count > room.combat.monsters.size()
            || room.combat.obstacle_count > room.combat.obstacles.size()
            || room.combat.fire_crate_count
                > room.combat.fire_crates.size()
            || !combat::valid_room_combat_checkpoint_structural(
                room.combat, room.generated_monsters)) {
        return false;
    }
    combat::MonsterOrdinal previous_monster = combat::kInvalidMonsterOrdinal;
    for (std::uint16_t index = 0U;
            index < room.combat.monster_count; ++index) {
        const combat::MonsterCombatCheckpoint& monster =
            room.combat.monsters[index];
        if (monster.ordinal >= room.generated_monsters
                || (index != 0U && monster.ordinal <= previous_monster)
                || bit_is_set(room.defeat_bits, monster.ordinal)
                || monster.hp <= 0 || monster.hp > monster.max_hp
                || !finite(monster.position) || !finite(monster.velocity)) {
            return false;
        }
        previous_monster = monster.ordinal;
    }
    std::uint16_t previous_equipment{};
    for (std::uint16_t index = 0U;
            index < room.equipment_ground_count; ++index) {
        const EquipmentGroundCheckpoint& ground = room.equipment_ground[index];
        if (ground.ordinal >= room.generated_monsters
                || (index != 0U && ground.ordinal <= previous_equipment)
                || bit_is_set(room.equipment_claim_bits, ground.ordinal)
                || !finite(ground.position)
                || !items::validate_item(ground.item)) {
            return false;
        }
        previous_equipment = ground.ordinal;
    }
    std::uint16_t previous_secondary{};
    for (std::uint16_t index = 0U;
            index < room.secondary_ground_count; ++index) {
        const SecondaryGroundCheckpoint& ground = room.secondary_ground[index];
        if (ground.tag > SecondaryGroundTag::health_potion
                || ground.ordinal >= kRoomSecondaryGroundCapacity
                || (index != 0U && ground.ordinal <= previous_secondary)
                || (room.secondary_claim_bits[ground.ordinal / 64U]
                    & (std::uint64_t{1U} << (ground.ordinal % 64U))) != 0U
                || !finite(ground.position)
                || (ground.tag == SecondaryGroundTag::material
                    && (ground.material >= items::MaterialId::count
                        || ground.source > static_cast<std::uint8_t>(
                            GroundMaterialSource::abyss_reward)
                        || (ground.ordinal
                                >= kCheckpointAbyssSecondaryOrdinalBegin)
                            != (ground.source == static_cast<std::uint8_t>(
                                GroundMaterialSource::abyss_reward))
                        || (ground.ordinal
                                < kCheckpointAbyssSecondaryOrdinalBegin
                            && ((ground.ordinal & 1U) != 0U
                                || ground.ordinal
                                    >= kCheckpointOrdinarySecondaryOrdinalEnd))))
                || (ground.tag == SecondaryGroundTag::health_potion
                    && (ground.material != items::MaterialId::count
                        || ground.ordinal
                            >= kCheckpointAbyssSecondaryOrdinalBegin
                        || (ground.ordinal & 1U) == 0U))) {
            return false;
        }
        previous_secondary = ground.ordinal;
    }
    return valid_abyss_reward_ground_ownership(room, state);
}

bool same_room_progress_checkpoint(
    const RoomProgressCheckpoint& left,
    const RoomProgressCheckpoint& right) noexcept {
    if (left.lifecycle != right.lifecycle
            || left.room_index != right.room_index
            || left.room_seed != right.room_seed
            || left.monster_generator_version
                != right.monster_generator_version
            || left.monster_blueprint_hash != right.monster_blueprint_hash
            || left.environment_generator_version
                != right.environment_generator_version
            || left.environment_blueprint_hash
                != right.environment_blueprint_hash
            || left.generated_monsters != right.generated_monsters
            || left.defeated_monsters != right.defeated_monsters
            || left.required_kills != right.required_kills
            || left.exits_unlocked != right.exits_unlocked
            || left.full_clear != right.full_clear
            || left.reward_committed != right.reward_committed
            || left.defeat_bits != right.defeat_bits
            || left.equipment_claim_bits != right.equipment_claim_bits
            || left.secondary_claim_bits != right.secondary_claim_bits
            || left.equipment_ground_count != right.equipment_ground_count
            || left.secondary_ground_count != right.secondary_ground_count
            || !same_combat(left.combat, right.combat)) return false;
    for (std::uint16_t index = 0U; index < left.equipment_ground_count;
            ++index) {
        const EquipmentGroundCheckpoint& a = left.equipment_ground[index];
        const EquipmentGroundCheckpoint& b = right.equipment_ground[index];
        if (a.ordinal != b.ordinal || a.source != b.source
                || a.reward_ordinal != b.reward_ordinal
                || !same_vec(a.position, b.position)
                || !same_item(a.item, b.item)) return false;
    }
    for (std::uint16_t index = 0U; index < left.secondary_ground_count;
            ++index) {
        const SecondaryGroundCheckpoint& a = left.secondary_ground[index];
        const SecondaryGroundCheckpoint& b = right.secondary_ground[index];
        if (a.tag != b.tag || a.ordinal != b.ordinal
                || a.source != b.source || a.material != b.material
                || !same_vec(a.position, b.position)) return false;
    }
    return true;
}

}  // namespace arpg::dungeon::checkpoint
