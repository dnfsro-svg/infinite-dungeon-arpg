#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_affix_types.hpp"
#include "modifiers/damage_types.hpp"

#include <cstdint>

namespace {

using namespace arpg::combat;

constexpr std::size_t kPhysical =
    arpg::modifiers::damage_index(arpg::modifiers::DamageType::physical);

PlayerDamageSource source(
    PlayerDamageSourceKind kind,
    MonsterId monster = MonsterId::count,
    std::uint16_t detail_id = 0U) noexcept {
    return PlayerDamageSource{kind, monster, detail_id};
}

CombatEncounterConfig one_monster(
    MonsterId id,
    Vec3 position,
    MonsterAffixSet affixes = {}) noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{id, position, affixes, 7U};
    return config;
}

bool tick_until_defeated(CombatWorld& world, int limit = 600) noexcept {
    for (int tick = 0; tick < limit && !world.player_defeated(); ++tick) {
        world.tick(MovementInput{});
    }
    return world.player_defeated();
}

arpg::test::Failure real_damage_producers_report_stable_sources() noexcept {
    CombatWorld melee{one_monster(
        MonsterId::chaos_chaser, Vec3{0.65F, 0.0F, 0.0F})};
    arpg::test::CombatWorldTestAccess::set_player_resources(melee, 1, 0);
    ARPG_REQUIRE(tick_until_defeated(melee));
    ARPG_REQUIRE(melee.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::monster_attack);
    ARPG_REQUIRE(melee.death_snapshot()->source.monster
                 == MonsterId::chaos_chaser);
    ARPG_REQUIRE(melee.death_snapshot()->source.detail_id == 0U);

    CombatWorld projectile{one_monster(
        MonsterId::lightning_shooter, Vec3{4.5F, 0.0F, 0.0F})};
    arpg::test::CombatWorldTestAccess::set_player_resources(projectile, 1, 0);
    ARPG_REQUIRE(tick_until_defeated(projectile));
    ARPG_REQUIRE(projectile.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::projectile);
    ARPG_REQUIRE(projectile.death_snapshot()->source.monster
                 == MonsterId::lightning_shooter);
    ARPG_REQUIRE(projectile.death_snapshot()->source.detail_id == 0U);

    CombatWorld hazard{one_monster(
        MonsterId::chaos_hazard, Vec3{2.0F, 0.0F, 0.0F})};
    arpg::test::CombatWorldTestAccess::set_player_resources(hazard, 1, 0);
    ARPG_REQUIRE(tick_until_defeated(hazard));
    ARPG_REQUIRE(hazard.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::ground_hazard);
    ARPG_REQUIRE(hazard.death_snapshot()->source.monster
                 == MonsterId::chaos_hazard);
    ARPG_REQUIRE(hazard.death_snapshot()->source.detail_id
                 == static_cast<std::uint16_t>(HazardKind::native));
    return {};
}

