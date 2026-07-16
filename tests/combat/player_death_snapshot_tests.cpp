#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_affix_types.hpp"
#include "modifiers/damage_types.hpp"

#include <array>
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
    MonsterAffixSet affixes = {},
    std::int64_t player_health_more_bp = 10000) noexcept {
    CombatEncounterConfig config{};
    config.player_build.values.max_health_more = player_health_more_bp;
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{id, position, affixes, 7U};
    return config;
}

MonsterAffixSet one_affix(MonsterAffixId id) noexcept {
    MonsterAffixSet result{};
    result.values[0] = {id, MonsterAffixTier::m1};
    result.count = 1U;
    return result;
}

bool tick_until_defeated(CombatWorld& world, int limit = 600) noexcept {
    for (int tick = 0; tick < limit && !world.player_defeated(); ++tick) {
        world.tick(MovementInput{});
    }
    return world.player_defeated();
}

PlayerDamageSource canonicalized(PlayerDamageSource input) noexcept {
    CombatWorld world{CombatEncounterConfig{}};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 1, 0);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{1}, DamageDelivery::ground_or_environment,
        input, Vec3{}, FeedbackLevel::heavy);
    return world.death_snapshot()->source;
}

arpg::test::Failure melee_death_freezes_inside_the_real_call_stack() noexcept {
    CombatWorld world{one_monster(
        MonsterId::chaos_chaser, Vec3{0.65F, 0.0F, 0.0F},
        one_affix(MonsterAffixId::blink_assault), 100)};
    arpg::test::CombatWorldTestAccess::set_blink_empowered(world, 0U, true);
    while (world.snapshot().monsters[0].ai_phase != MonsterAiPhase::active) {
        world.tick(MovementInput{});
    }
    const auto before = world.snapshot().monsters[0];
    const bool contact_before =
        arpg::test::CombatWorldTestAccess::monster_contact_resolved(world, 0U);
    const std::uint16_t ai_ticks_before =
        arpg::test::CombatWorldTestAccess::monster_ai_ticks(world, 0U);
    arpg::test::drain_events(world);
    world.tick(MovementInput{});

    ARPG_REQUIRE(world.player_defeated());
    ARPG_REQUIRE(world.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::monster_attack);
    ARPG_REQUIRE(world.death_snapshot()->source.monster
                 == MonsterId::chaos_chaser);
    ARPG_REQUIRE(world.death_snapshot()->source.detail_id == 0U);
    const auto after = world.snapshot().monsters[0];
    ARPG_REQUIRE(after.hp == before.hp);
    ARPG_REQUIRE(after.ai_phase == before.ai_phase);
    ARPG_REQUIRE(after.blink_empowered == before.blink_empowered);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_contact_resolved(
        world, 0U) == contact_before);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_ai_ticks(
        world, 0U) == ai_ticks_before);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::defeated);
    }
    return {};
}

arpg::test::Failure charger_death_freezes_special_contact_state() noexcept {
    CombatWorld world{one_monster(
        MonsterId::fire_charger, Vec3{3.0F, 0.0F, 0.0F}, {}, 100)};
    while (world.snapshot().monsters[0].ai_phase != MonsterAiPhase::active) {
        world.tick(MovementInput{});
    }
    const auto before = world.snapshot().monsters[0];
    const bool contact_before =
        arpg::test::CombatWorldTestAccess::monster_contact_resolved(world, 0U);
    const std::uint16_t ai_ticks_before =
        arpg::test::CombatWorldTestAccess::monster_ai_ticks(world, 0U);
    world.tick(MovementInput{});

    ARPG_REQUIRE(world.player_defeated());
    const auto after = world.snapshot().monsters[0];
    ARPG_REQUIRE(after.hp == before.hp);
    ARPG_REQUIRE(after.ai_phase == before.ai_phase);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_contact_resolved(
        world, 0U) == contact_before);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_ai_ticks(
        world, 0U) == ai_ticks_before);
    return {};
}

