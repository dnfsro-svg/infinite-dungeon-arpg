#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"

#include <array>
#include <cstddef>

namespace {

using namespace arpg::combat;
using arpg::test::drain_events;
using arpg::test::finish_attack;
using arpg::test::tick_n;

CombatLabConfig single_target_config() noexcept {
    CombatLabConfig config;
    config.dummy_spawns = {{{1.20F, 0.0F, 0.0F},
                            {6.00F, 3.0F, 0.0F},
                            {7.00F, -3.0F, 0.0F}}};
    return config;
}

CombatLabConfig light_and_normal_config() noexcept {
    CombatLabConfig config;
    config.dummy_spawns = {{{1.20F, 0.0F, 0.0F},
                            {1.20F, 0.0F, 0.0F},
                            {7.00F, -3.0F, 0.0F}}};
    return config;
}

CombatLabConfig normal_target_config() noexcept {
    CombatLabConfig config;
    config.dummy_spawns = {{{6.00F, 3.0F, 0.0F},
                            {1.20F, 0.0F, 0.0F},
                            {7.00F, -3.0F, 0.0F}}};
    return config;
}

bool start_j2_after_j1_hit(CombatWorld& world) noexcept {
    if (!world.queue_action(Action::light)) {
        return false;
    }
    world.tick(MovementInput{});
    tick_n(world, 5);
    tick_n(world, 3);
    tick_n(world, 3);
    if (world.snapshot().player.attack_elapsed_ticks != 8
        || !world.queue_action(Action::light)) {
        return false;
    }
    world.tick(MovementInput{});
    return world.snapshot().player.active_attack == AttackId::j2;
}

arpg::test::Failure j1_hitstun_uses_local_frozen_ticks() noexcept {
    CombatWorld world{light_and_normal_config()};
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    tick_n(world, 5);

    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::hitstun);
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::hitstun);
    ARPG_REQUIRE(snapshot.monsters[0].hit_stop_ticks == 3);
    ARPG_REQUIRE(snapshot.monsters[1].hit_stop_ticks == 3);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[0].position.x, 1.20, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[1].position.x, 1.20, 1.0e-4));

    tick_n(world, 3);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::hitstun);
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::hitstun);
    ARPG_REQUIRE(snapshot.monsters[0].hit_stop_ticks == 0);
    ARPG_REQUIRE(snapshot.monsters[1].hit_stop_ticks == 0);

    tick_n(world, 9);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::hitstun);
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::hitstun);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::hitstun);
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::idle);
    tick_n(world, 2);
    ARPG_REQUIRE(
        world.snapshot().monsters[0].reaction == ReactionState::hitstun);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().monsters[0].reaction == ReactionState::idle);
    return {};
}

arpg::test::Failure j2_knockback_scales_for_light_and_normal() noexcept {
    CombatWorld world{light_and_normal_config()};
    ARPG_REQUIRE(start_j2_after_j1_hit(world));
    tick_n(world, 6);

    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::hitstun);
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::hitstun);
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[0].velocity.x, 2.75, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].velocity.x, 2.20, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[0].position.x, 1.20, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].position.x, 1.20, 1.0e-4));

    tick_n(world, 5);
    snapshot = world.snapshot();
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[0].position.x, 1.20, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].position.x, 1.20, 1.0e-4));
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[0].position.x, 1.20 + 2.75 / 60.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[1].position.x, 1.20 + 2.20 / 60.0, 1.0e-4));
    return {};
}

