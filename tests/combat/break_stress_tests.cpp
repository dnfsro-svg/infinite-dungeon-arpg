#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "combat/combat_world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace arpg::combat;

CombatLabConfig heavy_target_config() noexcept {
    CombatLabConfig config;
    config.player_spawn = Vec3{6.0F, 0.0F, 0.0F};
    config.dummy_spawns = {{{-7.0F, 3.0F, 0.0F},
                            {-7.0F, -3.0F, 0.0F},
                            {7.2F, 0.0F, 0.0F}}};
    return config;
}

void tick_n(CombatWorld& world, int count) noexcept {
    for (int tick = 0; tick < count; ++tick) {
        world.tick(MovementInput{});
    }
}

bool finish_attack(CombatWorld& world) noexcept {
    for (int tick = 0; tick < 100; ++tick) {
        if (world.snapshot().player.active_attack == AttackId::none) {
            return true;
        }
        world.tick(MovementInput{});
    }
    return world.snapshot().player.active_attack == AttackId::none;
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

void drain_events(CombatWorld& world) noexcept {
    while (world.try_pop_event().has_value()) {
    }
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

bool heavy_hit_and_finish(CombatWorld& world) noexcept {
    if (!start_action_and_reach_hit(world, Action::heavy, 14)) {
        return false;
    }
    return finish_attack(world);
}

bool start_j1_j2_and_reach_second_hit(CombatWorld& world) noexcept {
    if (!start_action_and_reach_hit(world, Action::light, 5)) {
        return false;
    }
    tick_n(world, 3);
    tick_n(world, 3);
    if (!world.queue_action(Action::light)) {
        return false;
    }
    world.tick(MovementInput{});
    if (world.snapshot().player.active_attack != AttackId::j2) {
        return false;
    }
    tick_n(world, 6);
    return true;
}

bool light_combo_and_finish(CombatWorld& world) noexcept {
    if (!start_j1_j2_and_reach_second_hit(world)) {
        return false;
    }
    tick_n(world, 5);
    tick_n(world, 3);
    if (!world.queue_action(Action::light)) {
        return false;
    }
    world.tick(MovementInput{});
    if (world.snapshot().player.active_attack != AttackId::j3) {
        return false;
    }
    return finish_attack(world);
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

arpg::test::Failure armored_heavy_suppresses_control_before_break() noexcept {
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

    CombatWorld heavy{heavy_target_config()};
    ARPG_REQUIRE(heavy_hit_and_finish(heavy));
    snapshot = heavy.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].hp == 610);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 80);
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::idle);
    ARPG_REQUIRE(vec_equal(snapshot.dummies[2].velocity, Vec3{}));
    drain_events(heavy);
    ARPG_REQUIRE(heavy_hit_and_finish(heavy));
    snapshot = heavy.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].hp == 520);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 40);
    ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::armored);
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::idle);
    ARPG_REQUIRE(vec_equal(snapshot.dummies[2].velocity, Vec3{}));
    return {};
}

