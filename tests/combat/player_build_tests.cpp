#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"
#include "modifiers/damage_types.hpp"

#include <limits>

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

arpg::test::Failure fire_damage_reduction_then_barrier_absorbs_damage() noexcept {
    PlayerCombatBuild build{};
    build.values.damage_reduction[element_index(DamageType::fire)] = 7500;
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
    const auto packet = build_player_hit_packet(28, build);
    ARPG_REQUIRE(packet.has_value());
    ARPG_REQUIRE(packet->amount[damage_index(DamageType::physical)] == 28);
    ARPG_REQUIRE(packet->amount[damage_index(DamageType::fire)] == 3);
    return {};
}

arpg::test::Failure negative_water_damage_reduction_increases_damage() noexcept {
    PlayerCombatBuild build{};
    build.values.damage_reduction[element_index(DamageType::water)] = -5000;
    const DamagePacket packet{{0, 0, 20, 0, 0}};
    ARPG_REQUIRE(resolve_player_damage(packet, build) == 30);
    return {};
}

arpg::test::Failure chaos_more_damage_taken_is_applied_after_reduction() noexcept {
    PlayerCombatBuild build{};
    build.values.damage_taken = 11500;
    const DamagePacket packet{{0, 0, 0, 0, 20}};
    ARPG_REQUIRE(resolve_player_damage(packet, build) == 23);
    return {};
}

arpg::test::Failure default_build_preserves_j1_damage() noexcept {
    const auto packet = build_player_hit_packet(28, PlayerCombatBuild{});
    ARPG_REQUIRE(packet.has_value());
    ARPG_REQUIRE(packet->amount[damage_index(DamageType::physical)] == 28);
    ARPG_REQUIRE(resolve_player_damage(*packet, PlayerCombatBuild{}) == 28);
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

arpg::test::Failure wave_load_preserves_barrier_without_health_reset() noexcept {
    PlayerCombatBuild build{};
    build.values.max_barrier = 100000;
    CombatEncounterConfig config = one_monster_config();
    config.player_build = build;
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 4, Vec3{}, FeedbackLevel::light);
    const int remaining = world.snapshot().player.barrier;
    ARPG_REQUIRE(remaining == 6);

    EncounterWave next_wave{};
    next_wave.spawn_count = 1U;
    next_wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{2.0F, 0.0F, 0.0F}};
    ARPG_REQUIRE(world.load_wave(next_wave, false));
    ARPG_REQUIRE(world.snapshot().player.barrier == remaining);

    ARPG_REQUIRE(world.load_wave(next_wave, true));
    ARPG_REQUIRE(world.snapshot().player.barrier == 10);
    return {};
}

arpg::test::Failure hit_event_keeps_packet_total_when_monster_shield_absorbs() noexcept {
    PlayerCombatBuild build{};
    build.values.flat_damage[damage_index(DamageType::fire)] = 30000;
    CombatEncounterConfig config{};
    config.player_build = build;
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::water_bulwark, Vec3{1.0F, 0.0F, 0.0F}};
    CombatWorld world{config};
    arpg::test::drain_events(world);
    arpg::test::CombatWorldTestAccess::set_monster_shield(world, 0, 20);
    ARPG_REQUIRE(world.queue_action(Action::light));
    for (int tick = 0; tick < 20; ++tick) {
        world.tick(MovementInput{});
    }

    bool saw_hit = false;
    while (const auto event = world.try_pop_event()) {
        if (event->kind != CombatEventKind::hit) continue;
        saw_hit = true;
        ARPG_REQUIRE(event->value == 31);
        break;
    }
    ARPG_REQUIRE(saw_hit);
    ARPG_REQUIRE(world.snapshot().monsters[0].hp == 689);
    ARPG_REQUIRE(world.snapshot().monsters[0].shield == 0);
    return {};
}

bool same_player_state(
    const PlayerSnapshot& left, const PlayerSnapshot& right) noexcept {
    return left.position.x == right.position.x
        && left.position.y == right.position.y
        && left.position.z == right.position.z
        && left.velocity.x == right.velocity.x
        && left.velocity.y == right.velocity.y
        && left.velocity.z == right.velocity.z
        && left.facing == right.facing
        && left.state == right.state
        && left.active_attack == right.active_attack
        && left.attack_phase == right.attack_phase
        && left.attack_elapsed_ticks == right.attack_elapsed_ticks
        && left.combo_stage == right.combo_stage
        && left.hit_stop_ticks == right.hit_stop_ticks
        && left.air_attack_available == right.air_attack_available
        && left.hp == right.hp
        && left.max_hp == right.max_hp
        && left.barrier == right.barrier
        && left.max_barrier == right.max_barrier
        && left.damage_reduction == right.damage_reduction
        && left.damage_reduction_cap == right.damage_reduction_cap
        && left.armor == right.armor
        && left.evasion == right.evasion
        && left.armor_reduction_bp == right.armor_reduction_bp
        && left.evasion_rate_bp == right.evasion_rate_bp
        && left.hurt_ticks == right.hurt_ticks
        && left.invulnerability_ticks == right.invulnerability_ticks;
}

