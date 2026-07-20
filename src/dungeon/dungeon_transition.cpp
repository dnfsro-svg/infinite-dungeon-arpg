#include "dungeon/dungeon_session.hpp"

#include "combat/room_bounds.hpp"
#include "core/deterministic_rng.hpp"
#include "dungeon/abyss_reward.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "items/item_catalog.hpp"
#include "items/item_crafting.hpp"
#include "items/item_generation.hpp"
#include "passives/passive_tree_rules.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <new>
#include <utility>

namespace arpg::dungeon {
namespace {

inline constexpr std::uint64_t kReinforcementDomain =
    0x5245494E464F5243ULL;

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

bool reinforcement_succeeds(std::uint64_t root_seed,
    std::uint64_t item_id,
    std::uint32_t current,
    std::uint64_t nonce,
    std::uint16_t chance_bp) noexcept {
    const std::uint64_t stream = kReinforcementDomain
        ^ item_id
        ^ (static_cast<std::uint64_t>(current) << 32U)
        ^ nonce;
    core::DeterministicRng rng = core::DeterministicRng::derive_stream(
        root_seed, stream);
    return rng.next_bounded(10000U).value_or(9999U) < chance_bp;
}

template <std::size_t Size>
bool bit_is_set(
    const std::array<std::uint64_t, Size>& bits,
    std::uint16_t ordinal) noexcept {
    const std::size_t word = ordinal / 64U;
    const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
    return word < bits.size()
        && (bits[word] & (std::uint64_t{1U} << bit)) != 0U;
}

template <std::size_t Size>
void set_bit(
    std::array<std::uint64_t, Size>& bits,
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
            || left.reserved != right.reserved
            || left.reinforcement != right.reinforcement
            || left.extension_reserved != right.extension_reserved) {
        return false;
    }
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        const items::AffixRoll& a = left.affixes[index];
        const items::AffixRoll& b = right.affixes[index];
        if (a.affix_id != b.affix_id || a.tier != b.tier
                || a.variant != b.variant
                || a.value_roll_bp != b.value_roll_bp) {
            return false;
        }
    }
    return true;
}

std::uint8_t popcount8(std::uint8_t value) noexcept {
    std::uint8_t count = 0U;
    while (value != 0U) {
        count = static_cast<std::uint8_t>(count + (value & 1U));
        value = static_cast<std::uint8_t>(value >> 1U);
    }
    return count;
}

}  // namespace

bool DungeonSession::request_descent(bool player_in_range) noexcept {
    if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
            || !player_in_range || pending_save_.has_value()
            || !stable_state_.current_room.has_hole) {
        saturating_increment(diagnostics_.rejected_exit_count);
        return false;
    }
    const bool abandon = abyss_pending_reward_count() != 0U
        || abyss_unpicked_reward_count() != 0U;
    if (!confirm_abyss_exit(
            TransitionKind::descent, ExitDirection::none)) {
        return false;
    }
    return prepare_transition(
        TransitionKind::descent, ExitDirection::none, abandon);
}

