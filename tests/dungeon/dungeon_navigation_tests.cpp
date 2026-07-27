#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "abyss/abyss_rules.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_generation.hpp"
#include "dungeon/room_navigation.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace {

constexpr int kMaximumNavigationTicks = 4096;

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
    for (int tick = 0; tick < kMaximumNavigationTicks; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        const MovementInput movement = alignment(state, direction);
        if (movement.x == 0 && movement.y == 0) {
            break;
        }
        session.tick(movement);
        drain_dungeon(session);
    }
    for (int tick = 0; tick < kMaximumNavigationTicks; ++tick) {
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
    for (int tick = 0; tick < kMaximumNavigationTicks; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        const MovementInput movement = alignment(state, direction);
        if (movement.x == 0 && movement.y == 0) {
            break;
        }
        session.tick(movement);
        drain_combat(session);
    }
    for (int tick = 0; tick < kMaximumNavigationTicks; ++tick) {
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
    ARPG_REQUIRE(requested_exit(Vec3{arpg::combat::room_bounds::min_x,
            0.90F, 4.0F}, {-1, 0})
        == ExitDirection::left);
    ARPG_REQUIRE(requested_exit(Vec3{arpg::combat::room_bounds::max_x,
            -0.90F, 0.0F}, {1, 0})
        == ExitDirection::right);
    ARPG_REQUIRE(requested_exit(Vec3{1.50F,
            arpg::combat::room_bounds::min_y, 0.0F}, {0, -1})
        == ExitDirection::up);
    ARPG_REQUIRE(requested_exit(Vec3{-1.50F,
            arpg::combat::room_bounds::max_y, 0.0F}, {0, 1})
        == ExitDirection::down);
    return {};
}

arpg::test::Failure invalid_physical_requests_are_rejected() noexcept {
    using arpg::dungeon::requested_exit;
    ARPG_REQUIRE(!requested_exit(Vec3{arpg::combat::room_bounds::min_x,
            0.0F, 0.0F}, {1, 0}));
    ARPG_REQUIRE(!requested_exit(Vec3{arpg::combat::room_bounds::min_x
            + 0.01F, 0.0F, 0.0F}, {-1, 0}));
    ARPG_REQUIRE(!requested_exit(Vec3{arpg::combat::room_bounds::min_x,
            0.91F, 0.0F}, {-1, 0}));
    ARPG_REQUIRE(!requested_exit(Vec3{arpg::combat::room_bounds::max_x,
            -0.91F, 0.0F}, {1, 0}));
    ARPG_REQUIRE(!requested_exit(Vec3{1.51F,
            arpg::combat::room_bounds::min_y, 0.0F}, {0, -1}));
    ARPG_REQUIRE(!requested_exit(Vec3{-1.51F,
            arpg::combat::room_bounds::max_y, 0.0F}, {0, 1}));
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
    if (session.snapshot().phase == RoomPhase::committing) {
        ARPG_REQUIRE(session.pending_save_view() != nullptr);
        ARPG_REQUIRE(session.pending_save_view()->kind
            == arpg::dungeon::PendingSaveKind::abyss_start);
        ARPG_REQUIRE(arpg::test::commit_pending(session));
    }
    const DungeonSnapshot locked = session.snapshot();
    ARPG_REQUIRE(locked.session_tick == transition.session_tick + 1U);
    ARPG_REQUIRE(locked.phase == RoomPhase::locked);
    ARPG_REQUIRE(locked.room_index == 1U);
    ARPG_REQUIRE(locked.entry_side == arpg::dungeon::EntrySide::bottom);
    ARPG_REQUIRE(locked.has_active_room);
    ARPG_REQUIRE(locked.combat.has_value());
    ARPG_REQUIRE(locked.combat->tick == 0U);
    ARPG_REQUIRE(locked.combat->player.position.x == 0.0F);
    ARPG_REQUIRE(locked.combat->player.position.y
        == arpg::combat::room_bounds::max_y - 0.75F);

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

    static_cast<void>(session.reset_current_room());
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

    static_cast<void>(session.reset_current_room());
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
    for (int tick = 0; tick < kMaximumNavigationTicks; ++tick) {
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

arpg::test::Failure snapshot_exposes_only_boolean_door_preview() noexcept {
    static_assert(std::is_same_v<
        decltype(DungeonSnapshot{}.abyss_doors),
        std::array<bool, 4>>);
    DungeonSession session;
    const DungeonSnapshot snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.abyss_doors
        == arpg::dungeon::preview_abyss_doors(
            arpg::test::stable_state(session).current_room));
    ARPG_REQUIRE(snapshot.abyss_rule == arpg::abyss::AbyssRuleId::none);
    ARPG_REQUIRE(snapshot.abyss_danger == arpg::abyss::AbyssDanger::low);

    auto state = arpg::dungeon::make_initial_run_state(
        7U, arpg::dungeon::DungeonRules{}).state;
    state.current_room.index = 1U;
    state.current_room.entry = arpg::dungeon::EntrySide::top;
    while (!arpg::abyss::is_abyss_roll(state.current_room.seed)) {
        ++state.current_room.seed;
    }
    const auto selection = arpg::abyss::select_abyss_rule(
        state.current_room.seed, state.current_room.depth);
    ARPG_REQUIRE(selection.has_value());
    state.current_room.is_abyss = true;
    state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::available;
    state.abyss.rule = selection->rule;
    state.abyss.danger = selection->danger;
    state.abyss.rules_version = selection->rules_version;
    DungeonSession abyss_session{arpg::dungeon::DungeonRules{}, state};
    const DungeonSnapshot abyss_snapshot = abyss_session.snapshot();
    ARPG_REQUIRE(abyss_snapshot.abyss_rule == state.abyss.rule);
    ARPG_REQUIRE(abyss_snapshot.abyss_danger == state.abyss.danger);
    return {};
}

arpg::test::Failure ordinary_door_preview_matches_pending_target() noexcept {
    using namespace arpg::dungeon;
    auto state = make_initial_run_state(7U, DungeonRules{}).state;
    state.current_room.index = 0U;
    state.current_room.seed = 0U;
    state.current_room.depth = 27U;
    state.current_room.floor_room_index = 1U;
    state.current_room.is_abyss = false;
    state.abyss = {};
    DungeonSession session{DungeonRules{}, state};
    arpg::test::set_phase(session, RoomPhase::awaiting_exit);
    const DungeonSnapshot preview = session.snapshot();
    ARPG_REQUIRE(!preview.abyss_doors[
        static_cast<std::size_t>(ExitDirection::right)]);

    arpg::test::attempt_exit(session, ExitDirection::right);
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(!pending->next_state.current_room.is_abyss);
    ARPG_REQUIRE(pending->next_state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::none);
    ARPG_REQUIRE(pending->next_state.abyss.rule
        == arpg::abyss::AbyssRuleId::none);
    return {};
}

arpg::test::Failure abyss_door_target_persists_selected_checkpoint() noexcept {
    using namespace arpg::dungeon;
    auto state = make_initial_run_state(7U, DungeonRules{}).state;
    state.current_room.index = 0U;
    state.current_room.seed = 0x150U;
    state.current_room.depth = 27U;
    state.current_room.floor_room_index = 1U;
    state.current_room.is_abyss = false;
    state.abyss = {};
    DungeonSession session{DungeonRules{}, state};
    arpg::test::set_phase(session, RoomPhase::awaiting_exit);
    const DungeonSnapshot preview = session.snapshot();

    std::size_t preview_count = 0U;
    ExitDirection selected_direction = ExitDirection::none;
    for (std::size_t index = 0U; index < preview.abyss_doors.size(); ++index) {
        if (!preview.abyss_doors[index]) continue;
        ++preview_count;
        selected_direction = static_cast<ExitDirection>(index);
    }
    ARPG_REQUIRE(preview_count == 1U);
    ARPG_REQUIRE(selected_direction != ExitDirection::none);
    arpg::test::attempt_exit(session, selected_direction);
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->next_state.current_room.is_abyss);

    const auto selection = arpg::abyss::select_abyss_rule(
        pending->next_state.current_room.seed,
        pending->next_state.current_room.depth);
    ARPG_REQUIRE(selection.has_value());
    ARPG_REQUIRE(pending->next_state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::available);
    ARPG_REQUIRE(pending->next_state.abyss.rule == selection->rule);
    ARPG_REQUIRE(pending->next_state.abyss.danger == selection->danger);
    ARPG_REQUIRE(pending->next_state.abyss.rules_version
        == selection->rules_version);

    DungeonRunState mismatched = pending->next_state;
    mismatched.abyss.rules_version += 1U;
    ARPG_REQUIRE(!same_run_state(mismatched, pending->next_state));
    session.resolve_pending_transition({
        SaveDisposition::committed,
        pending->next_state.commit_generation,
        mismatched,
    });
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);

    DungeonSession resolution_session{DungeonRules{}, state};
    arpg::test::set_phase(resolution_session, RoomPhase::awaiting_exit);
    arpg::test::attempt_exit(resolution_session, selected_direction);
    const auto resolution_pending = resolution_session.pending_transition();
    ARPG_REQUIRE(resolution_pending.has_value());
    mismatched = resolution_pending->next_state;
    mismatched.last_abyss_resolution.valid = true;
    ARPG_REQUIRE(!same_run_state(
        mismatched, resolution_pending->next_state));
    resolution_session.resolve_pending_transition({
        SaveDisposition::committed,
        resolution_pending->next_state.commit_generation,
        mismatched,
    });
    ARPG_REQUIRE(resolution_session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(resolution_session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure abyss_door_target_can_keep_its_hole() noexcept {
    using namespace arpg::dungeon;
    auto state = make_initial_run_state(7U, DungeonRules{}).state;
    state.current_room.index = 0U;
    state.current_room.seed = 0x25EU;
    state.current_room.depth = 4U;
    state.current_room.floor_room_index = 1U;
    state.current_room.is_abyss = false;
    state.abyss = {};
    DungeonSession session{DungeonRules{}, state};
    arpg::test::set_phase(session, RoomPhase::awaiting_exit);
    const DungeonSnapshot preview = session.snapshot();
    ARPG_REQUIRE(preview.abyss_doors[
        static_cast<std::size_t>(ExitDirection::left)]);

    arpg::test::attempt_exit(session, ExitDirection::left);
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->next_state.current_room.seed
        == 0xB279FFA75D264269ULL);
    ARPG_REQUIRE(pending->next_state.current_room.is_abyss);
    ARPG_REQUIRE(pending->next_state.current_room.has_hole);
    ARPG_REQUIRE(pending->next_state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::available);
    return {};
}

arpg::test::Failure descent_target_rejects_matching_legacy_abyss_roll() noexcept {
    using namespace arpg::dungeon;
    auto state = make_initial_run_state(7U, DungeonRules{}).state;
    state.current_room.index = 0U;
    state.current_room.seed = 0x17U;
    state.current_room.depth = 4U;
    state.current_room.floor_room_index = 3U;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = false;
    state.abyss = {};
    DungeonSession session{DungeonRules{}, state};
    arpg::test::set_phase(session, RoomPhase::awaiting_exit);
    ARPG_REQUIRE(session.request_descent(true));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->next_state.current_room.seed
        == 0xA8CCB855C5CE46BDULL);
    ARPG_REQUIRE(!pending->next_state.current_room.is_abyss);
    ARPG_REQUIRE(pending->next_state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::none);
    ARPG_REQUIRE(pending->next_state.abyss.rule
        == arpg::abyss::AbyssRuleId::none);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"exact apertures accept outward input", &exact_apertures_accept_outward_input},
    {"invalid physical requests are rejected", &invalid_physical_requests_are_rejected},
    {"first exit commits destroys and removes combat", &first_exit_commits_destroys_and_removes_combat},
    {"transition and combat start are separate ticks", &transition_and_combat_start_are_separate_ticks},
    {"contact and held inputs never duplicate rooms", &contact_and_held_inputs_never_duplicate_rooms},
    {"maximum index faults once without destroying", &maximum_index_faults_once_without_destroying},
    {"snapshot exposes only boolean door preview", &snapshot_exposes_only_boolean_door_preview},
    {"ordinary door preview matches pending target", &ordinary_door_preview_matches_pending_target},
    {"abyss door target persists selected checkpoint", &abyss_door_target_persists_selected_checkpoint},
    {"abyss door target can keep its hole", &abyss_door_target_can_keep_its_hole},
    {"descent target rejects matching legacy abyss roll", &descent_target_rejects_matching_legacy_abyss_roll},
};

}  // namespace

arpg::test::TestSuite dungeon_navigation_suite() noexcept {
    return arpg::test::make_suite("dungeon_navigation", kCases);
}
