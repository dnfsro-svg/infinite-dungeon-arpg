#include "dungeon/dungeon_session.hpp"

#include "dungeon/dungeon_progression.hpp"

#include <cassert>
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
            || !player_in_range || pending_.has_value()
            || !stable_state_.current_room.has_hole) {
        saturating_increment(diagnostics_.rejected_exit_count);
        return false;
    }
    return prepare_transition(TransitionKind::descent, ExitDirection::none);
}

void DungeonSession::resolve_pending_transition(
    const TransitionSaveResult& result) noexcept {
    commit_transition(result);
}

void DungeonSession::attempt_exit(ExitDirection direction) noexcept {
    if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
            || direction == ExitDirection::none || pending_.has_value()) {
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
    pending_ = PendingTransition{
        kind,
        direction,
        next.state.commit_generation,
        next.state,
    };
    phase_ = RoomPhase::committing;
    const bool emitted = emit(
        DungeonEventKind::transition_requested,
        &stable_state_,
        &pending_->next_state,
        kind,
        direction);
    return emitted && phase_ == RoomPhase::committing;
}

void DungeonSession::commit_transition(
    const TransitionSaveResult& result) noexcept {
    if (phase_ != RoomPhase::committing || !pending_.has_value()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }
    if (result.disposition == SaveDisposition::indeterminate) {
        enter_fault(DungeonFault::save_commit_indeterminate);
        return;
    }
    if (result.disposition == SaveDisposition::not_committed) {
        const DungeonRunState next = pending_->next_state;
        pending_.reset();
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
    if (result.generation != pending_->expected_generation
            || pending_->expected_generation
                != pending_->next_state.commit_generation
            || !same_run_state(
                result.verified_state, pending_->next_state)) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return;
    }

    const DungeonRunState previous = stable_state_;
    stable_state_ = result.verified_state;
    combat_.reset();
    phase_ = RoomPhase::transitioning;
    emit_committed(previous, stable_state_);
    pending_.reset();
}

}  // namespace arpg::dungeon