void DungeonSession::resolve_pending_transition(
    const TransitionSaveResult& result) noexcept {
    if (!pending_save_.has_value()
            || (pending_save_->kind != PendingSaveKind::transition
                && pending_save_->kind != PendingSaveKind::abyss_abandon)) {
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
    const bool abandon = abyss_pending_reward_count() != 0U
        || abyss_unpicked_reward_count() != 0U;
    if (!confirm_abyss_exit(TransitionKind::door, direction)) return;
    static_cast<void>(prepare_transition(
        TransitionKind::door, direction, abandon));
}

std::uint8_t DungeonSession::abyss_pending_reward_count() const noexcept {
    if (!stable_state_.current_room.is_abyss
            || stable_state_.abyss.lifecycle
                != abyss::AbyssLifecycle::cleared
            || stable_state_.abyss.reward_total == 0U
            || stable_state_.abyss.reward_total > 3U) {
        return 0U;
    }
    const std::uint8_t valid = static_cast<std::uint8_t>(
        (1U << stable_state_.abyss.reward_total) - 1U);
    return popcount8(static_cast<std::uint8_t>(valid
        & static_cast<std::uint8_t>(~stable_state_.abyss.generated_mask)
        & static_cast<std::uint8_t>(~stable_state_.abyss.abandoned_mask)));
}

std::uint8_t DungeonSession::abyss_unpicked_reward_count() const noexcept {
    if (!stable_state_.current_room.is_abyss
            || stable_state_.abyss.lifecycle
                != abyss::AbyssLifecycle::cleared
            || stable_state_.abyss.reward_total == 0U
            || stable_state_.abyss.reward_total > 3U) {
        return 0U;
    }
    const std::uint8_t valid = static_cast<std::uint8_t>(
        (1U << stable_state_.abyss.reward_total) - 1U);
    return popcount8(static_cast<std::uint8_t>(valid
        & stable_state_.abyss.generated_mask
        & static_cast<std::uint8_t>(~stable_state_.abyss.claimed_mask)));
}

void DungeonSession::clear_abyss_exit_confirmation() noexcept {
    abyss_exit_confirmation_ = {};
}

bool DungeonSession::confirm_abyss_exit(
    TransitionKind kind, ExitDirection direction) noexcept {
    const std::uint8_t pending = abyss_pending_reward_count();
    const std::uint8_t unpicked = abyss_unpicked_reward_count();
    if (pending == 0U && unpicked == 0U) {
        clear_abyss_exit_confirmation();
        return true;
    }

    if (abyss_exit_confirmation_.armed) {
        if (abyss_exit_confirmation_.reward_revision
                    != stable_state_.abyss.reward_revision
                || abyss_exit_confirmation_.transition != kind
                || abyss_exit_confirmation_.direction != direction) {
            clear_abyss_exit_confirmation();
            return false;
        }
        if (kind == TransitionKind::door
                && !abyss_exit_confirmation_.door_input_released) {
            return false;
        }
        clear_abyss_exit_confirmation();
        return true;
    }

    if (!can_emit(1U)) {
        enter_fault(DungeonFault::event_overflow);
        return false;
    }
    const DungeonEvent warning{
        DungeonEventKind::abyss_exit_warning,
        session_tick_,
        stable_state_.current_room.index,
        stable_state_.current_room.seed,
        0U,
        0U,
        kind,
        direction,
        pending,
        unpicked,
    };
    if (!events_.try_push(warning)) {
        saturating_increment(diagnostics_.event_overflow_count);
        enter_fault(DungeonFault::event_overflow);
        return false;
    }
    abyss_exit_confirmation_ = {
        true, kind, direction, false, stable_state_.abyss.reward_revision};
    return false;
}

void DungeonSession::update_abyss_exit_confirmation_range(
    combat::Vec3 player_position) noexcept {
    if (!abyss_exit_confirmation_.armed) return;
    if (abyss_exit_confirmation_.reward_revision
            != stable_state_.abyss.reward_revision) {
        clear_abyss_exit_confirmation();
        return;
    }

    bool in_range = false;
    if (abyss_exit_confirmation_.transition == TransitionKind::descent) {
        constexpr combat::Vec3 kHoleCenter{0.0F, 3.5F, 0.0F};
        constexpr float kHoleRadius = 3.25F;
        const float x = player_position.x - kHoleCenter.x;
        const float y = player_position.y - kHoleCenter.y;
        in_range = x * x + y * y <= kHoleRadius * kHoleRadius;
    } else if (abyss_exit_confirmation_.transition == TransitionKind::door) {
        constexpr float kBoundaryTolerance = 0.01F;
        constexpr float kSideDoorHalfWidth = 0.90F;
        constexpr float kVerticalDoorHalfWidth = 1.50F;
        switch (abyss_exit_confirmation_.direction) {
        case ExitDirection::left:
            in_range = std::fabs(player_position.x - combat::room_bounds::min_x)
                    <= kBoundaryTolerance
                && std::fabs(player_position.y) <= kSideDoorHalfWidth;
            break;
        case ExitDirection::right:
            in_range = std::fabs(player_position.x - combat::room_bounds::max_x)
                    <= kBoundaryTolerance
                && std::fabs(player_position.y) <= kSideDoorHalfWidth;
            break;
        case ExitDirection::up:
            in_range = std::fabs(player_position.y - combat::room_bounds::min_y)
                    <= kBoundaryTolerance
                && std::fabs(player_position.x) <= kVerticalDoorHalfWidth;
            break;
        case ExitDirection::down:
            in_range = std::fabs(player_position.y - combat::room_bounds::max_y)
                    <= kBoundaryTolerance
                && std::fabs(player_position.x) <= kVerticalDoorHalfWidth;
            break;
        case ExitDirection::none:
            break;
        }
    }
    if (!in_range) {
        clear_abyss_exit_confirmation();
    }
}

bool DungeonSession::finalize_abyss_exit(
    DungeonRunState& next, bool abandon) const noexcept {
    if (!stable_state_.current_room.is_abyss
            || stable_state_.abyss.lifecycle
                != abyss::AbyssLifecycle::cleared) {
        return !abandon;
    }
    const auto& reward = stable_state_.abyss;
    if (reward.reward_total == 0U || reward.reward_total > 3U) return false;
    const std::uint8_t valid = static_cast<std::uint8_t>(
        (1U << reward.reward_total) - 1U);
    std::uint8_t abandoned = reward.abandoned_mask;
    if (abandon) {
        abandoned |= static_cast<std::uint8_t>(valid
            & static_cast<std::uint8_t>(~reward.generated_mask));
    }
    next.last_abyss_resolution.valid = true;
    next.last_abyss_resolution.room_seed = stable_state_.current_room.seed;
    next.last_abyss_resolution.rule = reward.rule;
    next.last_abyss_resolution.total = reward.reward_total;
    next.last_abyss_resolution.generated = popcount8(
        static_cast<std::uint8_t>(reward.generated_mask & valid));
    next.last_abyss_resolution.claimed = popcount8(
        static_cast<std::uint8_t>(reward.claimed_mask & valid));
    next.last_abyss_resolution.abandoned = popcount8(
        static_cast<std::uint8_t>(abandoned & valid));
    return true;
}

bool DungeonSession::prepare_transition(
    TransitionKind kind,
    ExitDirection direction,
    bool abandon_abyss) noexcept {
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
    next.state.item_ownership.material_claimed_drop_bits = {};
    if (!finalize_abyss_exit(next.state, abandon_abyss)) {
        enter_fault(DungeonFault::invalid_abyss_state);
        return false;
    }
    if (kind == TransitionKind::descent) {
        last_exit_ = ExitDirection::none;
    }
    const std::uint64_t expected_generation = next.state.commit_generation;
    pending_save_ = PendingSave{
        abandon_abyss ? PendingSaveKind::abyss_abandon
                      : PendingSaveKind::transition,
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
    RoomPhase resume_phase,
    std::optional<ReinforcementReceipt> reinforcement_receipt) noexcept {
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
    try {
        const std::size_t item_count = next.item_ownership.items.size();
        stable_state_.item_ownership.items.reserve(item_count);
        death_validation_scratch_.item_ownership.items.reserve(item_count);
    } catch (...) {
        return RequestResult::rejected;
    }
    ++next.commit_generation;
    const std::uint64_t expected_generation = next.commit_generation;
    if (reinforcement_receipt.has_value()) {
        reinforcement_receipt->commit_generation = expected_generation;
    }
    pending_save_.emplace(PendingSave{
        kind,
        expected_generation,
        std::move(next),
        TransitionKind::none,
        ExitDirection::none,
        resume_phase,
        0xFFFFU,
        std::nullopt,
        reinforcement_receipt,
    });
    pending_item_build_.emplace(candidate_build.build);
    phase_ = RoomPhase::committing;
    return RequestResult::accepted;
}

bool DungeonSession::pending_item_cache_consistent() const noexcept {
    const bool item_pending = pending_save_.has_value()
        && (pending_save_->kind == PendingSaveKind::equipment
            || pending_save_->kind == PendingSaveKind::craft
            || pending_save_->kind == PendingSaveKind::recipe
            || pending_save_->kind == PendingSaveKind::reinforcement);
    return item_pending == pending_item_build_.has_value();
}

bool DungeonSession::pending_abyss_cache_consistent() const noexcept {
    const bool start_pending = pending_save_.has_value()
        && pending_save_->kind == PendingSaveKind::abyss_start;
    return start_pending == pending_abyss_combat_.has_value();
}

bool DungeonSession::pending_abyss_reward_cache_consistent() const noexcept {
    const bool reward_pending = pending_save_.has_value()
        && pending_save_->kind
            == PendingSaveKind::abyss_reward_materialized;
    if (reward_pending != pending_abyss_reward_.has_value()) return false;
    if (!reward_pending) return true;

    const PendingAbyssReward& cache = *pending_abyss_reward_;
    if (stable_state_.abyss.lifecycle != abyss::AbyssLifecycle::cleared
            || cache.ground_index >= ground_items_.size()
            || ground_items_[cache.ground_index].active
            || cache.reward_ordinal >= stable_state_.abyss.reward_total
            || cache.reward_ordinal >= 3U
            || !cache.ground.active
            || cache.ground.drop_ordinal != cache.ground_index
            || cache.ground.source != GroundItemSource::abyss_chest
            || cache.ground.abyss_reward_ordinal != cache.reward_ordinal) {
        return false;
    }
    const std::uint8_t bit = static_cast<std::uint8_t>(
        1U << cache.reward_ordinal);
    const auto& next = pending_save_->next_state;
    if ((stable_state_.abyss.generated_mask & bit) != 0U
            || (stable_state_.abyss.abandoned_mask & bit) != 0U
            || next.abyss.generated_mask
                != static_cast<std::uint8_t>(
                    stable_state_.abyss.generated_mask | bit)
            || next.abyss.claimed_mask != stable_state_.abyss.claimed_mask
            || next.abyss.abandoned_mask != stable_state_.abyss.abandoned_mask
            || next.abyss.lifecycle != stable_state_.abyss.lifecycle
            || next.abyss.danger != stable_state_.abyss.danger
            || next.abyss.rule != stable_state_.abyss.rule
            || next.abyss.rules_version != stable_state_.abyss.rules_version
            || next.abyss.reward_total != stable_state_.abyss.reward_total
            || stable_state_.abyss.reward_revision
                == (std::numeric_limits<std::uint32_t>::max)()
            || next.abyss.reward_revision
                != stable_state_.abyss.reward_revision + 1U) {
        return false;
    }
    const std::uint8_t base_item_level = static_cast<std::uint8_t>(
        (std::min)(stable_state_.current_room.depth, std::uint64_t{100U}));
    const auto expected = derive_abyss_ground_item(
        stable_state_.current_room.seed, stable_state_.abyss.danger,
        base_item_level, cache.reward_ordinal, cache.ground_index);
    if (!expected.has_value()
            || !same_ground_item(*expected, cache.ground))
        return false;
    return !item_id_in_use(stable_state_.item_ownership, ground_items_,
        expected->item.id);
}

bool DungeonSession::pending_abyss_claim_cache_consistent() const noexcept {
    const bool claim_pending = pending_save_.has_value()
        && pending_save_->kind == PendingSaveKind::abyss_reward_claim;
    if (!claim_pending) return true;

    const std::uint16_t ground_index = pending_save_->pickup_ordinal;
    if (stable_state_.abyss.lifecycle != abyss::AbyssLifecycle::cleared
            || ground_index >= ground_items_.size()) {
        return false;
    }
    const GroundItem& ground = ground_items_[ground_index];
    if (!ground.active || ground.drop_ordinal != ground_index
            || ground.source != GroundItemSource::abyss_chest
            || ground.abyss_reward_ordinal >= stable_state_.abyss.reward_total
            || ground.abyss_reward_ordinal >= 3U) {
        return false;
    }
    const std::uint8_t bit = static_cast<std::uint8_t>(
        1U << ground.abyss_reward_ordinal);
    const auto& next = pending_save_->next_state;
    if ((stable_state_.abyss.generated_mask & bit) == 0U
            || (stable_state_.abyss.claimed_mask & bit) != 0U
            || (stable_state_.abyss.abandoned_mask & bit) != 0U
            || next.abyss.generated_mask != stable_state_.abyss.generated_mask
            || next.abyss.claimed_mask
                != static_cast<std::uint8_t>(
                    stable_state_.abyss.claimed_mask | bit)
            || next.abyss.abandoned_mask != stable_state_.abyss.abandoned_mask
            || next.abyss.lifecycle != stable_state_.abyss.lifecycle
            || next.abyss.danger != stable_state_.abyss.danger
            || next.abyss.rule != stable_state_.abyss.rule
            || next.abyss.rules_version != stable_state_.abyss.rules_version
            || next.abyss.reward_total != stable_state_.abyss.reward_total
            || stable_state_.abyss.reward_revision
                == (std::numeric_limits<std::uint32_t>::max)()
            || next.abyss.reward_revision
                != stable_state_.abyss.reward_revision + 1U
            || next.item_ownership.next_item_sequence
                != stable_state_.item_ownership.next_item_sequence
            || next.item_ownership.equipment.equipped_ids
                != stable_state_.item_ownership.equipment.equipped_ids
            || next.item_ownership.claimed_drop_bits
                != stable_state_.item_ownership.claimed_drop_bits
            || next.item_ownership.items.size()
                != stable_state_.item_ownership.items.size() + 1U) {
        return false;
    }
    for (std::size_t index = 0U;
         index < stable_state_.item_ownership.items.size(); ++index) {
        if (!same_item(next.item_ownership.items[index],
                stable_state_.item_ownership.items[index])) {
            return false;
        }
    }
    if (!same_item(next.item_ownership.items.back(), ground.item)) {
        return false;
    }
    const std::uint8_t base_item_level = static_cast<std::uint8_t>(
        (std::min)(stable_state_.current_room.depth, std::uint64_t{100U}));
    const auto expected = derive_abyss_ground_item(
        stable_state_.current_room.seed, stable_state_.abyss.danger,
        base_item_level, ground.abyss_reward_ordinal, ground_index);
    return expected.has_value() && same_ground_item(*expected, ground)
        && !item_id_in_use(stable_state_.item_ownership, ground_items_,
            ground.item.id, ground_index);
}

bool DungeonSession::pending_material_cache_consistent() const noexcept {
    if (!pending_save_.has_value()) return true;
    const PendingSaveKind kind = pending_save_->kind;
    const bool pickup = kind == PendingSaveKind::material_pickup;
    const bool vacuum = kind == PendingSaveKind::room_clear
        || kind == PendingSaveKind::abyss_clear;
    if (!pickup && !vacuum) return true;

    const auto& stable = stable_state_.item_ownership;
    const auto& next = pending_save_->next_state.item_ownership;
    if (next.items.size() != stable.items.size()
            || next.equipment.equipped_ids != stable.equipment.equipped_ids
            || next.claimed_drop_bits != stable.claimed_drop_bits
            || next.next_item_sequence != stable.next_item_sequence) {
        return false;
    }
    for (std::size_t index = 0U; index < stable.items.size(); ++index) {
        if (!same_item(next.items[index], stable.items[index])) return false;
    }

    auto expected_counts = stable.materials;
    auto expected_claims = stable.material_claimed_drop_bits;
    std::uint16_t expected_discovery = stable.material_discovery_bits;
    const auto append_ground = [&](const GroundMaterial& ground) noexcept {
        const std::size_t material_index = items::material_index(
            ground.material);
        if (!ground.active || ground.ordinal >= kGroundMaterialCapacity
                || material_index >= items::kMaterialCount
                || bit_is_set(expected_claims, ground.ordinal)
                || expected_counts[material_index]
                    == (std::numeric_limits<std::uint64_t>::max)()) {
            return false;
        }
        ++expected_counts[material_index];
        expected_discovery |= static_cast<std::uint16_t>(
            std::uint16_t{1U} << material_index);
        set_bit(expected_claims, ground.ordinal);
        return true;
    };

    if (pickup) {
        const std::uint16_t ordinal = pending_save_->pickup_ordinal;
        if (ordinal >= ground_materials_.size()
                || !append_ground(ground_materials_[ordinal])) {
            return false;
        }
    } else {
        for (const GroundMaterial& ground : ground_materials_) {
            if (ground.active && !append_ground(ground)) return false;
        }
    }
    return next.materials == expected_counts
        && next.material_discovery_bits == expected_discovery
        && next.material_claimed_drop_bits == expected_claims;
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

RequestResult DungeonSession::request_craft(
    items::MaterialId material,
    std::uint64_t item_id,
    std::optional<items::DirectedCategory> directed_category) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    const std::size_t material_index = items::material_index(material);
    if (!item_request_phase(phase_) || !combat_.has_value()
            || pending_save_.has_value() || item_id == 0U
            || material_index >= items::kMaterialCount
            || !items::material_is_crafting_currency(material)) {
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
    const items::ItemInstance* const item = find_item(
        stable_state_.item_ownership, item_id);
    if (item == nullptr
            || stable_state_.item_ownership.materials[material_index] == 0U) {
        return RequestResult::rejected;
    }
    const items::CraftResult crafted = items::craft_item({
        stable_state_.root_seed, stable_state_.commit_generation,
        *item, material, directed_category});
    if (!crafted.applied || !crafted.consumed || crafted.item.id != item_id) {
        return RequestResult::rejected;
    }
    try {
        DungeonRunState next = stable_state_;
        next.progression = room_progression_;
        bool replaced = false;
        for (items::ItemInstance& owned : next.item_ownership.items) {
            if (owned.id == item_id) {
                owned = crafted.item;
                replaced = true;
                break;
            }
        }
        if (!replaced) return RequestResult::rejected;
        --next.item_ownership.materials[material_index];
        return prepare_item_save(
            std::move(next), PendingSaveKind::craft, phase_);
    } catch (const std::bad_alloc&) {
        return RequestResult::rejected;
    } catch (...) {
        return RequestResult::rejected;
    }
}

RequestResult DungeonSession::request_reinforcement(
    std::uint64_t item_id) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    const std::size_t stone = items::material_index(
        items::MaterialId::reinforcement_stone);
    if (!item_request_phase(phase_) || !combat_.has_value()
            || pending_save_.has_value() || item_id == 0U
            || stone >= items::kMaterialCount
            || stable_state_.item_ownership.materials[stone] == 0U) {
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
    const items::ItemInstance* const item = find_item(
        stable_state_.item_ownership, item_id);
    if (item == nullptr) return RequestResult::rejected;
    const items::ReinforcementPreview preview =
        items::reinforcement_preview(*item);
    if (!preview.can_attempt) return RequestResult::rejected;

    const bool success = reinforcement_succeeds(stable_state_.root_seed,
        item->id, item->reinforcement, stable_state_.commit_generation,
        preview.success_chance_bp);
    ReinforcementReceipt receipt{true, false, success, false, 0U,
        item->id, item->reinforcement, item->reinforcement,
        preview.success_chance_bp, items::MaterialId::reinforcement_stone};
    try {
        DungeonRunState next = stable_state_;
        next.progression = room_progression_;
        auto& owned = next.item_ownership.items;
        if (success) {
            for (items::ItemInstance& candidate : owned) {
                if (candidate.id == item_id) {
                    candidate.reinforcement = preview.target;
                    receipt.after = candidate.reinforcement;
                    break;
                }
            }
        } else {
            const items::ReinforcementFailure failure =
                items::reinforcement_failure(item->reinforcement);
            if (failure == items::ReinforcementFailure::destroy) {
                std::size_t write = 0U;
                for (std::size_t read = 0U; read < owned.size(); ++read) {
                    if (owned[read].id == item_id) continue;
                    if (write != read) owned[write] = owned[read];
                    ++write;
                }
                owned.resize(write);
                for (std::uint64_t& equipped
                        : next.item_ownership.equipment.equipped_ids) {
                    if (equipped == item_id) equipped = 0U;
                }
                receipt.destroyed = true;
                receipt.after = 0U;
            } else {
                const std::uint32_t after = failure
                        == items::ReinforcementFailure::reset_six ? 6U
                    : failure == items::ReinforcementFailure::reset_zero ? 0U
                    : item->reinforcement;
                for (items::ItemInstance& candidate : owned) {
                    if (candidate.id == item_id) {
                        candidate.reinforcement = after;
                        receipt.after = after;
                        break;
                    }
                }
            }
        }
        --next.item_ownership.materials[stone];
        return prepare_item_save(std::move(next),
            PendingSaveKind::reinforcement, phase_, receipt);
    } catch (const std::bad_alloc&) {
        return RequestResult::rejected;
    } catch (...) {
        return RequestResult::rejected;
    }
}

RequestResult DungeonSession::request_coupon(
    items::MaterialId coupon, std::uint64_t item_id) noexcept {
    if (!pending_item_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    const std::size_t coupon_index = items::material_index(coupon);
    if (!item_request_phase(phase_) || !combat_.has_value()
            || pending_save_.has_value() || item_id == 0U
            || coupon_index >= items::kMaterialCount
            || !items::material_is_coupon(coupon)
            || stable_state_.item_ownership.materials[coupon_index] == 0U) {
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
    const items::ItemInstance* const item = find_item(
        stable_state_.item_ownership, item_id);
    if (item == nullptr) return RequestResult::rejected;
    const auto upgraded = items::apply_coupon(*item, coupon);
    if (!upgraded.has_value()) return RequestResult::rejected;

    ReinforcementReceipt receipt{true, true, true, false, 0U,
        item_id, item->reinforcement, upgraded->reinforcement, 10000U, coupon};
    try {
        DungeonRunState next = stable_state_;
        next.progression = room_progression_;
        bool replaced = false;
        for (items::ItemInstance& candidate : next.item_ownership.items) {
            if (candidate.id != item_id) continue;
            candidate = *upgraded;
            replaced = true;
            break;
        }
        if (!replaced) return RequestResult::rejected;
        --next.item_ownership.materials[coupon_index];
        return prepare_item_save(std::move(next),
            PendingSaveKind::reinforcement, phase_, receipt);
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
            || a->base_id != b->base_id || a->base_id != c->base_id
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
    if (item_id_in_use(stable_state_.item_ownership,
            ground_items_, product->id)) {
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
    if (!item_request_phase(phase_) || pending_save_.has_value()
            || drop_ordinal >= ground_items_.size()) {
        return RequestResult::rejected;
    }
    const GroundItem& ground = ground_items_[drop_ordinal];
    if (!ground.active || ground.drop_ordinal != drop_ordinal) {
        return RequestResult::rejected;
    }
    if (ground.source != GroundItemSource::monster_drop
            && ground.source != GroundItemSource::abyss_chest) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    const bool abyss_claim = ground.source == GroundItemSource::abyss_chest;
    if (!combat_.has_value() || !pickup_distance_ok(
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
    if (item_id_in_use(stable_state_.item_ownership, ground_items_,
            ground.item.id, drop_ordinal)) {
        enter_fault(DungeonFault::item_id_collision);
        return RequestResult::faulted;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return RequestResult::faulted;
    }

    std::uint8_t abyss_bit = 0U;
    if (abyss_claim) {
        if (!stable_state_.current_room.is_abyss
                || stable_state_.abyss.lifecycle
                    != abyss::AbyssLifecycle::cleared
                || ground.abyss_reward_ordinal >= stable_state_.abyss.reward_total
                || ground.abyss_reward_ordinal >= 3U) {
            enter_fault(DungeonFault::invalid_abyss_state);
            return RequestResult::faulted;
        }
        abyss_bit = static_cast<std::uint8_t>(
            1U << ground.abyss_reward_ordinal);
        if ((stable_state_.abyss.generated_mask & abyss_bit) == 0U
                || (stable_state_.abyss.claimed_mask & abyss_bit) != 0U
                || (stable_state_.abyss.abandoned_mask & abyss_bit) != 0U) {
            enter_fault(DungeonFault::invalid_abyss_state);
            return RequestResult::faulted;
        }
        const std::uint8_t base_item_level = static_cast<std::uint8_t>(
            (std::min)(stable_state_.current_room.depth, std::uint64_t{100U}));
        const auto expected = derive_abyss_ground_item(
            stable_state_.current_room.seed, stable_state_.abyss.danger,
            base_item_level, ground.abyss_reward_ordinal, drop_ordinal);
        if (!expected.has_value() || !same_ground_item(*expected, ground)) {
            enter_fault(DungeonFault::invalid_abyss_state);
            return RequestResult::faulted;
        }
        if (stable_state_.abyss.reward_revision
                == (std::numeric_limits<std::uint32_t>::max)()) {
            enter_fault(DungeonFault::abyss_reward_revision_overflow);
            return RequestResult::faulted;
        }
        if (stable_state_.item_ownership.items.size() >= 65535U) {
            return RequestResult::rejected;
        }
    }

    if (abyss_claim) clear_abyss_exit_confirmation();
    try {
        DungeonRunState next = stable_state_;
        next.progression = room_progression_;
        next.item_ownership.items.push_back(ground.item);
        if (abyss_claim) {
            next.abyss.claimed_mask |= abyss_bit;
            ++next.abyss.reward_revision;
        } else {
            set_bit(next.item_ownership.claimed_drop_bits, drop_ordinal);
        }
        const items::OwnershipValidationResult validation =
            items::validate_ownership_detailed(next.item_ownership);
        if (validation == items::OwnershipValidationResult::allocation_failure) {
            return RequestResult::rejected;
        }
        if (validation != items::OwnershipValidationResult::valid) {
            enter_fault(DungeonFault::invalid_item_state);
            return RequestResult::faulted;
        }
        const std::size_t item_count = next.item_ownership.items.size();
        stable_state_.item_ownership.items.reserve(item_count);
        death_validation_scratch_.item_ownership.items.reserve(item_count);
        ++next.commit_generation;
        const std::uint64_t expected_generation = next.commit_generation;
        pending_save_.emplace(PendingSave{
            abyss_claim ? PendingSaveKind::abyss_reward_claim
                        : PendingSaveKind::loot_pickup,
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

RequestResult DungeonSession::request_material_pickup(
    std::uint16_t ordinal) noexcept {
    if (!pending_item_cache_consistent()
            || !pending_material_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    if (!item_request_phase(phase_) || pending_save_.has_value()
            || ordinal >= ground_materials_.size()) {
        return RequestResult::rejected;
    }
    const GroundMaterial& ground = ground_materials_[ordinal];
    const std::size_t material_index = items::material_index(ground.material);
    if (!ground.active || ground.ordinal != ordinal) {
        return RequestResult::rejected;
    }
    if (material_index >= items::kMaterialCount
            || bit_is_set(
                stable_state_.item_ownership.material_claimed_drop_bits,
                ordinal)) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    if (!combat_.has_value() || !pickup_distance_ok(
            combat_->snapshot().player.position, ground.position)) {
        return RequestResult::rejected;
    }
    if (stable_state_.item_ownership.materials[material_index]
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::invalid_item_state);
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
        ++next.item_ownership.materials[material_index];
        next.item_ownership.material_discovery_bits |=
            static_cast<std::uint16_t>(
                std::uint16_t{1U} << material_index);
        set_bit(next.item_ownership.material_claimed_drop_bits, ordinal);
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
        pending_save_.emplace(PendingSave{
            PendingSaveKind::material_pickup,
            next.commit_generation,
            std::move(next),
            TransitionKind::none,
            ExitDirection::none,
            phase_,
            ordinal,
        });
        phase_ = RoomPhase::committing;
        return RequestResult::accepted;
    } catch (const std::bad_alloc&) {
        return RequestResult::rejected;
    } catch (...) {
        return RequestResult::rejected;
    }
}

bool auto_pickup_eligible(
    const GroundItem& ground,
    AutoPickupPolicy policy) noexcept {
    if (!ground.active) return false;
    if (ground.source == GroundItemSource::abyss_chest) return true;
    return ground.source == GroundItemSource::monster_drop
        && static_cast<std::uint8_t>(ground.item.rarity)
            >= static_cast<std::uint8_t>(policy.minimum_rarity);
}

void DungeonSession::request_nearby_pickups(
    combat::Vec3 player_position,
    AutoPickupPolicy pickup_policy) noexcept {
    if (!item_request_phase(phase_) || !combat_.has_value()
            || pending_save_.has_value()) {
        return;
    }
    for (std::uint16_t ordinal = 0U;
         ordinal < ground_items_.size(); ++ordinal) {
        const GroundItem& ground = ground_items_[ordinal];
        if (!auto_pickup_eligible(ground, pickup_policy)
                || !pickup_distance_ok(player_position, ground.position)) {
            continue;
        }
        const RequestResult result = request_pickup(ordinal);
        if (result != RequestResult::rejected) return;
    }
    if (phase_ != RoomPhase::combat && phase_ != RoomPhase::wave_delay) {
        return;
    }
    for (std::uint16_t ordinal = 0U;
         ordinal < ground_materials_.size(); ++ordinal) {
        const GroundMaterial& ground = ground_materials_[ordinal];
        if (!ground.active
                || !pickup_distance_ok(player_position, ground.position)) {
            continue;
        }
        const RequestResult result = request_material_pickup(ordinal);
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
    if (!pending_abyss_reward_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (!pending_abyss_claim_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (!pending_material_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (!pending_death_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    const bool death_pending = pending_save_->kind
            == PendingSaveKind::death_retreat
        || pending_save_->kind == PendingSaveKind::death_continue;
    if (death_pending && (!result.kind.has_value()
            || *result.kind != pending_save_->kind)) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (result.disposition == SaveDisposition::indeterminate) {
        enter_fault(DungeonFault::save_commit_indeterminate);
        return;
    }
    const bool abyss_commit = pending_save_->kind == PendingSaveKind::abyss_start
        || pending_save_->kind == PendingSaveKind::abyss_fail
        || pending_save_->kind == PendingSaveKind::abyss_clear
        || pending_save_->kind
            == PendingSaveKind::abyss_reward_materialized;
    const bool claim_pending = pending_save_->kind
        == PendingSaveKind::abyss_reward_claim;
    const bool abandon_pending = pending_save_->kind
        == PendingSaveKind::abyss_abandon;
    const bool protected_abyss_commit = abyss_commit || claim_pending
        || abandon_pending;
    if (result.disposition == SaveDisposition::not_committed
            && protected_abyss_commit) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (result.disposition == SaveDisposition::not_committed) {
        const RoomPhase resume_phase = pending_save_->resume_phase;
        death_continue_failed_ = pending_save_->kind
            == PendingSaveKind::death_continue;
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
        || kind == PendingSaveKind::craft
        || kind == PendingSaveKind::recipe
        || kind == PendingSaveKind::reinforcement;
    const bool reinforcement_commit = kind == PendingSaveKind::reinforcement;
    const bool pickup_commit = kind == PendingSaveKind::loot_pickup;
    const bool material_pickup_commit =
        kind == PendingSaveKind::material_pickup;
    const bool claim_commit = kind == PendingSaveKind::abyss_reward_claim;
    const bool start_commit = kind == PendingSaveKind::abyss_start;
    const bool fail_commit = kind == PendingSaveKind::abyss_fail;
    const bool abyss_clear_commit = kind == PendingSaveKind::abyss_clear;
    const bool room_clear_commit = kind == PendingSaveKind::room_clear;
    const bool clear_commit = abyss_clear_commit || room_clear_commit;
    const bool reward_commit = kind
        == PendingSaveKind::abyss_reward_materialized;
    const bool death_retreat_commit = kind == PendingSaveKind::death_retreat;
    const bool death_continue_commit = kind == PendingSaveKind::death_continue;
    const bool death_commit = death_retreat_commit || death_continue_commit;
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
    if (material_pickup_commit) {
        if (pickup_ordinal >= ground_materials_.size()
                || !ground_materials_[pickup_ordinal].active
                || ground_materials_[pickup_ordinal].ordinal
                    != pickup_ordinal) {
            enter_fault(DungeonFault::save_receipt_mismatch);
            return;
        }
    }
    ReinforcementReceipt published_reinforcement_receipt{};
    if (reinforcement_commit) {
        if (!pending_save_->reinforcement_receipt.has_value()) {
            enter_fault(DungeonFault::save_receipt_mismatch);
            return;
        }
        published_reinforcement_receipt = *pending_save_->reinforcement_receipt;
        const std::size_t material_index = items::material_index(
            published_reinforcement_receipt.material);
        const items::ItemInstance* const published = find_item(
            pending_save_->next_state.item_ownership,
            published_reinforcement_receipt.item_id);
        if (!published_reinforcement_receipt.valid
                || published_reinforcement_receipt.commit_generation
                    != pending_save_->expected_generation
                || material_index >= items::kMaterialCount
                || !((published_reinforcement_receipt.coupon
                        && items::material_is_coupon(
                            published_reinforcement_receipt.material))
                    || (!published_reinforcement_receipt.coupon
                        && published_reinforcement_receipt.material
                            == items::MaterialId::reinforcement_stone))
                || (published_reinforcement_receipt.destroyed
                    ? published != nullptr
                    : published == nullptr
                        || published->reinforcement
                            != published_reinforcement_receipt.after)) {
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
    PendingAbyssReward published_reward{};
    if (reward_commit) published_reward = *pending_abyss_reward_;
    MaterialPickupReceipt published_material_receipt{};
    if (material_pickup_commit || clear_commit) {
        published_material_receipt.valid = true;
        published_material_receipt.room_vacuum = clear_commit;
        published_material_receipt.commit_generation =
            pending_save_->next_state.commit_generation;
        for (std::size_t index = 0U;
                index < published_material_receipt.counts.size(); ++index) {
            published_material_receipt.counts[index] =
                pending_save_->next_state.item_ownership.materials[index]
                - stable_state_.item_ownership.materials[index];
        }
    }
    if (death_commit && !can_emit(1U)) {
        enter_fault(DungeonFault::event_overflow);
        return;
    }

    const checkpoint::RoomDescriptor previous_room =
        stable_state_.current_room;
    publish_run_state_reusing_items(
        stable_state_, pending_save_->next_state);
    const RoomPhase resume_phase = pending_save_->resume_phase;
    pending_save_.reset();
    pending_item_build_.reset();
    pending_abyss_reward_.reset();
    if (start_commit) {
        combat_.emplace(*pending_abyss_combat_);
        pending_abyss_combat_.reset();
        room_progression_ = stable_state_.progression;
        phase_ = RoomPhase::locked;
        return;
    }
    pending_abyss_combat_.reset();
    if (death_retreat_commit) {
        clear_transient_room_state();
        phase_ = RoomPhase::death_pending;
        static_cast<void>(emit(
            DungeonEventKind::death_retreat_committed,
            &stable_state_, nullptr,
            TransitionKind::death_retreat,
            ExitDirection::none));
        return;
    }
    if (death_continue_commit) {
        clear_transient_room_state();
        phase_ = RoomPhase::transitioning;
        static_cast<void>(emit(
            DungeonEventKind::death_continued,
            &stable_state_, nullptr,
            TransitionKind::death_retreat,
            ExitDirection::none));
        return;
    }
    if (fail_commit) {
        reset_to_normal_room(false);
        return;
    }
    if (clear_commit) {
        settle_room_experience();
        room_progression_ = stable_state_.progression;
        ground_materials_ = {};
        material_pickup_receipt_ = published_material_receipt;
        if (abyss_clear_commit) {
            combat_->clear_abyss_rule_preserving_resources();
        }
        // prepare_room_clear reserved both publication slots. While committing,
        // tick() is frozen and no other dungeon-event producer can consume them.
        publish_room_clear();
        return;
    }
    if (reward_commit) {
        ground_items_[published_reward.ground_index] = published_reward.ground;
        phase_ = resume_phase;
        return;
    }
    if (kind == PendingSaveKind::transition
            || kind == PendingSaveKind::abyss_abandon) {
        clear_abyss_exit_confirmation();
        ground_items_ = {};
        rolled_drop_bits_ = {};
        ground_materials_ = {};
        rolled_material_bits_ = {};
        combat_.reset();
        phase_ = RoomPhase::transitioning;
        emit_committed(previous_room, stable_state_);
        return;
    }
    room_progression_ = stable_state_.progression;
    if (item_commit) {
        combat_->apply_player_build(published_build);
        if (reinforcement_commit) {
            reinforcement_receipt_ = published_reinforcement_receipt;
        }
        phase_ = resume_phase;
        return;
    }
    if (pickup_commit || claim_commit) {
        ground_items_[pickup_ordinal] = GroundItem{};
        phase_ = resume_phase;
        return;
    }
    if (material_pickup_commit) {
        ground_materials_[pickup_ordinal] = GroundMaterial{};
        material_pickup_receipt_ = published_material_receipt;
        phase_ = resume_phase;
        return;
    }
    phase_ = RoomPhase::awaiting_exit;
}

}  // namespace arpg::dungeon
