#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/room_generation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

bool all_exits_are(
    const arpg::dungeon::DungeonSnapshot& state,
    bool expected) noexcept {
    for (const bool open : state.exits_open) {
        if (open != expected) {
            return false;
        }
    }
    return true;
}

arpg::test::Failure construction_and_first_tick_are_staged() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;

    const dungeon::DungeonSnapshot constructed = session.snapshot();
    ARPG_REQUIRE(constructed.phase == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(constructed.has_active_room);
    ARPG_REQUIRE(constructed.combat.has_value());
    ARPG_REQUIRE(constructed.combat->tick == 0U);
    ARPG_REQUIRE(constructed.remaining_targets > 0U);
    ARPG_REQUIRE(constructed.remaining_targets
        == constructed.combat->monster_count);
    ARPG_REQUIRE(all_exits_are(constructed, false));

    session.tick(combat::MovementInput{});
    const dungeon::DungeonSnapshot started = session.snapshot();
    ARPG_REQUIRE(started.phase == dungeon::RoomPhase::combat);
    ARPG_REQUIRE(started.session_tick == 1U);
    ARPG_REQUIRE(started.combat.has_value());
    ARPG_REQUIRE(started.combat->tick == 0U);

    const auto entered = session.try_pop_event();
    ARPG_REQUIRE(entered.has_value());
    ARPG_REQUIRE(entered->kind == dungeon::DungeonEventKind::room_entered);
    ARPG_REQUIRE(entered->session_tick == 0U);
    const auto started_event = session.try_pop_event();
    ARPG_REQUIRE(started_event.has_value());
    ARPG_REQUIRE(started_event->kind
        == dungeon::DungeonEventKind::combat_started);
    ARPG_REQUIRE(started_event->session_tick == 0U);
    ARPG_REQUIRE(!session.try_pop_event().has_value());
    ARPG_REQUIRE(!session.try_pop_combat_event().has_value());
    return {};
}

arpg::test::Failure closed_doors_ignore_pre_clear_contact() noexcept {
    using namespace arpg;
    constexpr std::array<combat::MovementInput, 4> kOutwardMovements{{
        {0, -1},
        {0, 1},
        {-1, 0},
        {1, 0},
    }};

    for (const combat::MovementInput movement : kOutwardMovements) {
        dungeon::DungeonSession session;
        session.tick(combat::MovementInput{});
        for (int tick = 0; tick < 128; ++tick) {
            session.tick(movement);
        }
        const dungeon::DungeonSnapshot state = session.snapshot();
        ARPG_REQUIRE(state.phase == dungeon::RoomPhase::combat);
        ARPG_REQUIRE(state.room_index == 0U);
        ARPG_REQUIRE(state.remaining_targets > 0U);
        ARPG_REQUIRE(state.remaining_targets == state.combat->monster_count);
        ARPG_REQUIRE(all_exits_are(state, false));
        ARPG_REQUIRE(state.diagnostics.rejected_exit_count == 0U);
    }
    return {};
}

