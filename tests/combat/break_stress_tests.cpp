#include "allocation_probe.hpp"
#include "combat_test_support.hpp"
#include "test_framework.hpp"

#include "combat/combat_world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace arpg::combat;
using arpg::test::drain_events;
using arpg::test::finish_attack;
using arpg::test::tick_n;

CombatLabConfig heavy_target_config() noexcept {
    CombatLabConfig config;
    config.player_spawn = Vec3{6.0F, 0.0F, 0.0F};
    config.dummy_spawns = {{{-7.0F, 3.0F, 0.0F},
                            {-7.0F, -3.0F, 0.0F},
                            {7.2F, 0.0F, 0.0F}}};
    return config;
}

bool start_action_and_reach_hit(
    CombatWorld& world,
    Action action,
    int startup_ticks) noexcept {
    if (!world.queue_action(action)) {
        return false;
    }
    world.tick(MovementInput{});
    tick_n(world, startup_ticks);
    return true;
}

bool vec_equal(const Vec3& lhs, const Vec3& rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool event_equal(const CombatEvent& lhs, const CombatEvent& rhs) noexcept {
    return lhs.kind == rhs.kind
        && lhs.tick == rhs.tick
        && lhs.attack == rhs.attack
        && lhs.target_index == rhs.target_index
        && lhs.hit_count == rhs.hit_count
        && lhs.feedback == rhs.feedback
        && vec_equal(lhs.position, rhs.position)
        && lhs.value == rhs.value;
}

bool snapshot_equal(
    const CombatSnapshot& lhs,
    const CombatSnapshot& rhs) noexcept {
    if (lhs.tick != rhs.tick
        || !vec_equal(lhs.player.position, rhs.player.position)
        || !vec_equal(lhs.player.velocity, rhs.player.velocity)
        || lhs.player.facing != rhs.player.facing
        || lhs.player.state != rhs.player.state
        || lhs.player.active_attack != rhs.player.active_attack
        || lhs.player.attack_phase != rhs.player.attack_phase
        || lhs.player.attack_elapsed_ticks != rhs.player.attack_elapsed_ticks
        || lhs.player.combo_stage != rhs.player.combo_stage
        || lhs.player.hit_stop_ticks != rhs.player.hit_stop_ticks
        || lhs.player.air_attack_available != rhs.player.air_attack_available) {
        return false;
    }

    for (std::size_t index = 0; index < kDummyCount; ++index) {
        const DummySnapshot& left = lhs.dummies[index];
        const DummySnapshot& right = rhs.dummies[index];
        if (!vec_equal(left.position, right.position)
            || !vec_equal(left.velocity, right.velocity)
            || left.kind != right.kind
            || left.reaction != right.reaction
            || left.armor != right.armor
            || left.hp != right.hp
            || left.max_hp != right.max_hp
            || left.break_value != right.break_value
            || left.max_break != right.max_break
            || left.break_window_ticks != right.break_window_ticks
            || left.hit_stop_ticks != right.hit_stop_ticks) {
            return false;
        }
    }

    return lhs.diagnostics.input_size == rhs.diagnostics.input_size
        && lhs.diagnostics.input_expired_count
            == rhs.diagnostics.input_expired_count
        && lhs.diagnostics.input_overflow_count
            == rhs.diagnostics.input_overflow_count
        && lhs.diagnostics.event_overflow_count
            == rhs.diagnostics.event_overflow_count;
}

bool drain_events_equal(CombatWorld& lhs, CombatWorld& rhs) noexcept {
    for (;;) {
        const auto left = lhs.try_pop_event();
        const auto right = rhs.try_pop_event();
        if (left.has_value() != right.has_value()) {
            return false;
        }
        if (!left.has_value()) {
            return true;
        }
        if (!event_equal(*left, *right)) {
            return false;
        }
    }
}

bool launcher_hit_and_finish(CombatWorld& world) noexcept {
    if (!start_action_and_reach_hit(world, Action::launcher, 7)) {
        return false;
    }
    return finish_attack(world, 100);
}

bool wait_for_target_grounded(
    CombatWorld& world,
    std::size_t target_index) noexcept {
    for (int tick = 0; tick < 240; ++tick) {
        const DummySnapshot& target =
            world.snapshot().dummies[target_index];
        if (target.position.z == 0.0F
            && target.reaction != ReactionState::airborne) {
            return true;
        }
        world.tick(MovementInput{});
    }
    const DummySnapshot& target = world.snapshot().dummies[target_index];
    return target.position.z == 0.0F
        && target.reaction != ReactionState::airborne;
}

bool wait_for_armor_restore(CombatWorld& world) noexcept {
    for (int tick = 0; tick < 260; ++tick) {
        if (world.snapshot().dummies[2].armor == ArmorState::armored) {
            return true;
        }
        world.tick(MovementInput{});
    }
    return world.snapshot().dummies[2].armor == ArmorState::armored;
}

arpg::test::Failure armored_launcher_suppresses_control_before_break() noexcept {
    CombatWorld launcher{heavy_target_config()};
    ARPG_REQUIRE(start_action_and_reach_hit(launcher, Action::launcher, 7));
    CombatSnapshot snapshot = launcher.snapshot();
    const DummySnapshot& launched = snapshot.dummies[2];
    ARPG_REQUIRE(launched.hp == 662);
    ARPG_REQUIRE(launched.break_value == 102);
    ARPG_REQUIRE(launched.armor == ArmorState::armored);
    ARPG_REQUIRE(launched.reaction == ReactionState::idle);
    ARPG_REQUIRE(vec_equal(launched.position, Vec3{7.2F, 0.0F, 0.0F}));
    ARPG_REQUIRE(vec_equal(launched.velocity, Vec3{}));
    ARPG_REQUIRE(launched.hit_stop_ticks == 5);
    while (const auto event = launcher.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::break_started);
    }

    CombatWorld repeated{heavy_target_config()};
    ARPG_REQUIRE(launcher_hit_and_finish(repeated));
    snapshot = repeated.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].hp == 662);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 102);
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::idle);
    ARPG_REQUIRE(vec_equal(snapshot.dummies[2].velocity, Vec3{}));
    drain_events(repeated);
    ARPG_REQUIRE(launcher_hit_and_finish(repeated));
    snapshot = repeated.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].hp == 624);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 84);
    ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::armored);
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::idle);
    ARPG_REQUIRE(vec_equal(snapshot.dummies[2].velocity, Vec3{}));
    return {};
}