arpg::test::Failure breaking_blow_has_exact_local_window() noexcept {
    CombatWorld world{heavy_target_config()};
    ARPG_REQUIRE(heavy_hit_and_finish(world));
    drain_events(world);
    ARPG_REQUIRE(heavy_hit_and_finish(world));
    drain_events(world);

    ARPG_REQUIRE(start_action_and_reach_hit(world, Action::heavy, 14));
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].hp == 430);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 0);
    ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::broken);
    ARPG_REQUIRE(snapshot.dummies[2].break_window_ticks == 180);
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::knockdown);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.dummies[2].velocity.x, 7.0, 1.0e-4));
    ARPG_REQUIRE(snapshot.dummies[2].hit_stop_ticks == 7);

    std::array<CombatEventKind, 4> kinds{};
    int event_count = 0;
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event_count < 4);
        kinds[static_cast<std::size_t>(event_count)] = event->kind;
        if (event->kind == CombatEventKind::break_started) {
            ARPG_REQUIRE(event->attack == AttackId::heavy);
            ARPG_REQUIRE(event->target_index == 2);
            ARPG_REQUIRE(event->feedback == FeedbackLevel::heavy);
            ARPG_REQUIRE(event->tick == snapshot.tick - 1);
        }
        ++event_count;
    }
    ARPG_REQUIRE(event_count == 4);
    ARPG_REQUIRE(kinds[0] == CombatEventKind::swing);
    ARPG_REQUIRE(kinds[1] == CombatEventKind::hit);
    ARPG_REQUIRE(kinds[2] == CombatEventKind::break_started);
    ARPG_REQUIRE(kinds[3] == CombatEventKind::impact_summary);

    tick_n(world, 7);
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
    ARPG_REQUIRE(heavy_hit_and_finish(independent));
    drain_events(independent);
    ARPG_REQUIRE(heavy_hit_and_finish(independent));
    drain_events(independent);
    ARPG_REQUIRE(heavy_hit_and_finish(independent));
    drain_events(independent);
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
    ARPG_REQUIRE(heavy_hit_and_finish(reaction));
    drain_events(reaction);
    ARPG_REQUIRE(heavy_hit_and_finish(reaction));
    drain_events(reaction);
    ARPG_REQUIRE(heavy_hit_and_finish(reaction));
    drain_events(reaction);
    for (int tick = 0; tick < 260; ++tick) {
        if (reaction.snapshot().dummies[2].break_window_ticks <= 22) {
            break;
        }
        reaction.tick(MovementInput{});
    }
    ARPG_REQUIRE(reaction.snapshot().dummies[2].break_window_ticks <= 22);
    ARPG_REQUIRE(reaction.snapshot().dummies[2].armor == ArmorState::broken);
    ARPG_REQUIRE(start_action_and_reach_hit(reaction, Action::heavy, 14));
    ARPG_REQUIRE(reaction.snapshot().dummies[2].reaction
                 == ReactionState::knockdown);
    ARPG_REQUIRE(wait_for_armor_restore(reaction));
    snapshot = reaction.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::knockdown);
    return {};
}