arpg::test::Failure affix_abyss_and_unknown_sources_are_explicit() noexcept {
    MonsterAffixSet burning{};
    burning.values[0] = {
        MonsterAffixId::burning_ground, MonsterAffixTier::m1};
    burning.count = 1U;
    CombatWorld affix{one_monster(
        MonsterId::fire_charger, Vec3{}, burning)};
    arpg::test::CombatWorldTestAccess::set_active_affix_ticks(
        affix, 0U, 0xFFFFU, 0U);
    arpg::test::CombatWorldTestAccess::tick_active_affixes(affix, 0U);
    arpg::test::CombatWorldTestAccess::set_player_resources(affix, 1, 0);
    affix.tick(MovementInput{});
    ARPG_REQUIRE(affix.player_defeated());
    ARPG_REQUIRE(affix.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::monster_affix);
    ARPG_REQUIRE(affix.death_snapshot()->source.detail_id
                 == static_cast<std::uint16_t>(
                     MonsterAffixId::burning_ground));

    CombatEncounterConfig abyss_config{};
    abyss_config.abyss.rule = arpg::abyss::AbyssRuleId::chaos_expansion;
    abyss_config.abyss.environment.active = true;
    abyss_config.abyss.environment.damage_type =
        arpg::modifiers::DamageType::chaos;
    abyss_config.abyss.environment.damage_bp = 10000U;
    abyss_config.abyss.environment.damage_interval_ticks = 1U;
    abyss_config.abyss.environment.expansion_interval_ticks = 1U;
    abyss_config.abyss.environment.radius_milliunits[0] = 10000U;
    abyss_config.abyss.environment.radius_count = 1U;
    CombatWorld abyss{abyss_config};
    arpg::test::CombatWorldTestAccess::set_player_resources(abyss, 1, 0);
    abyss.tick(MovementInput{});
    ARPG_REQUIRE(abyss.player_defeated());
    ARPG_REQUIRE(abyss.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::abyss_environment);
    ARPG_REQUIRE(abyss.death_snapshot()->source.detail_id
                 == static_cast<std::uint16_t>(
                     arpg::abyss::AbyssRuleId::chaos_expansion));

    CombatWorld unknown{one_monster(MonsterId::chaos_hazard, Vec3{})};
    const auto monster = unknown.snapshot().monsters[0];
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_hazard(
        unknown, MonsterHandle{0U, monster.generation}, HazardKind::native,
        DamagePacket{1000}, true));
    arpg::test::CombatWorldTestAccess::invalidate_first_hazard_owner(unknown);
    arpg::test::CombatWorldTestAccess::set_player_resources(unknown, 1, 0);
    unknown.tick(MovementInput{});
    ARPG_REQUIRE(unknown.player_defeated());
    ARPG_REQUIRE(unknown.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::unknown);
    ARPG_REQUIRE(unknown.death_snapshot()->source.monster == MonsterId::count);
    ARPG_REQUIRE(unknown.death_snapshot()->source.detail_id == 0U);
    return {};
}

arpg::test::Failure evasion_does_not_enter_recent_damage() noexcept {
    CombatWorld world{CombatEncounterConfig{}};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 20, 0);
    arpg::test::CombatWorldTestAccess::set_player_evasion_rate_bp(world, 10000);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{100}, DamageDelivery::direct,
        source(PlayerDamageSourceKind::monster_attack,
               MonsterId::fire_charger),
        Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(world.snapshot().player.hp == 20);
    arpg::test::CombatWorldTestAccess::set_player_evasion_rate_bp(world, 0);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{20}, DamageDelivery::ground_or_environment,
        source(PlayerDamageSourceKind::unknown),
        Vec3{}, FeedbackLevel::heavy);
    ARPG_REQUIRE(world.death_snapshot()->recent_damage[kPhysical] == 20U);
    return {};
}

arpg::test::Failure delayed_corrosion_keeps_the_affix_owner_source() noexcept {
    MonsterAffixSet corrosion{};
    corrosion.values[0] = {
        MonsterAffixId::chaos_corrosion, MonsterAffixTier::m1};
    corrosion.count = 1U;
    CombatWorld world{one_monster(
        MonsterId::chaos_chaser, Vec3{0.65F, 0.0F, 0.0F}, corrosion)};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 65, 0);
    ARPG_REQUIRE(tick_until_defeated(world));
    const auto& death = *world.death_snapshot();
    ARPG_REQUIRE(death.source.kind == PlayerDamageSourceKind::monster_affix);
    ARPG_REQUIRE(death.source.monster == MonsterId::chaos_chaser);
    ARPG_REQUIRE(death.source.detail_id == static_cast<std::uint16_t>(
        MonsterAffixId::chaos_corrosion));
    ARPG_REQUIRE(death.raw_damage == 20U);
    ARPG_REQUIRE(death.primary_type == arpg::modifiers::DamageType::chaos);
    return {};
}

