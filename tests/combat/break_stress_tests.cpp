#include "allocation_probe.hpp"
#include "combat_test_support.hpp"
#include "test_framework.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_pool.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using namespace arpg::combat;
using arpg::test::drain_events;
using arpg::test::finish_attack;
using arpg::test::tick_n;

CombatLabConfig heavy_target_config() noexcept {
    CombatLabConfig config;
    config.player_spawn = Vec3{10.6F, 0.0F, 0.0F};
    config.dummy_spawns = {{{-11.0F, 5.0F, 0.0F},
                            {-11.0F, -5.0F, 0.0F},
                            {11.8F, 0.0F, 0.0F}}};
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
        const MonsterSnapshot& left = lhs.monsters[index];
        const MonsterSnapshot& right = rhs.monsters[index];
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
        const MonsterSnapshot& target =
            world.snapshot().monsters[target_index];
        if (target.position.z == 0.0F
            && target.reaction != ReactionState::airborne) {
            return true;
        }
        world.tick(MovementInput{});
    }
    const MonsterSnapshot& target = world.snapshot().monsters[target_index];
    return target.position.z == 0.0F
        && target.reaction != ReactionState::airborne;
}

bool wait_for_armor_restore(CombatWorld& world) noexcept {
    for (int tick = 0; tick < 260; ++tick) {
        if (world.snapshot().monsters[2].armor == ArmorState::armored) {
            return true;
        }
        world.tick(MovementInput{});
    }
    return world.snapshot().monsters[2].armor == ArmorState::armored;
}

arpg::test::Failure armored_launcher_suppresses_control_before_break() noexcept {
    CombatWorld launcher{heavy_target_config()};
    ARPG_REQUIRE(start_action_and_reach_hit(launcher, Action::launcher, 7));
    CombatSnapshot snapshot = launcher.snapshot();
    const MonsterSnapshot& launched = snapshot.monsters[2];
    ARPG_REQUIRE(launched.hp == 662);
    ARPG_REQUIRE(launched.break_value == 102);
    ARPG_REQUIRE(launched.armor == ArmorState::armored);
    ARPG_REQUIRE(launched.reaction == ReactionState::idle);
    ARPG_REQUIRE(vec_equal(launched.position, Vec3{11.8F, 0.0F, 0.0F}));
    ARPG_REQUIRE(vec_equal(launched.velocity, Vec3{}));
    ARPG_REQUIRE(launched.hit_stop_ticks == 5);
    while (const auto event = launcher.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::break_started);
    }

    CombatWorld repeated{heavy_target_config()};
    ARPG_REQUIRE(launcher_hit_and_finish(repeated));
    snapshot = repeated.snapshot();
    ARPG_REQUIRE(snapshot.monsters[2].hp == 662);
    ARPG_REQUIRE(snapshot.monsters[2].break_value == 102);
    ARPG_REQUIRE(snapshot.monsters[2].reaction == ReactionState::idle);
    ARPG_REQUIRE(vec_equal(snapshot.monsters[2].velocity, Vec3{}));
    drain_events(repeated);
    ARPG_REQUIRE(launcher_hit_and_finish(repeated));
    snapshot = repeated.snapshot();
    ARPG_REQUIRE(snapshot.monsters[2].hp == 624);
    ARPG_REQUIRE(snapshot.monsters[2].break_value == 84);
    ARPG_REQUIRE(snapshot.monsters[2].armor == ArmorState::armored);
    ARPG_REQUIRE(snapshot.monsters[2].reaction == ReactionState::idle);
    ARPG_REQUIRE(vec_equal(snapshot.monsters[2].velocity, Vec3{}));
    return {};
}