arpg::test::Failure bomber_death_does_not_self_defeat_or_emit_monster_event() noexcept {
    CombatWorld world{one_monster(
        MonsterId::fire_bomber, Vec3{0.8F, 0.0F, 0.0F}, {}, 100)};
    while (world.snapshot().monsters[0].ai_phase != MonsterAiPhase::active) {
        world.tick(MovementInput{});
    }
    const auto before = world.snapshot().monsters[0];
    const bool contact_before =
        arpg::test::CombatWorldTestAccess::monster_contact_resolved(world, 0U);
    const std::uint16_t ai_ticks_before =
        arpg::test::CombatWorldTestAccess::monster_ai_ticks(world, 0U);
    arpg::test::drain_events(world);
    world.tick(MovementInput{});

    ARPG_REQUIRE(world.player_defeated());
    const auto after = world.snapshot().monsters[0];
    ARPG_REQUIRE(after.hp == before.hp);
    ARPG_REQUIRE(after.reaction == before.reaction);
    ARPG_REQUIRE(after.ai_phase == before.ai_phase);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_contact_resolved(
        world, 0U) == contact_before);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_ai_ticks(
        world, 0U) == ai_ticks_before);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::defeated);
    }
    return {};
}

arpg::test::Failure projectile_producer_reports_owner_and_zero_detail() noexcept {
    CombatWorld world{one_monster(
        MonsterId::lightning_shooter, Vec3{4.5F, 0.0F, 0.0F}, {}, 100)};
    ARPG_REQUIRE(tick_until_defeated(world));
    ARPG_REQUIRE(world.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::projectile);
    ARPG_REQUIRE(world.death_snapshot()->source.monster
                 == MonsterId::lightning_shooter);
    ARPG_REQUIRE(world.death_snapshot()->source.detail_id == 0U);
    return {};
}

arpg::test::Failure native_hazard_reports_owner_and_kind() noexcept {
    CombatWorld world{one_monster(
        MonsterId::chaos_hazard, Vec3{2.0F, 0.0F, 0.0F}, {}, 100)};
    ARPG_REQUIRE(tick_until_defeated(world));
    ARPG_REQUIRE(world.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::ground_hazard);
    ARPG_REQUIRE(world.death_snapshot()->source.monster
                 == MonsterId::chaos_hazard);
    ARPG_REQUIRE(world.death_snapshot()->source.detail_id
                 == static_cast<std::uint16_t>(HazardKind::native));
    return {};
}

arpg::test::Failure burning_ground_reports_real_affix_source() noexcept {
    CombatWorld world{one_monster(
        MonsterId::fire_charger, Vec3{},
        one_affix(MonsterAffixId::burning_ground), 100)};
    arpg::test::CombatWorldTestAccess::freeze_monster_ai(world, 0U, 300U);
    ARPG_REQUIRE(tick_until_defeated(world, 240));
    ARPG_REQUIRE(world.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::monster_affix);
    ARPG_REQUIRE(world.death_snapshot()->source.monster
                 == MonsterId::fire_charger);
    ARPG_REQUIRE(world.death_snapshot()->source.detail_id
                 == static_cast<std::uint16_t>(
                     MonsterAffixId::burning_ground));
    return {};
}

arpg::test::Failure chain_lightning_reports_real_affix_source() noexcept {
    CombatWorld world{one_monster(
        MonsterId::chaos_chaser, Vec3{0.65F, 0.0F, 0.0F},
        one_affix(MonsterAffixId::chain_lightning), 1000)};
    ARPG_REQUIRE(tick_until_defeated(world, 180));
    ARPG_REQUIRE(world.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::monster_affix);
    ARPG_REQUIRE(world.death_snapshot()->source.monster
                 == MonsterId::chaos_chaser);
    ARPG_REQUIRE(world.death_snapshot()->source.detail_id
                 == static_cast<std::uint16_t>(
                     MonsterAffixId::chain_lightning));
    return {};
}