arpg::test::Failure barrier_absorption_and_lethal_split_are_exact() noexcept {
    CombatWorld absorbed{CombatEncounterConfig{}};
    arpg::test::CombatWorldTestAccess::set_player_resources(absorbed, 20, 30);
    arpg::test::CombatWorldTestAccess::apply_damage(
        absorbed, DamagePacket{10}, DamageDelivery::direct,
        source(PlayerDamageSourceKind::unknown), Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(absorbed.snapshot().player.barrier == 20);
    ARPG_REQUIRE(absorbed.snapshot().player.hp == 20);

    CombatWorld lethal{CombatEncounterConfig{}};
    arpg::test::CombatWorldTestAccess::set_player_resources(lethal, 20, 30);
    arpg::test::CombatWorldTestAccess::apply_damage(
        lethal, DamagePacket{100}, DamageDelivery::ground_or_environment,
        source(PlayerDamageSourceKind::ground_hazard,
               MonsterId::chaos_hazard,
               static_cast<std::uint16_t>(HazardKind::native)),
        Vec3{}, FeedbackLevel::heavy);
    const auto& death = *lethal.death_snapshot();
    ARPG_REQUIRE(death.barrier_loss == 30U);
    ARPG_REQUIRE(death.health_loss == 20U);
    ARPG_REQUIRE(death.final_damage == 50U);
    ARPG_REQUIRE(death.defense.barrier == 0);
    ARPG_REQUIRE(death.defense.hp == 0);
    return {};
}

arpg::test::Failure overkill_is_capped_and_distributed_by_actual_loss() noexcept {
    CombatWorld world{CombatEncounterConfig{}};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 20, 30);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{{200, 200, 200, 200, 200}},
        DamageDelivery::ground_or_environment,
        source(PlayerDamageSourceKind::unknown), Vec3{}, FeedbackLevel::heavy);
    const auto& death = *world.death_snapshot();
    ARPG_REQUIRE(death.raw_damage == 1000U);
    ARPG_REQUIRE(death.final_damage == 50U);
    std::uint64_t recent_total{};
    for (std::uint64_t value : death.recent_damage) recent_total += value;
    ARPG_REQUIRE(recent_total == 50U);
    for (std::uint64_t value : death.recent_damage) {
        ARPG_REQUIRE(value == 10U);
    }
    ARPG_REQUIRE(death.primary_type == arpg::modifiers::DamageType::physical);

    CombatWorld remainder{CombatEncounterConfig{}};
    arpg::test::CombatWorldTestAccess::set_player_resources(remainder, 1, 0);
    arpg::test::CombatWorldTestAccess::apply_damage(
        remainder, DamagePacket{{1, 1, 0, 0, 0}},
        DamageDelivery::ground_or_environment,
        source(PlayerDamageSourceKind::unknown), Vec3{}, FeedbackLevel::heavy);
    ARPG_REQUIRE(remainder.death_snapshot()->recent_damage[kPhysical] == 1U);
    ARPG_REQUIRE(remainder.death_snapshot()->recent_damage[
        arpg::modifiers::damage_index(arpg::modifiers::DamageType::fire)] == 0U);
    return {};
}

arpg::test::Failure first_lethal_hit_wins_within_the_same_tick() noexcept {
    CombatWorld world{CombatEncounterConfig{}};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 1, 0);
    const PlayerDamageSource first = source(
        PlayerDamageSourceKind::monster_attack, MonsterId::fire_bomber);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{1}, DamageDelivery::direct,
        first, Vec3{}, FeedbackLevel::light);
    const CombatDeathSnapshot frozen = *world.death_snapshot();
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{1000}, DamageDelivery::ground_or_environment,
        source(PlayerDamageSourceKind::abyss_environment, MonsterId::count,
               static_cast<std::uint16_t>(
                   arpg::abyss::AbyssRuleId::thunderstorm)),
        Vec3{}, FeedbackLevel::heavy);
    ARPG_REQUIRE(world.death_snapshot()->source.kind == first.kind);
    ARPG_REQUIRE(world.death_snapshot()->source.monster == first.monster);
    ARPG_REQUIRE(world.death_snapshot()->final_damage == frozen.final_damage);
    ARPG_REQUIRE(world.death_snapshot()->recent_damage == frozen.recent_damage);
    return {};
}

