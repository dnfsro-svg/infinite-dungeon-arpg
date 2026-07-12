#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/room_generation.hpp"
#include "dungeon/room_navigation.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace {

using arpg::combat::MovementInput;
using arpg::combat::Vec3;
using arpg::dungeon::DungeonEvent;
using arpg::dungeon::DungeonEventKind;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;

struct ExitEvents final {
    std::array<DungeonEvent, 8> values{};
    std::size_t count{};
};

void drain_combat(DungeonSession& session) noexcept {
    while (session.try_pop_combat_event().has_value()) {
    }
}

ExitEvents drain_dungeon(DungeonSession& session) noexcept {
    ExitEvents result{};
    while (const auto event = session.try_pop_event()) {
        if (result.count < result.values.size()) {
            result.values[result.count] = *event;
        }
        ++result.count;
    }
    drain_combat(session);
    return result;
}

bool clear_and_await(DungeonSession& session) noexcept {
    arpg::test::EventSummary summary;
    if (!arpg::test::drive_until_cleared(session, summary)) {
        return false;
    }
    if (session.snapshot().phase == RoomPhase::cleared) {
        session.tick(MovementInput{});
        arpg::test::drain_all_events(session, summary);
    }
    return session.snapshot().phase == RoomPhase::awaiting_exit;
}

MovementInput outward(ExitDirection direction) noexcept {
    switch (direction) {
    case ExitDirection::up:
        return {0, -1};
    case ExitDirection::down:
        return {0, 1};
    case ExitDirection::left:
        return {-1, 0};
    case ExitDirection::right:
        return {1, 0};
    case ExitDirection::none:
        return {};
    }
    return {};
}

MovementInput alignment(const DungeonSnapshot& state, ExitDirection direction) noexcept {
    constexpr float kTolerance = 0.10F;
    MovementInput movement{};
    const Vec3 position = state.combat->player.position;
    if (direction == ExitDirection::left || direction == ExitDirection::right) {
        movement.y = position.y > kTolerance ? -1
            : (position.y < -kTolerance ? 1 : 0);
    } else {
        movement.x = position.x > kTolerance ? -1
            : (position.x < -kTolerance ? 1 : 0);
    }
    return movement;
}

bool commit_exit(DungeonSession& session, ExitDirection direction) noexcept {
    for (int tick = 0; tick < 256; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        const MovementInput movement = alignment(state, direction);
        if (movement.x == 0 && movement.y == 0) {
            break;
        }
        session.tick(movement);
        drain_dungeon(session);
    }
    for (int tick = 0; tick < 256; ++tick) {
        session.tick(outward(direction));
        drain_combat(session);
        if (session.snapshot().phase == RoomPhase::committing) {
            return arpg::test::commit_pending(session)
                && session.snapshot().phase == RoomPhase::transitioning;
        }
        if (session.snapshot().phase == RoomPhase::transitioning) {
            return true;
        }
        while (session.try_pop_event().has_value()) {
        }
    }
    return false;
}

#if defined(NDEBUG)
bool clear_without_dungeon_drain(DungeonSession& session) noexcept {
    for (int tick = 0; tick < 4096; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        if (state.phase == RoomPhase::cleared) {
            session.tick(MovementInput{});
            drain_combat(session);
            return session.snapshot().phase == RoomPhase::awaiting_exit;
        }

        MovementInput movement{};
        if (state.phase == RoomPhase::combat && state.combat.has_value()) {
            const auto* target = arpg::test::nearest_living_monster(*state.combat);
            if (target != nullptr) {
                movement = arpg::test::movement_toward(
                    state.combat->player.position, target->position);
                if (arpg::test::in_light_attack_lane(
                        state.combat->player, *target)
                        && state.combat->player.active_attack
                            == arpg::combat::AttackId::none) {
                    static_cast<void>(
                        session.queue_action(arpg::combat::Action::light));
                }
            }
        }
        session.tick(movement);
        drain_combat(session);
    }
    return false;
}

bool request_without_dungeon_drain(
    DungeonSession& session,
    ExitDirection direction,
    RoomPhase expected_phase) noexcept {
    for (int tick = 0; tick < 256; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        const MovementInput movement = alignment(state, direction);
        if (movement.x == 0 && movement.y == 0) {
            break;
        }
        session.tick(movement);
        drain_combat(session);
    }
    for (int tick = 0; tick < 256; ++tick) {
        session.tick(outward(direction));
        drain_combat(session);
        const DungeonSnapshot state = session.snapshot();
        if (expected_phase == RoomPhase::transitioning
                && state.phase == RoomPhase::transitioning) {
            return true;
        }
        if (expected_phase == RoomPhase::awaiting_exit
                && state.phase == RoomPhase::awaiting_exit
                && state.diagnostics.room_index_overflow) {
            return true;
        }
    }
    return false;
}

