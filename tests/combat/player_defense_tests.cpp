#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"
#include "modifiers/damage_types.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace {

using namespace arpg::combat;
using namespace arpg::modifiers;

static_assert(std::is_same_v<
    decltype(PlayerSnapshot{}.damage_reduction),
    std::array<std::int32_t, 4>>);
static_assert(std::is_same_v<
    decltype(PlayerSnapshot{}.damage_reduction_cap),
    std::array<std::int32_t, 4>>);
static_assert(std::is_same_v<decltype(PlayerSnapshot{}.armor), std::int64_t>);
static_assert(std::is_same_v<decltype(PlayerSnapshot{}.evasion), std::int64_t>);
static_assert(std::is_same_v<
    decltype(PlayerSnapshot{}.armor_reduction_bp), std::int32_t>);
static_assert(std::is_same_v<
    decltype(PlayerSnapshot{}.evasion_rate_bp), std::int32_t>);

CombatEncounterConfig defense_config(PlayerCombatBuild build = {}) noexcept {
    CombatEncounterConfig config{};
    config.player_build = build;
    config.evasion_seed = 1U;
    return config;
}

void wait_for_protection(CombatWorld& world) noexcept {
    arpg::test::tick_n(world, 30);
}

bool same_player_snapshot(
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

arpg::test::Failure armor_and_each_element_resolve_independently() noexcept {
    PlayerCombatBuild build{};
    build.values.armor = 100;
    build.values.damage_reduction[element_index(DamageType::fire)] = 5000;
    build.values.damage_reduction[element_index(DamageType::water)] = -6000;
    build.values.damage_reduction[element_index(DamageType::lightning)] = 7500;
    build.values.damage_reduction[element_index(DamageType::chaos)] = 9500;
    build.values.damage_reduction_cap_bonus[element_index(DamageType::chaos)] =
        2000;
    const auto damage = resolve_player_damage(
        DamagePacket{{10, 10, 10, 1, 100}}, build);
    ARPG_REQUIRE(damage.has_value());
    ARPG_REQUIRE(*damage == 35);
    return {};
}

arpg::test::Failure elemental_cap_defaults_to_7500_and_can_reach_9500() noexcept {
    PlayerCombatBuild build{};
    build.values.damage_reduction[element_index(DamageType::fire)] = 9500;
    auto damage = resolve_player_damage(DamagePacket{{0, 100, 0, 0, 0}}, build);
    ARPG_REQUIRE(damage.has_value());
    ARPG_REQUIRE(*damage == 25);

    build.values.damage_reduction_cap_bonus[element_index(DamageType::fire)] =
        2000;
    damage = resolve_player_damage(DamagePacket{{0, 100, 0, 0, 0}}, build);
    ARPG_REQUIRE(damage.has_value());
    ARPG_REQUIRE(*damage == 5);
    return {};
}

arpg::test::Failure positive_component_uses_ceil_and_stays_at_least_one() noexcept {
    PlayerCombatBuild build{};
    build.values.armor = 10000000;
    build.values.damage_reduction[element_index(DamageType::fire)] = 9500;
    build.values.damage_reduction_cap_bonus[element_index(DamageType::fire)] =
        2000;
    const auto damage = resolve_player_damage(
        DamagePacket{{1, 1, 0, 0, 0}}, build);
    ARPG_REQUIRE(damage.has_value());
    ARPG_REQUIRE(*damage == 2);
    return {};
}

arpg::test::Failure invalid_resolution_is_distinct_from_legal_zero() noexcept {
    const auto zero = resolve_player_damage(DamagePacket{}, PlayerCombatBuild{});
    ARPG_REQUIRE(zero.has_value());
    ARPG_REQUIRE(*zero == 0);

    PlayerCombatBuild invalid{};
    invalid.values.valid = false;
    ARPG_REQUIRE(!resolve_player_damage(DamagePacket{1}, invalid).has_value());
    invalid = PlayerCombatBuild{};
    invalid.weapon_physical = -1;
    ARPG_REQUIRE(!resolve_player_damage(DamagePacket{1}, invalid).has_value());
    invalid = PlayerCombatBuild{};
    invalid.local_attack_speed_bp = -1;
    ARPG_REQUIRE(!resolve_player_damage(DamagePacket{1}, invalid).has_value());

    PlayerCombatBuild overflowing{};
    overflowing.values.damage_taken = 20000;
    constexpr int maximum = (std::numeric_limits<int>::max)();
    ARPG_REQUIRE(!resolve_player_damage(
        DamagePacket{{maximum, maximum, maximum, maximum, maximum}},
        overflowing).has_value());
    return {};
}

arpg::test::Failure invalid_direct_packet_does_not_advance_evasion_rng() noexcept {
    PlayerCombatBuild build{};
    build.values.evasion = 50;
    CombatWorld with_invalid{defense_config(build)};
    CombatWorld control{defense_config(build)};
    arpg::test::drain_events(with_invalid);
    arpg::test::drain_events(control);

    const auto before = with_invalid.snapshot();
    constexpr int maximum = (std::numeric_limits<int>::max)();
    arpg::test::CombatWorldTestAccess::apply_damage(
        with_invalid,
        DamagePacket{{maximum, maximum, maximum, maximum, maximum}},
        DamageDelivery::direct, Vec3{}, FeedbackLevel::heavy);
    const auto after_invalid = with_invalid.snapshot();
    ARPG_REQUIRE(after_invalid.tick == before.tick);
    ARPG_REQUIRE(same_player_snapshot(after_invalid.player, before.player));
    ARPG_REQUIRE(!with_invalid.try_pop_event().has_value());

    arpg::test::CombatWorldTestAccess::apply_damage(
        with_invalid, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    arpg::test::CombatWorldTestAccess::apply_damage(
        control, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(with_invalid.snapshot().player.hp
                 == control.snapshot().player.hp);
    ARPG_REQUIRE(with_invalid.snapshot().player.hp
                 == before.player.max_hp - 10);

    wait_for_protection(with_invalid);
    wait_for_protection(control);
    arpg::test::CombatWorldTestAccess::apply_damage(
        with_invalid, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    arpg::test::CombatWorldTestAccess::apply_damage(
        control, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(with_invalid.snapshot().player.hp
                 == control.snapshot().player.hp);
    ARPG_REQUIRE(with_invalid.snapshot().player.hp
                 == before.player.max_hp - 10);
    return {};
}

arpg::test::Failure zero_evasion_does_not_advance_future_evasion_stream() noexcept {
    CombatWorld world{defense_config()};
    const int maximum = world.snapshot().player.max_hp;
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 10);
    wait_for_protection(world);

    PlayerCombatBuild evasive{};
    evasive.values.evasion = 50;
    world.apply_player_build(evasive);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 20);
    return {};
}

arpg::test::Failure identical_seed_and_trace_produce_identical_damage() noexcept {
    PlayerCombatBuild build{};
    build.values.evasion = 50;
    CombatWorld left{defense_config(build)};
    CombatWorld right{defense_config(build)};
    for (int index = 0; index < 4; ++index) {
        const DamageDelivery delivery = index == 2
            ? DamageDelivery::ground_or_environment
            : DamageDelivery::direct;
        const DamagePacket packet = index == 1
            ? DamagePacket{}
            : DamagePacket{{3, 2, 1, 4, 5}};
        arpg::test::CombatWorldTestAccess::apply_damage(
            left, packet, delivery, Vec3{}, FeedbackLevel::light);
        arpg::test::CombatWorldTestAccess::apply_damage(
            right, packet, delivery, Vec3{}, FeedbackLevel::light);
        ARPG_REQUIRE(left.snapshot().player.hp == right.snapshot().player.hp);
        ARPG_REQUIRE(
            left.snapshot().player.barrier == right.snapshot().player.barrier);
        wait_for_protection(left);
        wait_for_protection(right);
    }
    return {};
}

arpg::test::Failure real_ground_hazard_bypasses_evasion() noexcept {
    PlayerCombatBuild build{};
    build.values.evasion = 1000000;
    CombatEncounterConfig config = defense_config(build);
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_hazard, Vec3{4.0F, 0.0F, 0.0F}};
    CombatWorld world{config};
    const int maximum = world.snapshot().player.max_hp;
    for (int tick = 0; tick < 500 && world.snapshot().player.hp == maximum;
         ++tick) {
        world.tick(MovementInput{});
    }
    ARPG_REQUIRE(world.snapshot().player.hp < maximum);
    return {};
}

arpg::test::Failure five_component_direct_packet_consumes_one_roll() noexcept {
    PlayerCombatBuild build{};
    build.values.evasion = 50;
    CombatWorld world{defense_config(build)};
    const int maximum = world.snapshot().player.max_hp;
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{{1, 1, 1, 1, 1}}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 5);

    wait_for_protection(world);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 5);
    return {};
}