arpg::test::Failure death_blast_reports_defeated_owner_source() noexcept {
    CombatWorld world{one_monster(
        MonsterId::fire_bomber, Vec3{0.8F, 0.0F, 0.0F},
        one_affix(MonsterAffixId::death_blast), 2000)};
    ARPG_REQUIRE(tick_until_defeated(world, 180));
    ARPG_REQUIRE(world.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::monster_affix);
    ARPG_REQUIRE(world.death_snapshot()->source.monster
                 == MonsterId::fire_bomber);
    ARPG_REQUIRE(world.death_snapshot()->source.detail_id
                 == static_cast<std::uint16_t>(MonsterAffixId::death_blast));
    return {};
}

arpg::test::Failure abyss_environment_reports_rule_and_no_monster() noexcept {
    CombatEncounterConfig abyss_config{};
    abyss_config.player_build.values.max_health_more = 100;
    abyss_config.abyss.rule = arpg::abyss::AbyssRuleId::chaos_expansion;
    abyss_config.abyss.environment.active = true;
    abyss_config.abyss.environment.damage_type =
        arpg::modifiers::DamageType::chaos;
    abyss_config.abyss.environment.damage_bp = 10000U;
    abyss_config.abyss.environment.damage_interval_ticks = 1U;
    abyss_config.abyss.environment.expansion_interval_ticks = 1U;
    abyss_config.abyss.environment.radius_milliunits[0] = 10000U;
    abyss_config.abyss.environment.radius_count = 1U;
    CombatWorld world{abyss_config};
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.player_defeated());
    ARPG_REQUIRE(world.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::abyss_environment);
    ARPG_REQUIRE(world.death_snapshot()->source.monster == MonsterId::count);
    ARPG_REQUIRE(world.death_snapshot()->source.detail_id
                 == static_cast<std::uint16_t>(
                     arpg::abyss::AbyssRuleId::chaos_expansion));
    return {};
}

arpg::test::Failure malformed_sources_are_canonicalized_at_entry() noexcept {
    constexpr std::array<PlayerDamageSourceKind, 4> monster_kinds{{
        PlayerDamageSourceKind::monster_attack,
        PlayerDamageSourceKind::projectile,
        PlayerDamageSourceKind::ground_hazard,
        PlayerDamageSourceKind::monster_affix,
    }};
    for (const PlayerDamageSourceKind kind : monster_kinds) {
        const PlayerDamageSource actual = canonicalized(
            source(kind, MonsterId::count, 77U));
        ARPG_REQUIRE(actual.kind == PlayerDamageSourceKind::unknown);
        ARPG_REQUIRE(actual.monster == MonsterId::count);
        ARPG_REQUIRE(actual.detail_id == 0U);
    }

    ARPG_REQUIRE(canonicalized(source(
        PlayerDamageSourceKind::monster_attack,
        MonsterId::fire_charger, 99U)).detail_id == 0U);
    ARPG_REQUIRE(canonicalized(source(
        PlayerDamageSourceKind::projectile,
        MonsterId::lightning_shooter, 99U)).detail_id == 0U);

    const PlayerDamageSource abyss = canonicalized(source(
        PlayerDamageSourceKind::abyss_environment,
        MonsterId::fire_bomber,
        static_cast<std::uint16_t>(
            arpg::abyss::AbyssRuleId::thunderstorm)));
    ARPG_REQUIRE(abyss.monster == MonsterId::count);

    const PlayerDamageSource invalid_abyss = canonicalized(
        source(PlayerDamageSourceKind::abyss_environment,
               MonsterId::count,
               static_cast<std::uint16_t>(arpg::abyss::AbyssRuleId::none)));
    ARPG_REQUIRE(invalid_abyss.kind == PlayerDamageSourceKind::unknown);

    const PlayerDamageSource unknown = canonicalized(source(
        PlayerDamageSourceKind::unknown, MonsterId::fire_bomber, 88U));
    ARPG_REQUIRE(unknown.monster == MonsterId::count);
    ARPG_REQUIRE(unknown.detail_id == 0U);
    return {};
}