arpg::test::Failure hot_swap_preserves_runtime_and_clamps_only_downward() noexcept {
    PlayerCombatBuild initial{};
    initial.values.max_barrier = 100000;
    CombatEncounterConfig config = one_monster_config();
    config.player_build = initial;
    CombatWorld world{config};
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 15, Vec3{}, FeedbackLevel::light);
    const auto damaged = world.snapshot().player;
    ARPG_REQUIRE(damaged.hp == 995);
    ARPG_REQUIRE(damaged.barrier == 0);
    ARPG_REQUIRE(damaged.active_attack == AttackId::j1);
    ARPG_REQUIRE(damaged.hurt_ticks != 0U);

    PlayerCombatBuild higher = initial;
    higher.values.max_health = 500000;
    higher.values.max_barrier = 200000;
    world.apply_player_build(higher);
    const auto raised = world.snapshot().player;
    ARPG_REQUIRE(raised.max_hp == 1050);
    ARPG_REQUIRE(raised.max_barrier == 20);
    ARPG_REQUIRE(raised.hp == damaged.hp);
    ARPG_REQUIRE(raised.barrier == damaged.barrier);
    ARPG_REQUIRE(raised.position.x == damaged.position.x);
    ARPG_REQUIRE(raised.velocity.x == damaged.velocity.x);
    ARPG_REQUIRE(raised.active_attack == damaged.active_attack);
    ARPG_REQUIRE(raised.attack_elapsed_ticks == damaged.attack_elapsed_ticks);
    ARPG_REQUIRE(raised.hurt_ticks == damaged.hurt_ticks);
    ARPG_REQUIRE(raised.invulnerability_ticks == damaged.invulnerability_ticks);

    PlayerCombatBuild lower{};
    lower.values.max_health_more = 9000;
    world.apply_player_build(lower);
    const auto lowered = world.snapshot().player;
    ARPG_REQUIRE(lowered.max_hp == 900);
    ARPG_REQUIRE(lowered.max_barrier == 0);
    ARPG_REQUIRE(lowered.hp == 900);
    ARPG_REQUIRE(lowered.barrier == 0);
    ARPG_REQUIRE(lowered.position.x == raised.position.x);
    ARPG_REQUIRE(lowered.active_attack == raised.active_attack);
    ARPG_REQUIRE(lowered.hurt_ticks == raised.hurt_ticks);
    ARPG_REQUIRE(lowered.invulnerability_ticks == raised.invulnerability_ticks);
    return {};
}

arpg::test::Failure invalid_hot_swap_keeps_old_build_and_player_state() noexcept {
    CombatWorld world{one_monster_config()};
    const auto before = world.snapshot().player;
    PlayerCombatBuild invalid{};
    invalid.values.valid = false;
    invalid.values.max_health = 500000;
    invalid.values.armor = 1000000;
    invalid.weapon_physical = -1;
    invalid.local_attack_speed_bp = -1;
    world.apply_player_build(invalid);
    ARPG_REQUIRE(same_player_state(before, world.snapshot().player));

    arpg::test::CombatWorldTestAccess::apply_damage(
        world, 10, Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == before.hp - 10);

    PlayerCombatBuild overflowing{};
    overflowing.values.max_health =
        (std::numeric_limits<std::int64_t>::max)();
    const auto before_overflow = world.snapshot().player;
    world.apply_player_build(overflowing);
    ARPG_REQUIRE(same_player_state(before_overflow, world.snapshot().player));
    return {};
}

arpg::test::Failure invalid_constructor_build_is_rejected() noexcept {
    CombatEncounterConfig config{};
    config.player_build.values.valid = false;
    config.player_build.values.max_health = 500000;
    config.player_build.values.armor = 1000000;
    CombatWorld world{config};
    const auto player = world.snapshot().player;
    ARPG_REQUIRE(player.max_hp == 1000);
    ARPG_REQUIRE(player.armor == 0);
    ARPG_REQUIRE(player.armor_reduction_bp == 0);
    return {};
}