arpg::test::Failure breaking_blow_has_exact_local_window() noexcept {
    CombatWorld world{heavy_target_config()};
    for (int hit = 0; hit < 6; ++hit) {
        ARPG_REQUIRE(launcher_hit_and_finish(world));
        drain_events(world);
    }
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[2].hp == 472);
    ARPG_REQUIRE(snapshot.monsters[2].break_value == 12);
    ARPG_REQUIRE(snapshot.monsters[2].armor == ArmorState::armored);

    ARPG_REQUIRE(start_action_and_reach_hit(world, Action::launcher, 7));
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[2].hp == 434);
    ARPG_REQUIRE(snapshot.monsters[2].break_value == 0);
    ARPG_REQUIRE(snapshot.monsters[2].armor == ArmorState::broken);
    ARPG_REQUIRE(snapshot.monsters[2].break_window_ticks == 180);
    ARPG_REQUIRE(snapshot.monsters[2].reaction == ReactionState::airborne);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[2].velocity.x, 1.2, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[2].velocity.z, 9.5, 1.0e-4));
    ARPG_REQUIRE(snapshot.monsters[2].hit_stop_ticks == 5);

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
    ARPG_REQUIRE(world.snapshot().monsters[2].break_window_ticks == 180);
    tick_n(world, 179);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[2].armor == ArmorState::broken);
    ARPG_REQUIRE(snapshot.monsters[2].break_window_ticks == 1);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[2].armor == ArmorState::armored);
    ARPG_REQUIRE(snapshot.monsters[2].break_value == 120);
    ARPG_REQUIRE(snapshot.monsters[2].break_window_ticks == 0);

    CombatLabConfig independent_config = heavy_target_config();
    independent_config.dummy_spawns[0] = Vec3{9.0F, 0.0F, 0.0F};
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
    ARPG_REQUIRE(snapshot.monsters[0].hit_stop_ticks == 3);
    ARPG_REQUIRE(snapshot.monsters[2].hit_stop_ticks == 0);
    ARPG_REQUIRE(snapshot.monsters[2].armor == ArmorState::broken);
    const std::uint16_t independent_window =
        snapshot.monsters[2].break_window_ticks;
    ARPG_REQUIRE(independent_window > 3);
    tick_n(independent, 3);
    ARPG_REQUIRE(
        independent.snapshot().monsters[2].break_window_ticks
        == independent_window - 3);

    CombatWorld reaction{heavy_target_config()};
    for (int hit = 0; hit < 7; ++hit) {
        ARPG_REQUIRE(launcher_hit_and_finish(reaction));
        drain_events(reaction);
    }
    for (int tick = 0; tick < 260; ++tick) {
        if (reaction.snapshot().monsters[2].break_window_ticks <= 22) {
            break;
        }
        reaction.tick(MovementInput{});
    }
    ARPG_REQUIRE(reaction.snapshot().monsters[2].break_window_ticks <= 22);
    ARPG_REQUIRE(reaction.snapshot().monsters[2].armor == ArmorState::broken);
    ARPG_REQUIRE(start_action_and_reach_hit(reaction, Action::launcher, 7));
    ARPG_REQUIRE(reaction.snapshot().monsters[2].reaction
                 == ReactionState::airborne);
    ARPG_REQUIRE(wait_for_armor_restore(reaction));
    snapshot = reaction.snapshot();
    ARPG_REQUIRE(snapshot.monsters[2].reaction == ReactionState::airborne);
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
    ARPG_REQUIRE(pre_defeat.monsters[2].hp == 16);
    ARPG_REQUIRE(pre_defeat.monsters[2].armor == ArmorState::armored);
    ARPG_REQUIRE(pre_defeat.monsters[2].break_window_ticks == 0);

    ARPG_REQUIRE(start_action_and_reach_hit(world, Action::launcher, 7));
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[2].hp == 0);
    ARPG_REQUIRE(snapshot.monsters[2].reaction == ReactionState::defeated);
    ARPG_REQUIRE(snapshot.monsters[2].break_window_ticks == 0);
    ARPG_REQUIRE(vec_equal(snapshot.monsters[2].velocity, Vec3{}));

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
    ARPG_REQUIRE(respawned.monsters[2].reaction == ReactionState::respawning);
    ARPG_REQUIRE(respawned.monsters[2].armor == ArmorState::armored);
    ARPG_REQUIRE(respawned.monsters[2].break_value == 120);
    ARPG_REQUIRE(respawned.monsters[2].hp == 700);
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
    ARPG_REQUIRE(world.snapshot().monsters[2].hp < 700);

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