arpg::test::Failure invalid_projectile_owner_never_damages_player() noexcept {
    CombatWorld world{one_monster(
        MonsterId::lightning_shooter, Vec3{4.0F, 0.0F, 0.0F})};
    const auto monster = world.snapshot().monsters[0];
    const MonsterHandle owner{0U, monster.generation};
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_projectile(
        world, owner, Vec3{}, Vec3{}, 100U, DamagePacket{1000}, 0.2F,
        false, {}));
    ARPG_REQUIRE(world.destroy_monster(owner));
    const int hp = world.snapshot().player.hp;
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().player.hp == hp);
    ARPG_REQUIRE(!world.death_snapshot().has_value());
    ARPG_REQUIRE(world.active_projectile_count() == 0U);
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
    CombatEncounterConfig config = one_monster(
        MonsterId::chaos_chaser, Vec3{0.65F, 0.0F, 0.0F},
        one_affix(MonsterAffixId::chaos_corrosion));
    config.abyss.rule = arpg::abyss::AbyssRuleId::thunderstorm;
    config.abyss.environment.active = true;
    config.abyss.environment.damage_type =
        arpg::modifiers::DamageType::lightning;
    config.abyss.environment.damage_bp = 10000U;
    config.abyss.environment.cycle_ticks = 120U;
    config.abyss.environment.warning_ticks = 0U;
    config.abyss.environment.duration_ticks = 1U;
    config.abyss.environment.damage_interval_ticks = 1U;
    config.abyss.environment.radius_milliunits[0] = 10000U;
    config.abyss.environment.radius_count = 1U;
    CombatWorld world{config};
    arpg::test::tick_n(world, 120);
    ARPG_REQUIRE(!world.player_defeated());
    ARPG_REQUIRE(world.snapshot().player.corrosion_ticks != 0U);
    const auto initial = world.snapshot();
    const MonsterHandle owner{0U, initial.monsters[0].generation};
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_projectile(
        world, owner, Vec3{8.0F, 4.0F, 0.0F}, Vec3{0.01F, 0.0F, 0.0F},
        1000U, DamagePacket{1}, 0.1F, false, {}));
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_hazard(world, owner));
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.player_defeated());
    ARPG_REQUIRE(world.death_snapshot()->source.kind
                 == PlayerDamageSourceKind::abyss_environment);
    const auto before = world.snapshot();
    const CombatDeathSnapshot death = *world.death_snapshot();
    arpg::test::tick_n(world, 600);
    const auto after = world.snapshot();
    ARPG_REQUIRE(after.tick == before.tick + 600U);
    ARPG_REQUIRE(after.player.position.x == before.player.position.x);
    ARPG_REQUIRE(after.player.corrosion_ticks == before.player.corrosion_ticks);
    ARPG_REQUIRE(after.monsters[0].position.x == before.monsters[0].position.x);
    ARPG_REQUIRE(after.monsters[0].ai_phase == before.monsters[0].ai_phase);
    ARPG_REQUIRE(after.projectiles[0].lifetime_ticks
                 == before.projectiles[0].lifetime_ticks);
    ARPG_REQUIRE(after.hazard_count == before.hazard_count);
    for (std::size_t index = 0U; index < before.hazard_count; ++index) {
        ARPG_REQUIRE(after.hazards[index].lifetime_ticks
                     == before.hazards[index].lifetime_ticks);
    }
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
    CombatLabConfig config{};
    config.dummy_spawns = {{{1.2F, 0.0F, 0.0F},
                            {7.0F, 3.0F, 0.0F},
                            {7.0F, -3.0F, 0.0F}}};
    config.respawn_defeated_dummies = false;
    CombatWorld world{config};
    const auto initial = world.snapshot();
    ARPG_REQUIRE(world.destroy_monster(MonsterHandle{
        1U, initial.monsters[1].generation}));
    ARPG_REQUIRE(world.destroy_monster(MonsterHandle{
        2U, initial.monsters[2].generation}));
    while (world.snapshot().monsters[0].hp > 30) {
        ARPG_REQUIRE(world.queue_action(Action::light));
        world.tick(MovementInput{});
        ARPG_REQUIRE(arpg::test::finish_attack(world, 128));
        arpg::test::drain_events(world);
    }

    arpg::abyss::AbyssCombatConfig abyss{};
    abyss.rule = arpg::abyss::AbyssRuleId::thunderstorm;
    abyss.environment.active = true;
    abyss.environment.damage_type = arpg::modifiers::DamageType::lightning;
    abyss.environment.damage_bp = 10000U;
    abyss.environment.cycle_ticks = static_cast<std::uint16_t>(
        world.snapshot().tick + 5U);
    abyss.environment.warning_ticks = 0U;
    abyss.environment.duration_ticks = 1U;
    abyss.environment.damage_interval_ticks = 1U;
    abyss.environment.radius_milliunits[0] = 10000U;
    abyss.environment.radius_count = 1U;
    arpg::test::CombatWorldTestAccess::activate_abyss_environment(
        world, abyss);
    arpg::test::drain_events(world);
    ARPG_REQUIRE(world.queue_action(Action::light));
    arpg::test::tick_n(world, 6);

    ARPG_REQUIRE(world.active_monster_count() == 1U);
    ARPG_REQUIRE(world.snapshot().monsters[0].reaction
                 == ReactionState::defeated);
    ARPG_REQUIRE(world.player_defeated());
    std::optional<std::uint64_t> monster_defeated_tick{};
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::defeated) {
            monster_defeated_tick = event->tick;
        }
    }
    ARPG_REQUIRE(monster_defeated_tick.has_value());
    ARPG_REQUIRE(*monster_defeated_tick == world.death_snapshot()->tick);
    return {};
}

