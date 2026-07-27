#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"

#include "abyss/abyss_rules.hpp"

#include <cstddef>
#include <cstdint>

namespace {

using namespace arpg::combat;
using arpg::test::tick_n;

CombatEncounterConfig single_chaser_encounter() noexcept {
    CombatEncounterConfig config{};
    config.player_spawn = Vec3{0.0F, 0.0F, 0.0F};
    config.initial_facing = Facing::right;
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{1.0F, 0.0F, 0.0F}};
    config.reset_player_health = true;
    return config;
}

void drain_events(CombatWorld& world) noexcept {
    while (world.try_pop_event().has_value()) {
    }
}

arpg::test::Failure player_damage_reaches_zero() noexcept {
    CombatWorld world{single_chaser_encounter()};
    drain_events(world);
    const int max_hp = world.snapshot().player.max_hp;
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 350, Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::light);
    const int after_first = world.snapshot().player.hp;
    ARPG_REQUIRE(after_first == max_hp - 350);
    ARPG_REQUIRE(world.snapshot().player.hurt_ticks == 12);
    ARPG_REQUIRE(world.snapshot().player.invulnerability_ticks == 30);

    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 350, Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == after_first);

    tick_n(world, 30);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, max_hp, Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::heavy);
    ARPG_REQUIRE(world.snapshot().player.hp == 0);
    return {};
}

arpg::test::Failure accepted_hit_emits_payload_and_hurt_started() noexcept {
    CombatWorld world{single_chaser_encounter()};
    drain_events(world);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 125, Vec3{1.25F, -0.5F, 0.0F}, FeedbackLevel::medium);

    const auto hit = world.try_pop_event();
    const auto hurt = world.try_pop_event();
    ARPG_REQUIRE(hit.has_value());
    ARPG_REQUIRE(hurt.has_value());
    ARPG_REQUIRE(hit->kind == CombatEventKind::player_hit);
    ARPG_REQUIRE(hit->value == 125);
    ARPG_REQUIRE(hit->feedback == FeedbackLevel::medium);
    ARPG_REQUIRE(hit->position.x == 1.25F);
    ARPG_REQUIRE(hit->position.y == -0.5F);
    ARPG_REQUIRE(hurt->kind == CombatEventKind::player_hurt_started);
    ARPG_REQUIRE(hurt->value == 125);
    ARPG_REQUIRE(hurt->feedback == FeedbackLevel::medium);
    ARPG_REQUIRE(!world.try_pop_event().has_value());
    return {};
}

arpg::test::Failure player_defeat_event_emits_once() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.wave = {};
    CombatWorld world{config};
    drain_events(world);
    const int max_hp = world.snapshot().player.max_hp;
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, max_hp, Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::heavy);

    const auto hit = world.try_pop_event();
    const auto hurt = world.try_pop_event();
    const auto defeated = world.try_pop_event();
    ARPG_REQUIRE(hit.has_value());
    ARPG_REQUIRE(hurt.has_value());
    ARPG_REQUIRE(defeated.has_value());
    ARPG_REQUIRE(hit->kind == CombatEventKind::player_hit);
    ARPG_REQUIRE(hurt->kind == CombatEventKind::player_hurt_started);
    ARPG_REQUIRE(defeated->kind == CombatEventKind::player_defeated);
    ARPG_REQUIRE(!world.try_pop_event().has_value());

    tick_n(world, 30);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 1, Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == 0);
    ARPG_REQUIRE(!world.try_pop_event().has_value());
    return {};
}

arpg::test::Failure player_defeat_latch_survives_event_overflow() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.wave = {};
    CombatWorld world{config};
    drain_events(world);
    arpg::test::CombatWorldTestAccess::fill_event_queue(
        world, kCombatEventCapacity - 2U);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, world.snapshot().player.max_hp,
        Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::heavy);
    ARPG_REQUIRE(world.snapshot().player.hp == 0);
    ARPG_REQUIRE(world.player_defeated());
    ARPG_REQUIRE(world.snapshot().diagnostics.event_overflow_count == 1U);
    return {};
}