static_assert(
    kScheduledActions.size() == 4U &&
        kScheduledActions[0].period == 37 &&
        kScheduledActions[0].action == Action::light &&
        kScheduledActions[0].bit == static_cast<std::uint8_t>(1U << 0U) &&
        kScheduledActions[1].period == 181 &&
        kScheduledActions[1].action == Action::jump &&
        kScheduledActions[1].bit == static_cast<std::uint8_t>(1U << 1U) &&
        kScheduledActions[2].period == 251 &&
        kScheduledActions[2].action == Action::launcher &&
        kScheduledActions[2].bit == static_cast<std::uint8_t>(1U << 2U) &&
        kScheduledActions[3].period == 307 &&
        kScheduledActions[3].action == Action::launcher &&
        kScheduledActions[3].bit == static_cast<std::uint8_t>(1U << 3U),
    "stress action schedule contract changed");

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

CombatWorld all_roles_world() noexcept {
    EncounterWave wave{};
    wave.spawn_count = static_cast<std::uint8_t>(MonsterId::count);
    for (std::size_t index = 0; index < wave.spawn_count; ++index) {
        wave.spawns[index] = MonsterSpawnSpec{
            static_cast<MonsterId>(index),
            Vec3{2.0F + static_cast<float>(index % 4U) * 1.1F,
                -1.5F + static_cast<float>(index / 4U) * 3.0F, 0.0F},
        };
    }
    CombatEncounterConfig config{};
    config.wave = wave;
    return CombatWorld{config};
}

MonsterHandle first_active_owner(const CombatWorld& world) noexcept {
    const CombatSnapshot snapshot = world.snapshot();
    return {0U, snapshot.monsters[0].generation};
}

bool same_projectiles(
    const CombatSnapshot& lhs,
    const CombatSnapshot& rhs) noexcept {
    if (lhs.projectile_count != rhs.projectile_count) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.projectiles.size(); ++index) {
        const ProjectileSnapshot& a = lhs.projectiles[index];
        const ProjectileSnapshot& b = rhs.projectiles[index];
        if (a.active != b.active || a.generation != b.generation
                || a.owner.index != b.owner.index
                || a.owner.generation != b.owner.generation
                || !vec_equal(a.position, b.position)
                || !vec_equal(a.velocity, b.velocity)
                || a.lifetime_ticks != b.lifetime_ticks
                || a.damage != b.damage || a.radius != b.radius) {
            return false;
        }
    }
    return true;
}

bool same_monster_runtime(
    const MonsterRuntime& lhs,
    const MonsterRuntime& rhs) noexcept {
    return lhs.active == rhs.active && lhs.generation == rhs.generation
        && lhs.id == rhs.id && lhs.kind == rhs.kind
        && vec_equal(lhs.spawn, rhs.spawn)
        && vec_equal(lhs.position, rhs.position)
        && vec_equal(lhs.velocity, rhs.velocity)
        && lhs.facing == rhs.facing && lhs.reaction == rhs.reaction
        && lhs.armor == rhs.armor && lhs.reaction_ticks == rhs.reaction_ticks
        && lhs.ai_phase == rhs.ai_phase && lhs.ai_ticks == rhs.ai_ticks
        && lhs.attack_serial == rhs.attack_serial
        && lhs.contact_attack_resolved == rhs.contact_attack_resolved
        && lhs.hp == rhs.hp && lhs.max_hp == rhs.max_hp
        && lhs.break_value == rhs.break_value && lhs.max_break == rhs.max_break
        && lhs.shield == rhs.shield && lhs.max_shield == rhs.max_shield
        && lhs.shield_ticks == rhs.shield_ticks
        && lhs.max_shield_ticks == rhs.max_shield_ticks
        && lhs.break_window_ticks == rhs.break_window_ticks
        && lhs.hit_stop_ticks == rhs.hit_stop_ticks
        && lhs.owner_transient_counter == rhs.owner_transient_counter
        && vec_equal(lhs.attack_target_position, rhs.attack_target_position)
        && vec_equal(lhs.attack_vector, rhs.attack_vector);
}

bool same_hazards(
    const CombatSnapshot& lhs,
    const CombatSnapshot& rhs) noexcept {
    if (lhs.hazard_count != rhs.hazard_count) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.hazards.size(); ++index) {
        const HazardSnapshot& a = lhs.hazards[index];
        const HazardSnapshot& b = rhs.hazards[index];
        if (a.active != b.active || a.generation != b.generation
                || a.owner.index != b.owner.index
                || a.owner.generation != b.owner.generation
                || !vec_equal(a.center, b.center) || a.radius != b.radius
                || a.telegraph_ticks != b.telegraph_ticks
                || a.active_ticks != b.active_ticks
                || a.lifetime_ticks != b.lifetime_ticks
                || a.damage_interval_ticks != b.damage_interval_ticks
                || a.player_latched != b.player_latched
                || a.damage != b.damage) {
            return false;
        }
    }
    return true;
}