arpg::test::Failure weapon_physical_uses_physical_flat_increased_and_more() noexcept {
    PlayerCombatBuild build{};
    build.weapon_physical = 12;
    build.values.flat_damage[damage_index(DamageType::physical)] = 20000;
    build.values.damage_increased[damage_index(DamageType::physical)] = 15000;
    build.values.melee_damage = 20000;
    const auto packet = build_player_hit_packet(28, build);
    ARPG_REQUIRE(packet.has_value());
    ARPG_REQUIRE(packet->amount[damage_index(DamageType::physical)] == 126);
    return {};
}

arpg::test::Failure hit_packet_reports_zero_and_invalid_without_partial_packet() noexcept {
    const auto zero = build_player_hit_packet(0, PlayerCombatBuild{});
    ARPG_REQUIRE(zero.has_value());
    ARPG_REQUIRE(*zero == DamagePacket{});
    ARPG_REQUIRE(!build_player_hit_packet(-1, PlayerCombatBuild{}).has_value());

    PlayerCombatBuild invalid{};
    invalid.values.valid = false;
    ARPG_REQUIRE(!build_player_hit_packet(1, invalid).has_value());
    invalid = PlayerCombatBuild{};
    invalid.weapon_physical = -1;
    ARPG_REQUIRE(!build_player_hit_packet(1, invalid).has_value());
    invalid = PlayerCombatBuild{};
    invalid.local_attack_speed_bp = -10001;
    ARPG_REQUIRE(!build_player_hit_packet(1, invalid).has_value());
    PlayerCombatBuild slowed{};
    slowed.local_attack_speed_bp = -1500;
    ARPG_REQUIRE(build_player_hit_packet(1, slowed).has_value());

    PlayerCombatBuild overflowing{};
    overflowing.weapon_physical =
        (std::numeric_limits<std::int64_t>::max)();
    const auto saturated_weapon = build_player_hit_packet(1, overflowing);
    ARPG_REQUIRE(saturated_weapon.has_value());
    ARPG_REQUIRE(saturated_weapon->amount[damage_index(DamageType::physical)]
        == (std::numeric_limits<int>::max)());
    overflowing = PlayerCombatBuild{};
    overflowing.values.flat_damage[damage_index(DamageType::physical)] =
        (std::numeric_limits<std::int64_t>::max)();
    overflowing.values.damage_increased[damage_index(DamageType::physical)] =
        (std::numeric_limits<std::int32_t>::max)();
    const auto saturated_flat = build_player_hit_packet(52, overflowing);
    ARPG_REQUIRE(saturated_flat.has_value());
    ARPG_REQUIRE(saturated_flat->amount[damage_index(DamageType::physical)]
        == (std::numeric_limits<int>::max)());
    return {};
}

arpg::test::Failure unrelated_invalid_build_field_is_rejected_consistently() noexcept {
    PlayerCombatBuild invalid{};
    invalid.values.max_health = -1;
    invalid.values.armor = 100;
    ARPG_REQUIRE(!build_player_hit_packet(28, invalid).has_value());
    ARPG_REQUIRE(!resolve_player_damage(DamagePacket{10}, invalid).has_value());

    CombatEncounterConfig config{};
    config.player_build = invalid;
    CombatWorld constructed{config};
    ARPG_REQUIRE(constructed.snapshot().player.max_hp == 1000);
    ARPG_REQUIRE(constructed.snapshot().player.armor == 0);

    CombatWorld hot_swap{CombatEncounterConfig{}};
    const auto before = hot_swap.snapshot().player;
    hot_swap.apply_player_build(invalid);
    ARPG_REQUIRE(same_player_state(before, hot_swap.snapshot().player));
    return {};
}

arpg::test::Failure overflowing_health_build_is_rejected_consistently() noexcept {
    PlayerCombatBuild invalid{};
    invalid.values.max_health =
        (std::numeric_limits<std::int64_t>::max)();
    ARPG_REQUIRE(!build_player_hit_packet(28, invalid).has_value());
    ARPG_REQUIRE(!resolve_player_damage(DamagePacket{10}, invalid).has_value());

    CombatEncounterConfig config{};
    config.player_build = invalid;
    CombatWorld constructed{config};
    ARPG_REQUIRE(constructed.snapshot().player.max_hp == 1000);

    CombatWorld hot_swap{CombatEncounterConfig{}};
    const auto before = hot_swap.snapshot().player;
    hot_swap.apply_player_build(invalid);
    ARPG_REQUIRE(same_player_state(before, hot_swap.snapshot().player));
    return {};
}