arpg::test::Failure breaking_blow_has_exact_local_window() noexcept {
    CombatWorld world{heavy_target_config()};
    for (int hit = 0; hit < 6; ++hit) {
        ARPG_REQUIRE(launcher_hit_and_finish(world));
        drain_events(world);
    }
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].hp == 472);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 12);
    ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::armored);

    ARPG_REQUIRE(start_action_and_reach_hit(world, Action::launcher, 7));
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].hp == 434);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 0);
    ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::broken);
    ARPG_REQUIRE(snapshot.dummies[2].break_window_ticks == 180);
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::airborne);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.dummies[2].velocity.x, 1.2, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.dummies[2].velocity.z, 9.5, 1.0e-4));
    ARPG_REQUIRE(snapshot.dummies[2].hit_stop_ticks == 5);

    std::array<CombatEventKind, 4> kinds{};
    int event_count = 0;
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event_count < 4);
        kinds[static_cast<std::size_t>(event_count)] = event->kind;
        if (event->kind == CombatEventKind::break_started) {
            ARPG_REQUIRE(event->attack == AttackId::launcher);
            ARPG_REQUIRE(event->target_index == 2);
            ARPG_REQUIRE(event->feedback == FeedbackLevel::medium);
            ARPG_REQUIRE(event->tick == snapshot.tick - 1);
        }
        ++event_count;
    }
    ARPG_REQUIRE(event_count == 4);
    ARPG_REQUIRE(kinds[0] == CombatEventKind::swing);
    ARPG_REQUIRE(kinds[1] == CombatEventKind::hit);
    ARPG_REQUIRE(kinds[2] == CombatEventKind::break_started);
    ARPG_REQUIRE(kinds[3] == CombatEventKind::impact_summary);

    tick_n(world, 5);
    ARPG_REQUIRE(world.snapshot().dummies[2].break_window_ticks == 180);
    tick_n(world, 179);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::broken);
    ARPG_REQUIRE(snapshot.dummies[2].break_window_ticks == 1);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::armored);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 120);
    ARPG_REQUIRE(snapshot.dummies[2].break_window_ticks == 0);

    CombatLabConfig independent_config = heavy_target_config();
    independent_config.dummy_spawns[0] = Vec3{4.0F, 0.0F, 0.0F};
    CombatWorld independent{independent_config};
    for (int hit = 0; hit < 7; ++hit) {
        ARPG_REQUIRE(launcher_hit_and_finish(independent));
        drain_events(independent);
    }
    for (int tick = 0; tick < 24; ++tick) {
        independent.tick(MovementInput{-1, 0});
    }
    ARPG_REQUIRE(start_action_and_reach_hit(independent, Action::light, 5));
    snapshot = independent.snapshot();
    ARPG_REQUIRE(snapshot.player.hit_stop_ticks == 3);
    ARPG_REQUIRE(snapshot.dummies[0].hit_stop_ticks == 3);
    ARPG_REQUIRE(snapshot.dummies[2].hit_stop_ticks == 0);
    ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::broken);
    const std::uint16_t independent_window =
        snapshot.dummies[2].break_window_ticks;
    ARPG_REQUIRE(independent_window > 3);
    tick_n(independent, 3);
    ARPG_REQUIRE(
        independent.snapshot().dummies[2].break_window_ticks
        == independent_window - 3);

    CombatWorld reaction{heavy_target_config()};
    for (int hit = 0; hit < 7; ++hit) {
        ARPG_REQUIRE(launcher_hit_and_finish(reaction));
        drain_events(reaction);
    }
    for (int tick = 0; tick < 260; ++tick) {
        if (reaction.snapshot().dummies[2].break_window_ticks <= 22) {
            break;
        }
        reaction.tick(MovementInput{});
    }
    ARPG_REQUIRE(reaction.snapshot().dummies[2].break_window_ticks <= 22);
    ARPG_REQUIRE(reaction.snapshot().dummies[2].armor == ArmorState::broken);
    ARPG_REQUIRE(start_action_and_reach_hit(reaction, Action::launcher, 7));
    ARPG_REQUIRE(reaction.snapshot().dummies[2].reaction
                 == ReactionState::airborne);
    ARPG_REQUIRE(wait_for_armor_restore(reaction));
    snapshot = reaction.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::airborne);
    return {};
}