bool same_snapshot(
    const CombatSnapshot& lhs,
    const CombatSnapshot& rhs) noexcept {
    const PlayerSnapshot& left_player = lhs.player;
    const PlayerSnapshot& right_player = rhs.player;
    if (lhs.tick != rhs.tick
        || !vec_equal(left_player.position, right_player.position)
        || !vec_equal(left_player.velocity, right_player.velocity)
        || left_player.facing != right_player.facing
        || left_player.state != right_player.state
        || left_player.active_attack != right_player.active_attack
        || left_player.attack_phase != right_player.attack_phase
        || left_player.attack_elapsed_ticks != right_player.attack_elapsed_ticks
        || left_player.combo_stage != right_player.combo_stage
        || left_player.hit_stop_ticks != right_player.hit_stop_ticks
        || left_player.air_attack_available != right_player.air_attack_available
        || left_player.hp != right_player.hp
        || left_player.max_hp != right_player.max_hp
        || left_player.hurt_ticks != right_player.hurt_ticks
        || left_player.invulnerability_ticks != right_player.invulnerability_ticks
        || lhs.monster_count != rhs.monster_count
        || lhs.projectile_count != rhs.projectile_count
        || lhs.hazard_count != rhs.hazard_count) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.monsters.size(); ++index) {
        const MonsterSnapshot& left = lhs.monsters[index];
        const MonsterSnapshot& right = rhs.monsters[index];
        if (left.active != right.active || left.generation != right.generation
            || left.id != right.id || !vec_equal(left.spawn, right.spawn)
            || !vec_equal(left.position, right.position)
            || !vec_equal(left.velocity, right.velocity)
            || left.kind != right.kind || left.facing != right.facing
            || left.reaction != right.reaction || left.armor != right.armor
            || left.hp != right.hp || left.max_hp != right.max_hp
            || left.break_value != right.break_value
            || left.max_break != right.max_break || left.shield != right.shield
            || left.max_shield != right.max_shield
            || left.shield_ticks != right.shield_ticks
            || left.max_shield_ticks != right.max_shield_ticks
            || left.break_window_ticks != right.break_window_ticks
            || left.hit_stop_ticks != right.hit_stop_ticks
            || left.ai_phase != right.ai_phase
            || !vec_equal(left.attack_target_position, right.attack_target_position)
            || !vec_equal(left.attack_vector, right.attack_vector)) {
            return false;
        }
    }

    if (!same_projectiles(lhs, rhs) || !same_hazards(lhs, rhs)) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.dummies.size(); ++index) {
        const MonsterSnapshot& left = lhs.dummies[index];
        const MonsterSnapshot& right = rhs.dummies[index];
        if (left.active != right.active || left.generation != right.generation
            || left.id != right.id || !vec_equal(left.spawn, right.spawn)
            || !vec_equal(left.position, right.position)
            || !vec_equal(left.velocity, right.velocity)
            || left.kind != right.kind || left.facing != right.facing
            || left.reaction != right.reaction || left.armor != right.armor
            || left.hp != right.hp || left.max_hp != right.max_hp
            || left.break_value != right.break_value
            || left.max_break != right.max_break || left.shield != right.shield
            || left.max_shield != right.max_shield
            || left.shield_ticks != right.shield_ticks
            || left.max_shield_ticks != right.max_shield_ticks
            || left.break_window_ticks != right.break_window_ticks
            || left.hit_stop_ticks != right.hit_stop_ticks
            || left.ai_phase != right.ai_phase
            || !vec_equal(left.attack_target_position, right.attack_target_position)
            || !vec_equal(left.attack_vector, right.attack_vector)) {
            return false;
        }
    }

    const CombatDiagnostics& left = lhs.diagnostics;
    const CombatDiagnostics& right = rhs.diagnostics;
    return left.input_size == right.input_size
        && left.input_expired_count == right.input_expired_count
        && left.input_overflow_count == right.input_overflow_count
        && left.event_overflow_count == right.event_overflow_count
        && left.projectile_saturation_count == right.projectile_saturation_count
        && left.projectile_invalid_owner_count
            == right.projectile_invalid_owner_count
        && left.hazard_saturation_count == right.hazard_saturation_count
        && left.hazard_invalid_owner_count == right.hazard_invalid_owner_count
        && left.effect_owner_count == right.effect_owner_count
        && left.active_effect_count == right.active_effect_count
        && left.effect_overflow_count == right.effect_overflow_count
        && left.effect_command_overflow_count
            == right.effect_command_overflow_count;
}