arpg::test::Failure ground_and_zero_packets_do_not_consume_evasion_rng() noexcept {
    PlayerCombatBuild build{};
    build.values.evasion = 50;
    CombatWorld ground{defense_config(build)};
    const int maximum = ground.snapshot().player.max_hp;
    arpg::test::CombatWorldTestAccess::apply_damage(
        ground, DamagePacket{10}, DamageDelivery::ground_or_environment,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(ground.snapshot().player.hp == maximum - 10);
    wait_for_protection(ground);
    arpg::test::CombatWorldTestAccess::apply_damage(
        ground, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(ground.snapshot().player.hp == maximum - 20);

    CombatWorld zero{defense_config(build)};
    arpg::test::CombatWorldTestAccess::apply_damage(
        zero, DamagePacket{}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    arpg::test::CombatWorldTestAccess::apply_damage(
        zero, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(zero.snapshot().player.hp == maximum - 10);
    return {};
}

arpg::test::Failure reset_reseeds_evasion_stream() noexcept {
    PlayerCombatBuild build{};
    build.values.evasion = 50;
    CombatWorld world{defense_config(build)};
    const int maximum = world.snapshot().player.max_hp;
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 10);
    wait_for_protection(world);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 10);

    world.reset();
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{10}, DamageDelivery::direct,
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 10);
    return {};
}

bool first_attack_is_evaded(MonsterId id, Vec3 spawn) noexcept {
    PlayerCombatBuild build{};
    build.values.evasion = 1000000;
    CombatEncounterConfig config = defense_config(build);
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{id, spawn};
    CombatWorld world{config};
    const int maximum = world.snapshot().player.max_hp;
    bool saw_active = false;
    bool saw_projectile = false;
    bool completed_attempt = false;
    for (int tick = 0; tick < 500; ++tick) {
        world.tick(MovementInput{});
        const auto snapshot = world.snapshot();
        if (snapshot.player.invulnerability_ticks != 0U) return false;
        saw_active = saw_active
            || snapshot.monsters[0].ai_phase == MonsterAiPhase::active;
        saw_projectile = saw_projectile || snapshot.projectile_count != 0U;
        if (id == MonsterId::lightning_shooter
            && saw_projectile && snapshot.projectile_count == 0U) {
            completed_attempt = true;
            break;
        }
        if ((id == MonsterId::chaos_chaser || id == MonsterId::fire_charger)
            && saw_active
            && snapshot.monsters[0].ai_phase == MonsterAiPhase::recovery) {
            completed_attempt = true;
            break;
        }
    }
    return completed_attempt && world.snapshot().player.hp == maximum;
}

arpg::test::Failure melee_projectile_and_charge_calls_are_direct() noexcept {
    ARPG_REQUIRE(first_attack_is_evaded(
        MonsterId::chaos_chaser, Vec3{0.65F, 0.0F, 0.0F}));
    ARPG_REQUIRE(first_attack_is_evaded(
        MonsterId::lightning_shooter, Vec3{4.5F, 0.0F, 0.0F}));
    ARPG_REQUIRE(first_attack_is_evaded(
        MonsterId::fire_charger, Vec3{3.0F, 0.0F, 0.0F}));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"armor and elemental components", &armor_and_each_element_resolve_independently},
    {"elemental cap bonus", &elemental_cap_defaults_to_7500_and_can_reach_9500},
    {"positive component ceil", &positive_component_uses_ceil_and_stays_at_least_one},
    {"invalid versus zero", &invalid_resolution_is_distinct_from_legal_zero},
    {"invalid direct does not roll", &invalid_direct_packet_does_not_advance_evasion_rng},
    {"zero evasion does not roll", &zero_evasion_does_not_advance_future_evasion_stream},
    {"identical seed and trace", &identical_seed_and_trace_produce_identical_damage},
    {"real hazard bypasses evasion", &real_ground_hazard_bypasses_evasion},
    {"one direct roll per packet", &five_component_direct_packet_consumes_one_roll},
    {"ground and zero do not roll", &ground_and_zero_packets_do_not_consume_evasion_rng},
    {"reset reseeds evasion", &reset_reseeds_evasion_stream},
    {"direct delivery call sites", &melee_projectile_and_charge_calls_are_direct},
};

}  // namespace

arpg::test::TestSuite player_defense_suite() noexcept {
    return arpg::test::make_suite("player_defense", kCases);
}
