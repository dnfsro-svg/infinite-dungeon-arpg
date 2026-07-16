#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"

#include <cstddef>

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

constexpr arpg::test::TestCase kCases[] = {
    {"damage reaches zero", &player_damage_reaches_zero},
    {"accepted hit payload and hurt event", &accepted_hit_emits_payload_and_hurt_started},
    {"player defeat event emits once", &player_defeat_event_emits_once},
    {"hurt movement lock", &hurt_ticks_lock_movement_until_expired},
    {"wave load restores full health", &load_wave_reset_restores_full_health},
};

}  // namespace

arpg::test::TestSuite player_health_suite() noexcept {
    return arpg::test::make_suite("player_health", kCases);
}
