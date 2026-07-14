#include "dungeon/dungeon_session.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "passives/passive_tree_rules.hpp"

#include <cstdint>
#include <limits>

namespace arpg::dungeon {
namespace {

void saturating_increment(std::uint32_t& value) noexcept {
    if (value != (std::numeric_limits<std::uint32_t>::max)()) {
        ++value;
    }
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
    const RunStateBuildResult built = kind == TransitionKind::descent
        ? make_descent_transition(stable_state_, rules_)
        : make_door_transition(stable_state_, direction, rules_);
    if (built.fault != DungeonFault::none) {
        if (built.fault == DungeonFault::room_index_overflow) {
            diagnostics_.room_index_overflow = true;
        }
        enter_fault(built.fault);
        return false;
    }

    RunStateBuildResult next = built;
    next.state.progression = room_progression_;
    if (kind == TransitionKind::descent) {
        last_exit_ = ExitDirection::none;
    }
    pending_save_ = PendingSave{
        PendingSaveKind::transition,
        next.state.commit_generation,
        next.state,
        kind,
        direction,
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
    };
    phase_ = RoomPhase::committing;
    return true;
}

void DungeonSession::commit_pending_save(
    const PendingSaveResult& result) noexcept {
    if (phase_ != RoomPhase::committing || !pending_save_.has_value()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (result.disposition == SaveDisposition::indeterminate) {
        enter_fault(DungeonFault::save_commit_indeterminate);
        return;
    }
    if (result.disposition == SaveDisposition::not_committed) {
        const DungeonRunState next = pending_save_->next_state;
        pending_save_.reset();
        phase_ = RoomPhase::awaiting_exit;
        saturating_increment(diagnostics_.save_failure_count);
        static_cast<void>(emit(
            DungeonEventKind::save_failed,
            &stable_state_,
            &next,
            next.last_transition,
            next.last_direction));
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

    const DungeonRunState previous = stable_state_;
    stable_state_ = result.verified_state;
    const PendingSaveKind kind = pending_save_->kind;
    pending_save_.reset();
    if (kind == PendingSaveKind::transition) {
        combat_.reset();
        phase_ = RoomPhase::transitioning;
        emit_committed(previous, stable_state_);
        return;
    }
    room_progression_ = stable_state_.progression;
    phase_ = RoomPhase::awaiting_exit;
}

}  // namespace arpg::dungeon