arpg::test::Failure fixed_3600_tick_script_is_deterministic_replay() noexcept {
    CombatWorld first;
    CombatWorld second;
    for (std::uint32_t tick = 0; tick < 3600U; ++tick) {
        const bool pulse = tick % 41U == 0U;
        if (pulse) {
            ARPG_REQUIRE(first.queue_action(Action::light));
            ARPG_REQUIRE(second.queue_action(Action::light));
        }
        const MovementInput movement{
            static_cast<std::int8_t>(tick % 120U < 60U ? 1 : -1), 0};
        first.tick(movement);
        second.tick(movement);
        ARPG_REQUIRE(same_snapshot(first.snapshot(), second.snapshot()));
        ARPG_REQUIRE(drain_events_equal(first, second));
    }
    return {};
}

CombatEncounterConfig mixed_role_golden_config() noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 5U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::water_bulwark, Vec3{3.0F, 0.0F, 0.0F}};
    config.wave.spawns[1] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{1.5F, 1.0F, 0.0F}};
    config.wave.spawns[2] = MonsterSpawnSpec{
        MonsterId::lightning_shooter, Vec3{4.5F, 0.0F, 0.0F}};
    config.wave.spawns[3] = MonsterSpawnSpec{
        MonsterId::water_support, Vec3{4.0F, 2.0F, 0.0F}};
    config.wave.spawns[4] = MonsterSpawnSpec{
        MonsterId::chaos_hazard, Vec3{4.0F, -2.0F, 0.0F}};
    return config;
}

