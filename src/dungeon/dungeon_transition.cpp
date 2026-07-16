#include "dungeon/dungeon_session.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "passives/passive_tree_rules.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>

namespace arpg::dungeon {
namespace {

void saturating_increment(std::uint32_t& value) noexcept {
    if (value != (std::numeric_limits<std::uint32_t>::max)()) {
        ++value;
    }
}

bool item_request_phase(RoomPhase phase) noexcept {
    return phase == RoomPhase::locked || phase == RoomPhase::combat
        || phase == RoomPhase::wave_delay || phase == RoomPhase::cleared
        || phase == RoomPhase::awaiting_exit;
}

const items::ItemInstance* find_item(
    const items::ItemOwnershipState& state,
    std::uint64_t id) noexcept {
    for (const items::ItemInstance& item : state.items) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

bool is_equipped(
    const items::ItemOwnershipState& state,
    std::uint64_t id) noexcept {
    for (const std::uint64_t equipped : state.equipment.equipped_ids) {
        if (equipped == id) return true;
    }
    return false;
}

bool contains_id(
    const std::array<std::uint64_t, 3>& ids,
    std::uint64_t id) noexcept {
    return ids[0] == id || ids[1] == id || ids[2] == id;
}

bool bit_is_set(
    const std::array<std::uint64_t, 3>& bits,
    std::uint16_t ordinal) noexcept {
    const std::size_t word = ordinal / 64U;
    const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
    return word < bits.size()
        && (bits[word] & (std::uint64_t{1U} << bit)) != 0U;
}

void set_bit(
    std::array<std::uint64_t, 3>& bits,
    std::uint16_t ordinal) noexcept {
    const std::size_t word = ordinal / 64U;
    const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
    if (word < bits.size()) bits[word] |= std::uint64_t{1U} << bit;
}

bool pickup_distance_ok(
    combat::Vec3 player,
    combat::Vec3 item) noexcept {
    const float x = player.x - item.x;
    const float y = player.y - item.y;
    return x * x + y * y <= kPickupRadius * kPickupRadius;
}

bool same_item(
    const items::ItemInstance& left,
    const items::ItemInstance& right) noexcept {
    if (left.id != right.id || left.base_id != right.base_id
            || left.rarity != right.rarity
            || left.item_level != right.item_level
            || left.required_level != right.required_level
            || left.affix_count != right.affix_count
            || left.reserved != right.reserved) {
        return false;
    }
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        const items::AffixRoll& a = left.affixes[index];
        const items::AffixRoll& b = right.affixes[index];
        if (a.affix_id != b.affix_id || a.tier != b.tier
                || a.variant != b.variant) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool DungeonSession::request_descent(bool player_in_range) noexcept {
    if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
            || !player_in_range || pending_save_.has_value()
            || !stable_state_.current_room.has_hole) {
        saturating_increment(diagnostics_.rejected_exit_count);
        return false;
    }
    return prepare_transition(TransitionKind::descent, ExitDirection::none);
}

void DungeonSession::resolve_pending_transition(
    const TransitionSaveResult& result) noexcept {
    if (!pending_save_.has_value()
            || pending_save_->kind != PendingSaveKind::transition) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    resolve_pending_save(result);
}

void DungeonSession::resolve_pending_save(
    const PendingSaveResult& result) noexcept {
    commit_pending_save(result);
}

void DungeonSession::attempt_exit(ExitDirection direction) noexcept {
    if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
            || direction == ExitDirection::none || pending_save_.has_value()) {
        saturating_increment(diagnostics_.rejected_exit_count);
        return;
    }

    last_exit_ = direction;
    static_cast<void>(prepare_transition(TransitionKind::door, direction));
}

bool DungeonSession::prepare_transition(
    TransitionKind kind,
    ExitDirection direction) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return false;
    }
    RunStateBuildResult next = kind == TransitionKind::descent
        ? make_descent_transition(stable_state_, rules_)
        : make_door_transition(stable_state_, direction, rules_);
    if (next.fault != DungeonFault::none) {
        if (next.fault == DungeonFault::room_index_overflow) {
            diagnostics_.room_index_overflow = true;
        }
        enter_fault(next.fault);
        return false;
    }

