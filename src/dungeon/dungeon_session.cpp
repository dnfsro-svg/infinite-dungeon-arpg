#include "dungeon/dungeon_session.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_combat_template.hpp"
#include "dungeon/room_navigation.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

namespace arpg::dungeon {
namespace {

constexpr std::uint16_t kWaveDelayTicks = 45U;

void saturating_increment(std::uint32_t& value) noexcept {
    if (value != (std::numeric_limits<std::uint32_t>::max)()) {
        ++value;
    }
}

[[nodiscard]] DungeonRunState initial_state_for(
    const DungeonSessionConfig& config) noexcept {
    DungeonRunState state = make_initial_run_state(
        config.root_seed, DungeonRules{}).state;
    if (state.current_room.index != config.initial_room_index) {
        state.current_room.index = config.initial_room_index;
        state.current_room.seed = derive_initial_room_seed(
            config.root_seed, config.initial_room_index);
    }
    return state;
}

}  // namespace

DungeonSession::DungeonSession(DungeonSessionConfig config) noexcept
    : DungeonSession(DungeonRules{}, initial_state_for(config)) {}

DungeonSession::DungeonSession(
    DungeonRules rules,
    DungeonRunState stable_state) noexcept
    : rules_(rules), stable_state_(stable_state),
      last_exit_(stable_state.last_direction) {
    if (validate_rules(rules_) != DungeonFault::none) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    construct_current_room();
}

bool DungeonSession::queue_action(combat::Action action) noexcept {
    return phase_ == RoomPhase::combat && combat_.has_value()
        ? combat_->queue_action(action)
        : false;
}

void DungeonSession::tick(combat::MovementInput movement) noexcept {
    if (phase_ == RoomPhase::committing || phase_ == RoomPhase::faulted) {
        ++session_tick_;
        return;
    }

    if (phase_ == RoomPhase::locked) {
        phase_ = RoomPhase::combat;
        if (emit(DungeonEventKind::room_entered)
                && phase_ != RoomPhase::faulted) {
            static_cast<void>(emit(DungeonEventKind::combat_started));
        }
    } else if (phase_ == RoomPhase::transitioning) {
        construct_current_room();
    } else if (combat_.has_value()) {
        if (phase_ == RoomPhase::cleared) {
            phase_ = RoomPhase::awaiting_exit;
        } else if (phase_ == RoomPhase::wave_delay) {
            if (wave_delay_ticks_ != 0U) {
                --wave_delay_ticks_;
            }
            if (wave_delay_ticks_ == 0U) {
                start_next_wave();
            }
        } else if (phase_ == RoomPhase::combat
                || phase_ == RoomPhase::awaiting_exit) {
            combat_->tick(movement);
            relay_combat_events();

            if (phase_ == RoomPhase::combat && remaining_targets() == 0U) {
                if (wave_index_ + 1U < encounter_plan_.wave_count) {
                    phase_ = RoomPhase::wave_delay;
                    wave_delay_ticks_ = kWaveDelayTicks;
                } else {
                    phase_ = RoomPhase::cleared;
                    if (emit(DungeonEventKind::room_cleared)
                            && phase_ != RoomPhase::faulted) {
                        static_cast<void>(emit(DungeonEventKind::exits_opened));
                    }
                }
            }
        }

        if (phase_ == RoomPhase::awaiting_exit) {
            const combat::CombatSnapshot state = combat_->snapshot();
            if (const auto requested = requested_exit(
                    state.player.position, movement)) {
                attempt_exit(*requested);
            }
        }
    }

    ++session_tick_;
}

bool DungeonSession::request_descent(bool player_in_range) noexcept {
    if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
            || !player_in_range || pending_.has_value()
            || !stable_state_.current_room.has_hole) {
        saturating_increment(diagnostics_.rejected_exit_count);
        return false;
    }

    const RunStateBuildResult built = make_descent_transition(
        stable_state_, rules_);
    if (built.fault != DungeonFault::none) {
        if (built.fault == DungeonFault::room_index_overflow) {
            diagnostics_.room_index_overflow = true;
        }
        enter_fault(built.fault);
        return false;
    }

    pending_ = PendingTransition{
        TransitionKind::descent,
        ExitDirection::none,
        built.state.commit_generation,
        built.state,
    };
    last_exit_ = ExitDirection::none;
    phase_ = RoomPhase::committing;
    const bool emitted = emit(
        DungeonEventKind::transition_requested,
        &stable_state_,
        &pending_->next_state,
        TransitionKind::descent,
        ExitDirection::none);
    return emitted && phase_ == RoomPhase::committing;
}

std::optional<PendingTransition>
DungeonSession::pending_transition() const noexcept {
    return pending_;
}