arpg::test::Failure mixed_role_golden_replay_preserves_public_behavior() noexcept {
    CombatWorld world{mixed_role_golden_config()};
    std::array<CombatEvent, 8> early_events{};
    std::size_t early_event_count = 0U;
    CombatSnapshot tick_45{};
    CombatSnapshot tick_180{};
    for (std::uint32_t tick = 0; tick <= 180U; ++tick) {
        if (tick % 90U == 0U) {
            ARPG_REQUIRE(world.queue_action(Action::light));
        }
        world.tick(MovementInput{
            static_cast<std::int8_t>(tick % 120U < 60U ? 1 : -1), 0});
        while (const auto event = world.try_pop_event()) {
            if (tick <= 45U) {
                ARPG_REQUIRE(early_event_count < early_events.size());
                early_events[early_event_count] = *event;
                ++early_event_count;
            }
        }
        if (tick == 45U) tick_45 = world.snapshot();
        if (tick == 180U) tick_180 = world.snapshot();
    }

    ARPG_REQUIRE(early_event_count == 5U);
    ARPG_REQUIRE(early_events[0].kind == CombatEventKind::swing);
    ARPG_REQUIRE(early_events[0].tick == 0U);
    ARPG_REQUIRE(early_events[1].kind == CombatEventKind::hit);
    ARPG_REQUIRE(early_events[1].tick == 5U);
    ARPG_REQUIRE(early_events[1].target_index == 1U);
    ARPG_REQUIRE(early_events[1].value == 28);
    ARPG_REQUIRE(early_events[2].kind == CombatEventKind::impact_summary);
    ARPG_REQUIRE(early_events[2].tick == 5U);
    ARPG_REQUIRE(early_events[3].kind == CombatEventKind::player_hit);
    ARPG_REQUIRE(early_events[3].tick == 39U);
    ARPG_REQUIRE(early_events[3].value == 45);
    ARPG_REQUIRE(early_events[4].kind == CombatEventKind::player_hurt_started);
    ARPG_REQUIRE(early_events[4].tick == 39U);
    ARPG_REQUIRE(early_events[4].value == 45);

    ARPG_REQUIRE(tick_45.tick == 46U);
    ARPG_REQUIRE(tick_45.player.hp == 955);
    ARPG_REQUIRE(tick_45.monster_count == 5U);
    ARPG_REQUIRE(tick_45.monsters[0].id == MonsterId::water_bulwark);
    ARPG_REQUIRE(tick_45.monsters[0].ai_phase == MonsterAiPhase::telegraph);
    ARPG_REQUIRE(tick_45.monsters[0].shield == 90);
    ARPG_REQUIRE(tick_45.monsters[0].shield_ticks == 119U);
    ARPG_REQUIRE(tick_45.monsters[1].id == MonsterId::chaos_chaser);
    ARPG_REQUIRE(tick_45.monsters[1].hp == 232);
    ARPG_REQUIRE(tick_45.monsters[1].ai_phase == MonsterAiPhase::recovery);
    ARPG_REQUIRE(tick_45.monsters[2].id == MonsterId::lightning_shooter);
    ARPG_REQUIRE(tick_45.monsters[2].ai_phase == MonsterAiPhase::recovery);
    ARPG_REQUIRE(tick_45.monsters[3].id == MonsterId::water_support);
    ARPG_REQUIRE(tick_45.monsters[3].ai_phase == MonsterAiPhase::recovery);
    ARPG_REQUIRE(tick_45.monsters[4].id == MonsterId::chaos_hazard);
    ARPG_REQUIRE(tick_45.monsters[4].ai_phase == MonsterAiPhase::telegraph);
    ARPG_REQUIRE(tick_45.hazard_count == 1U);
    ARPG_REQUIRE(tick_45.hazards[0].active);
    ARPG_REQUIRE(tick_45.hazards[0].owner.index == 4U);
    ARPG_REQUIRE(tick_45.diagnostics.effect_owner_count == 1U);
    ARPG_REQUIRE(tick_45.diagnostics.active_effect_count == 1U);
    ARPG_REQUIRE(tick_45.diagnostics.effect_overflow_count == 0U);
    ARPG_REQUIRE(tick_45.diagnostics.effect_command_overflow_count == 0U);

    ARPG_REQUIRE(tick_180.tick == 181U);
    ARPG_REQUIRE(tick_180.player.hp == 845);
    ARPG_REQUIRE(tick_180.monsters[0].shield == 0);
    ARPG_REQUIRE(tick_180.monsters[0].shield_ticks == 0U);
    ARPG_REQUIRE(tick_180.hazard_count == 1U);
    ARPG_REQUIRE(tick_180.diagnostics.effect_owner_count == 1U);
    ARPG_REQUIRE(tick_180.diagnostics.active_effect_count == 0U);
    ARPG_REQUIRE(tick_180.diagnostics.effect_overflow_count == 0U);
    ARPG_REQUIRE(tick_180.diagnostics.effect_command_overflow_count == 0U);
    ARPG_REQUIRE(tick_180.diagnostics.event_overflow_count == 0U);
    return {};
}

arpg::test::Failure all_monster_roles_tick_without_allocation_or_overflow() noexcept {
    CombatWorld world = all_roles_world();
    for (int tick = 0; tick < 180; ++tick) {
        world.tick(scheduled_movement(tick));
        drain_events(world);
    }

    const std::uint64_t allocations_before = arpg::test::allocation_count();
    for (int tick = 0; tick < 12000; ++tick) {
        world.tick(scheduled_movement(tick));
        drain_events(world);
    }
    const CombatSnapshot final = world.snapshot();
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    ARPG_REQUIRE(final.monster_count <= kMonsterCapacity);
    ARPG_REQUIRE(final.projectile_count <= kProjectileCapacity);
    ARPG_REQUIRE(final.hazard_count <= kHazardCapacity);
    ARPG_REQUIRE(final.diagnostics.event_overflow_count == 0U);
    ARPG_REQUIRE(final.diagnostics.projectile_saturation_count == 0U);
    ARPG_REQUIRE(final.diagnostics.hazard_saturation_count == 0U);
    ARPG_REQUIRE(final.diagnostics.projectile_invalid_owner_count == 0U);
    ARPG_REQUIRE(final.diagnostics.hazard_invalid_owner_count == 0U);
    return {};
}