    next.state.progression = room_progression_;
    next.state.item_ownership.claimed_drop_bits = {};
    if (kind == TransitionKind::descent) {
        last_exit_ = ExitDirection::none;
    }
    const std::uint64_t expected_generation = next.state.commit_generation;
    pending_save_ = PendingSave{
        PendingSaveKind::transition,
        expected_generation,
        std::move(next.state),
        kind,
        direction,
        RoomPhase::awaiting_exit,
    };
    phase_ = RoomPhase::committing;
    const bool emitted = emit(
        DungeonEventKind::transition_requested,
        &stable_state_,
        &pending_save_->next_state,
        kind,
        direction);
    return emitted && phase_ == RoomPhase::committing;
}

bool DungeonSession::request_passive_allocation(
    passives::PassiveNodeId node) noexcept {
    return prepare_passive_mutation(node, false);
}

bool DungeonSession::request_passive_refund(
    passives::PassiveNodeId node) noexcept {
    return prepare_passive_mutation(node, true);
}

bool DungeonSession::prepare_passive_mutation(
    passives::PassiveNodeId node, bool refund) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return false;
    }
    if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
            || pending_save_.has_value()) {
        return false;
    }

    DungeonRunState next = stable_state_;
    next.progression = room_progression_;
    const passives::PassiveTreeResult mutation = refund
        ? passives::refund_node(next.passive_tree, next.progression, node)
        : passives::allocate_node(next.passive_tree, next.progression, node);
    last_passive_tree_error_ = mutation.error;
    if (!mutation.changed) {
        return false;
    }
    if (next.commit_generation == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return false;
    }
    ++next.commit_generation;
    pending_save_ = PendingSave{
        PendingSaveKind::passive_tree,
        next.commit_generation,
        next,
        TransitionKind::none,
        ExitDirection::none,
        RoomPhase::awaiting_exit,
    };
    phase_ = RoomPhase::committing;
    return true;
}

RequestResult DungeonSession::prepare_item_save(
    DungeonRunState&& next,
    PendingSaveKind kind,
    RoomPhase resume_phase) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    const items::OwnershipValidationResult validation =
        items::validate_ownership_detailed(next.item_ownership);
    if (validation == items::OwnershipValidationResult::allocation_failure) {
        return RequestResult::rejected;
    }
    if (validation != items::OwnershipValidationResult::valid
            || !passives::valid_passive_tree_state(
                next.passive_tree, next.progression)) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    const PlayerBuildResult candidate_build = build_for(next);
    if (candidate_build.status == PlayerBuildStatus::allocation_failure) {
        return RequestResult::rejected;
    }
    if (candidate_build.status != PlayerBuildStatus::valid) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    if (next.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return RequestResult::faulted;
    }
    ++next.commit_generation;
    const std::uint64_t expected_generation = next.commit_generation;
    pending_save_.emplace(PendingSave{
        kind,
        expected_generation,
        std::move(next),
        TransitionKind::none,
        ExitDirection::none,
        resume_phase,
    });
    pending_item_build_.emplace(candidate_build.build);
    phase_ = RoomPhase::committing;
    return RequestResult::accepted;
}

bool DungeonSession::pending_item_cache_consistent() const noexcept {
    const bool item_pending = pending_save_.has_value()
        && (pending_save_->kind == PendingSaveKind::equipment
            || pending_save_->kind == PendingSaveKind::recipe);
    return item_pending == pending_item_build_.has_value();
}

bool DungeonSession::pending_abyss_cache_consistent() const noexcept {
    const bool start_pending = pending_save_.has_value()
        && pending_save_->kind == PendingSaveKind::abyss_start;
    return start_pending == pending_abyss_combat_.has_value();
}