arpg::test::Failure death_freezes_combat_for_six_hundred_ticks() noexcept {
    CombatWorld world{one_monster(
        MonsterId::lightning_shooter, Vec3{4.0F, 0.0F, 0.0F})};
    const auto initial = world.snapshot();
    const MonsterHandle owner{0U, initial.monsters[0].generation};
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_projectile(
        world, owner, Vec3{8.0F, 4.0F, 0.0F}, Vec3{0.01F, 0.0F, 0.0F},
        1000U, DamagePacket{1}, 0.1F, false, {}));
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_hazard(
        world, owner, HazardKind::native, DamagePacket{1}));
    PlayerStatusRuntime status{};
    status.slow_bp = 1000;
    status.slow_ticks = 120U;
    status.corrosion_damage_per_second = 1;
    status.corrosion_ticks = 120U;
    arpg::test::CombatWorldTestAccess::set_player_status(world, status);
    ARPG_REQUIRE(world.queue_action(Action::light));
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 1, 0);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{1}, DamageDelivery::ground_or_environment,
        source(PlayerDamageSourceKind::unknown), Vec3{}, FeedbackLevel::heavy);
    const auto before = world.snapshot();
    const CombatDeathSnapshot death = *world.death_snapshot();
    arpg::test::tick_n(world, 600);
    const auto after = world.snapshot();
    ARPG_REQUIRE(after.tick == before.tick + 600U);
    ARPG_REQUIRE(after.player.position.x == before.player.position.x);
    ARPG_REQUIRE(after.player.slow_ticks == before.player.slow_ticks);
    ARPG_REQUIRE(after.player.corrosion_ticks == before.player.corrosion_ticks);
    ARPG_REQUIRE(after.monsters[0].position.x == before.monsters[0].position.x);
    ARPG_REQUIRE(after.monsters[0].ai_phase == before.monsters[0].ai_phase);
    ARPG_REQUIRE(after.projectiles[0].lifetime_ticks
                 == before.projectiles[0].lifetime_ticks);
    ARPG_REQUIRE(after.hazards[0].lifetime_ticks
                 == before.hazards[0].lifetime_ticks);
    ARPG_REQUIRE(after.diagnostics.input_size == before.diagnostics.input_size);
    ARPG_REQUIRE(world.death_snapshot()->recent_damage == death.recent_damage);
    ARPG_REQUIRE(world.death_snapshot()->tick == death.tick);
    ARPG_REQUIRE(world.death_snapshot()->source.kind == death.source.kind);
    ARPG_REQUIRE(!world.queue_action(Action::light));
    int defeated_events{};
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::player_defeated) ++defeated_events;
    }
    ARPG_REQUIRE(defeated_events == 1);
    return {};
}

arpg::test::Failure same_tick_mutual_defeat_prefers_player_death() noexcept {
    CombatWorld world{one_monster(MonsterId::fire_bomber, Vec3{})};
    arpg::test::CombatWorldTestAccess::defeat_monster(world, 0U, true);
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 1, 0);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{1}, DamageDelivery::ground_or_environment,
        source(PlayerDamageSourceKind::unknown), Vec3{}, FeedbackLevel::heavy);
    ARPG_REQUIRE(world.active_monster_count() == 1U);
    ARPG_REQUIRE(world.snapshot().monsters[0].reaction
                 == ReactionState::defeated);
    ARPG_REQUIRE(world.player_defeated());
    return {};
}

arpg::test::Failure lethal_tick_performs_no_heap_allocations() noexcept {
    CombatEncounterConfig config{};
    config.abyss.rule = arpg::abyss::AbyssRuleId::chaos_expansion;
    config.abyss.environment.active = true;
    config.abyss.environment.damage_type = arpg::modifiers::DamageType::chaos;
    config.abyss.environment.damage_bp = 10000U;
    config.abyss.environment.damage_interval_ticks = 1U;
    config.abyss.environment.expansion_interval_ticks = 1U;
    config.abyss.environment.radius_milliunits[0] = 10000U;
    config.abyss.environment.radius_count = 1U;
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 1, 0);
    const std::uint64_t before = arpg::test::allocation_count();
    world.tick(MovementInput{});
    const std::uint64_t after = arpg::test::allocation_count();
    ARPG_REQUIRE(world.player_defeated());
    ARPG_REQUIRE(after == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"real producer source matrix", &real_damage_producers_report_stable_sources},
    {"affix abyss and unknown sources", &affix_abyss_and_unknown_sources_are_explicit},
    {"evasion excluded from history", &evasion_does_not_enter_recent_damage},
    {"corrosion keeps source", &delayed_corrosion_keeps_the_affix_owner_source},
    {"barrier and lethal split", &barrier_absorption_and_lethal_split_are_exact},
    {"overkill actual distribution", &overkill_is_capped_and_distributed_by_actual_loss},
    {"first lethal hit wins", &first_lethal_hit_wins_within_the_same_tick},
    {"death freezes combat", &death_freezes_combat_for_six_hundred_ticks},
    {"same tick mutual defeat", &same_tick_mutual_defeat_prefers_player_death},
    {"lethal tick zero allocations", &lethal_tick_performs_no_heap_allocations},
};

}  // namespace

arpg::test::TestSuite player_death_snapshot_suite() noexcept {
    return arpg::test::make_suite("player_death_snapshot", kCases);
}