arpg::test::Failure full_pools_reject_without_mutation_and_saturate_diagnostics() noexcept {
    MonsterPool monsters;
    for (std::size_t index = 0; index < kMonsterCapacity; ++index) {
        ARPG_REQUIRE(monsters.spawn(MonsterId::chaos_chaser,
            Vec3{static_cast<float>(index), 0.0F, 0.0F}).has_value());
    }
    const auto monster_slots = monsters.slots();
    ARPG_REQUIRE(!monsters.spawn(MonsterId::fire_bomber, Vec3{}).has_value());
    for (std::size_t index = 0; index < monster_slots.size(); ++index) {
        ARPG_REQUIRE(same_monster_runtime(
            monsters.slots()[index], monster_slots[index]));
    }

    CombatWorld projectile_world = all_roles_world();
    const MonsterHandle projectile_owner = first_active_owner(projectile_world);
    for (std::size_t index = 0; index < kProjectileCapacity; ++index) {
        ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_projectile(
            projectile_world, projectile_owner));
    }
    const CombatSnapshot projectiles_before = projectile_world.snapshot();
    arpg::test::CombatWorldTestAccess::set_saturation_counts(
        projectile_world, (std::numeric_limits<std::uint32_t>::max)(), 0U);
    ARPG_REQUIRE(!arpg::test::CombatWorldTestAccess::spawn_projectile(
        projectile_world, projectile_owner));
    const CombatSnapshot projectiles_after = projectile_world.snapshot();
    ARPG_REQUIRE(same_projectiles(projectiles_before, projectiles_after));
    ARPG_REQUIRE(projectiles_after.diagnostics.projectile_saturation_count
        == (std::numeric_limits<std::uint32_t>::max)());

    CombatWorld hazard_world = all_roles_world();
    const MonsterHandle hazard_owner = first_active_owner(hazard_world);
    for (std::size_t index = 0; index < kHazardCapacity; ++index) {
        ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_hazard(
            hazard_world, hazard_owner));
    }
    const CombatSnapshot hazards_before = hazard_world.snapshot();
    arpg::test::CombatWorldTestAccess::set_saturation_counts(
        hazard_world, 0U, (std::numeric_limits<std::uint32_t>::max)());
    ARPG_REQUIRE(!arpg::test::CombatWorldTestAccess::spawn_hazard(
        hazard_world, hazard_owner));
    const CombatSnapshot hazards_after = hazard_world.snapshot();
    ARPG_REQUIRE(same_hazards(hazards_before, hazards_after));
    ARPG_REQUIRE(hazards_after.diagnostics.hazard_saturation_count
        == (std::numeric_limits<std::uint32_t>::max)());

    CombatWorld invalid_owner_world = all_roles_world();
    const CombatSnapshot invalid_before = invalid_owner_world.snapshot();
    const MonsterHandle invalid_owner{0xFFFFU, 0U};
    arpg::test::CombatWorldTestAccess::set_invalid_owner_counts(
        invalid_owner_world, (std::numeric_limits<std::uint32_t>::max)(),
        (std::numeric_limits<std::uint32_t>::max)());
    ARPG_REQUIRE(!arpg::test::CombatWorldTestAccess::spawn_projectile(
        invalid_owner_world, invalid_owner));
    ARPG_REQUIRE(!arpg::test::CombatWorldTestAccess::spawn_hazard(
        invalid_owner_world, invalid_owner));
    const CombatSnapshot invalid_after = invalid_owner_world.snapshot();
    ARPG_REQUIRE(same_projectiles(invalid_before, invalid_after));
    ARPG_REQUIRE(same_hazards(invalid_before, invalid_after));
    ARPG_REQUIRE(invalid_after.diagnostics.projectile_invalid_owner_count
        == (std::numeric_limits<std::uint32_t>::max)());
    ARPG_REQUIRE(invalid_after.diagnostics.hazard_invalid_owner_count
        == (std::numeric_limits<std::uint32_t>::max)());
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
    {"fixed 3600 tick script is deterministic replay",
     &fixed_3600_tick_script_is_deterministic_replay},
    {"mixed role golden replay preserves public behavior",
     &mixed_role_golden_replay_preserves_public_behavior},
    {"all monster roles tick without allocation or overflow",
     &all_monster_roles_tick_without_allocation_or_overflow},
    {"full pools reject without mutation and saturate diagnostics",
     &full_pools_reject_without_mutation_and_saturate_diagnostics},
};

}  // namespace

arpg::test::TestSuite break_stress_suite() noexcept {
    return arpg::test::make_suite("break_stress", kCases);
}
