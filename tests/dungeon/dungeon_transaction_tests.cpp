#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"

#include <cstdint>

namespace {

using arpg::combat::MovementInput;
using arpg::dungeon::DungeonEventKind;
using arpg::dungeon::DungeonFault;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;

DungeonRunState initial_state(
    std::uint64_t seed = 0xA11CE5EEDULL) noexcept {
    return arpg::dungeon::make_initial_run_state(seed, DungeonRules{}).state;
}

bool clear_and_await(DungeonSession& session) noexcept {
    arpg::test::EventSummary summary;
    if (!arpg::test::drive_until_cleared(session, summary)) {
        return false;
    }
    if (session.snapshot().phase == RoomPhase::cleared) {
        session.tick(MovementInput{});
    }
    return session.snapshot().phase == RoomPhase::awaiting_exit;
}

bool drive_to_open_door(
    DungeonSession& session,
    ExitDirection direction) noexcept {
    if (!clear_and_await(session)) {
        return false;
    }
    MovementInput movement{};
    switch (direction) {
    case ExitDirection::up:
        movement = {0, -1};
        break;
    case ExitDirection::down:
        movement = {0, 1};
        break;
    case ExitDirection::left:
        movement = {-1, 0};
        break;
    case ExitDirection::right:
        movement = {1, 0};
        break;
    case ExitDirection::none:
        return false;
    }
    for (int tick = 0; tick < 256; ++tick) {
        const auto state = session.snapshot();
        if (!state.combat.has_value()) {
            return false;
        }
        if (direction == ExitDirection::left
                || direction == ExitDirection::right) {
            movement.y = state.combat->player.position.y > 0.1F ? -1
                : (state.combat->player.position.y < -0.1F ? 1 : 0);
        } else {
            movement.x = state.combat->player.position.x > 0.1F ? -1
                : (state.combat->player.position.x < -0.1F ? 1 : 0);
        }
        if (movement.x == 0 && movement.y == 0) {
            break;
        }
        session.tick(movement);
        arpg::test::EventSummary ignored;
        arpg::test::drain_all_events(session, ignored);
    }
    for (int tick = 0; tick < 256; ++tick) {
        session.tick(movement);
        arpg::test::EventSummary ignored;
        arpg::test::drain_all_events(session, ignored);
        if (session.snapshot().phase == RoomPhase::committing) {
            return true;
        }
    }
    return false;
}

arpg::test::Failure constructor_uses_saved_room_descriptor() noexcept {
    DungeonRules rules;
    DungeonRunState saved = initial_state(0x5555U);
    saved.current_room.index = 37U;
    saved.current_room.seed = 0x123456789ABCDEF0ULL;
    saved.current_room.depth = 9U;
    saved.current_room.floor_room_index = 4U;
    saved.current_room.entry = arpg::dungeon::EntrySide::left;
    saved.current_room.ecology = arpg::dungeon::DungeonElement::chaos;
    saved.current_room.has_hole = true;
    saved.current_room.is_abyss = true;

    DungeonSession session{rules, saved};
    const DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.room_index == saved.current_room.index);
    ARPG_REQUIRE(state.room_seed == saved.current_room.seed);
    ARPG_REQUIRE(state.depth == saved.current_room.depth);
    ARPG_REQUIRE(state.floor_room_index == saved.current_room.floor_room_index);
    ARPG_REQUIRE(state.entry_side == saved.current_room.entry);
    ARPG_REQUIRE(state.ecology == saved.current_room.ecology);
    ARPG_REQUIRE(state.has_hole == saved.current_room.has_hole);
    ARPG_REQUIRE(state.is_abyss == saved.current_room.is_abyss);
    return {};
}

arpg::test::Failure door_request_freezes_old_combat_until_commit() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    const DungeonSnapshot before = session.snapshot();
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->expected_generation
        == pending->next_state.commit_generation);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    ARPG_REQUIRE(session.snapshot().combat.has_value());
    ARPG_REQUIRE(session.snapshot().room_index == before.room_index);
    const auto frozen = session.snapshot().combat->tick;
    session.tick({0, -1});
    ARPG_REQUIRE(session.snapshot().combat->tick == frozen);
    return {};
}

arpg::test::Failure exact_commit_adopts_verified_state_and_destroys_old_room() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    session.resolve_pending_transition({
        SaveDisposition::committed,
        pending->next_state.commit_generation,
        pending->next_state,
    });
    const DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.phase == RoomPhase::transitioning);
    ARPG_REQUIRE(!state.combat.has_value());
    ARPG_REQUIRE(state.room_index == pending->next_state.current_room.index);
    ARPG_REQUIRE(state.room_seed == pending->next_state.current_room.seed);
    ARPG_REQUIRE(!session.pending_transition().has_value());
    return {};
}

