#include "test_framework.hpp"

#include "combat/combat_world.hpp"

namespace {

using namespace arpg::combat;

constexpr double kFloatTolerance = 1.0e-4;

CombatLabConfig isolated_attack_config() noexcept {
    CombatLabConfig config;
    config.dummy_spawns = {{{6.0F, 3.0F, 0.0F},
                            {7.0F, -3.0F, 0.0F},
                            {-6.0F, 3.0F, 0.0F}}};
    return config;
}

void tick_n(CombatWorld& world, int count, MovementInput movement = {}) noexcept {
    for (int tick = 0; tick < count; ++tick) {
        world.tick(movement);
    }
}

arpg::test::Failure ground_timelines_and_lunges_are_deterministic() noexcept {
    CombatWorld light{isolated_attack_config()};
    ARPG_REQUIRE(light.queue_action(Action::light));
    light.tick(MovementInput{1, 1});
    CombatSnapshot snapshot = light.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::j1);
    ARPG_REQUIRE(snapshot.player.attack_phase == AttackPhase::startup);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 0);
    ARPG_REQUIRE(snapshot.player.combo_stage == 1);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, 0.10, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(snapshot.player.position.y, 0.0));

    tick_n(light, 4, MovementInput{1, 0});
    snapshot = light.snapshot();
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 4);
    ARPG_REQUIRE(snapshot.player.attack_phase == AttackPhase::startup);
    light.tick(MovementInput{1, 0});
    ARPG_REQUIRE(light.snapshot().player.attack_phase == AttackPhase::active);
    tick_n(light, 2, MovementInput{1, 0});
    snapshot = light.snapshot();
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 7);
    ARPG_REQUIRE(snapshot.player.attack_phase == AttackPhase::active);
    light.tick(MovementInput{1, 0});
    ARPG_REQUIRE(light.snapshot().player.attack_phase == AttackPhase::recovery);
    tick_n(light, 8, MovementInput{1, 0});
    snapshot = light.snapshot();
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 16);
    ARPG_REQUIRE(snapshot.player.attack_phase == AttackPhase::recovery);
    light.tick(MovementInput{1, 0});
    snapshot = light.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::none);
    ARPG_REQUIRE(snapshot.player.attack_phase == AttackPhase::finished);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 0);
    ARPG_REQUIRE(snapshot.player.combo_stage == 0);
    ARPG_REQUIRE(snapshot.player.state == PlayerState::idle);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, 0.10, kFloatTolerance));

    CombatWorld heavy{isolated_attack_config()};
    ARPG_REQUIRE(heavy.queue_action(Action::heavy));
    heavy.tick(MovementInput{});
    snapshot = heavy.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::heavy);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 0);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, 0.24, kFloatTolerance));
    tick_n(heavy, 13);
    ARPG_REQUIRE(heavy.snapshot().player.attack_phase == AttackPhase::startup);
    heavy.tick(MovementInput{});
    ARPG_REQUIRE(heavy.snapshot().player.attack_phase == AttackPhase::active);
    tick_n(heavy, 4);
    ARPG_REQUIRE(heavy.snapshot().player.attack_phase == AttackPhase::active);
    heavy.tick(MovementInput{});
    ARPG_REQUIRE(heavy.snapshot().player.attack_phase == AttackPhase::recovery);
    tick_n(heavy, 21);
    snapshot = heavy.snapshot();
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 40);
    ARPG_REQUIRE(snapshot.player.attack_phase == AttackPhase::recovery);
    heavy.tick(MovementInput{});
    ARPG_REQUIRE(heavy.snapshot().player.active_attack == AttackId::none);

    CombatWorld launcher{isolated_attack_config()};
    ARPG_REQUIRE(launcher.queue_action(Action::launcher));
    launcher.tick(MovementInput{});
    snapshot = launcher.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::launcher);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, 0.12, kFloatTolerance));
    tick_n(launcher, 27);
    snapshot = launcher.snapshot();
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 27);
    ARPG_REQUIRE(snapshot.player.attack_phase == AttackPhase::recovery);
    launcher.tick(MovementInput{});
    ARPG_REQUIRE(launcher.snapshot().player.active_attack == AttackId::none);
    return {};
}

arpg::test::Failure whiff_windows_chain_j1_j2_and_end_at_j3() noexcept {
    CombatWorld world{isolated_attack_config()};
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});

    tick_n(world, 12);
    ARPG_REQUIRE(world.snapshot().player.attack_elapsed_ticks == 12);
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::j1);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 13);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 1);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::j2);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 0);
    ARPG_REQUIRE(snapshot.player.combo_stage == 2);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, 0.24, kFloatTolerance));

    tick_n(world, 14);
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::j2);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 15);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 1);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::j3);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 0);
    ARPG_REQUIRE(snapshot.player.combo_stage == 3);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, 0.44, kFloatTolerance));

    tick_n(world, 7);
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::startup);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::active);
    tick_n(world, 3);
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::active);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::recovery);
    tick_n(world, 15);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 27);
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::none);
    ARPG_REQUIRE(snapshot.player.attack_phase == AttackPhase::finished);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 0);
    ARPG_REQUIRE(snapshot.player.combo_stage == 0);
    ARPG_REQUIRE(snapshot.player.state == PlayerState::idle);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 1);
    return {};
}