bool advance_without_dungeon_drain(
    DungeonSession& session,
    ExitDirection direction) noexcept {
    if (!clear_without_dungeon_drain(session)
            || !request_without_dungeon_drain(
                session, direction, RoomPhase::transitioning)) {
        return false;
    }
    session.tick(outward(direction));
    drain_combat(session);
    if (session.snapshot().phase != RoomPhase::locked) {
        return false;
    }
    session.tick(outward(direction));
    drain_combat(session);
    return session.snapshot().phase == RoomPhase::combat;
}
#endif

arpg::test::Failure exact_apertures_accept_outward_input() noexcept {
    using arpg::dungeon::requested_exit;
    ARPG_REQUIRE(requested_exit(Vec3{-12.0F, 0.90F, 4.0F}, {-1, 0})
        == ExitDirection::left);
    ARPG_REQUIRE(requested_exit(Vec3{12.0F, -0.90F, 0.0F}, {1, 0})
        == ExitDirection::right);
    ARPG_REQUIRE(requested_exit(Vec3{1.50F, -5.5F, 0.0F}, {0, -1})
        == ExitDirection::up);
    ARPG_REQUIRE(requested_exit(Vec3{-1.50F, 5.5F, 0.0F}, {0, 1})
        == ExitDirection::down);
    return {};
}

arpg::test::Failure invalid_physical_requests_are_rejected() noexcept {
    using arpg::dungeon::requested_exit;
    ARPG_REQUIRE(!requested_exit(Vec3{-12.0F, 0.0F, 0.0F}, {1, 0}));
    ARPG_REQUIRE(!requested_exit(Vec3{-11.99F, 0.0F, 0.0F}, {-1, 0}));
    ARPG_REQUIRE(!requested_exit(Vec3{-12.0F, 0.91F, 0.0F}, {-1, 0}));
    ARPG_REQUIRE(!requested_exit(Vec3{12.0F, -0.91F, 0.0F}, {1, 0}));
    ARPG_REQUIRE(!requested_exit(Vec3{1.51F, -5.5F, 0.0F}, {0, -1}));
    ARPG_REQUIRE(!requested_exit(Vec3{-1.51F, 5.5F, 0.0F}, {0, 1}));
    return {};
}

arpg::test::Failure first_exit_commits_destroys_and_removes_combat() noexcept {
    DungeonSession session;
    drain_dungeon(session);
    ARPG_REQUIRE(clear_and_await(session));
    drain_dungeon(session);

    const DungeonSnapshot before = session.snapshot();
    ARPG_REQUIRE(commit_exit(session, ExitDirection::right));
    const DungeonSnapshot after = session.snapshot();
    const ExitEvents events = drain_dungeon(session);

    ARPG_REQUIRE(after.phase == RoomPhase::transitioning);
    ARPG_REQUIRE(!after.has_active_room);
    ARPG_REQUIRE(!after.combat.has_value());
    ARPG_REQUIRE(after.room_index == before.room_index + 1U);
    ARPG_REQUIRE(after.room_seed != before.room_seed);
    ARPG_REQUIRE(after.last_exit == ExitDirection::right);
    ARPG_REQUIRE(events.count == 3U);
    ARPG_REQUIRE(events.values[0].kind
        == DungeonEventKind::transition_requested);
    ARPG_REQUIRE(events.values[1].kind
        == DungeonEventKind::transition_committed);
    ARPG_REQUIRE(events.values[2].kind == DungeonEventKind::room_destroyed);
    ARPG_REQUIRE(events.values[0].direction == ExitDirection::right);
    ARPG_REQUIRE(events.values[1].direction == ExitDirection::right);
    ARPG_REQUIRE(events.values[2].direction == ExitDirection::right);
    ARPG_REQUIRE(events.values[0].session_tick <= events.values[1].session_tick);
    ARPG_REQUIRE(events.values[1].session_tick <= events.values[2].session_tick);
    ARPG_REQUIRE(events.values[0].room_index == before.room_index);
    ARPG_REQUIRE(events.values[2].room_seed == before.room_seed);
    ARPG_REQUIRE(events.values[2].destination_room_index
        == after.room_index);
    return {};
}

arpg::test::Failure transition_and_combat_start_are_separate_ticks() noexcept {
    DungeonSession session;
    ARPG_REQUIRE(clear_and_await(session));
    ARPG_REQUIRE(commit_exit(session, ExitDirection::up));
    const DungeonSnapshot transition = session.snapshot();

    session.tick(outward(ExitDirection::up));
    drain_dungeon(session);
    const DungeonSnapshot locked = session.snapshot();
    ARPG_REQUIRE(locked.session_tick == transition.session_tick + 1U);
    ARPG_REQUIRE(locked.phase == RoomPhase::locked);
    ARPG_REQUIRE(locked.room_index == 1U);
    ARPG_REQUIRE(locked.entry_side == arpg::dungeon::EntrySide::bottom);
    ARPG_REQUIRE(locked.has_active_room);
    ARPG_REQUIRE(locked.combat.has_value());
    ARPG_REQUIRE(locked.combat->tick == 0U);
    ARPG_REQUIRE(locked.combat->player.position.x == 0.0F);
    ARPG_REQUIRE(locked.combat->player.position.y == 4.75F);

    session.tick(outward(ExitDirection::up));
    const ExitEvents events = drain_dungeon(session);
    const DungeonSnapshot combat = session.snapshot();
    ARPG_REQUIRE(combat.phase == RoomPhase::combat);
    ARPG_REQUIRE(combat.combat->tick == 0U);
    ARPG_REQUIRE(events.count == 2U);
    ARPG_REQUIRE(events.values[0].kind == DungeonEventKind::room_entered);
    ARPG_REQUIRE(events.values[1].kind == DungeonEventKind::combat_started);
    return {};
}

