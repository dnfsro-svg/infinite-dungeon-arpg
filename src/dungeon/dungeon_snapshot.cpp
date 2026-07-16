#include "dungeon/dungeon_session.hpp"

#include "dungeon/room_generation.hpp"
#include "items/item_catalog.hpp"

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
    result.exits_open.fill(
        phase_ == RoomPhase::cleared || phase_ == RoomPhase::awaiting_exit);
    result.abyss_doors = preview_abyss_doors(stable_state_.current_room);
    result.wave_index = wave_index_;
    result.wave_count = encounter_plan_.wave_count;
    result.wave_delay_ticks = wave_delay_ticks_;
    result.remaining_targets = remaining_targets();
    result.entry_side = stable_state_.current_room.entry;
    result.last_exit = last_exit_;
    result.last_transition = stable_state_.last_transition;
    result.ecology = stable_state_.current_room.ecology;
    result.has_hole = stable_state_.current_room.has_hole;
    result.is_abyss = stable_state_.current_room.is_abyss;
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
        const items::BaseDefinition* base =
            items::base_definition(ground.item.base_id);
        if (base != nullptr) packed.slot = base->slot;
        packed.rarity = ground.item.rarity;
    }
    if (pending_save_.has_value()) {
        result.pending_save_kind = pending_save_->kind;
    }
    if (combat_.has_value()) {
        result.combat.emplace(combat_->snapshot());
    }
    result.encounter.total_budget = encounter_plan_.total_budget;
    result.encounter.plan_valid = encounter_plan_legal(
        encounter_plan_, rules_.encounter);
    if (wave_index_ < encounter_plan_.wave_count) {
        const auto& wave = encounter_plan_.waves[wave_index_];
        result.encounter.current_wave_budget = wave.spent_budget;
        result.encounter.current_wave_spawn_count = wave.spawn_count;
    }
    result.diagnostics = diagnostics_;
    result.progression = room_progression_;
    result.pending_room_experience = pending_room_experience_;
    result.last_room_experience = last_room_experience_;
    result.last_levels_gained = last_levels_gained_;
    return result;
}

}  // namespace arpg::dungeon