arpg::test::Failure defeated_has_priority_over_break_recovery() noexcept {
    CombatWorld world{heavy_target_config()};
    for (int cycle = 0; cycle < 2; ++cycle) {
        for (int hit = 0; hit < 3; ++hit) {
            ARPG_REQUIRE(heavy_hit_and_finish(world));
            drain_events(world);
        }
        ARPG_REQUIRE(world.snapshot().dummies[2].armor == ArmorState::broken);
        ARPG_REQUIRE(wait_for_armor_restore(world));
    }
    ARPG_REQUIRE(world.snapshot().dummies[2].hp == 160);
    ARPG_REQUIRE(heavy_hit_and_finish(world));
    drain_events(world);
    ARPG_REQUIRE(world.snapshot().dummies[2].hp == 70);
    ARPG_REQUIRE(world.snapshot().dummies[2].break_value == 80);

    ARPG_REQUIRE(start_action_and_reach_hit(world, Action::heavy, 14));
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[2].hp == 0);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 80);
    ARPG_REQUIRE(snapshot.dummies[2].armor == ArmorState::armored);
    ARPG_REQUIRE(snapshot.dummies[2].reaction == ReactionState::defeated);
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

    CombatWorld broken_death{heavy_target_config()};
    ARPG_REQUIRE(light_combo_and_finish(broken_death));
    drain_events(broken_death);
    ARPG_REQUIRE(light_combo_and_finish(broken_death));
    drain_events(broken_death);
    ARPG_REQUIRE(start_j1_j2_and_reach_second_hit(broken_death));
    ARPG_REQUIRE(finish_attack(broken_death));
    drain_events(broken_death);
    ARPG_REQUIRE(broken_death.snapshot().dummies[2].hp == 410);
    ARPG_REQUIRE(broken_death.snapshot().dummies[2].break_value == 14);

    ARPG_REQUIRE(heavy_hit_and_finish(broken_death));
    drain_events(broken_death);
    ARPG_REQUIRE(broken_death.snapshot().dummies[2].armor
                 == ArmorState::broken);
    ARPG_REQUIRE(broken_death.snapshot().dummies[2].hp == 320);
    for (int hit = 0; hit < 3; ++hit) {
        ARPG_REQUIRE(heavy_hit_and_finish(broken_death));
        drain_events(broken_death);
    }
    ARPG_REQUIRE(broken_death.snapshot().dummies[2].hp == 50);
    ARPG_REQUIRE(broken_death.snapshot().dummies[2].armor
                 == ArmorState::broken);
    ARPG_REQUIRE(broken_death.snapshot().dummies[2].break_window_ticks > 0);

    ARPG_REQUIRE(start_j1_j2_and_reach_second_hit(broken_death));
    CombatSnapshot broken_snapshot = broken_death.snapshot();
    ARPG_REQUIRE(broken_snapshot.dummies[2].hp == 0);
    ARPG_REQUIRE(broken_snapshot.dummies[2].reaction
                 == ReactionState::defeated);
    ARPG_REQUIRE(broken_snapshot.dummies[2].armor == ArmorState::broken);
    ARPG_REQUIRE(broken_snapshot.dummies[2].break_window_ticks == 0);
    ARPG_REQUIRE(vec_equal(broken_snapshot.dummies[2].velocity, Vec3{}));
    while (const auto event = broken_death.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::break_started);
    }

    int defeated_ticks = 0;
    while (defeated_ticks < 120
           && broken_death.snapshot().dummies[2].reaction
                  == ReactionState::defeated) {
        broken_snapshot = broken_death.snapshot();
        ARPG_REQUIRE(broken_snapshot.dummies[2].armor == ArmorState::broken);
        ARPG_REQUIRE(broken_snapshot.dummies[2].break_window_ticks == 0);
        broken_death.tick(MovementInput{});
        ++defeated_ticks;
    }
    broken_snapshot = broken_death.snapshot();
    ARPG_REQUIRE(broken_snapshot.dummies[2].reaction
                 == ReactionState::respawning);
    ARPG_REQUIRE(broken_snapshot.dummies[2].armor == ArmorState::armored);
    ARPG_REQUIRE(broken_snapshot.dummies[2].break_value == 120);
    ARPG_REQUIRE(broken_snapshot.dummies[2].hp == 700);
    int respawn_events = 0;
    while (const auto event = broken_death.try_pop_event()) {
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
    ARPG_REQUIRE(start_action_and_reach_hit(world, Action::heavy, 14));
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

std::uint8_t expected_action_mask(int tick) noexcept {
    std::uint8_t mask = 0;
    if (tick % 37 == 0) {
        mask |= 1U << 0U;
    }
    if (tick % 181 == 0) {
        mask |= 1U << 1U;
    }
    if (tick % 251 == 0) {
        mask |= 1U << 2U;
    }
    if (tick % 307 == 0) {
        mask |= 1U << 3U;
    }
    return mask;
}

std::uint8_t schedule_actions(CombatWorld& world, int tick) noexcept {
    std::uint8_t accepted = 0;
    if (tick % 37 == 0) {
        if (world.queue_action(Action::light)) {
            accepted |= 1U << 0U;
        }
    }
    if (tick % 181 == 0) {
        if (world.queue_action(Action::jump)) {
            accepted |= 1U << 1U;
        }
    }
    if (tick % 251 == 0) {
        if (world.queue_action(Action::heavy)) {
            accepted |= 1U << 2U;
        }
    }
    if (tick % 307 == 0) {
        if (world.queue_action(Action::launcher)) {
            accepted |= 1U << 3U;
        }
    }
    return accepted;
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
        const std::uint8_t left_accepted = schedule_actions(left, tick);
        const std::uint8_t right_accepted = schedule_actions(right, tick);
        ARPG_REQUIRE(left_accepted == right_accepted);
        ARPG_REQUIRE(left_accepted == expected_action_mask(tick));
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
        all_queued = schedule_actions(stress, tick)
                == expected_action_mask(tick)
            && all_queued;
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
     &armored_heavy_suppresses_control_before_break},
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
    return {"break_stress", kCases, sizeof(kCases) / sizeof(kCases[0])};
}