arpg::test::Failure defeated_player_rejects_and_discards_input() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.wave = {};
    CombatWorld world{config};
    drain_events(world);
    ARPG_REQUIRE(world.queue_action(Action::light));
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, world.snapshot().player.max_hp,
        Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::heavy);
    drain_events(world);
    ARPG_REQUIRE(!world.queue_action(Action::launcher));
    const Vec3 defeated_position = world.snapshot().player.position;
    world.tick(MovementInput{1, 1});
    const auto dead = world.snapshot();
    ARPG_REQUIRE(dead.player.position.x == defeated_position.x);
    ARPG_REQUIRE(dead.player.position.y == defeated_position.y);
    ARPG_REQUIRE(dead.player.active_attack == AttackId::none);
    ARPG_REQUIRE(dead.diagnostics.input_size == 0U);
    ARPG_REQUIRE(!world.try_pop_event().has_value());

    EncounterWave empty{};
    ARPG_REQUIRE(world.load_wave(empty, true));
    world.tick({});
    ARPG_REQUIRE(world.snapshot().player.active_attack == AttackId::none);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::swing);
    }
    return {};
}

arpg::test::Failure hurt_ticks_lock_movement_until_expired() noexcept {
    CombatWorld world{single_chaser_encounter()};
    drain_events(world);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 1, Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::light);
    const float start_x = world.snapshot().player.position.x;
    world.tick(MovementInput{-1, 0});
    ARPG_REQUIRE(world.snapshot().player.position.x == start_x);
    ARPG_REQUIRE(world.snapshot().player.hurt_ticks == 11);
    tick_n(world, 11, MovementInput{-1, 0});
    ARPG_REQUIRE(world.snapshot().player.hurt_ticks == 0);
    world.tick(MovementInput{-1, 0});
    ARPG_REQUIRE(world.snapshot().player.position.x < start_x);
    return {};
}

arpg::test::Failure load_wave_reset_restores_full_health() noexcept {
    CombatWorld world{single_chaser_encounter()};
    drain_events(world);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 400, Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == 600);
    EncounterWave wave{};
    wave.spawn_count = 1U;
    wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{3.0F, 0.0F, 0.0F}};
    ARPG_REQUIRE(world.load_wave(wave, true));
    ARPG_REQUIRE(world.snapshot().player.hp == world.snapshot().player.max_hp);
    ARPG_REQUIRE(world.snapshot().player.hurt_ticks == 0);
    ARPG_REQUIRE(world.snapshot().player.invulnerability_ticks == 0);
    const auto reset = world.try_pop_event();
    ARPG_REQUIRE(reset.has_value());
    ARPG_REQUIRE(reset->kind == CombatEventKind::player_health_reset);
    ARPG_REQUIRE(reset->value == world.snapshot().player.max_hp);
    return {};
}

arpg::test::Failure exhausted_recovery_caps_initial_resources_once() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.player_build.values.max_barrier = 1010000;
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::exhausted_recovery);
    CombatWorld world{config};
    ARPG_REQUIRE(world.snapshot().player.max_hp == 1000);
    ARPG_REQUIRE(world.snapshot().player.hp == 700);
    ARPG_REQUIRE(world.snapshot().player.max_barrier == 101);
    ARPG_REQUIRE(world.snapshot().player.barrier == 71);

    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 100, Vec3{}, FeedbackLevel::light);
    const PlayerSnapshot damaged = world.snapshot().player;
    EncounterWave next = config.wave;
    next.spawns[0].position.x = 3.0F;
    ARPG_REQUIRE(world.load_wave(next, false));
    ARPG_REQUIRE(world.snapshot().player.hp == damaged.hp);
    ARPG_REQUIRE(world.snapshot().player.barrier == damaged.barrier);
    return {};
}

arpg::test::Failure exhausted_recovery_scales_explicit_and_wave_restore() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.player_build.values.max_barrier = 1010000;
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::exhausted_recovery);
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 600, 50);
    world.restore_player_resources(1, 1);
    ARPG_REQUIRE(world.snapshot().player.hp == 601);
    ARPG_REQUIRE(world.snapshot().player.barrier == 51);
    world.restore_player_resources(10, 10);
    ARPG_REQUIRE(world.snapshot().player.hp == 608);
    ARPG_REQUIRE(world.snapshot().player.barrier == 58);

    ARPG_REQUIRE(world.load_wave(config.wave, true));
    ARPG_REQUIRE(world.snapshot().player.hp == 700);
    ARPG_REQUIRE(world.snapshot().player.barrier == 71);
    return {};
}