arpg::test::Failure launcher_integrates_then_lands_in_knockdown() noexcept {
    CombatWorld world{normal_target_config()};
    ARPG_REQUIRE(world.queue_action(Action::launcher));
    world.tick(MovementInput{});
    tick_n(world, 7);

    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::airborne);
    ARPG_REQUIRE(snapshot.monsters[1].hit_stop_ticks == 5);
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].position.z, 0.0));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].velocity.x, 1.2, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].velocity.z, 9.5, 1.0e-4));
    drain_events(world);

    tick_n(world, 5);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::airborne);
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].position.z, 0.0));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].velocity.z, 9.5, 1.0e-4));
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[1].position.z, 9.5 / 60.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].velocity.z, 9.1, 1.0e-4));

    for (int tick = 0;
         tick < 120
         && world.snapshot().monsters[1].reaction != ReactionState::knockdown;
         ++tick) {
        world.tick(MovementInput{});
    }
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::knockdown);
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].position.z, 0.0));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].velocity.z, 0.0));
    int landing_events = 0;
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::landing) {
            ++landing_events;
            ARPG_REQUIRE(event->target_index == 1);
            ARPG_REQUIRE(arpg::test::near(event->position.z, 0.0));
        }
    }
    ARPG_REQUIRE(landing_events == 1);
    tick_n(world, 44);
    ARPG_REQUIRE(
        world.snapshot().monsters[1].reaction == ReactionState::knockdown);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().monsters[1].reaction == ReactionState::rising);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::landing);
    }

    CombatWorld airborne_followup{normal_target_config()};
    ARPG_REQUIRE(airborne_followup.queue_action(Action::launcher));
    airborne_followup.tick(MovementInput{});
    tick_n(airborne_followup, 7);
    ARPG_REQUIRE(finish_attack(airborne_followup, 128));
    ARPG_REQUIRE(
        airborne_followup.snapshot().monsters[1].reaction
        == ReactionState::airborne);
    ARPG_REQUIRE(airborne_followup.queue_action(Action::light));
    airborne_followup.tick(MovementInput{});
    tick_n(airborne_followup, 5);
    snapshot = airborne_followup.snapshot();
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::airborne);
    ARPG_REQUIRE(snapshot.monsters[1].hit_stop_ticks == 3);
    const float frozen_z = snapshot.monsters[1].position.z;
    const float frozen_velocity_z = snapshot.monsters[1].velocity.z;
    tick_n(airborne_followup, 3);
    snapshot = airborne_followup.snapshot();
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[1].position.z, frozen_z, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[1].velocity.z, frozen_velocity_z, 1.0e-4));
    airborne_followup.tick(MovementInput{});
    snapshot = airborne_followup.snapshot();
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::airborne);
    ARPG_REQUIRE(!arpg::test::near(
        snapshot.monsters[1].position.z, frozen_z, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[1].velocity.z,
        frozen_velocity_z - 24.0 / 60.0,
        1.0e-4));

    CombatLabConfig independent_config;
    independent_config.dummy_spawns = {{{-1.20F, 0.0F, 0.0F},
                                        {1.80F, 0.0F, 0.0F},
                                        {7.00F, -3.0F, 0.0F}}};
    CombatWorld independent{independent_config};
    ARPG_REQUIRE(independent.queue_action(Action::launcher));
    independent.tick(MovementInput{});
    tick_n(independent, 7);
    ARPG_REQUIRE(finish_attack(independent, 128));
    ARPG_REQUIRE(
        independent.snapshot().monsters[1].reaction
        == ReactionState::airborne);
    independent.tick(MovementInput{-1, 0});
    ARPG_REQUIRE(independent.snapshot().player.facing == Facing::left);
    ARPG_REQUIRE(independent.queue_action(Action::light));
    independent.tick(MovementInput{});
    tick_n(independent, 5);
    snapshot = independent.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].hit_stop_ticks == 3);
    ARPG_REQUIRE(snapshot.monsters[1].hit_stop_ticks == 0);
    const float independent_z = snapshot.monsters[1].position.z;
    tick_n(independent, 3);
    snapshot = independent.snapshot();
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 5);
    ARPG_REQUIRE(snapshot.monsters[0].hit_stop_ticks == 0);
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::airborne);
    ARPG_REQUIRE(!arpg::test::near(
        snapshot.monsters[1].position.z, independent_z, 1.0e-4));
    return {};
}

arpg::test::Failure knockdown_and_rising_have_exact_boundaries() noexcept {
    CombatWorld j3{normal_target_config()};
    ARPG_REQUIRE(start_j2_after_j1_hit(j3));
    tick_n(j3, 6);
    tick_n(j3, 5);
    tick_n(j3, 3);
    ARPG_REQUIRE(j3.snapshot().player.attack_elapsed_ticks == 9);
    ARPG_REQUIRE(j3.queue_action(Action::light));
    j3.tick(MovementInput{});
    ARPG_REQUIRE(j3.snapshot().player.active_attack == AttackId::j3);
    tick_n(j3, 8);
    CombatSnapshot snapshot = j3.snapshot();
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::knockdown);
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[1].velocity.x, 5.0, 1.0e-4));
    ARPG_REQUIRE(snapshot.monsters[1].hit_stop_ticks == 7);
    tick_n(j3, 7);
    tick_n(j3, 44);
    ARPG_REQUIRE(
        j3.snapshot().monsters[1].reaction == ReactionState::knockdown);
    j3.tick(MovementInput{});
    ARPG_REQUIRE(j3.snapshot().monsters[1].reaction == ReactionState::rising);
    tick_n(j3, 29);
    ARPG_REQUIRE(j3.snapshot().monsters[1].reaction == ReactionState::rising);
    j3.tick(MovementInput{});
    ARPG_REQUIRE(j3.snapshot().monsters[1].reaction == ReactionState::idle);
    return {};
}