void DungeonSession::resolve_pending_transition(
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

void DungeonSession::reset_current_room() noexcept {
    if (phase_ == RoomPhase::transitioning
            || phase_ == RoomPhase::committing
            || phase_ == RoomPhase::faulted) {
        return;
    }

    while (events_.try_pop().has_value()) {
    }
    while (combat_events_.try_pop().has_value()) {
    }
    combat_.reset();
    pending_.reset();
    construct_current_room();
    static_cast<void>(emit(DungeonEventKind::room_reset));
}

DungeonSnapshot DungeonSession::snapshot() const noexcept {
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
    result.has_pending_transition = pending_.has_value();
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
    return result;
}

std::optional<DungeonEvent> DungeonSession::try_pop_event() noexcept {
    return events_.try_pop();
}

std::optional<combat::CombatEvent>
DungeonSession::try_pop_combat_event() noexcept {
    return combat_events_.try_pop();
}

void DungeonSession::construct_current_room() noexcept {
    const EncounterPlanResult plan = build_encounter_plan(
        stable_state_.current_room.seed,
        stable_state_.current_room.depth,
        stable_state_.current_room.ecology,
        rules_.encounter);
    if (plan.fault != DungeonFault::none || plan.plan.wave_count == 0U) {
        enter_fault(plan.fault == DungeonFault::none
            ? DungeonFault::invalid_rules : plan.fault);
        return;
    }
    const auto config = make_combat_encounter_config(
        stable_state_.current_room.entry,
        rules_.rules_version,
        plan.plan.waves[0],
        true);
    if (!config.has_value()) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    encounter_plan_ = plan.plan;
    wave_index_ = 0U;
    wave_delay_ticks_ = 0U;
    combat_.emplace(*config);
    phase_ = RoomPhase::locked;
}

void DungeonSession::start_next_wave() noexcept {
    if (!combat_.has_value() || wave_index_ + 1U >= encounter_plan_.wave_count) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    const std::uint8_t next_wave = static_cast<std::uint8_t>(wave_index_ + 1U);
    if (!combat_->load_wave(encounter_plan_.waves[next_wave], false)) {
        enter_fault(DungeonFault::invalid_rules);
        return;
    }
    wave_index_ = next_wave;
    phase_ = RoomPhase::combat;
}

void DungeonSession::relay_combat_events() noexcept {
    if (!combat_.has_value()) {
        return;
    }
    while (auto event = combat_->try_pop_event()) {
        const bool relayed = combat_events_.try_push(*event);
        if (!relayed) {
            saturating_increment(diagnostics_.combat_relay_overflow_count);
            enter_fault(DungeonFault::combat_relay_overflow);
            assert(relayed && "Dungeon combat event relay overflow");
            return;
        }
    }
}

void DungeonSession::attempt_exit(ExitDirection direction) noexcept {
    if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
            || direction == ExitDirection::none || pending_.has_value()) {
        saturating_increment(diagnostics_.rejected_exit_count);
        return;
    }

    last_exit_ = direction;

    const RunStateBuildResult built = make_door_transition(
        stable_state_, direction, rules_);
    if (built.fault != DungeonFault::none) {
        if (built.fault == DungeonFault::room_index_overflow) {
            diagnostics_.room_index_overflow = true;
        }
        enter_fault(built.fault);
        return;
    }

    pending_ = PendingTransition{
        TransitionKind::door,
        direction,
        built.state.commit_generation,
        built.state,
    };
    phase_ = RoomPhase::committing;
    static_cast<void>(emit(
        DungeonEventKind::transition_requested,
        &stable_state_,
        &pending_->next_state,
        TransitionKind::door,
        direction));
}

void DungeonSession::enter_fault(DungeonFault fault) noexcept {
    if (fault == DungeonFault::none) {
        return;
    }
    diagnostics_.fault = fault;
    phase_ = RoomPhase::faulted;

    const DungeonEvent event{
        DungeonEventKind::faulted,
        session_tick_,
        stable_state_.current_room.index,
        stable_state_.current_room.seed,
        0U,
        0U,
        TransitionKind::none,
        last_exit_,
    };
    if (!events_.try_push(event)) {
        saturating_increment(diagnostics_.event_overflow_count);
    }
}

void DungeonSession::emit_committed(
    const DungeonRunState& previous,
    const DungeonRunState& current) noexcept {
    if (!emit(
        DungeonEventKind::transition_committed,
        &previous,
        &current,
        current.last_transition,
        current.last_direction)) {
        return;
    }
    static_cast<void>(emit(
        DungeonEventKind::room_destroyed,
        &previous,
        &current,
        current.last_transition,
        current.last_direction));
}

bool DungeonSession::emit(
    DungeonEventKind kind,
    const DungeonRunState* subject,
    const DungeonRunState* destination,
    TransitionKind transition,
    ExitDirection direction) noexcept {
    const DungeonRunState& subject_state = subject != nullptr
        ? *subject : stable_state_;
    const DungeonEvent event{
        kind,
        session_tick_,
        subject_state.current_room.index,
        subject_state.current_room.seed,
        destination != nullptr ? destination->current_room.index : 0U,
        destination != nullptr ? destination->current_room.seed : 0U,
        transition,
        direction,
    };
    const bool emitted = events_.try_push(event);
    if (!emitted) {
        saturating_increment(diagnostics_.event_overflow_count);
        enter_fault(DungeonFault::event_overflow);
        assert(emitted && "Dungeon event queue overflow");
    }
    return emitted;
}

std::uint8_t DungeonSession::remaining_targets() const noexcept {
    if (!combat_.has_value()) {
        return 0U;
    }
    std::uint8_t remaining = 0U;
    const combat::CombatSnapshot state = combat_->snapshot();
    for (const auto& monster : state.monsters) {
        if (monster.active && monster.hp > 0) {
            ++remaining;
        }
    }
    return remaining;
}

}  // namespace arpg::dungeon