arpg::test::Failure life_sacrifice_maps_ratio_and_clear_does_not_refill() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.wave = {};
    config.player_build.values.max_health = 10000;
    config.player_build.values.max_barrier = 1010000;
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::life_sacrifice);
    CombatWorld world{config};
    ARPG_REQUIRE(world.snapshot().player.max_hp == 551);
    ARPG_REQUIRE(world.snapshot().player.hp == 551);
    ARPG_REQUIRE(world.snapshot().player.max_barrier == 101);
    ARPG_REQUIRE(world.snapshot().player.barrier == 101);

    arpg::test::CombatWorldTestAccess::set_player_resources(world, 276, 37);
    world.clear_abyss_rule_preserving_resources();
    ARPG_REQUIRE(world.snapshot().player.max_hp == 1001);
    ARPG_REQUIRE(world.snapshot().player.hp == 501);
    ARPG_REQUIRE(world.snapshot().player.max_barrier == 101);
    ARPG_REQUIRE(world.snapshot().player.barrier == 37);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::abyss_config(world).rule
                 == arpg::abyss::AbyssRuleId::none);
    return {};
}

arpg::test::Failure percent_health_restore_uses_actual_max_and_bypasses_abyss_multiplier() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.player_build.values.max_health = 10000;  // actual max becomes 1001
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 500, 0);
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 251);
    ARPG_REQUIRE(world.snapshot().player.hp == 751);
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 900, 0);
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 101);
    ARPG_REQUIRE(world.snapshot().player.hp == 1001);
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 0);

    config = single_chaser_encounter();
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::exhausted_recovery);
    CombatWorld abyss{config};
    arpg::test::CombatWorldTestAccess::set_player_resources(abyss, 600, 0);
    ARPG_REQUIRE(abyss.restore_player_health_percent(2500U) == 250);
    ARPG_REQUIRE(abyss.snapshot().player.hp == 850);
    ARPG_REQUIRE(abyss.restore_player_health_percent(0U) == 0);
    return {};
}

arpg::test::Failure percent_health_restore_never_revives_defeated_player() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.wave = {};
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::apply_damage(world,
        world.snapshot().player.max_hp, Vec3{}, FeedbackLevel::heavy);
    const auto death = world.death_snapshot();
    ARPG_REQUIRE(death.has_value());
    const std::uint64_t death_tick = death->tick;
    const std::uint64_t final_damage = death->final_damage;
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 0);
    ARPG_REQUIRE(world.snapshot().player.hp == 0);
    ARPG_REQUIRE(world.death_snapshot().has_value());
    ARPG_REQUIRE(world.death_snapshot()->tick == death_tick);
    ARPG_REQUIRE(world.death_snapshot()->final_damage == final_damage);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"damage reaches zero", &player_damage_reaches_zero},
    {"accepted hit payload and hurt event", &accepted_hit_emits_payload_and_hurt_started},
    {"player defeat event emits once", &player_defeat_event_emits_once},
    {"player defeat latch survives event overflow", &player_defeat_latch_survives_event_overflow},
    {"defeated player rejects and discards input", &defeated_player_rejects_and_discards_input},
    {"hurt movement lock", &hurt_ticks_lock_movement_until_expired},
    {"wave load restores full health", &load_wave_reset_restores_full_health},
    {"exhausted initial cap applied once",
     &exhausted_recovery_caps_initial_resources_once},
    {"exhausted explicit and wave restore",
     &exhausted_recovery_scales_explicit_and_wave_restore},
    {"life sacrifice ratio and clear",
     &life_sacrifice_maps_ratio_and_clear_does_not_refill},
    {"percent health restore uses actual max and bypasses abyss multiplier",
     &percent_health_restore_uses_actual_max_and_bypasses_abyss_multiplier},
    {"percent health restore never revives defeated player",
     &percent_health_restore_never_revives_defeated_player},
};

}  // namespace

arpg::test::TestSuite player_health_suite() noexcept {
    return arpg::test::make_suite("player_health", kCases);
}