arpg::test::Failure unrepresentable_speed_build_is_rejected_consistently() noexcept {
    PlayerCombatBuild invalid{};
    invalid.values.attack_speed = 1;
    ARPG_REQUIRE(!build_player_hit_packet(28, invalid).has_value());
    ARPG_REQUIRE(!resolve_player_damage(DamagePacket{10}, invalid).has_value());

    CombatEncounterConfig config{};
    config.player_build = invalid;
    CombatWorld constructed{config};
    ARPG_REQUIRE(constructed.snapshot().player.max_hp == 1000);
    ARPG_REQUIRE(constructed.snapshot().player.evasion_rate_bp == 0);

    CombatWorld hot_swap{CombatEncounterConfig{}};
    const auto before = hot_swap.snapshot().player;
    hot_swap.apply_player_build(invalid);
    ARPG_REQUIRE(same_player_state(before, hot_swap.snapshot().player));
    return {};
}

arpg::test::Failure weapon_physical_is_applied_by_real_melee_hit() noexcept {
    PlayerCombatBuild build{};
    build.weapon_physical = 50;
    CombatEncounterConfig config{};
    config.player_build = build;
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{1.0F, 0.0F, 0.0F}};
    CombatWorld world{config};
    arpg::test::drain_events(world);
    ARPG_REQUIRE(world.queue_action(Action::light));
    arpg::test::tick_n(world, 8);
    ARPG_REQUIRE(world.snapshot().monsters[0].hp == 182);
    bool saw_weapon_hit = false;
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::hit) {
            saw_weapon_hit = true;
            ARPG_REQUIRE(event->value == 78);
        }
    }
    ARPG_REQUIRE(saw_weapon_hit);
    return {};
}

arpg::test::Failure local_attack_speed_multiplies_global_without_scaling_active() noexcept {
    PlayerCombatBuild build{};
    build.values.attack_speed = 15000;
    build.local_attack_speed_bp = 10000;
    CombatEncounterConfig config{};
    config.player_build = build;
    CombatWorld world{config};
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::startup);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::active);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::active);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::active);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.attack_phase == AttackPhase::recovery);
    return {};
}

arpg::test::Failure overflowing_weapon_build_saturates_atomically() noexcept {
    CombatWorld world{one_monster_config()};
    const auto before = world.snapshot().player;
    PlayerCombatBuild invalid{};
    invalid.weapon_physical =
        (std::numeric_limits<std::int64_t>::max)();
    invalid.values.max_health = 500000;
    world.apply_player_build(invalid);
    ARPG_REQUIRE(world.snapshot().player.max_hp == 1050);
    const auto saturated = world.snapshot().player;

    PlayerCombatBuild unrepresentable_speed{};
    unrepresentable_speed.values.attack_speed = 1;
    unrepresentable_speed.values.max_health = 500000;
    world.apply_player_build(unrepresentable_speed);
    ARPG_REQUIRE(!same_player_state(before, saturated));
    ARPG_REQUIRE(same_player_state(saturated, world.snapshot().player));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"fire damage reduction and barrier",
        &fire_damage_reduction_then_barrier_absorbs_damage},
    {"elemental add does not convert physical", &player_attack_adds_element_without_converting_physical},
    {"negative water damage reduction",
        &negative_water_damage_reduction_increases_damage},
    {"chaos damage taken",
        &chaos_more_damage_taken_is_applied_after_reduction},
    {"default j1 damage", &default_build_preserves_j1_damage},
    {"attack speed phase scaling", &attack_speed_scales_only_startup_and_recovery},
    {"impulse scale", &impulse_scale_affects_knockback_and_launch},
    {"wave load barrier policy", &wave_load_preserves_barrier_without_health_reset},
    {"shield preserves packet event total", &hit_event_keeps_packet_total_when_monster_shield_absorbs},
    {"hot swap clamps without healing", &hot_swap_preserves_runtime_and_clamps_only_downward},
    {"invalid hot swap is atomic", &invalid_hot_swap_keeps_old_build_and_player_state},
    {"invalid constructor build", &invalid_constructor_build_is_rejected},
    {"weapon physical evaluation", &weapon_physical_uses_physical_flat_increased_and_more},
    {"hit packet invalid semantics", &hit_packet_reports_zero_and_invalid_without_partial_packet},
    {"unrelated invalid build field", &unrelated_invalid_build_field_is_rejected_consistently},
    {"overflowing health build consistency", &overflowing_health_build_is_rejected_consistently},
    {"unrepresentable speed build consistency", &unrepresentable_speed_build_is_rejected_consistently},
    {"weapon physical real hit", &weapon_physical_is_applied_by_real_melee_hit},
    {"local and global attack speed", &local_attack_speed_multiplies_global_without_scaling_active},
    {"overflowing weapon build", &overflowing_weapon_build_saturates_atomically},
};

}  // namespace

arpg::test::TestSuite player_build_suite() noexcept {
    return arpg::test::make_suite("player_build", kCases);
}
