#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon_test_support.hpp"

#include "dungeon/room_generation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

using arpg::combat::CombatEvent;
using arpg::combat::CombatSnapshot;
using arpg::combat::MovementInput;
using arpg::dungeon::DungeonEvent;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;

constexpr std::array<ExitDirection, 4> kRoute{{
    ExitDirection::up,
    ExitDirection::right,
    ExitDirection::down,
    ExitDirection::left,
}};

struct StressSummary final {
    std::uint32_t dungeon_events{};
    std::uint32_t combat_events{};
    std::uint32_t dungeon_overflow{};
    std::uint32_t relay_overflow{};
    std::uint32_t combat_overflow{};
    std::uint32_t input_overflow{};
};

bool same_vec(const arpg::combat::Vec3& lhs, const arpg::combat::Vec3& rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool same_combat(const CombatSnapshot& lhs, const CombatSnapshot& rhs) noexcept {
    if (lhs.tick != rhs.tick
            || !same_vec(lhs.player.position, rhs.player.position)
            || !same_vec(lhs.player.velocity, rhs.player.velocity)
            || lhs.player.facing != rhs.player.facing
            || lhs.player.state != rhs.player.state
            || lhs.player.active_attack != rhs.player.active_attack
            || lhs.player.attack_phase != rhs.player.attack_phase
            || lhs.player.attack_elapsed_ticks != rhs.player.attack_elapsed_ticks
            || lhs.player.combo_stage != rhs.player.combo_stage
            || lhs.player.hit_stop_ticks != rhs.player.hit_stop_ticks
            || lhs.player.air_attack_available != rhs.player.air_attack_available
            || lhs.diagnostics.input_size != rhs.diagnostics.input_size
            || lhs.diagnostics.input_expired_count != rhs.diagnostics.input_expired_count
            || lhs.diagnostics.input_overflow_count != rhs.diagnostics.input_overflow_count
            || lhs.diagnostics.event_overflow_count != rhs.diagnostics.event_overflow_count) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.dummies.size(); ++index) {
        const auto& a = lhs.dummies[index];
        const auto& b = rhs.dummies[index];
        if (!same_vec(a.position, b.position) || !same_vec(a.velocity, b.velocity)
                || a.kind != b.kind || a.reaction != b.reaction
                || a.armor != b.armor || a.hp != b.hp || a.max_hp != b.max_hp
                || a.break_value != b.break_value || a.max_break != b.max_break
                || a.break_window_ticks != b.break_window_ticks
                || a.hit_stop_ticks != b.hit_stop_ticks) {
            return false;
        }
    }
    return true;
}

bool same_snapshot(const DungeonSnapshot& lhs, const DungeonSnapshot& rhs) noexcept {
    if (lhs.session_tick != rhs.session_tick || lhs.room_index != rhs.room_index
            || lhs.room_seed != rhs.room_seed || lhs.phase != rhs.phase
            || lhs.has_active_room != rhs.has_active_room
            || lhs.exits_open != rhs.exits_open
            || lhs.remaining_targets != rhs.remaining_targets
            || lhs.entry_side != rhs.entry_side || lhs.last_exit != rhs.last_exit
            || lhs.diagnostics.event_overflow_count
                != rhs.diagnostics.event_overflow_count
            || lhs.diagnostics.combat_relay_overflow_count
                != rhs.diagnostics.combat_relay_overflow_count
            || lhs.diagnostics.rejected_exit_count
                != rhs.diagnostics.rejected_exit_count
            || lhs.diagnostics.room_index_overflow
                != rhs.diagnostics.room_index_overflow
            || lhs.combat.has_value() != rhs.combat.has_value()) {
        return false;
    }
    return !lhs.combat.has_value() || same_combat(*lhs.combat, *rhs.combat);
}

bool same_event(const DungeonEvent& lhs, const DungeonEvent& rhs) noexcept {
    return lhs.kind == rhs.kind && lhs.session_tick == rhs.session_tick
        && lhs.room_index == rhs.room_index && lhs.room_seed == rhs.room_seed
        && lhs.direction == rhs.direction;
}

