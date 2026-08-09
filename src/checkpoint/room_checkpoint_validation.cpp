#include "checkpoint/room_checkpoint_validation.hpp"

#include "checkpoint/room_checkpoint_schema.hpp"

#include "abyss/abyss_rewards.hpp"
#include "items/item_catalog.hpp"
#include "modifiers/effect_set.hpp"
#include "skills/active_skill_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace arpg::checkpoint {

static_assert(limits::kRoomMonsterCapacity == 1152U);
static_assert(kRoomEnvironmentCellCount == 400U);
static_assert(kFireRoomCrateCapacity == 2U);
static_assert(kPlayerDamageHistoryTicks == 300U);

namespace {

[[nodiscard]] bool finite(const CheckpointVec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z);
}

[[nodiscard]] bool valid_position(const CheckpointVec3 value) noexcept {
    return finite(value) && value.x >= kRoomMinX && value.x <= kRoomMaxX
        && value.y >= kRoomMinY && value.y <= kRoomMaxY
        && value.z >= kRoomMinZ && value.z <= kRoomMaxZ;
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
    const std::uint32_t limit) noexcept {
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

template <std::size_t Count>
[[nodiscard]] bool no_bits_in_range(
    const std::array<std::uint64_t, Count>& values,
    const std::uint32_t begin, const std::uint32_t end) noexcept {
    const std::uint32_t capacity = static_cast<std::uint32_t>(Count * 64U);
    const std::uint32_t clamped_end = (std::min)(end, capacity);
    for (std::uint32_t ordinal = begin;
            ordinal < clamped_end; ++ordinal) {
        if ((values[ordinal / 64U]
                & (std::uint64_t{1U} << (ordinal % 64U))) != 0U) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_secondary_claim_domain(
    const std::array<std::uint64_t, limits::kRoomSecondaryClaimWords>& bits,
    const std::uint32_t generated_monsters) noexcept {
    const std::uint32_t ordinary_end = generated_monsters * 2U;
    constexpr std::uint32_t kReserveEnd =
        kAbyssSecondaryOrdinalBegin + 16U;
    return no_bits_in_range(bits, ordinary_end,
            kAbyssSecondaryOrdinalBegin)
        && no_bits_at_or_above(bits, kReserveEnd);
}

template <std::size_t Count>
[[nodiscard]] std::uint32_t count_bits_in_range(
    const std::array<std::uint64_t, Count>& values,
    const std::uint32_t begin, const std::uint32_t end) noexcept {
    std::uint32_t result{};
    const std::uint32_t capacity = static_cast<std::uint32_t>(Count * 64U);
    const std::uint32_t clamped_end = (std::min)(end, capacity);
    for (std::uint32_t ordinal = begin;
            ordinal < clamped_end; ++ordinal) {
        if ((values[ordinal / 64U]
                & (std::uint64_t{1U} << (ordinal % 64U))) != 0U) {
            ++result;
        }
    }
    return result;
}

[[nodiscard]] bool bit_is_set(
    const std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>& bits,
    const std::uint16_t ordinal) noexcept {
    return ordinal < limits::kRoomMonsterCapacity
        && (bits[ordinal / 64U]
            & (std::uint64_t{1U} << (ordinal % 64U))) != 0U;
}

[[nodiscard]] bool same_vec(
    const CheckpointVec3 left, const CheckpointVec3 right) noexcept {
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

[[nodiscard]] bool same_source(const PlayerDamageSource& left,
    const PlayerDamageSource& right) noexcept {
    return left.kind == right.kind && left.monster == right.monster
        && left.detail_id == right.detail_id;
}

[[nodiscard]] PlayerDamageSource canonical_source(
    PlayerDamageSource source) noexcept {
    const auto valid_monster = [](const MonsterId value) noexcept {
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

[[nodiscard]] bool valid_damage_history(
    const PlayerDamageHistoryCheckpoint& history) noexcept {
    if (!history.initialized) {
        if (history.active_tick != 0U) return false;
        for (const auto& bucket : history.buckets) {
            for (const std::uint64_t value : bucket) {
                if (value != 0U) return false;
            }
        }
        return true;
    }

    std::array<std::uint64_t, modifiers::kDamageTypeCount> totals{};
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    for (const auto& bucket : history.buckets) {
        for (std::size_t index = 0U; index < bucket.size(); ++index) {
            if (totals[index] > maximum - bucket[index]) return false;
            totals[index] += bucket[index];
        }
    }
    return true;
}

[[nodiscard]] std::array<std::uint64_t, modifiers::kDamageTypeCount>
damage_history_totals(
    const PlayerDamageHistoryCheckpoint& history) noexcept {
    std::array<std::uint64_t, modifiers::kDamageTypeCount> totals{};
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    for (const auto& bucket : history.buckets) {
        for (std::size_t index = 0U; index < bucket.size(); ++index) {
            totals[index] = totals[index] > maximum - bucket[index]
                ? maximum : totals[index] + bucket[index];
        }
    }
    return totals;
}

[[nodiscard]] bool same_item(const items::ItemInstance& left,
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
                || a.occupied != b.occupied) return false;
    }
    for (std::size_t index = 0U; index < left.commands.size(); ++index) {
        const auto& a = left.commands[index];
        const auto& b = right.commands[index];
        if (a.kind != b.kind || a.value != b.value
                || a.effect_id != b.effect_id) return false;
    }
    return true;
}

[[nodiscard]] bool same_player(const PlayerCombatCheckpoint& left,
    const PlayerCombatCheckpoint& right) noexcept {
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

[[nodiscard]] bool same_abyss(const AbyssEnvironmentRuntime& left,
    const AbyssEnvironmentRuntime& right) noexcept {
    return left.rule == right.rule && left.cycle_tick == right.cycle_tick
        && left.stage_tick == right.stage_tick
        && same_vec(left.locked_center, right.locked_center)
        && left.expansion_stage == right.expansion_stage
        && left.warning == right.warning && left.active == right.active;
}

[[nodiscard]] bool same_attack(const AttackCheckpoint& left,
    const AttackCheckpoint& right) noexcept {
    return left.id == right.id && left.elapsed_ticks == right.elapsed_ticks
        && left.startup_ticks == right.startup_ticks
        && left.recovery_ticks == right.recovery_ticks
        && left.connected == right.connected
        && left.impact_event_emitted == right.impact_event_emitted
        && left.hit_targets.words == right.hit_targets.words;
}

[[nodiscard]] bool same_monster(const MonsterCombatCheckpoint& left,
    const MonsterCombatCheckpoint& right) noexcept {
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
        && left.engagement_latch == right.engagement_latch
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

[[nodiscard]] bool same_obstacle(const RoomObstacleCheckpoint& left,
    const RoomObstacleCheckpoint& right) noexcept {
    return left.ordinal == right.ordinal && left.hp == right.hp
        && left.max_hp == right.max_hp
        && left.broken_tick == right.broken_tick
        && left.intact == right.intact
        && same_effects(left.effects, right.effects);
}

[[nodiscard]] bool same_death(const CombatDeathSnapshot& left,
    const CombatDeathSnapshot& right) noexcept {
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

[[nodiscard]] bool canonical_combat_none(
    const RoomCombatCheckpoint& combat) noexcept {
    const auto zero_vec = [](const CheckpointVec3 value) noexcept {
        return value.x == 0.0F && value.y == 0.0F && value.z == 0.0F;
    };
    const PlayerCombatCheckpoint canonical_player{};
    const AbyssEnvironmentRuntime canonical_abyss{};
    const AttackCheckpoint canonical_attack{};
    const auto& player = combat.player;
    const auto& abyss_environment = combat.abyss_environment;
    const auto& attack = combat.attack;
    if (combat.tick != 0U
            || combat.evasion_rng_state != core::DeterministicRng::State{}
            || !zero_vec(player.position) || !zero_vec(player.velocity)
            || player.facing != canonical_player.facing
            || player.state != canonical_player.state
            || player.combo_stage != canonical_player.combo_stage
            || player.air_attack_available
                != canonical_player.air_attack_available
            || player.hp != 0 || player.max_hp != 0
            || player.barrier != 0 || player.max_barrier != 0
            || player.damage_reduction != canonical_player.damage_reduction
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
            || player.skill_cooldowns != canonical_player.skill_cooldowns
            || abyss_environment.rule != canonical_abyss.rule
            || abyss_environment.cycle_tick != 0U
            || abyss_environment.stage_tick != 0U
            || !zero_vec(abyss_environment.locked_center)
            || abyss_environment.expansion_stage
                != canonical_abyss.expansion_stage
            || abyss_environment.warning || abyss_environment.active
            || attack.id != canonical_attack.id
            || attack.elapsed_ticks != 0U
            || attack.startup_ticks != 0U
            || attack.recovery_ticks != 0U
            || attack.connected || attack.impact_event_emitted
            || attack.hit_targets.words != canonical_attack.hit_targets.words
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
    const AbyssEnvironmentRuntime& value) noexcept {
    return value.rule == abyss::AbyssRuleId::none
        && value.cycle_tick == 0U && value.stage_tick == 0U
        && value.locked_center.x == 0.0F
        && value.locked_center.y == 0.0F
        && value.locked_center.z == 0.0F
        && value.expansion_stage == 0xFFU
        && !value.warning && !value.active;
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
        && room.pending_room_experience == 0U
        && popcount(room.defeat_bits) == 0U
        && popcount(room.equipment_claim_bits) == 0U
        && popcount(room.secondary_claim_bits) == 0U
        && room.equipment_ground_count == 0U
        && room.secondary_ground_count == 0U
        && canonical_combat_none(room.combat);
}

[[nodiscard]] bool same_death_owner(
    const CombatDeathSnapshot& combat_death,
    const DeathCheckpoint& durable_death) noexcept {
    const bool source_has_monster = combat_death.source.kind
            != PlayerDamageSourceKind::abyss_environment
        && combat_death.source.kind != PlayerDamageSourceKind::unknown;
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
    if (room.full_clear && (!room.exits_unlocked || !room.reward_committed)) {
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
    constexpr std::uint8_t kMonsterDropSource = 0U;
    constexpr std::uint8_t kAbyssChestSource = 1U;
    const bool cleared = state.abyss.lifecycle
        == abyss::AbyssLifecycle::cleared;
    std::uint8_t visible_rewards{};
    for (std::uint16_t index = 0U;
            index < room.equipment_ground_count; ++index) {
        const EquipmentGroundCheckpoint& ground = room.equipment_ground[index];
        if (ground.source > kAbyssChestSource) return false;
        if (ground.source == kMonsterDropSource) {
            if (ground.reward_ordinal != 0xFFU) return false;
            continue;
        }
        if (!cleared || ground.reward_ordinal >= state.abyss.reward_total
                || ground.reward_ordinal >= 3U) return false;
        const auto bit = static_cast<std::uint8_t>(
            1U << ground.reward_ordinal);
        if ((state.abyss.generated_mask & bit) == 0U
                || (state.abyss.claimed_mask & bit) != 0U
                || (visible_rewards & bit) != 0U) return false;
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
                & state.abyss.generated_mask) != 0U) return false;
    const auto expected_visible = static_cast<std::uint8_t>(
        state.abyss.generated_mask
        & static_cast<std::uint8_t>(~state.abyss.claimed_mask)
        & valid);
    return visible_rewards == expected_visible;
}

[[nodiscard]] bool valid_reserve_drop_ownership(
    const RoomProgressCheckpoint& room,
    const DungeonRunState& state) noexcept {
    constexpr std::uint32_t kReserveCapacity = 16U;
    constexpr std::uint32_t kAbyssSecondaryOrdinalEnd =
        kAbyssSecondaryOrdinalBegin + kReserveCapacity;
    const bool cleared = state.abyss.lifecycle
        == abyss::AbyssLifecycle::cleared;
    const std::uint32_t equipment_reserve_claims = count_bits_in_range(
        room.equipment_claim_bits, room.generated_monsters,
        limits::kRoomMonsterCapacity);
    const std::uint32_t durable_equipment_claims =
        popcount(static_cast<std::uint64_t>(state.abyss.claimed_mask));
    if (equipment_reserve_claims != 0U
            && (!cleared
                || equipment_reserve_claims > durable_equipment_claims)) {
        return false;
    }

    const std::uint32_t material_reserve_claims = count_bits_in_range(
        room.secondary_claim_bits, kAbyssSecondaryOrdinalBegin,
        kAbyssSecondaryOrdinalEnd);
    std::uint32_t visible_material_reserves{};
    constexpr std::uint8_t kAbyssMaterialSource = 2U;
    for (std::uint16_t index = 0U;
            index < room.secondary_ground_count; ++index) {
        const SecondaryGroundCheckpoint& ground = room.secondary_ground[index];
        if (ground.tag == SecondaryGroundTag::material
                && ground.source == kAbyssMaterialSource) {
            ++visible_material_reserves;
        }
    }
    const std::uint32_t material_reserves = material_reserve_claims
        + visible_material_reserves;
    if (material_reserves != 0U && !cleared) return false;
    const std::uint32_t expected_material_reserves = cleared
        ? abyss::reward_profile_for(state.abyss.danger, 1U).item_count : 0U;
    if (material_reserves > expected_material_reserves) return false;

    const std::uint32_t generated_equipment_reserves =
        popcount(static_cast<std::uint64_t>(state.abyss.generated_mask));
    return generated_equipment_reserves + material_reserves
        <= kReserveCapacity;
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
            || player.invulnerability_ticks > 180U
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

[[nodiscard]] bool valid_entry_protection_provenance(
    const RoomProgressCheckpoint& room,
    const DungeonRunState& state) noexcept {
    constexpr std::uint16_t kNormalInvulnerabilityTicks = 30U;
    constexpr std::uint64_t kEntryInvulnerabilityTicks = 180U;
    const std::uint16_t remaining =
        room.combat.player.invulnerability_ticks;
    if (remaining <= kNormalInvulnerabilityTicks) return true;
    if (room.lifecycle != RoomProgressLifecycle::active
            || state.current_room.depth < 1U
            || state.current_room.depth > 3U
            || room.combat.tick >= kEntryInvulnerabilityTicks) {
        return false;
    }
    return remaining == static_cast<std::uint16_t>(
        kEntryInvulnerabilityTicks - room.combat.tick);
}

[[nodiscard]] bool valid_attack(const AttackCheckpoint& attack,
    const std::uint32_t generated) noexcept {
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
        const std::size_t attack_index = static_cast<std::size_t>(attack.id);
        if (attack_index >= kAttackActiveTicks.size()
                || attack.startup_ticks == 0U
                || attack.recovery_ticks == 0U
                || attack.elapsed_ticks
                    >= static_cast<std::uint32_t>(attack.startup_ticks)
                        + kAttackActiveTicks[attack_index]
                        + attack.recovery_ticks
                || attack.connected != any_hit_target
                || attack.impact_event_emitted != any_hit_target
                || (attack.elapsed_ticks < attack.startup_ticks
                    && any_hit_target)) return false;
    }
    for (std::uint32_t ordinal = generated;
            ordinal < limits::kRoomMonsterCapacity; ++ordinal) {
        if (attack.hit_targets.contains(
                static_cast<MonsterOrdinal>(ordinal))) return false;
    }
    return true;
}

[[nodiscard]] const CheckpointAffixTiming* affix_timing(
    const MonsterAffixSet& affixes,
    const MonsterAffixId id) noexcept {
    for (std::size_t index = 0U;
            index < affixes.count && index < affixes.values.size(); ++index) {
        const MonsterAffixInstance& instance = affixes.values[index];
        if (instance.id != id) continue;
        const std::size_t tier = static_cast<std::size_t>(instance.tier);
        if (tier >= static_cast<std::size_t>(MonsterAffixTier::count)) {
            return nullptr;
        }
        if (id == MonsterAffixId::burning_ground) {
            return &kBurningGroundTimings[tier];
        }
        if (id == MonsterAffixId::blink_assault) {
            return &kBlinkAssaultTimings[tier];
        }
        return nullptr;
    }
    return nullptr;
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

    const CheckpointAffixTiming* burning = affix_timing(
        monster.affixes, MonsterAffixId::burning_ground);
    if ((burning == nullptr && monster.burning_ground_ticks != 0U)
            || (burning != nullptr
                && (burning->interval_ticks == 0U
                    ? monster.burning_ground_ticks != 0U
                    : monster.burning_ground_ticks
                        >= burning->interval_ticks))) return false;
    const CheckpointAffixTiming* blink = affix_timing(
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

}  // namespace

void clear_room_combat_checkpoint(RoomCombatCheckpoint& out) noexcept {
    out.tick = 0U;
    out.evasion_rng_state = {};
    out.player = {};
    out.abyss_environment = {};
    out.attack = {};
    for (auto& monster : out.monsters) monster = {};
    out.monster_count = 0U;
    for (auto& obstacle : out.obstacles) obstacle = {};
    out.obstacle_count = 0U;
    out.fire_crates = {};
    out.fire_crate_count = 0U;
    out.player_damage_history = {};
    out.has_death_snapshot = false;
    out.death_snapshot = {};
}

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
    out.pending_room_experience = 0U;
    out.defeat_bits = {};
    out.equipment_claim_bits = {};
    out.secondary_claim_bits = {};
    clear_room_combat_checkpoint(out.combat);
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

bool valid_room_combat_checkpoint_structural(
    const RoomCombatCheckpoint& checkpoint,
    const std::uint32_t generated_monsters) noexcept {
    bool rng_nonzero = false;
    for (const std::uint64_t word : checkpoint.evasion_rng_state) {
        rng_nonzero = rng_nonzero || word != 0U;
    }
    if (!rng_nonzero || generated_monsters > limits::kRoomMonsterCapacity
            || !valid_player(checkpoint.player)
            || !valid_attack(checkpoint.attack, generated_monsters)
            || !valid_damage_history(checkpoint.player_damage_history)
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
    for (std::uint16_t index = 0U; index < checkpoint.monster_count;
            ++index) {
        const MonsterCombatCheckpoint& monster = checkpoint.monsters[index];
        if (!valid_monster(monster) || monster.ordinal >= generated_monsters
                || (index != 0U && monster.ordinal <= previous_monster)) {
            return false;
        }
        previous_monster = monster.ordinal;
    }
    std::uint16_t previous_obstacle{};
    for (std::uint16_t index = 0U; index < checkpoint.obstacle_count;
            ++index) {
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
    for (std::uint8_t index = 0U; index < checkpoint.fire_crate_count;
            ++index) {
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
        if (recent_total
                > (std::numeric_limits<std::uint64_t>::max)() - value) {
            return false;
        }
        recent_total += value;
    }
    const std::size_t primary_index = static_cast<std::size_t>(
        death.primary_type);
    return death.tick <= checkpoint.tick && death.final_damage != 0U
        && death.health_loss != 0U && death.defense.hp == 0
        && death.raw_damage >= death.final_damage
        && death.barrier_loss <= static_cast<std::uint64_t>(
            checkpoint.player.max_barrier - checkpoint.player.barrier)
        && death.health_loss <= static_cast<std::uint64_t>(
            checkpoint.player.max_hp)
        && death.barrier_loss
            <= (std::numeric_limits<std::uint64_t>::max)()
                - death.health_loss
        && death.final_damage == death.barrier_loss + death.health_loss
        && death.source.kind <= PlayerDamageSourceKind::unknown
        && death.primary_type <= DamageType::chaos
        && recent_total >= death.final_damage
        && primary_index < death.recent_damage.size()
        && death.recent_damage[primary_index] != 0U
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
        && death.defense.damage_reduction
            == checkpoint.player.damage_reduction
        && death.defense.damage_reduction_cap
            == checkpoint.player.damage_reduction_cap
        && death.recent_damage
            == damage_history_totals(checkpoint.player_damage_history)
        && checkpoint.player_damage_history.initialized
        && checkpoint.player_damage_history.active_tick == death.tick
        && same_source(death.source, canonical_source(death.source));
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
            || room.required_kills != required_kills(room.generated_monsters)
            || popcount(room.defeat_bits) != room.defeated_monsters
            || !no_bits_at_or_above(
                room.defeat_bits, room.generated_monsters)
            || !valid_secondary_claim_domain(
                room.secondary_claim_bits, room.generated_monsters)
            || (room.exits_unlocked
                && room.defeated_monsters < room.required_kills)
            || (room.full_clear
                && room.defeated_monsters != room.generated_monsters)
            || (room.reward_committed && !room.full_clear)
            || (room.pending_room_experience != 0U
                && (room.lifecycle != RoomProgressLifecycle::active
                    || room.full_clear || room.reward_committed))
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
            || !valid_room_combat_checkpoint_structural(
                room.combat, room.generated_monsters)
            || !valid_entry_protection_provenance(room, state)) {
        return false;
    }
    MonsterOrdinal previous_monster = kInvalidMonsterOrdinal;
    for (std::uint16_t index = 0U; index < room.combat.monster_count;
            ++index) {
        const MonsterCombatCheckpoint& monster = room.combat.monsters[index];
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
        constexpr std::uint8_t kMonsterDropSource = 0U;
        constexpr std::uint8_t kAbyssChestSource = 1U;
        if (ground.ordinal >= limits::kRoomMonsterCapacity
                || ground.source > kAbyssChestSource
                || (ground.source == kMonsterDropSource
                    && ground.ordinal >= room.generated_monsters)
                || (index != 0U && ground.ordinal <= previous_equipment)
                || bit_is_set(room.equipment_claim_bits, ground.ordinal)
                || !finite(ground.position)
                || !items::validate_item(ground.item)) return false;
        previous_equipment = ground.ordinal;
    }
    constexpr std::uint8_t kMonsterCommonSource = 0U;
    constexpr std::uint8_t kMonsterCouponSource = 1U;
    constexpr std::uint8_t kAbyssMaterialSource = 2U;
    const std::uint32_t ordinary_secondary_end =
        room.generated_monsters * 2U;
    constexpr std::uint16_t kAbyssSecondaryOrdinalEnd =
        kAbyssSecondaryOrdinalBegin + 16U;
    std::uint16_t previous_secondary{};
    for (std::uint16_t index = 0U;
            index < room.secondary_ground_count; ++index) {
        const SecondaryGroundCheckpoint& ground = room.secondary_ground[index];
        if (ground.tag > SecondaryGroundTag::health_potion
                || ground.ordinal >= kRoomSecondaryGroundCapacity
                || (index != 0U && ground.ordinal <= previous_secondary)
                || (room.secondary_claim_bits[ground.ordinal / 64U]
                    & (std::uint64_t{1U}
                        << (ground.ordinal % 64U))) != 0U
                || !finite(ground.position)
                || (ground.tag == SecondaryGroundTag::material
                    && (ground.material >= items::MaterialId::count
                        || ground.source > kAbyssMaterialSource
                        || !((ground.source == kMonsterCommonSource
                                && ground.ordinal < ordinary_secondary_end
                                && (ground.ordinal & 1U) == 0U)
                            || (ground.source == kMonsterCouponSource
                                && ground.ordinal < ordinary_secondary_end
                                && (ground.ordinal & 1U) != 0U)
                            || (ground.source == kAbyssMaterialSource
                                && ground.ordinal
                                    >= kAbyssSecondaryOrdinalBegin
                                && ground.ordinal
                                    < kAbyssSecondaryOrdinalEnd))))
                || (ground.tag == SecondaryGroundTag::health_potion
                    && (ground.material != items::MaterialId::count
                        || ground.source != kMonsterCommonSource
                        || ground.ordinal >= ordinary_secondary_end
                        || (ground.ordinal & 1U) == 0U))) {
            return false;
        }
        previous_secondary = ground.ordinal;
    }
    return valid_abyss_reward_ground_ownership(room, state)
        && valid_reserve_drop_ownership(room, state);
}

bool same_room_combat_checkpoint(const RoomCombatCheckpoint& left,
    const RoomCombatCheckpoint& right) noexcept {
    if (left.monster_count > left.monsters.size()
            || right.monster_count > right.monsters.size()
            || left.obstacle_count > left.obstacles.size()
            || right.obstacle_count > right.obstacles.size()
            || left.fire_crate_count > left.fire_crates.size()
            || right.fire_crate_count > right.fire_crates.size()) {
        return false;
    }
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

bool same_room_progress_checkpoint(const RoomProgressCheckpoint& left,
    const RoomProgressCheckpoint& right) noexcept {
    if (left.equipment_ground_count > left.equipment_ground.size()
            || right.equipment_ground_count > right.equipment_ground.size()
            || left.secondary_ground_count > left.secondary_ground.size()
            || right.secondary_ground_count > right.secondary_ground.size()) {
        return false;
    }
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
            || left.pending_room_experience
                != right.pending_room_experience
            || left.defeat_bits != right.defeat_bits
            || left.equipment_claim_bits != right.equipment_claim_bits
            || left.secondary_claim_bits != right.secondary_claim_bits
            || left.equipment_ground_count != right.equipment_ground_count
            || left.secondary_ground_count != right.secondary_ground_count
            || !same_room_combat_checkpoint(left.combat, right.combat)) {
        return false;
    }
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

}  // namespace arpg::checkpoint