RequestResult DungeonSession::request_equip(std::uint64_t item_id) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    if (!item_request_phase(phase_) || !combat_.has_value()
            || pending_save_.has_value() || item_id == 0U) {
        return RequestResult::rejected;
    }
    const items::OwnershipValidationResult stable_validation =
        items::validate_ownership_detailed(stable_state_.item_ownership);
    if (stable_validation == items::OwnershipValidationResult::allocation_failure) {
        return RequestResult::rejected;
    }
    if (stable_validation != items::OwnershipValidationResult::valid) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    const items::ItemInstance* item = find_item(
        stable_state_.item_ownership, item_id);
    const items::BaseDefinition* base = item == nullptr
        ? nullptr : items::base_definition(item->base_id);
    if (item == nullptr || base == nullptr
            || item->required_level > room_progression_.level) {
        return RequestResult::rejected;
    }
    const std::size_t slot = static_cast<std::size_t>(base->slot);
    if (slot >= stable_state_.item_ownership.equipment.equipped_ids.size()
            || stable_state_.item_ownership.equipment.equipped_ids[slot]
                == item_id) {
        return RequestResult::rejected;
    }

    try {
        DungeonRunState next = stable_state_;
        next.progression = room_progression_;
        next.item_ownership.equipment.equipped_ids[slot] = item_id;
        return prepare_item_save(
            std::move(next), PendingSaveKind::equipment, phase_);
    } catch (const std::bad_alloc&) {
        return RequestResult::rejected;
    } catch (...) {
        return RequestResult::rejected;
    }
}

RequestResult DungeonSession::request_unequip(items::ItemSlot slot) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    const std::size_t slot_index = static_cast<std::size_t>(slot);
    if (!item_request_phase(phase_) || !combat_.has_value()
            || pending_save_.has_value()
            || slot_index >= static_cast<std::size_t>(items::ItemSlot::count)) {
        return RequestResult::rejected;
    }
    const items::OwnershipValidationResult stable_validation =
        items::validate_ownership_detailed(stable_state_.item_ownership);
    if (stable_validation == items::OwnershipValidationResult::allocation_failure) {
        return RequestResult::rejected;
    }
    if (stable_validation != items::OwnershipValidationResult::valid) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    if (stable_state_.item_ownership.equipment.equipped_ids[slot_index] == 0U) {
        return RequestResult::rejected;
    }

    try {
        DungeonRunState next = stable_state_;
        next.progression = room_progression_;
        next.item_ownership.equipment.equipped_ids[slot_index] = 0U;
        return prepare_item_save(
            std::move(next), PendingSaveKind::equipment, phase_);
    } catch (const std::bad_alloc&) {
        return RequestResult::rejected;
    } catch (...) {
        return RequestResult::rejected;
    }
}

RequestResult DungeonSession::request_recipe(
    const std::array<std::uint64_t, 3>& item_ids) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    if (!item_request_phase(phase_) || !combat_.has_value()
            || pending_save_.has_value() || item_ids[0] == 0U
            || item_ids[1] == 0U || item_ids[2] == 0U
            || item_ids[0] == item_ids[1] || item_ids[0] == item_ids[2]
            || item_ids[1] == item_ids[2]) {
        return RequestResult::rejected;
    }
    const items::OwnershipValidationResult stable_validation =
        items::validate_ownership_detailed(stable_state_.item_ownership);
    if (stable_validation == items::OwnershipValidationResult::allocation_failure) {
        return RequestResult::rejected;
    }
    if (stable_validation != items::OwnershipValidationResult::valid) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    const items::ItemInstance* a = find_item(
        stable_state_.item_ownership, item_ids[0]);
    const items::ItemInstance* b = find_item(
        stable_state_.item_ownership, item_ids[1]);
    const items::ItemInstance* c = find_item(
        stable_state_.item_ownership, item_ids[2]);
    if (a == nullptr || b == nullptr || c == nullptr
            || is_equipped(stable_state_.item_ownership, a->id)
            || is_equipped(stable_state_.item_ownership, b->id)
            || is_equipped(stable_state_.item_ownership, c->id)) {
        return RequestResult::rejected;
    }
    const items::BaseDefinition* a_base = items::base_definition(a->base_id);
    const items::BaseDefinition* b_base = items::base_definition(b->base_id);
    const items::BaseDefinition* c_base = items::base_definition(c->base_id);
    if (a_base == nullptr || b_base == nullptr || c_base == nullptr
            || a_base->slot != b_base->slot || a_base->slot != c_base->slot
            || a->rarity != b->rarity || a->rarity != c->rarity) {
        return RequestResult::rejected;
    }
    if (stable_state_.item_ownership.next_item_sequence
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::item_sequence_overflow);
        return RequestResult::faulted;
    }
    const auto product = items::generate_recipe_item(
        stable_state_.root_seed,
        stable_state_.item_ownership.next_item_sequence,
        *a, *b, *c);
    if (!product.has_value()) {
        enter_fault(DungeonFault::item_id_collision);
        return RequestResult::faulted;
    }
    if (find_item(stable_state_.item_ownership, product->id) != nullptr) {
        enter_fault(DungeonFault::item_id_collision);
        return RequestResult::faulted;
    }

    try {
        DungeonRunState next = stable_state_;
        next.progression = room_progression_;
        auto& owned = next.item_ownership.items;
        std::size_t write = 0U;
        for (std::size_t read = 0U; read < owned.size(); ++read) {
            if (!contains_id(item_ids, owned[read].id)) {
                if (write != read) owned[write] = owned[read];
                ++write;
            }
        }
        owned.resize(write);
        owned.push_back(*product);
        ++next.item_ownership.next_item_sequence;
        return prepare_item_save(
            std::move(next), PendingSaveKind::recipe, phase_);
    } catch (const std::bad_alloc&) {
        return RequestResult::rejected;
    } catch (...) {
        return RequestResult::rejected;
    }
}