bool same_event(const CombatEvent& lhs, const CombatEvent& rhs) noexcept {
    return lhs.kind == rhs.kind && lhs.tick == rhs.tick
        && lhs.attack == rhs.attack && lhs.feedback == rhs.feedback
        && lhs.target_index == rhs.target_index
        && lhs.hit_count == rhs.hit_count && lhs.value == rhs.value
        && same_vec(lhs.position, rhs.position);
}

void sample_diagnostics(const DungeonSnapshot& state, StressSummary& summary) noexcept {
    summary.dungeon_overflow = state.diagnostics.event_overflow_count;
    summary.relay_overflow = state.diagnostics.combat_relay_overflow_count;
    if (state.combat.has_value()) {
        summary.combat_overflow += state.combat->diagnostics.event_overflow_count;
        summary.input_overflow += state.combat->diagnostics.input_overflow_count;
    }
}

void drain(DungeonSession& session, StressSummary& summary) noexcept {
    while (session.try_pop_event().has_value()) {
        ++summary.dungeon_events;
    }
    while (session.try_pop_combat_event().has_value()) {
        ++summary.combat_events;
    }
    sample_diagnostics(session.snapshot(), summary);
}

bool tick_equal(
    DungeonSession& lhs,
    DungeonSession& rhs,
    MovementInput movement) noexcept {
    lhs.tick(movement);
    rhs.tick(movement);
    if (!same_snapshot(lhs.snapshot(), rhs.snapshot())) {
        return false;
    }
    for (;;) {
        const auto a = lhs.try_pop_event();
        const auto b = rhs.try_pop_event();
        if (a.has_value() != b.has_value()) {
            return false;
        }
        if (!a.has_value()) {
            break;
        }
        if (!same_event(*a, *b)) {
            return false;
        }
    }
    for (;;) {
        const auto a = lhs.try_pop_combat_event();
        const auto b = rhs.try_pop_combat_event();
        if (a.has_value() != b.has_value()) {
            return false;
        }
        if (!a.has_value()) {
            break;
        }
        if (!same_event(*a, *b)) {
            return false;
        }
    }
    return true;
}

MovementInput outward(ExitDirection direction) noexcept {
    switch (direction) {
    case ExitDirection::up: return {0, -1};
    case ExitDirection::down: return {0, 1};
    case ExitDirection::left: return {-1, 0};
    case ExitDirection::right: return {1, 0};
    case ExitDirection::none: return {};
    }
    return {};
}

MovementInput align_center(const DungeonSnapshot& state, ExitDirection direction) noexcept {
    constexpr float kTolerance = 0.10F;
    MovementInput movement{};
    const auto position = state.combat->player.position;
    if (direction == ExitDirection::left || direction == ExitDirection::right) {
        movement.y = position.y > kTolerance ? -1
            : (position.y < -kTolerance ? 1 : 0);
    } else {
        movement.x = position.x > kTolerance ? -1
            : (position.x < -kTolerance ? 1 : 0);
    }
    return movement;
}

bool drive_clear(DungeonSession& session, StressSummary& summary) noexcept {
    for (int tick = 0; tick < 2048; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        if (state.phase == RoomPhase::cleared) {
            session.tick(MovementInput{});
            drain(session, summary);
            return session.snapshot().phase == RoomPhase::awaiting_exit;
        }
        MovementInput movement{};
        if (state.phase == RoomPhase::combat && state.combat.has_value()) {
            const auto* target = arpg::test::nearest_living_dummy(*state.combat);
            if (target != nullptr) {
                movement = arpg::test::movement_toward(
                    state.combat->player.position, target->position);
                if (arpg::test::in_light_attack_lane(state.combat->player, *target)
                        && state.combat->player.active_attack
                            == arpg::combat::AttackId::none) {
                    static_cast<void>(session.queue_action(arpg::combat::Action::light));
                }
            }
        }
        session.tick(movement);
        drain(session, summary);
    }
    return false;
}