arpg::test::Failure real_combat_clears_once_without_respawn() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    test::EventSummary events;
    const std::uint8_t initial_targets = session.snapshot().remaining_targets;

    session.tick(combat::MovementInput{});
    test::drain_all_events(session, events);
    ARPG_REQUIRE(test::drive_until_cleared(session, events));

    const dungeon::DungeonSnapshot cleared = session.snapshot();
    ARPG_REQUIRE(cleared.phase == dungeon::RoomPhase::cleared);
    ARPG_REQUIRE(cleared.remaining_targets == 0U);
    ARPG_REQUIRE(all_exits_are(cleared, true));
    ARPG_REQUIRE(events.room_cleared_count == 1U);
    ARPG_REQUIRE(events.exits_opened_count == 1U);
    ARPG_REQUIRE(events.defeated_count == initial_targets);

    const std::uint64_t cleared_session_tick = cleared.session_tick;
    const std::uint64_t combat_tick = cleared.combat->tick;
    const float player_x = cleared.combat->player.position.x;
    session.tick(combat::MovementInput{-1, 0});
    test::drain_all_events(session, events);
    const dungeon::DungeonSnapshot transitioned = session.snapshot();
    ARPG_REQUIRE(transitioned.phase == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(transitioned.session_tick == cleared_session_tick + 1U);
    ARPG_REQUIRE(transitioned.combat->tick == cleared.combat->tick);
    ARPG_REQUIRE(transitioned.combat->player.position.x
        == cleared.combat->player.position.x);
    ARPG_REQUIRE(transitioned.combat->player.position.y
        == cleared.combat->player.position.y);
    ARPG_REQUIRE(transitioned.combat->player.position.z
        == cleared.combat->player.position.z);
    ARPG_REQUIRE(transitioned.combat->player.state
        == cleared.combat->player.state);
    ARPG_REQUIRE(transitioned.combat->player.active_attack
        == cleared.combat->player.active_attack);
    ARPG_REQUIRE(transitioned.combat->player.attack_phase
        == cleared.combat->player.attack_phase);
    ARPG_REQUIRE(transitioned.combat->player.attack_elapsed_ticks
        == cleared.combat->player.attack_elapsed_ticks);
    ARPG_REQUIRE(transitioned.combat->player.combo_stage
        == cleared.combat->player.combo_stage);
    ARPG_REQUIRE(transitioned.combat->player.hit_stop_ticks
        == cleared.combat->player.hit_stop_ticks);

    bool moved_while_awaiting_exit = false;
    for (int tick = 0; tick < 64; ++tick) {
        session.tick(combat::MovementInput{-1, 0});
        test::drain_all_events(session, events);
        const dungeon::DungeonSnapshot awaiting = session.snapshot();
        ARPG_REQUIRE(awaiting.phase == dungeon::RoomPhase::awaiting_exit);
        if (awaiting.combat->player.position.x < player_x) {
            moved_while_awaiting_exit = true;
            break;
        }
    }
    const dungeon::DungeonSnapshot awaiting = session.snapshot();
    ARPG_REQUIRE(awaiting.combat->tick > combat_tick);
    ARPG_REQUIRE(moved_while_awaiting_exit);
    ARPG_REQUIRE(!session.queue_action(combat::Action::light));

    for (int tick = 0; tick < 180; ++tick) {
        session.tick(combat::MovementInput{});
        test::drain_all_events(session, events);
    }
    const dungeon::DungeonSnapshot stable = session.snapshot();
    ARPG_REQUIRE(stable.phase == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(stable.remaining_targets == 0U);
    ARPG_REQUIRE(all_exits_are(stable, true));
    ARPG_REQUIRE(events.room_cleared_count == 1U);
    ARPG_REQUIRE(events.exits_opened_count == 1U);
    ARPG_REQUIRE(events.defeated_count == initial_targets);
    return {};
}

arpg::test::Failure reset_reconstructs_same_room_and_clears_queues() noexcept {
    using namespace arpg;
    dungeon::DungeonSessionConfig config;
    config.root_seed = 0xA5A55A5AF00DFACEULL;
    config.initial_room_index = 19U;
    dungeon::DungeonSession session{config};

    session.tick(combat::MovementInput{});
    ARPG_REQUIRE(session.queue_action(combat::Action::light));
    session.tick(combat::MovementInput{});
    const dungeon::DungeonSnapshot before = session.snapshot();
    ARPG_REQUIRE(before.combat->tick == 1U);

    session.reset_current_room();
    const dungeon::DungeonSnapshot reset = session.snapshot();
    ARPG_REQUIRE(reset.session_tick == before.session_tick);
    ARPG_REQUIRE(reset.room_index == before.room_index);
    ARPG_REQUIRE(reset.room_seed == before.room_seed);
    ARPG_REQUIRE(reset.entry_side == before.entry_side);
    ARPG_REQUIRE(reset.last_exit == before.last_exit);
    ARPG_REQUIRE(reset.phase == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(reset.combat.has_value());
    ARPG_REQUIRE(reset.combat->tick == 0U);
    ARPG_REQUIRE(reset.remaining_targets > 0U);
    ARPG_REQUIRE(reset.remaining_targets == reset.combat->monster_count);
    ARPG_REQUIRE(all_exits_are(reset, false));
    ARPG_REQUIRE(reset.diagnostics.event_overflow_count
        == before.diagnostics.event_overflow_count);
    ARPG_REQUIRE(reset.diagnostics.combat_relay_overflow_count
        == before.diagnostics.combat_relay_overflow_count);

    const auto reset_event = session.try_pop_event();
    ARPG_REQUIRE(reset_event.has_value());
    ARPG_REQUIRE(reset_event->kind == dungeon::DungeonEventKind::room_reset);
    ARPG_REQUIRE(!session.try_pop_event().has_value());
    ARPG_REQUIRE(!session.try_pop_combat_event().has_value());
    return {};
}

arpg::test::Failure combat_events_relay_in_source_order() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    test::EventSummary relayed;
    const std::uint8_t initial_targets = session.snapshot().remaining_targets;

    session.tick(combat::MovementInput{});
    test::EventSummary dungeon_events;
    test::drain_all_events(session, dungeon_events);

    test::force_defeat_current_wave(session);
    session.tick(combat::MovementInput{});
    test::drain_all_events(session, relayed);

    ARPG_REQUIRE(relayed.combat_count == initial_targets);
    ARPG_REQUIRE(relayed.defeated_count == initial_targets);
    ARPG_REQUIRE(relayed.room_cleared_count == 1U);
    ARPG_REQUIRE(relayed.exits_opened_count == 1U);
    const dungeon::DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.diagnostics.combat_relay_overflow_count == 0U);
    ARPG_REQUIRE(state.diagnostics.event_overflow_count == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"construction and first tick are staged", &construction_and_first_tick_are_staged},
    {"closed doors ignore pre-clear contact", &closed_doors_ignore_pre_clear_contact},
    {"real combat clears once without respawn", &real_combat_clears_once_without_respawn},
    {"reset reconstructs same room and clears queues", &reset_reconstructs_same_room_and_clears_queues},
    {"combat events relay in source order", &combat_events_relay_in_source_order},
};

}  // namespace

arpg::test::TestSuite dungeon_lifecycle_suite() noexcept {
    return arpg::test::make_suite("dungeon_lifecycle", kCases);
}