RequestResult DungeonSession::request_pickup(
    std::uint16_t drop_ordinal) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    if (!item_request_phase(phase_) || !combat_.has_value()
            || pending_save_.has_value()
            || drop_ordinal >= ground_items_.size()) {
        return RequestResult::rejected;
    }
    const GroundItem& ground = ground_items_[drop_ordinal];
    if (!ground.active || ground.drop_ordinal != drop_ordinal
            || !pickup_distance_ok(
                combat_->snapshot().player.position, ground.position)) {
        return RequestResult::rejected;
    }
    const items::OwnershipValidationResult stable_validation =
        items::validate_ownership_detailed(stable_state_.item_ownership);
    if (stable_validation
            == items::OwnershipValidationResult::allocation_failure) {
        return RequestResult::rejected;
    }
    if (stable_validation != items::OwnershipValidationResult::valid
            || !items::validate_item(ground.item)
            || !passives::valid_passive_tree_state(
                stable_state_.passive_tree, room_progression_)) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    if (find_item(stable_state_.item_ownership, ground.item.id) != nullptr) {
        enter_fault(DungeonFault::item_id_collision);
        return RequestResult::faulted;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return RequestResult::faulted;
    }

    try {
        DungeonRunState next = stable_state_;
        next.progression = room_progression_;
        next.item_ownership.items.push_back(ground.item);
        set_bit(next.item_ownership.claimed_drop_bits, drop_ordinal);
        const items::OwnershipValidationResult validation =
            items::validate_ownership_detailed(next.item_ownership);
        if (validation == items::OwnershipValidationResult::allocation_failure) {
            return RequestResult::rejected;
        }
        if (validation != items::OwnershipValidationResult::valid) {
            enter_fault(DungeonFault::invalid_item_state);
            return RequestResult::faulted;
        }
        ++next.commit_generation;
        const std::uint64_t expected_generation = next.commit_generation;
        pending_save_.emplace(PendingSave{
            PendingSaveKind::loot_pickup,
            expected_generation,
            std::move(next),
            TransitionKind::none,
            ExitDirection::none,
            phase_,
            drop_ordinal,
        });
        phase_ = RoomPhase::committing;
        return RequestResult::accepted;
    } catch (const std::bad_alloc&) {
        return RequestResult::rejected;
    } catch (...) {
        return RequestResult::rejected;
    }
}

void DungeonSession::request_nearby_pickups(
    combat::Vec3 player_position) noexcept {
    if (!item_request_phase(phase_) || !combat_.has_value()
            || pending_save_.has_value()) {
        return;
    }
    for (std::uint16_t ordinal = 0U;
         ordinal < ground_items_.size(); ++ordinal) {
        const GroundItem& ground = ground_items_[ordinal];
        if (!ground.active
                || !pickup_distance_ok(player_position, ground.position)) {
            continue;
        }
        const RequestResult result = request_pickup(ordinal);
        if (result != RequestResult::rejected) return;
    }
}