arpg::test::Failure action_priority_and_illegal_cancels_are_deterministic() noexcept {
    CombatWorld all_actions{isolated_attack_config()};
    ARPG_REQUIRE(all_actions.queue_action(Action::light));
    ARPG_REQUIRE(all_actions.queue_action(Action::launcher));
    ARPG_REQUIRE(all_actions.queue_action(Action::heavy));
    ARPG_REQUIRE(all_actions.queue_action(Action::jump));
    all_actions.tick(MovementInput{});
    CombatSnapshot snapshot = all_actions.snapshot();
    ARPG_REQUIRE(snapshot.player.state == PlayerState::jump_rise);
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::none);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 3);

    CombatWorld no_jump{isolated_attack_config()};
    ARPG_REQUIRE(no_jump.queue_action(Action::light));
    ARPG_REQUIRE(no_jump.queue_action(Action::launcher));
    ARPG_REQUIRE(no_jump.queue_action(Action::heavy));
    no_jump.tick(MovementInput{});
    snapshot = no_jump.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::heavy);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 2);

    CombatWorld no_jump_or_heavy{isolated_attack_config()};
    ARPG_REQUIRE(no_jump_or_heavy.queue_action(Action::light));
    ARPG_REQUIRE(no_jump_or_heavy.queue_action(Action::launcher));
    no_jump_or_heavy.tick(MovementInput{});
    snapshot = no_jump_or_heavy.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::launcher);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 1);

    CombatWorld blocked{isolated_attack_config()};
    ARPG_REQUIRE(blocked.queue_action(Action::heavy));
    blocked.tick(MovementInput{});
    ARPG_REQUIRE(blocked.queue_action(Action::jump));
    ARPG_REQUIRE(blocked.queue_action(Action::heavy));
    ARPG_REQUIRE(blocked.queue_action(Action::launcher));
    ARPG_REQUIRE(blocked.queue_action(Action::light));
    blocked.tick(MovementInput{});
    snapshot = blocked.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::heavy);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 1);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 4);
    tick_n(blocked, 7);
    snapshot = blocked.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::heavy);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 8);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 0);
    ARPG_REQUIRE(snapshot.diagnostics.input_expired_count == 4);
    return {};
}

arpg::test::Failure air_j_is_limited_to_once_per_airtime() noexcept {
    CombatWorld world{isolated_attack_config()};
    ARPG_REQUIRE(world.queue_action(Action::jump));
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.air_attack_available);
    ARPG_REQUIRE(world.queue_action(Action::heavy));
    ARPG_REQUIRE(world.queue_action(Action::launcher));
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{1, 0});
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::air_j);
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 0);
    ARPG_REQUIRE(snapshot.player.state == PlayerState::attack_startup);
    ARPG_REQUIRE(!snapshot.player.air_attack_available);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 2);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, 0.08, kFloatTolerance));

    tick_n(world, 3);
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::startup);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::active);
    tick_n(world, 4);
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::active);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::recovery);
    tick_n(world, 11);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.attack_elapsed_ticks == 20);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::none);
    ARPG_REQUIRE(snapshot.player.state == PlayerState::jump_fall);
    ARPG_REQUIRE(!snapshot.player.air_attack_available);
    ARPG_REQUIRE(snapshot.diagnostics.input_expired_count == 2);

    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::none);
    ARPG_REQUIRE(!snapshot.player.air_attack_available);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 1);

    for (int tick = 0;
         tick < 40 && world.snapshot().player.state != PlayerState::landing;
         ++tick) {
        world.tick(MovementInput{});
    }
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.state == PlayerState::landing);
    ARPG_REQUIRE(snapshot.player.air_attack_available);
    world.tick(MovementInput{});

    ARPG_REQUIRE(world.queue_action(Action::light));
    ARPG_REQUIRE(world.queue_action(Action::jump));
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.state == PlayerState::jump_rise);
    ARPG_REQUIRE(world.snapshot().diagnostics.input_size == 1);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::air_j);
    ARPG_REQUIRE(!snapshot.player.air_attack_available);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"ground timelines and one-shot lunges",
     &ground_timelines_and_lunges_are_deterministic},
    {"J whiff windows and terminal J3",
     &whiff_windows_chain_j1_j2_and_end_at_j3},
    {"action priority and illegal cancel rejection",
     &action_priority_and_illegal_cancels_are_deterministic},
    {"one air J per airtime", &air_j_is_limited_to_once_per_airtime},
};

}  // namespace

arpg::test::TestSuite attack_state_suite() noexcept {
    return {
        "attack_state",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