bool drive_exit(
    DungeonSession& session,
    ExitDirection direction,
    StressSummary& summary,
    bool verify_phases) noexcept {
    for (int tick = 0; tick < 256; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        const MovementInput movement = align_center(state, direction);
        if (movement.x == 0 && movement.y == 0) {
            break;
        }
        session.tick(movement);
        drain(session, summary);
    }
    for (int tick = 0; tick < 256; ++tick) {
        session.tick(outward(direction));
        drain(session, summary);
        if (session.snapshot().phase == RoomPhase::transitioning) {
            break;
        }
    }
    const DungeonSnapshot transition = session.snapshot();
    if (transition.phase != RoomPhase::transitioning
            || transition.has_active_room || transition.combat.has_value()) {
        return false;
    }
    session.tick(outward(direction));
    drain(session, summary);
    const DungeonSnapshot locked = session.snapshot();
    if (locked.phase != RoomPhase::locked || !locked.has_active_room
            || !locked.combat.has_value() || locked.combat->tick != 0U) {
        return false;
    }
    session.tick(outward(direction));
    drain(session, summary);
    const DungeonSnapshot combat = session.snapshot();
    return combat.phase == RoomPhase::combat && combat.has_active_room
        && combat.combat.has_value() && combat.combat->tick == 0U
        && (!verify_phases || combat.room_index == transition.room_index + 1U);
}

bool drive_rooms(
    DungeonSession& session,
    std::size_t exits,
    StressSummary& summary,
    bool verify_phases = false) noexcept {
    drain(session, summary);
    for (std::size_t index = 0; index < exits; ++index) {
        if (!drive_clear(session, summary)
                || !drive_exit(session, kRoute[index % kRoute.size()], summary,
                    verify_phases)) {
            return false;
        }
    }
    return true;
}

arpg::test::Failure identical_seed_and_route_are_field_equal() noexcept {
    arpg::dungeon::DungeonSessionConfig config;
    config.root_seed = 0x1020304050607080ULL;
    DungeonSession lhs{config};
    DungeonSession rhs{config};
    ARPG_REQUIRE(same_snapshot(lhs.snapshot(), rhs.snapshot()));

    std::size_t exits = 0;
    for (int tick = 0; tick < 20000 && exits < 4U; ++tick) {
        const DungeonSnapshot state = lhs.snapshot();
        MovementInput movement{};
        if (state.phase == RoomPhase::combat && state.combat.has_value()) {
            const auto* target = arpg::test::nearest_living_dummy(*state.combat);
            if (target != nullptr) {
                movement = arpg::test::movement_toward(
                    state.combat->player.position, target->position);
                if (arpg::test::in_light_attack_lane(state.combat->player, *target)
                        && state.combat->player.active_attack
                            == arpg::combat::AttackId::none) {
                    ARPG_REQUIRE(lhs.queue_action(arpg::combat::Action::light));
                    ARPG_REQUIRE(rhs.queue_action(arpg::combat::Action::light));
                }
            }
        } else if (state.phase == RoomPhase::awaiting_exit) {
            const ExitDirection direction = kRoute[exits];
            movement = align_center(state, direction);
            if (movement.x == 0 && movement.y == 0) {
                movement = outward(direction);
            }
        }
        const std::uint64_t before_index = state.room_index;
        ARPG_REQUIRE(tick_equal(lhs, rhs, movement));
        if (lhs.snapshot().room_index == before_index + 1U) {
            ++exits;
        }
    }
    ARPG_REQUIRE(exits == 4U);
    return {};
}