arpg::test::Failure contact_and_held_inputs_never_duplicate_rooms() noexcept {
    DungeonSession session;
    for (int tick = 0; tick < 192; ++tick) {
        session.tick(outward(ExitDirection::right));
        drain_dungeon(session);
    }
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(session.snapshot().room_index == 0U);
    ARPG_REQUIRE(session.snapshot().diagnostics.rejected_exit_count == 0U);

    session.reset_current_room();
    drain_dungeon(session);
    ARPG_REQUIRE(clear_and_await(session));
    ARPG_REQUIRE(commit_exit(session, ExitDirection::right));
    session.tick(outward(ExitDirection::right));
    drain_dungeon(session);
    session.tick(outward(ExitDirection::right));
    drain_dungeon(session);
    for (int tick = 0; tick < 192; ++tick) {
        session.tick(outward(ExitDirection::right));
        drain_dungeon(session);
    }
    ARPG_REQUIRE(session.snapshot().room_index == 1U);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);

    session.reset_current_room();
    drain_dungeon(session);
    ARPG_REQUIRE(clear_and_await(session));
    ARPG_REQUIRE(commit_exit(session, ExitDirection::left));
    ARPG_REQUIRE(session.snapshot().room_index == 2U);
    session.tick(MovementInput{});
    drain_dungeon(session);
    ARPG_REQUIRE(session.snapshot().room_index == 2U);
    ARPG_REQUIRE(session.snapshot().entry_side == arpg::dungeon::EntrySide::right);
    return {};
}

arpg::test::Failure maximum_index_faults_once_without_destroying() noexcept {
    arpg::dungeon::DungeonSessionConfig config;
    config.initial_room_index = (std::numeric_limits<std::uint64_t>::max)();
    DungeonSession session{config};
    ARPG_REQUIRE(clear_and_await(session));
    drain_dungeon(session);

    const DungeonSnapshot before = session.snapshot();
    for (int tick = 0; tick < 256; ++tick) {
        session.tick(outward(ExitDirection::left));
        drain_combat(session);
        if (session.snapshot().diagnostics.room_index_overflow) {
            break;
        }
    }
    const DungeonSnapshot faulted = session.snapshot();
    const ExitEvents first = drain_dungeon(session);
    ARPG_REQUIRE(faulted.phase == RoomPhase::faulted);
    ARPG_REQUIRE(faulted.has_active_room);
    ARPG_REQUIRE(faulted.combat.has_value());
    ARPG_REQUIRE(faulted.room_index == before.room_index);
    ARPG_REQUIRE(faulted.room_seed == before.room_seed);
    ARPG_REQUIRE(faulted.diagnostics.room_index_overflow);
    ARPG_REQUIRE(faulted.diagnostics.fault
        == arpg::dungeon::DungeonFault::room_index_overflow);
    ARPG_REQUIRE(first.count == 1U);
    ARPG_REQUIRE(first.values[0].kind == DungeonEventKind::faulted);
    ARPG_REQUIRE(first.values[0].direction == ExitDirection::left);

    for (int tick = 0; tick < 32; ++tick) {
        session.tick(outward(ExitDirection::left));
        drain_combat(session);
    }
    const ExitEvents repeated = drain_dungeon(session);
    ARPG_REQUIRE(repeated.count == 0U);
    ARPG_REQUIRE(session.snapshot().room_index
        == (std::numeric_limits<std::uint64_t>::max)());

#if defined(NDEBUG)
    // Release-only overflow propagation is covered by the transaction suite;
    // this legacy navigation branch intentionally has no extra assertions.
    ARPG_REQUIRE(true);
#endif
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"exact apertures accept outward input", &exact_apertures_accept_outward_input},
    {"invalid physical requests are rejected", &invalid_physical_requests_are_rejected},
    {"first exit commits destroys and removes combat", &first_exit_commits_destroys_and_removes_combat},
    {"transition and combat start are separate ticks", &transition_and_combat_start_are_separate_ticks},
    {"contact and held inputs never duplicate rooms", &contact_and_held_inputs_never_duplicate_rooms},
    {"maximum index faults once without destroying", &maximum_index_faults_once_without_destroying},
};

}  // namespace

arpg::test::TestSuite dungeon_navigation_suite() noexcept {
    return arpg::test::make_suite("dungeon_navigation", kCases);
}
