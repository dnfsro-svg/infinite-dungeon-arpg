#include "dungeon/dungeon_session.hpp"

#include "dungeon/room_generation.hpp"

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

DungeonSession::DungeonSession(DungeonSessionConfig config) noexcept
    : config_(config), current_room_(make_initial_room(config_)) {
    construct_current_room();
}

bool DungeonSession::queue_action(combat::Action action) noexcept {
    return phase_ == RoomPhase::combat && combat_.has_value()
        ? combat_->queue_action(action)
        : false;
}

void DungeonSession::tick(combat::MovementInput movement) noexcept {
    if (phase_ == RoomPhase::locked) {
        phase_ = RoomPhase::combat;
        emit(DungeonEventKind::room_entered);
        emit(DungeonEventKind::combat_started);
    } else if (phase_ != RoomPhase::transitioning && combat_.has_value()) {
        if (phase_ == RoomPhase::cleared) {
            phase_ = RoomPhase::awaiting_exit;
        } else {
            combat_->tick(movement);
            relay_combat_events();

            if (phase_ == RoomPhase::combat && remaining_targets() == 0U) {
                phase_ = RoomPhase::cleared;
                emit(DungeonEventKind::room_cleared);
                emit(DungeonEventKind::exits_opened);
            }
        }
    }

    ++session_tick_;
}

void DungeonSession::reset_current_room() noexcept {
    if (phase_ == RoomPhase::transitioning) {
        return;
    }

    while (events_.try_pop().has_value()) {
    }
    while (combat_events_.try_pop().has_value()) {
    }
    combat_.reset();
    pending_room_.reset();
    construct_current_room();
    emit(DungeonEventKind::room_reset);
}

DungeonSnapshot DungeonSession::snapshot() const noexcept {
    DungeonSnapshot result{};
    result.session_tick = session_tick_;
    result.room_index = current_room_.index;
    result.room_seed = current_room_.seed;
    result.phase = phase_;
    result.has_active_room = combat_.has_value();
    result.exits_open.fill(
        phase_ == RoomPhase::cleared || phase_ == RoomPhase::awaiting_exit);
    result.remaining_targets = remaining_targets();
    result.entry_side = current_room_.entry;
    result.last_exit = last_exit_;
    if (combat_.has_value()) {
        result.combat.emplace(combat_->snapshot());
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
    combat_.emplace(current_room_.combat);
    phase_ = RoomPhase::locked;
}

void DungeonSession::relay_combat_events() noexcept {
    if (!combat_.has_value()) {
        return;
    }
    while (auto event = combat_->try_pop_event()) {
        const bool relayed = combat_events_.try_push(*event);
        if (!relayed) {
            saturating_increment(diagnostics_.combat_relay_overflow_count);
            overflow_fault_emitted_ = true;
            assert(relayed && "Dungeon combat event relay overflow");
        }
    }
}

void DungeonSession::attempt_exit(ExitDirection direction) noexcept {
    static_cast<void>(direction);
}

void DungeonSession::emit(
    DungeonEventKind kind,
    ExitDirection direction) noexcept {
    const DungeonEvent event{
        kind,
        session_tick_,
        current_room_.index,
        current_room_.seed,
        direction,
    };
    const bool emitted = events_.try_push(event);
    if (!emitted) {
        saturating_increment(diagnostics_.event_overflow_count);
        overflow_fault_emitted_ = true;
        assert(emitted && "Dungeon event queue overflow");
    }
}

std::uint8_t DungeonSession::remaining_targets() const noexcept {
    if (!combat_.has_value()) {
        return 0U;
    }

    std::uint8_t remaining = 0U;
    const combat::CombatSnapshot state = combat_->snapshot();
    for (const auto& dummy : state.dummies) {
        if (dummy.hp > 0) {
            ++remaining;
        }
    }
    return remaining;
}

}  // namespace arpg::dungeon
