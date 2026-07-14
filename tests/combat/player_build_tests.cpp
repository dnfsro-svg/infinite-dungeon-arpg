#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"
#include "modifiers/damage_types.hpp"

namespace {

using namespace arpg::combat;
using namespace arpg::modifiers;

CombatEncounterConfig one_monster_config() noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{1.0F, 0.0F, 0.0F}};
    return config;
}

arpg::test::Failure fire_resistance_then_barrier_absorbs_damage() noexcept {
    PlayerCombatBuild build{};
    build.values.resistance[element_index(DamageType::fire)] = 7500;
    build.values.max_barrier = 100000;
    CombatEncounterConfig config = one_monster_config();
    config.player_build = build;
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{{0, 40, 0, 0, 0}}, Vec3{}, FeedbackLevel::light);
    const auto player = world.snapshot().player;
    ARPG_REQUIRE(player.barrier == 0);
    ARPG_REQUIRE(player.hp == player.max_hp);
    return {};
}

arpg::test::Failure player_attack_adds_element_without_converting_physical() noexcept {
    PlayerCombatBuild build{};
    build.values.flat_damage[damage_index(DamageType::fire)] = 30000;
    const DamagePacket packet = build_player_hit_packet(28, build);
    ARPG_REQUIRE(packet.amount[damage_index(DamageType::physical)] == 28);
    ARPG_REQUIRE(packet.amount[damage_index(DamageType::fire)] == 3);
    return {};
}

arpg::test::Failure negative_water_resistance_increases_damage() noexcept {
    PlayerCombatBuild build{};
    build.values.resistance[element_index(DamageType::water)] = -5000;
    const DamagePacket packet{{0, 0, 20, 0, 0}};
    ARPG_REQUIRE(resolve_player_damage(packet, build) == 30);
    return {};
}

arpg::test::Failure chaos_more_damage_taken_is_applied_after_resistance() noexcept {
    PlayerCombatBuild build{};
    build.values.damage_taken = 11500;
    const DamagePacket packet{{0, 0, 0, 0, 20}};
    ARPG_REQUIRE(resolve_player_damage(packet, build) == 23);
    return {};
}

arpg::test::Failure default_build_preserves_j1_damage() noexcept {
    const DamagePacket packet = build_player_hit_packet(28, PlayerCombatBuild{});
    ARPG_REQUIRE(packet.amount[damage_index(DamageType::physical)] == 28);
    ARPG_REQUIRE(resolve_player_damage(packet, PlayerCombatBuild{}) == 28);
    return {};
}

arpg::test::Failure attack_speed_scales_only_startup_and_recovery() noexcept {
    PlayerCombatBuild build{};
    build.values.attack_speed = 20000;
    CombatEncounterConfig config{};
    config.player_build = build;
    CombatWorld world{config};
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::startup);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::startup);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::active);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::active);
    return {};
}

arpg::test::Failure impulse_scale_affects_knockback_and_launch() noexcept {
    PlayerCombatBuild build{};
    build.values.impulse_scale = 20000;
    CombatEncounterConfig config{};
    config.player_build = build;
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{1.0F, 0.0F, 0.0F}};
    CombatWorld world{config};
    ARPG_REQUIRE(world.queue_action(Action::launcher));
    for (int tick = 0; tick < 8; ++tick) world.tick(MovementInput{});
    const auto monster = world.snapshot().monsters[0];
    ARPG_REQUIRE(monster.reaction == ReactionState::airborne);
    ARPG_REQUIRE(monster.velocity.x > 2.0F);
    ARPG_REQUIRE(monster.velocity.z > 10.0F);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"fire resistance and barrier", &fire_resistance_then_barrier_absorbs_damage},
    {"elemental add does not convert physical", &player_attack_adds_element_without_converting_physical},
    {"negative water resistance", &negative_water_resistance_increases_damage},
    {"chaos damage taken", &chaos_more_damage_taken_is_applied_after_resistance},
    {"default j1 damage", &default_build_preserves_j1_damage},
    {"attack speed phase scaling", &attack_speed_scales_only_startup_and_recovery},
    {"impulse scale", &impulse_scale_affects_knockback_and_launch},
};

}  // namespace

arpg::test::TestSuite player_build_suite() noexcept {
    return arpg::test::make_suite("player_build", kCases);
}
