#include "dungeon/death_checkpoint.hpp"

#include "combat/monster_affix_types.hpp"
#include "dungeon/room_generation.hpp"

#include <cstdint>

namespace arpg::dungeon {
namespace {

checkpoint::DeathSourceKind stable_source_kind(
    combat::PlayerDamageSourceKind kind) noexcept {
    switch (kind) {
    case combat::PlayerDamageSourceKind::monster_attack:
        return checkpoint::DeathSourceKind::monster_attack;
    case combat::PlayerDamageSourceKind::projectile:
        return checkpoint::DeathSourceKind::projectile;
    case combat::PlayerDamageSourceKind::ground_hazard:
        return checkpoint::DeathSourceKind::ground_hazard;
    case combat::PlayerDamageSourceKind::monster_affix:
        return checkpoint::DeathSourceKind::monster_affix;
    case combat::PlayerDamageSourceKind::abyss_environment:
        return checkpoint::DeathSourceKind::abyss_environment;
    case combat::PlayerDamageSourceKind::unknown:
        return checkpoint::DeathSourceKind::unknown;
    }
    return checkpoint::DeathSourceKind::unknown;
}

bool valid_source_catalog(
    const checkpoint::DeathCheckpoint& death) noexcept {
    const bool valid_monster = death.source_monster_id
        < static_cast<std::uint8_t>(combat::MonsterId::count);
    switch (death.source_kind) {
    case checkpoint::DeathSourceKind::monster_attack:
    case checkpoint::DeathSourceKind::projectile:
        return valid_monster && death.source_detail_id == 0U;
    case checkpoint::DeathSourceKind::ground_hazard:
        return valid_monster && death.source_detail_id
            <= static_cast<std::uint16_t>(combat::HazardKind::chaos_expansion);
    case checkpoint::DeathSourceKind::monster_affix:
        return valid_monster && death.source_detail_id
            < static_cast<std::uint16_t>(combat::MonsterAffixId::count);
    case checkpoint::DeathSourceKind::abyss_environment:
        return death.source_monster_id == 0xFFU
            && death.source_detail_id <= static_cast<std::uint16_t>(
                abyss::AbyssRuleId::life_sacrifice);
    case checkpoint::DeathSourceKind::unknown:
        return death.source_monster_id == 0xFFU
            && death.source_detail_id == 0U;
    }
    return false;
}

bool same_room(
    const checkpoint::RoomDescriptor& lhs,
    const checkpoint::RoomDescriptor& rhs) noexcept {
    return lhs.index == rhs.index
        && lhs.seed == rhs.seed
        && lhs.depth == rhs.depth
        && lhs.floor_room_index == rhs.floor_room_index
        && lhs.entry == rhs.entry
        && lhs.ecology == rhs.ecology
        && lhs.has_hole == rhs.has_hole
        && lhs.is_abyss == rhs.is_abyss;
}

}  // namespace

checkpoint::DeathCheckpoint make_death_checkpoint(
    const combat::CombatDeathSnapshot& combat_death,
    const checkpoint::RoomDescriptor& death_room,
    const checkpoint::RoomDescriptor& target_room) noexcept {
    checkpoint::DeathCheckpoint result{};
    result.lifecycle = checkpoint::DeathLifecycle::pending_continue;
    result.data_version = checkpoint::kDeathCheckpointDataVersion;
    result.death_depth = death_room.depth;
    result.death_floor_room_index = death_room.floor_room_index;
    result.death_ecology = death_room.ecology;
    result.death_was_abyss = death_room.is_abyss;
    result.source_kind = stable_source_kind(combat_death.source.kind);
    const bool has_monster = result.source_kind
            != checkpoint::DeathSourceKind::abyss_environment
        && result.source_kind != checkpoint::DeathSourceKind::unknown;
    result.source_monster_id = has_monster
        ? static_cast<std::uint8_t>(combat_death.source.monster)
        : 0xFFU;
    result.source_detail_id = combat_death.source.detail_id;
    result.damage_type = static_cast<checkpoint::DeathDamageType>(
        combat_death.primary_type);
    result.raw_damage = combat_death.raw_damage;
    result.barrier_loss = combat_death.barrier_loss;
    result.health_loss = combat_death.health_loss;
    result.final_damage = combat_death.final_damage;
    result.recent_damage = combat_death.recent_damage;
    result.hp = combat_death.defense.hp;
    result.max_hp = combat_death.defense.max_hp;
    result.barrier = combat_death.defense.barrier;
    result.max_barrier = combat_death.defense.max_barrier;
    result.armor = combat_death.defense.armor;
    result.evasion = combat_death.defense.evasion;
    result.armor_reduction_bp = combat_death.defense.armor_reduction_bp;
    result.evasion_rate_bp = combat_death.defense.evasion_rate_bp;
    result.damage_reduction = combat_death.defense.damage_reduction;
    result.damage_reduction_cap = combat_death.defense.damage_reduction_cap;
    result.target_room = target_room;
    return result;
}

bool valid_death_checkpoint_dungeon(
    const checkpoint::DeathCheckpoint& death,
    const checkpoint::RoomDescriptor& death_room,
    std::uint64_t commit_generation,
    std::uint64_t next_death_sequence,
    const DungeonRules& rules) noexcept {
    if (!checkpoint::valid_death_checkpoint_structural(death)
        || death.lifecycle != checkpoint::DeathLifecycle::pending_continue
        || !valid_source_catalog(death)
        || death.death_depth != death_room.depth
        || death.death_floor_room_index != death_room.floor_room_index
        || death.death_ecology != death_room.ecology
        || death.death_was_abyss != death_room.is_abyss
        || next_death_sequence == 0U) {
        return false;
    }
    checkpoint::DungeonRunState input{};
    input.commit_generation = commit_generation;
    input.current_room = death_room;
    input.death_sequence = next_death_sequence - 1U;
    const DeathRetreatTargetResult regenerated = make_death_retreat_target(
        input, next_death_sequence, rules);
    return regenerated.fault == DungeonFault::none
        && same_room(regenerated.room, death.target_room);
}

}  // namespace arpg::dungeon