arpg::test::Failure defeated_has_priority_over_break_recovery() noexcept {
    CombatWorld world{heavy_target_config()};
    for (int hit = 0; hit < 18; ++hit) {
        ARPG_REQUIRE(launcher_hit_and_finish(world));
        ARPG_REQUIRE(wait_for_target_grounded(world, 2));
        drain_events(world);
    }
    const CombatSnapshot pre_defeat = world.snapshot();
    ARPG_REQUIRE(pre_defeat.dummies[2].hp == 16);
    ARPG_REQUIRE(pre_defeat.dummies[2].armor == ArmorState::broken);
    ARPG_REQUIRE(pre_defeat.dummies[2].break_window_ticks > 0);

    ARPG_REQUIRE(start_action_and_reach_hit(world, Action::launcher, 7));
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].hp == 0);
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::defeated);
    ARPG_REQUIRE(snapshot.dummies[2].break_window_ticks == 0);
    ARPG_REQUIRE(vec_equal(snapshot.dummies[2].velocity, Vec3{}));

    std::array<CombatEventKind, 4> kinds{};
    int event_count = 0;
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::break_started);
        ARPG_REQUIRE(event_count < 4);
        kinds[static_cast<std::size_t>(event_count)] = event->kind;
        ++event_count;
    }
    ARPG_REQUIRE(event_count == 4);
    ARPG_REQUIRE(kinds[0] == CombatEventKind::swing);
    ARPG_REQUIRE(kinds[1] == CombatEventKind::hit);
    ARPG_REQUIRE(kinds[2] == CombatEventKind::defeated);
    ARPG_REQUIRE(kinds[3] == CombatEventKind::impact_summary);

    tick_n(world, 5);
    tick_n(world, 90);
    const CombatSnapshot respawned = world.snapshot();
    ARPG_REQUIRE(respawned.dummies[2].reaction == ReactionState::respawning);
    ARPG_REQUIRE(respawned.dummies[2].armor == ArmorState::armored);
    ARPG_REQUIRE(respawned.dummies[2].break_value == 120);
    ARPG_REQUIRE(respawned.dummies[2].hp == 700);
    int respawn_events = 0;
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::respawned) {
            ++respawn_events;
        }
    }
    ARPG_REQUIRE(respawn_events == 1);
    return {};
}

arpg::test::Failure reset_reconstructs_runtime_and_emits_once() noexcept {
    const CombatLabConfig config = heavy_target_config();
    CombatWorld fresh{config};
    ARPG_REQUIRE(!fresh.try_pop_event().has_value());

    CombatWorld world{config};
    ARPG_REQUIRE(start_action_and_reach_hit(world, Action::launcher, 7));
    for (std::size_t index = 0; index < InputBuffer::kCapacity; ++index) {
        ARPG_REQUIRE(world.queue_action(Action::light));
    }
    ARPG_REQUIRE(!world.queue_action(Action::light));
    ARPG_REQUIRE(world.snapshot().diagnostics.input_overflow_count == 1);
    ARPG_REQUIRE(world.snapshot().dummies[2].hp < 700);

    world.reset();
    const CombatSnapshot reset_snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot_equal(reset_snapshot, fresh.snapshot()));
    const auto event = world.try_pop_event();
    ARPG_REQUIRE(event.has_value());
    ARPG_REQUIRE(event->kind == CombatEventKind::reset);
    ARPG_REQUIRE(event->tick == 0);
    ARPG_REQUIRE(event->attack == AttackId::none);
    ARPG_REQUIRE(event->target_index == 0xFF);
    ARPG_REQUIRE(event->hit_count == 0);
    ARPG_REQUIRE(vec_equal(event->position, config.player_spawn));
    ARPG_REQUIRE(event->value == 0);
    ARPG_REQUIRE(!world.try_pop_event().has_value());
    world.tick(MovementInput{});
    ARPG_REQUIRE(!world.try_pop_event().has_value());
    return {};
}

