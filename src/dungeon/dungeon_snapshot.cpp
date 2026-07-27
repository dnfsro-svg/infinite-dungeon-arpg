#include "dungeon/dungeon_session.hpp"

#include "dungeon/death_checkpoint.hpp"
#include "dungeon/room_generation.hpp"
#include "items/item_catalog.hpp"

#include <limits>

namespace arpg::dungeon {
namespace {

bool equipped(const items::EquipmentState& equipment,
    std::uint64_t item_id) noexcept {
    for (const std::uint64_t equipped_id : equipment.equipped_ids) {
        if (equipped_id == item_id) return true;
    }
    return false;
}

}  // namespace

DungeonSnapshot DungeonSession::snapshot() const noexcept {
    return build_dungeon_snapshot();
}

DungeonSnapshot DungeonSession::build_dungeon_snapshot() const noexcept {
    DungeonSnapshot result{};
    result.session_tick = session_tick_;
    result.root_seed = stable_state_.root_seed;
    result.commit_generation = stable_state_.commit_generation;
    result.room_index = stable_state_.current_room.index;
    result.room_seed = stable_state_.current_room.seed;
    result.depth = stable_state_.current_room.depth;
    result.floor_room_index = stable_state_.current_room.floor_room_index;
    result.biases = stable_state_.biases;
    result.phase = phase_;
    result.has_active_room = combat_.has_value();
    const checkpoint::DeathCheckpoint* visible_death = nullptr;
    checkpoint::DeathCheckpoint retry_death{};
    bool death_saving = false;
    if (pending_save_.has_value()
            && pending_save_->next_state.death.lifecycle
                == checkpoint::DeathLifecycle::pending_continue) {
        visible_death = &pending_save_->next_state.death;
        death_saving = true;
    } else if (pending_save_.has_value()
            && pending_save_->kind == PendingSaveKind::death_continue
            && stable_state_.death.lifecycle
                == checkpoint::DeathLifecycle::pending_continue) {
        visible_death = &stable_state_.death;
        death_saving = true;
    } else if (stable_state_.death.lifecycle
            == checkpoint::DeathLifecycle::pending_continue) {
        visible_death = &stable_state_.death;
    } else if (combat_.has_value() && combat_->death_snapshot().has_value()
            && stable_state_.death_sequence
                != (std::numeric_limits<std::uint64_t>::max)()) {
        const auto target = make_death_retreat_target(stable_state_,
            stable_state_.death_sequence + 1U, rules_);
        if (target.fault == DungeonFault::none) {
            retry_death = make_death_checkpoint(*combat_->death_snapshot(),
                stable_state_.current_room, target.room);
            visible_death = &retry_death;
            death_saving = true;
        }
    }
    if (visible_death != nullptr) {
        result.death.emplace(DeathSnapshot{
            *visible_death,
            death_saving,
            !death_saving && phase_ == RoomPhase::death_pending,
            !death_saving && death_continue_failed_,
        });
    }
    bool exits_open = phase_ == RoomPhase::cleared
        || phase_ == RoomPhase::awaiting_exit;
    if (!exits_open && phase_ == RoomPhase::committing
            && pending_save_.has_value()
            && pending_save_->kind != PendingSaveKind::abyss_clear) {
        exits_open = pending_save_->resume_phase == RoomPhase::cleared
            || pending_save_->resume_phase == RoomPhase::awaiting_exit;
    }
    result.exits_open.fill(exits_open);
    result.abyss_doors = visible_death == nullptr
        ? preview_abyss_doors(stable_state_.current_room)
        : std::array<bool, 4>{};
    result.wave_index = 0U;
    result.wave_count = room_progress_.initial_monster_count == 0U ? 0U : 1U;
    result.wave_delay_ticks = 0U;
    result.initial_monster_count = room_progress_.initial_monster_count;
    result.defeated_monster_count = room_progress_.defeated_monster_count;
    result.remaining_targets = remaining_targets();
    const combat::RoomMonsterField* monster_field = combat_.has_value()
        ? combat_->room_monster_field() : nullptr;
    if (monster_field != nullptr) {
        result.monster_generator_version =
            monster_field->plan().generator_version;
        result.monster_blueprint_hash = monster_field->plan().blueprint_hash;
    }
    if (room_environment_ != nullptr) {
        result.environment_generator_version =
            room_environment_->generator_version;
        result.environment_blueprint_hash = room_environment_->blueprint_hash;
    }
    result.entry_side = stable_state_.current_room.entry;
    result.last_exit = last_exit_;
    result.last_transition = stable_state_.last_transition;
    result.ecology = stable_state_.current_room.ecology;
    result.has_hole = stable_state_.current_room.has_hole;
    result.is_abyss = stable_state_.current_room.is_abyss;
    if (result.is_abyss) {
        result.abyss_danger = stable_state_.abyss.danger;
        result.abyss_rule = stable_state_.abyss.rule;
    }
    result.abyss_pending_rewards = abyss_pending_reward_count();
    result.abyss_unpicked_rewards = abyss_unpicked_reward_count();
    result.abyss_exit_confirmation_armed = abyss_exit_confirmation_.armed;
    result.abyss_exit_confirmation_transition =
        abyss_exit_confirmation_.transition;
    result.abyss_exit_confirmation_direction =
        abyss_exit_confirmation_.direction;
    result.has_pending_transition = pending_save_.has_value()
        && (pending_save_->kind == PendingSaveKind::transition
            || pending_save_->kind == PendingSaveKind::abyss_abandon);
    result.passive_tree = stable_state_.passive_tree;
    result.skill_loadout = stable_state_.skill_loadout;
    result.passive_save_pending = pending_save_.has_value()
        && pending_save_->kind == PendingSaveKind::passive_tree;
    result.passive_tree_error = last_passive_tree_error_;
    result.equipped_ids = stable_state_.item_ownership.equipment.equipped_ids;
    for (const items::ItemInstance& item : stable_state_.item_ownership.items) {
        if (!equipped(stable_state_.item_ownership.equipment, item.id)) {
            ++result.inventory_count;
        }
    }
    for (const GroundItem& ground : ground_items_) {
        if (!ground.active) continue;
        GroundItemSnapshot& packed =
            result.ground_items[result.ground_item_count++];
        packed.ordinal = ground.drop_ordinal;
        packed.source = ground.source;
        packed.abyss_reward_ordinal = ground.abyss_reward_ordinal;
        packed.position = ground.position;
        packed.item_id = ground.item.id;
        packed.base_id = ground.item.base_id;
        packed.item_level = ground.item.item_level;
        const items::BaseDefinition* base =
            items::base_definition(ground.item.base_id);
        if (base != nullptr) packed.slot = base->slot;
        packed.rarity = ground.item.rarity;
    }
    for (const GroundMaterial& ground : ground_materials_) {
        if (!ground.active) continue;
        GroundMaterialSnapshot& packed =
            result.ground_materials[result.ground_material_count++];
        packed.ordinal = ground.ordinal;
        packed.source = ground.source;
        packed.position = ground.position;
        packed.material = ground.material;
    }
    for (const GroundHealthPotion& ground : ground_health_potions_) {
        if (!ground.active) continue;
        GroundHealthPotionSnapshot& packed =
            result.ground_health_potions[
                result.ground_health_potion_count++];
        packed.spawn_ordinal = ground.spawn_ordinal;
        packed.claim_ordinal = ground.claim_ordinal;
        packed.position = ground.position;
    }
    result.health_potion_pickup_receipt = health_potion_pickup_receipt_;
    result.material_pickup_receipt = material_pickup_receipt_;
    result.reinforcement_receipt = reinforcement_receipt_;
    if (pending_save_.has_value()) {
        result.pending_save_kind = pending_save_->kind;
        if (pending_save_->kind == PendingSaveKind::loot_pickup
                || pending_save_->kind
                    == PendingSaveKind::abyss_reward_claim) {
            result.pending_pickup_ordinal = pending_save_->pickup_ordinal;
        }
        if (pending_save_->kind == PendingSaveKind::material_pickup) {
            result.pending_material_pickup_ordinal =
                pending_save_->pickup_ordinal;
        }
        if (pending_save_->kind == PendingSaveKind::health_potion_pickup) {
            result.pending_health_potion_spawn_ordinal =
                pending_save_->pickup_ordinal;
        }
    }
    if (combat_.has_value()) {
        result.combat.emplace(combat_->snapshot());
    }
    result.encounter.plan_valid = monster_field != nullptr
        && room_environment_ != nullptr
        && result.monster_generator_version != 0U
        && result.monster_blueprint_hash != 0U
        && result.environment_generator_version != 0U
        && result.environment_blueprint_hash != 0U;
    result.diagnostics = diagnostics_;
    result.progression = room_progression_;
    result.pending_room_experience = pending_room_experience_;
    result.last_room_experience = last_room_experience_;
    result.last_levels_gained = last_levels_gained_;
    return result;
}

}  // namespace arpg::dungeon