arpg::test::Failure not_committed_discards_pending_and_retries_identically() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::right));
    const auto first = session.pending_transition();
    ARPG_REQUIRE(first.has_value());
    session.resolve_pending_transition({SaveDisposition::not_committed, 0U, {}});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(!session.pending_transition().has_value());
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::right));
    const auto second = session.pending_transition();
    ARPG_REQUIRE(second.has_value());
    ARPG_REQUIRE(second->kind == first->kind);
    ARPG_REQUIRE(second->direction == first->direction);
    ARPG_REQUIRE(second->expected_generation == first->expected_generation);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        second->next_state, first->next_state));
    return {};
}

arpg::test::Failure indeterminate_faults_and_blocks_all_actions() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    session.resolve_pending_transition({SaveDisposition::indeterminate, 0U, {}});
    const DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.phase == RoomPhase::faulted);
    ARPG_REQUIRE(state.diagnostics.fault == DungeonFault::save_commit_indeterminate);
    ARPG_REQUIRE(!session.queue_action(arpg::combat::Action::light));
    ARPG_REQUIRE(!session.request_descent(true));
    session.tick({1, 0});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    return {};
}

arpg::test::Failure generation_mismatch_faults_with_receipt_mismatch() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    session.resolve_pending_transition({
        SaveDisposition::committed,
        pending->expected_generation + 1U,
        pending->next_state,
    });
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure verified_state_mismatch_faults_with_receipt_mismatch() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    DungeonRunState wrong = pending->next_state;
    ++wrong.current_room.index;
    session.resolve_pending_transition({
        SaveDisposition::committed,
        pending->expected_generation,
        wrong,
    });
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure descent_requires_clear_hole_range_and_edge() noexcept {
    DungeonRunState saved = initial_state();
    saved.current_room.has_hole = true;
    DungeonSession session{DungeonRules{}, saved};
    ARPG_REQUIRE(!session.request_descent(false));
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::locked);
    session.tick({});
    ARPG_REQUIRE(!session.request_descent(false));
    ARPG_REQUIRE(!session.request_descent(true));
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(clear_and_await(session));
    ARPG_REQUIRE(session.request_descent(true));
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    ARPG_REQUIRE(session.pending_transition()->kind
        == arpg::dungeon::TransitionKind::descent);
    return {};
}

arpg::test::Failure queue_overflow_faults_without_release_propagation() noexcept {
#if defined(NDEBUG)
    DungeonRules rules;
    rules.hole_threshold = 10000U;
    DungeonRunState saved = arpg::dungeon::make_initial_run_state(
        0xF00DULL, rules).state;
    saved.current_room.has_hole = true;
    DungeonSession session{rules, saved};
    for (int room = 0; room < 16
            && session.snapshot().phase != RoomPhase::faulted; ++room) {
        for (int tick = 0; tick < 4096
                && session.snapshot().phase != RoomPhase::cleared
                && session.snapshot().phase != RoomPhase::faulted; ++tick) {
            const auto state = session.snapshot();
            MovementInput movement{};
            if (state.phase == RoomPhase::combat && state.combat.has_value()) {
                const auto* target = arpg::test::nearest_living_dummy(
                    *state.combat);
                if (target != nullptr) {
                    movement = arpg::test::movement_toward(
                        state.combat->player.position, target->position);
                    if (arpg::test::in_light_attack_lane(
                            state.combat->player, *target)
                            && state.combat->player.active_attack
                                == arpg::combat::AttackId::none) {
                        static_cast<void>(session.queue_action(
                            arpg::combat::Action::light));
                    }
                }
            }
            session.tick(movement);
        }
        if (session.snapshot().phase == RoomPhase::cleared) {
            session.tick({});
        }
        if (session.snapshot().phase != RoomPhase::awaiting_exit) {
            break;
        }
        if (!session.request_descent(true)
                || !arpg::test::commit_pending(session)) {
            break;
        }
        session.tick({});
        session.tick({});
    }
    const DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.diagnostics.event_overflow_count > 0U
        || state.diagnostics.combat_relay_overflow_count > 0U);
    ARPG_REQUIRE(state.phase == RoomPhase::faulted);
#else
    // The overflow branch intentionally asserts in Debug; Release is the
    // build where this no-propagation contract is exercised.
    ARPG_REQUIRE(true);
#endif
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"constructor uses saved room descriptor", &constructor_uses_saved_room_descriptor},
    {"door request freezes old combat until commit", &door_request_freezes_old_combat_until_commit},
    {"exact commit adopts verified state and destroys old room", &exact_commit_adopts_verified_state_and_destroys_old_room},
    {"not committed discards pending and retries identically", &not_committed_discards_pending_and_retries_identically},
    {"indeterminate faults and blocks all actions", &indeterminate_faults_and_blocks_all_actions},
    {"generation mismatch faults with receipt mismatch", &generation_mismatch_faults_with_receipt_mismatch},
    {"verified state mismatch faults with receipt mismatch", &verified_state_mismatch_faults_with_receipt_mismatch},
    {"descent requires clear hole range and edge", &descent_requires_clear_hole_range_and_edge},
    {"queue overflow faults without release propagation", &queue_overflow_faults_without_release_propagation},
};

}  // namespace

arpg::test::TestSuite dungeon_transaction_suite() noexcept {
    return arpg::test::make_suite("dungeon_transaction", kCases);
}