struct ScheduledAction final {
    int period;
    Action action;
    std::uint8_t bit;
};

struct ScheduledActions final {
    std::uint8_t requested{};
    std::uint8_t accepted{};
};

constexpr std::array<ScheduledAction, 4> kScheduledActions{{
    {37, Action::light, static_cast<std::uint8_t>(1U << 0U)},
    {181, Action::jump, static_cast<std::uint8_t>(1U << 1U)},
    {251, Action::launcher, static_cast<std::uint8_t>(1U << 2U)},
    {307, Action::launcher, static_cast<std::uint8_t>(1U << 3U)},
}};

ScheduledActions schedule_actions(CombatWorld& world, int tick) noexcept {
    ScheduledActions result{};
    for (const auto& scheduled : kScheduledActions) {
        if (tick % scheduled.period != 0) {
            continue;
        }
        result.requested |= scheduled.bit;
        if (world.queue_action(scheduled.action)) {
            result.accepted |= scheduled.bit;
        }
    }
    return result;
}

MovementInput scheduled_movement(int tick) noexcept {
    return {
        static_cast<std::int8_t>((tick / 120) % 2 == 0 ? 1 : -1),
        static_cast<std::int8_t>((tick / 180) % 2 == 0 ? 1 : -1),
    };
}

arpg::test::Failure replay_and_stress_are_deterministic_without_allocations() noexcept {
    CombatWorld left;
    CombatWorld right;
    for (int tick = 0; tick < 2400; ++tick) {
        const ScheduledActions left_scheduled = schedule_actions(left, tick);
        const ScheduledActions right_scheduled = schedule_actions(right, tick);
        ARPG_REQUIRE(left_scheduled.requested == right_scheduled.requested);
        ARPG_REQUIRE(left_scheduled.accepted == right_scheduled.accepted);
        ARPG_REQUIRE(left_scheduled.accepted == left_scheduled.requested);
        ARPG_REQUIRE(right_scheduled.accepted == right_scheduled.requested);
        left.tick(scheduled_movement(tick));
        right.tick(scheduled_movement(tick));
        ARPG_REQUIRE(snapshot_equal(left.snapshot(), right.snapshot()));
        ARPG_REQUIRE(drain_events_equal(left, right));
        if (tick > 0 && tick % 600 == 0) {
            left.reset();
            right.reset();
            ARPG_REQUIRE(snapshot_equal(left.snapshot(), right.snapshot()));
            ARPG_REQUIRE(drain_events_equal(left, right));
        }
    }

    CombatWorld stress;
    bool all_queued = true;
    std::uint32_t max_input_overflow = 0;
    std::uint32_t max_event_overflow = 0;
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    for (int tick = 0; tick < 36000; ++tick) {
        const ScheduledActions scheduled = schedule_actions(stress, tick);
        all_queued = scheduled.accepted == scheduled.requested && all_queued;
        stress.tick(scheduled_movement(tick));
        const CombatDiagnostics diagnostics = stress.snapshot().diagnostics;
        if (diagnostics.input_overflow_count > max_input_overflow) {
            max_input_overflow = diagnostics.input_overflow_count;
        }
        if (diagnostics.event_overflow_count > max_event_overflow) {
            max_event_overflow = diagnostics.event_overflow_count;
        }
        drain_events(stress);
        if (tick > 0 && tick % 3600 == 0) {
            stress.reset();
            drain_events(stress);
        }
    }
    ARPG_REQUIRE(all_queued);
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    ARPG_REQUIRE(max_input_overflow == 0);
    ARPG_REQUIRE(max_event_overflow == 0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"armored suppression before break",
     &armored_launcher_suppresses_control_before_break},
    {"breaking blow and exact local window",
     &breaking_blow_has_exact_local_window},
    {"Defeated priority over break recovery",
     &defeated_has_priority_over_break_recovery},
    {"complete reset and one reset event",
     &reset_reconstructs_runtime_and_emits_once},
    {"deterministic replay and allocation stress",
     &replay_and_stress_are_deterministic_without_allocations},
};

}  // namespace

arpg::test::TestSuite break_stress_suite() noexcept {
    return arpg::test::make_suite("break_stress", kCases);
}