void DungeonSession::commit_pending_save(
    const PendingSaveResult& result) noexcept {
    if (phase_ != RoomPhase::committing || !pending_save_.has_value()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (!pending_abyss_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (result.disposition == SaveDisposition::indeterminate) {
        enter_fault(DungeonFault::save_commit_indeterminate);
        return;
    }
    const bool abyss_commit = pending_save_->kind == PendingSaveKind::abyss_start
        || pending_save_->kind == PendingSaveKind::abyss_fail
        || pending_save_->kind == PendingSaveKind::abyss_clear;
    if (result.disposition == SaveDisposition::not_committed && abyss_commit) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (result.disposition == SaveDisposition::not_committed) {
        const RoomPhase resume_phase = pending_save_->resume_phase;
        phase_ = resume_phase;
        saturating_increment(diagnostics_.save_failure_count);
        static_cast<void>(emit(
            DungeonEventKind::save_failed,
            &stable_state_,
            &pending_save_->next_state,
            pending_save_->next_state.last_transition,
            pending_save_->next_state.last_direction));
        pending_save_.reset();
        pending_item_build_.reset();
        return;
    }
    if (result.generation != pending_save_->expected_generation
            || pending_save_->expected_generation
                != pending_save_->next_state.commit_generation
            || !same_run_state(
                result.verified_state, pending_save_->next_state)) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }

    const PendingSaveKind kind = pending_save_->kind;
    const bool item_commit = kind == PendingSaveKind::equipment
        || kind == PendingSaveKind::recipe;
    const bool pickup_commit = kind == PendingSaveKind::loot_pickup;
    const bool start_commit = kind == PendingSaveKind::abyss_start;
    const bool fail_commit = kind == PendingSaveKind::abyss_fail;
    const bool clear_commit = kind == PendingSaveKind::abyss_clear;
    const std::uint16_t pickup_ordinal = pending_save_->pickup_ordinal;
    if (pickup_commit) {
        if (pickup_ordinal >= ground_items_.size()) {
            enter_fault(DungeonFault::save_receipt_mismatch);
            return;
        }
        const GroundItem& ground = ground_items_[pickup_ordinal];
        const items::ItemInstance* published = find_item(
            pending_save_->next_state.item_ownership, ground.item.id);
        if (!ground.active || ground.drop_ordinal != pickup_ordinal
                || published == nullptr
                || !same_item(*published, ground.item)
                || !bit_is_set(
                    pending_save_->next_state.item_ownership.claimed_drop_bits,
                    pickup_ordinal)) {
            enter_fault(DungeonFault::save_receipt_mismatch);
            return;
        }
    }
    combat::PlayerCombatBuild published_build{};
    if (item_commit) {
        if (!combat_.has_value()) {
            enter_fault(DungeonFault::invalid_item_state);
            return;
        }
        published_build = *pending_item_build_;
    }

    const checkpoint::RoomDescriptor previous_room =
        stable_state_.current_room;
    stable_state_ = std::move(pending_save_->next_state);
    const RoomPhase resume_phase = pending_save_->resume_phase;
    pending_save_.reset();
    pending_item_build_.reset();
    if (start_commit) {
        combat_.emplace(*pending_abyss_combat_);
        pending_abyss_combat_.reset();
        room_progression_ = stable_state_.progression;
        phase_ = RoomPhase::locked;
        return;
    }
    pending_abyss_combat_.reset();
    if (fail_commit) {
        reset_to_normal_room(false);
        return;
    }
    if (clear_commit) {
        settle_room_experience();
        room_progression_ = stable_state_.progression;
        combat_->clear_abyss_rule_preserving_resources();
        publish_room_clear();
        return;
    }
    if (kind == PendingSaveKind::transition) {
        ground_items_ = {};
        rolled_drop_bits_ = {};
        combat_.reset();
        phase_ = RoomPhase::transitioning;
        emit_committed(previous_room, stable_state_);
        return;
    }
    room_progression_ = stable_state_.progression;
    if (item_commit) {
        combat_->apply_player_build(published_build);
        phase_ = resume_phase;
        return;
    }
    if (pickup_commit) {
        ground_items_[pickup_ordinal] = GroundItem{};
        phase_ = resume_phase;
        return;
    }
    phase_ = RoomPhase::awaiting_exit;
}

}  // namespace arpg::dungeon
