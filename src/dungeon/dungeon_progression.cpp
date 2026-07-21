#include "dungeon/dungeon_progression.hpp"

#include "abyss/abyss_rules.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace arpg::dungeon {
namespace {

[[nodiscard]] RunStateBuildResult unchanged(
    DungeonFault fault,
    const checkpoint::DungeonRunState& state) noexcept {
    RunStateBuildResult result;
    result.fault = fault;
    result.state = state;
    return result;
}

[[nodiscard]] bool checked_increment(
    std::uint64_t value,
    std::uint64_t& next) noexcept {
    if (value == (std::numeric_limits<std::uint64_t>::max)()) {
        return false;
    }
    next = value + 1U;
    return true;
}

[[nodiscard]] bool same_item(
    const items::ItemInstance& lhs,
    const items::ItemInstance& rhs) noexcept {
    if (lhs.id != rhs.id || lhs.base_id != rhs.base_id
            || lhs.rarity != rhs.rarity
            || lhs.item_level != rhs.item_level
            || lhs.required_level != rhs.required_level
            || lhs.affix_count != rhs.affix_count
            || lhs.reserved != rhs.reserved
            || lhs.reinforcement != rhs.reinforcement
            || lhs.extension_reserved != rhs.extension_reserved) {
        return false;
    }
    for (std::size_t index = 0U; index < lhs.affixes.size(); ++index) {
        const items::AffixRoll& a = lhs.affixes[index];
        const items::AffixRoll& b = rhs.affixes[index];
        if (a.affix_id != b.affix_id || a.tier != b.tier
                || a.variant != b.variant
                || a.value_roll_bp != b.value_roll_bp) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool same_item_ownership(
    const items::ItemOwnershipState& lhs,
    const items::ItemOwnershipState& rhs) noexcept {
    if (lhs.items.size() != rhs.items.size()
            || lhs.equipment.equipped_ids != rhs.equipment.equipped_ids
            || lhs.materials != rhs.materials
            || lhs.material_discovery_bits != rhs.material_discovery_bits
            || lhs.material_claimed_drop_bits
                != rhs.material_claimed_drop_bits
            || lhs.claimed_drop_bits != rhs.claimed_drop_bits
            || lhs.next_item_sequence != rhs.next_item_sequence) {
        return false;
    }
    for (std::size_t index = 0U; index < lhs.items.size(); ++index) {
        if (!same_item(lhs.items[index], rhs.items[index])) return false;
    }
    return true;
}

[[nodiscard]] bool same_room(
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

[[nodiscard]] bool same_death_checkpoint(
    const checkpoint::DeathCheckpoint& lhs,
    const checkpoint::DeathCheckpoint& rhs) noexcept {
    return lhs.lifecycle == rhs.lifecycle
        && lhs.data_version == rhs.data_version
        && lhs.death_depth == rhs.death_depth
        && lhs.death_floor_room_index == rhs.death_floor_room_index
        && lhs.death_ecology == rhs.death_ecology
        && lhs.death_was_abyss == rhs.death_was_abyss
        && lhs.source_kind == rhs.source_kind
        && lhs.source_monster_id == rhs.source_monster_id
        && lhs.source_detail_id == rhs.source_detail_id
        && lhs.damage_type == rhs.damage_type
        && lhs.raw_damage == rhs.raw_damage
        && lhs.barrier_loss == rhs.barrier_loss
        && lhs.health_loss == rhs.health_loss
        && lhs.final_damage == rhs.final_damage
        && lhs.recent_damage == rhs.recent_damage
        && lhs.hp == rhs.hp
        && lhs.max_hp == rhs.max_hp
        && lhs.barrier == rhs.barrier
        && lhs.max_barrier == rhs.max_barrier
        && lhs.armor == rhs.armor
        && lhs.evasion == rhs.evasion
        && lhs.armor_reduction_bp == rhs.armor_reduction_bp
        && lhs.evasion_rate_bp == rhs.evasion_rate_bp
        && lhs.damage_reduction == rhs.damage_reduction
        && lhs.damage_reduction_cap == rhs.damage_reduction_cap
        && same_room(lhs.target_room, rhs.target_room);
}

[[nodiscard]] bool checked_increment(
    std::uint32_t value,
    std::uint32_t& next) noexcept {
    if (value == (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    next = value + 1U;
    return true;
}

}  // namespace

RunStateBuildResult make_initial_run_state(
    std::uint64_t root_seed,
    const DungeonRules& rules) noexcept {
    const std::array<std::uint32_t, 4> biases{};
    const auto generated = generate_room_descriptor(
        derive_initial_room_seed(root_seed, 0U),
        0U,
        1U,
        1U,
        checkpoint::EntrySide::initial,
        biases,
        rules);
    if (generated.fault != DungeonFault::none) {
        return {generated.fault, {}, {}};
    }

    checkpoint::DungeonRunState state;
    state.root_seed = root_seed;
    state.commit_generation = 1U;
    state.biases = biases;
    state.current_room = generated.room;
    state.current_room.is_abyss = false;
    state.abyss = {};
    state.last_transition = checkpoint::TransitionKind::none;
    state.last_direction = checkpoint::ExitDirection::none;
    return {DungeonFault::none, state, generated.samples};
}

RunStateBuildResult make_door_transition(
    const checkpoint::DungeonRunState& current,
    checkpoint::ExitDirection direction,
    const DungeonRules& rules) noexcept {
    const auto element = element_for_exit(direction);
    if (!element.has_value()) {
        return unchanged(DungeonFault::invalid_direction, current);
    }

    checkpoint::DungeonRunState next = current;
    if (!checked_increment(
            current.commit_generation, next.commit_generation)) {
        return unchanged(DungeonFault::commit_generation_overflow, current);
    }
    if (!checked_increment(
            current.current_room.index, next.current_room.index)) {
        return unchanged(DungeonFault::room_index_overflow, current);
    }
    if (!checked_increment(
            current.current_room.floor_room_index,
            next.current_room.floor_room_index)) {
        return unchanged(DungeonFault::floor_room_overflow, current);
    }
    const auto element_index = static_cast<std::size_t>(element.value());
    if (!checked_increment(
            current.biases[element_index], next.biases[element_index])) {
        return unchanged(DungeonFault::bias_overflow, current);
    }

    next.current_room.seed = derive_door_room_seed(
        current.current_room.seed,
        next.current_room.index,
        direction);
    next.current_room.entry = entry_side_for_exit(direction);
    const auto generated = generate_room_descriptor(
        next.current_room.seed,
        next.current_room.index,
        next.current_room.depth,
        next.current_room.floor_room_index,
        next.current_room.entry,
        next.biases,
        rules);
    if (generated.fault != DungeonFault::none) {
        return unchanged(generated.fault, current);
    }

    next.current_room = generated.room;
    next.abyss = {};
    const auto preview = preview_abyss_doors(current.current_room);
    next.current_room.is_abyss = preview[
        static_cast<std::size_t>(direction)];
    if (next.current_room.is_abyss) {
        const auto selection = abyss::select_abyss_rule(
            next.current_room.seed, next.current_room.depth);
        if (!selection.has_value()) {
            return unchanged(DungeonFault::invalid_rules, current);
        }
        next.abyss.lifecycle = abyss::AbyssLifecycle::available;
        next.abyss.danger = selection->danger;
        next.abyss.rule = selection->rule;
        next.abyss.rules_version = selection->rules_version;
    }
    next.last_transition = checkpoint::TransitionKind::door;
    next.last_direction = direction;
    return {DungeonFault::none, std::move(next), generated.samples};
}

RunStateBuildResult make_descent_transition(
    const checkpoint::DungeonRunState& current,
    const DungeonRules& rules) noexcept {
    checkpoint::DungeonRunState next = current;
    if (!checked_increment(
            current.commit_generation, next.commit_generation)) {
        return unchanged(DungeonFault::commit_generation_overflow, current);
    }
    if (!checked_increment(
            current.current_room.index, next.current_room.index)) {
        return unchanged(DungeonFault::room_index_overflow, current);
    }
    if (!checked_increment(
            current.current_room.depth, next.current_room.depth)) {
        return unchanged(DungeonFault::depth_overflow, current);
    }

    next.biases = {};
    next.current_room.seed = derive_descent_room_seed(
        current.current_room.seed,
        next.current_room.index);
    next.current_room.floor_room_index = 1U;
    next.current_room.entry = checkpoint::EntrySide::initial;
    const auto generated = generate_room_descriptor(
        next.current_room.seed,
        next.current_room.index,
        next.current_room.depth,
        next.current_room.floor_room_index,
        next.current_room.entry,
        next.biases,
        rules);
    if (generated.fault != DungeonFault::none) {
        return unchanged(generated.fault, current);
    }

    next.current_room = generated.room;
    next.current_room.is_abyss = false;
    next.abyss = {};
    next.last_transition = checkpoint::TransitionKind::descent;
    next.last_direction = checkpoint::ExitDirection::none;
    return {DungeonFault::none, std::move(next), generated.samples};
}

bool same_run_state(
    const checkpoint::DungeonRunState& lhs,
    const checkpoint::DungeonRunState& rhs) noexcept {
    bool same_skill_loadout = lhs.skill_loadout.owned_active_bits
        == rhs.skill_loadout.owned_active_bits;
    for (std::size_t slot = 0U;
            same_skill_loadout && slot < lhs.skill_loadout.slots.size();
            ++slot) {
        same_skill_loadout = lhs.skill_loadout.slots[slot].active
                == rhs.skill_loadout.slots[slot].active
            && lhs.skill_loadout.slots[slot].supports
                == rhs.skill_loadout.slots[slot].supports;
    }
    return same_skill_loadout
        && lhs.root_seed == rhs.root_seed
        && lhs.commit_generation == rhs.commit_generation
        && lhs.biases == rhs.biases
        && lhs.current_room.index == rhs.current_room.index
        && lhs.current_room.seed == rhs.current_room.seed
        && lhs.current_room.depth == rhs.current_room.depth
        && lhs.current_room.floor_room_index == rhs.current_room.floor_room_index
        && lhs.current_room.entry == rhs.current_room.entry
        && lhs.current_room.ecology == rhs.current_room.ecology
        && lhs.current_room.has_hole == rhs.current_room.has_hole
        && lhs.current_room.is_abyss == rhs.current_room.is_abyss
        && lhs.abyss.lifecycle == rhs.abyss.lifecycle
        && lhs.abyss.danger == rhs.abyss.danger
        && lhs.abyss.rule == rhs.abyss.rule
        && lhs.abyss.rules_version == rhs.abyss.rules_version
        && lhs.abyss.reward_total == rhs.abyss.reward_total
        && lhs.abyss.generated_mask == rhs.abyss.generated_mask
        && lhs.abyss.claimed_mask == rhs.abyss.claimed_mask
        && lhs.abyss.abandoned_mask == rhs.abyss.abandoned_mask
        && lhs.abyss.reward_revision == rhs.abyss.reward_revision
        && lhs.last_abyss_resolution.valid
            == rhs.last_abyss_resolution.valid
        && lhs.last_abyss_resolution.room_seed
            == rhs.last_abyss_resolution.room_seed
        && lhs.last_abyss_resolution.rule
            == rhs.last_abyss_resolution.rule
        && lhs.last_abyss_resolution.total
            == rhs.last_abyss_resolution.total
        && lhs.last_abyss_resolution.generated
            == rhs.last_abyss_resolution.generated
        && lhs.last_abyss_resolution.claimed
            == rhs.last_abyss_resolution.claimed
        && lhs.last_abyss_resolution.abandoned
            == rhs.last_abyss_resolution.abandoned
        && lhs.progression.level == rhs.progression.level
        && lhs.progression.experience == rhs.progression.experience
        && lhs.progression.earned_passive_points
            == rhs.progression.earned_passive_points
        && lhs.progression.unspent_passive_points
            == rhs.progression.unspent_passive_points
        && lhs.passive_tree.allocated_bits == rhs.passive_tree.allocated_bits
        && same_item_ownership(lhs.item_ownership, rhs.item_ownership)
        && lhs.death_sequence == rhs.death_sequence
        && same_death_checkpoint(lhs.death, rhs.death)
        && lhs.last_transition == rhs.last_transition
        && lhs.last_direction == rhs.last_direction;
}

}  // namespace arpg::dungeon