arpg::test::Failure defeated_respawns_after_ninety_active_ticks() noexcept {
    CombatWorld world{single_target_config()};
    ARPG_REQUIRE(start_j2_after_j1_hit(world));
    tick_n(world, 6);
    ARPG_REQUIRE(finish_attack(world, 128));
    drain_events(world);

    for (int attack = 0; attack < 8; ++attack) {
        ARPG_REQUIRE(world.queue_action(Action::light));
        world.tick(MovementInput{});
        tick_n(world, 5);
        ARPG_REQUIRE(finish_attack(world, 128));
        drain_events(world);
    }

    ARPG_REQUIRE(world.snapshot().monsters[0].hp == 14);
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    tick_n(world, 5);
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].hp == 0);
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::defeated);
    ARPG_REQUIRE(snapshot.monsters[0].hit_stop_ticks == 3);
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[0].velocity.x, 0.0));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[0].velocity.z, 0.0));

    std::array<CombatEventKind, 4> kinds{};
    std::size_t event_count = 0;
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event_count < kinds.size());
        kinds[event_count] = event->kind;
        if (event->kind == CombatEventKind::defeated) {
            ARPG_REQUIRE(event->attack == AttackId::j1);
            ARPG_REQUIRE(event->target_index == 0);
        }
        ++event_count;
    }
    ARPG_REQUIRE(event_count == 4);
    ARPG_REQUIRE(kinds[0] == CombatEventKind::swing);
    ARPG_REQUIRE(kinds[1] == CombatEventKind::hit);
    ARPG_REQUIRE(kinds[2] == CombatEventKind::defeated);
    ARPG_REQUIRE(kinds[3] == CombatEventKind::impact_summary);

    tick_n(world, 3);
    ARPG_REQUIRE(finish_attack(world, 128));
    drain_events(world);
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    tick_n(world, 5);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].hp == 0);
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::defeated);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind == CombatEventKind::swing);
    }
    ARPG_REQUIRE(finish_attack(world, 128));
    drain_events(world);

    tick_n(world, 53);
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    tick_n(world, 5);
    ARPG_REQUIRE(
        world.snapshot().monsters[0].reaction == ReactionState::defeated);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::respawning);
    ARPG_REQUIRE(snapshot.monsters[0].hp == snapshot.monsters[0].max_hp);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[0].position.x, 1.20, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[0].position.y, 0.0, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[0].position.z, 0.0, 1.0e-4));

    int respawn_events = 0;
    int respawn_tick_hits = 0;
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::respawned) {
            ++respawn_events;
            ARPG_REQUIRE(event->target_index == 0);
            ARPG_REQUIRE(event->attack == AttackId::none);
            ARPG_REQUIRE(arpg::test::near(
                event->position.x, 1.20, 1.0e-4));
        } else if (event->kind == CombatEventKind::hit) {
            ++respawn_tick_hits;
        }
    }
    ARPG_REQUIRE(respawn_events == 1);
    ARPG_REQUIRE(respawn_tick_hits == 0);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::hitstun);
    ARPG_REQUIRE(snapshot.monsters[0].hp == 272);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::respawned);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"J1 light and normal hitstun", &j1_hitstun_uses_local_frozen_ticks},
    {"J2 light and normal knockback", &j2_knockback_scales_for_light_and_normal},
    {"launcher gravity and landing", &launcher_integrates_then_lands_in_knockdown},
    {"knockdown and rising boundaries", &knockdown_and_rising_have_exact_boundaries},
    {"defeated and ninety-tick respawn", &defeated_respawns_after_ninety_active_ticks},
};

}  // namespace

arpg::test::TestSuite dummy_reaction_suite() noexcept {
    return arpg::test::make_suite("dummy_reaction", kCases);
}