arpg::test::Failure one_changed_direction_changes_only_committed_room() noexcept {
    arpg::dungeon::DungeonSessionConfig config;
    config.root_seed = 0x9988776655443322ULL;
    DungeonSession up{config};
    DungeonSession right{config};
    StressSummary up_summary{};
    StressSummary right_summary{};
    ARPG_REQUIRE(drive_clear(up, up_summary));
    ARPG_REQUIRE(drive_clear(right, right_summary));
    ARPG_REQUIRE(drive_exit(up, ExitDirection::up, up_summary, true));
    ARPG_REQUIRE(drive_exit(right, ExitDirection::right, right_summary, true));
    const DungeonSnapshot a = up.snapshot();
    const DungeonSnapshot b = right.snapshot();
    ARPG_REQUIRE(a.room_index == 1U && b.room_index == 1U);
    ARPG_REQUIRE(a.room_seed != b.room_seed);
    ARPG_REQUIRE(a.room_seed == arpg::dungeon::derive_next_room_seed(
        arpg::dungeon::derive_initial_room_seed(config.root_seed, 0U),
        1U, ExitDirection::up));
    ARPG_REQUIRE(b.room_seed == arpg::dungeon::derive_next_room_seed(
        arpg::dungeon::derive_initial_room_seed(config.root_seed, 0U),
        1U, ExitDirection::right));
    ARPG_REQUIRE(a.entry_side == arpg::dungeon::EntrySide::bottom);
    ARPG_REQUIRE(b.entry_side == arpg::dungeon::EntrySide::left);
    ARPG_REQUIRE(!same_vec(a.combat->player.position, b.combat->player.position));
    return {};
}

arpg::test::Failure thousand_real_rooms_preserve_single_world_invariants() noexcept {
    DungeonSession session;
    StressSummary summary{};
    ARPG_REQUIRE(drive_rooms(session, 1000U, summary, true));
    const DungeonSnapshot final = session.snapshot();
    ARPG_REQUIRE(final.room_index == 1000U);
    ARPG_REQUIRE(final.phase == RoomPhase::combat);
    ARPG_REQUIRE(final.has_active_room);
    ARPG_REQUIRE(final.combat.has_value());
    ARPG_REQUIRE(final.combat->tick == 0U);
    return {};
}

arpg::test::Failure measured_thousand_rooms_allocate_nothing_and_never_overflow() noexcept {
    {
        DungeonSession warm_up;
        StressSummary warm_summary{};
        ARPG_REQUIRE(drive_rooms(warm_up, 1U, warm_summary));
    }

    DungeonSession measured;
    StressSummary summary{};
    drain(measured, summary);
    const std::uint64_t before = arpg::test::allocation_count();
    ARPG_REQUIRE(drive_rooms(measured, 1000U, summary));
    const std::uint64_t after = arpg::test::allocation_count();
    const DungeonSnapshot final = measured.snapshot();
    std::printf(
        "[stress] exits=1000 index=%llu allocation_before=%llu "
        "allocation_after=%llu delta=%llu dungeon_overflow=%u "
        "relay_overflow=%u combat_overflow=%u input_overflow=%u\n",
        static_cast<unsigned long long>(final.room_index),
        static_cast<unsigned long long>(before),
        static_cast<unsigned long long>(after),
        static_cast<unsigned long long>(after - before),
        summary.dungeon_overflow,
        summary.relay_overflow,
        summary.combat_overflow,
        summary.input_overflow);
    ARPG_REQUIRE(final.room_index == 1000U);
    ARPG_REQUIRE(after == before);
    ARPG_REQUIRE(summary.dungeon_overflow == 0U);
    ARPG_REQUIRE(summary.relay_overflow == 0U);
    ARPG_REQUIRE(summary.combat_overflow == 0U);
    ARPG_REQUIRE(summary.input_overflow == 0U);
    ARPG_REQUIRE(final.diagnostics.event_overflow_count == 0U);
    ARPG_REQUIRE(final.diagnostics.combat_relay_overflow_count == 0U);
    ARPG_REQUIRE(final.combat->diagnostics.event_overflow_count == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"identical seed and route are field equal", &identical_seed_and_route_are_field_equal},
    {"one changed direction changes only committed room", &one_changed_direction_changes_only_committed_room},
    {"thousand real rooms preserve single world invariants", &thousand_real_rooms_preserve_single_world_invariants},
    {"measured thousand rooms allocate nothing and never overflow", &measured_thousand_rooms_allocate_nothing_and_never_overflow},
};

}  // namespace

arpg::test::TestSuite dungeon_stress_suite() noexcept {
    return arpg::test::make_suite("dungeon_stress", kCases);
}