arpg::test::Failure lethal_tick_performs_no_heap_allocations() noexcept {
    CombatEncounterConfig config{};
    config.player_build.values.max_health_more = 100;
    config.abyss.rule = arpg::abyss::AbyssRuleId::chaos_expansion;
    config.abyss.environment.active = true;
    config.abyss.environment.damage_type = arpg::modifiers::DamageType::chaos;
    config.abyss.environment.damage_bp = 10000U;
    config.abyss.environment.damage_interval_ticks = 1U;
    config.abyss.environment.expansion_interval_ticks = 1U;
    config.abyss.environment.radius_milliunits[0] = 10000U;
    config.abyss.environment.radius_count = 1U;
    CombatWorld world{config};
    const std::uint64_t before = arpg::test::allocation_count();
    world.tick(MovementInput{});
    const std::uint64_t after = arpg::test::allocation_count();
    ARPG_REQUIRE(world.player_defeated());
    ARPG_REQUIRE(after == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"melee immediate death freeze", &melee_death_freezes_inside_the_real_call_stack},
    {"charger immediate death freeze", &charger_death_freezes_special_contact_state},
    {"bomber immediate death freeze", &bomber_death_does_not_self_defeat_or_emit_monster_event},
    {"projectile real source", &projectile_producer_reports_owner_and_zero_detail},
    {"native hazard real source", &native_hazard_reports_owner_and_kind},
    {"burning ground real source", &burning_ground_reports_real_affix_source},
    {"chain lightning real source", &chain_lightning_reports_real_affix_source},
    {"death blast real source", &death_blast_reports_defeated_owner_source},
    {"abyss real source", &abyss_environment_reports_rule_and_no_monster},
    {"canonical malformed sources", &malformed_sources_are_canonicalized_at_entry},
    {"invalid projectile owner", &invalid_projectile_owner_never_damages_player},
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
