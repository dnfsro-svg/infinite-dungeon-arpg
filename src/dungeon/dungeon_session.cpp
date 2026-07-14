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

void saturating_add(std::uint64_t& value, std::uint64_t addition) noexcept {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    value = addition > maximum - value ? maximum : value + addition;
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
                    settle_room_experience();
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

std::optional<PendingTransition>
DungeonSession::pending_transition() const noexcept {
    return pending_;
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

std::optional<DungeonEvent> DungeonSession::try_pop_event() noexcept {
    return events_.try_pop();
}

std::optional<combat::CombatEvent>
DungeonSession::try_pop_combat_event() noexcept {
    return combat_events_.try_pop();
}

void DungeonSession::construct_current_room() noexcept {
    room_progression_ = stable_state_.progression;
    pending_room_experience_ = 0U;
    last_room_experience_ = 0U;
    last_levels_gained_ = 0U;
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
        if (event->kind == combat::CombatEventKind::defeated
            && event->target_index < combat_->snapshot().monsters.size()) {
            const combat::MonsterId id = combat_->snapshot()
                .monsters[event->target_index].id;
            const std::size_t monster_index = static_cast<std::size_t>(id);
            if (monster_index < progression_rules_.monster_experience.size()) {
                saturating_add(pending_room_experience_,
                    progression_rules_.monster_experience[monster_index]);
            }
        }
        const bool relayed = combat_events_.try_push(*event);
        if (!relayed) {
            saturating_increment(diagnostics_.combat_relay_overflow_count);
            enter_fault(DungeonFault::combat_relay_overflow);
            assert(relayed && "Dungeon combat event relay overflow");
            return;
        }
    }
}

void DungeonSession::settle_room_experience() noexcept {
    saturating_add(pending_room_experience_,
        progression_rules_.room_clear_experience);
    last_room_experience_ = pending_room_experience_;
    const progression::ProgressionAward award = progression::apply_experience(
        room_progression_, pending_room_experience_, progression_rules_);
    room_progression_ = award.state;
    last_levels_gained_ = award.levels_gained;
    pending_room_experience_ = 0U;
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
