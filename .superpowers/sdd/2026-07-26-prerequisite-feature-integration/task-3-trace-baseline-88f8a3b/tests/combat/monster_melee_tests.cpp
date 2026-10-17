#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_catalog.hpp"

#include "abyss/abyss_rules.hpp"
#include "combat/monster_affix_runtime.hpp"

#include <cmath>

namespace {

using namespace arpg::combat;
using arpg::test::tick_n;

CombatEncounterConfig encounter_for(
    MonsterId id,
    float monster_x,
    float player_x = 0.0F,
    float player_y = 0.0F) noexcept {
    CombatEncounterConfig config{};
    config.player_spawn = Vec3{player_x, player_y, 0.0F};
    config.initial_facing = Facing::right;
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        id, Vec3{monster_x, 0.0F, 0.0F}};
    return config;
}

MonsterAffixSet one_affix(
    MonsterAffixId id,
    MonsterAffixTier tier) noexcept {
    MonsterAffixSet result{};
    result.values[0] = {id, tier};
    result.count = 1U;
    return result;
}

MonsterAffixSet two_affixes(
    MonsterAffixId first,
    MonsterAffixTier first_tier,
    MonsterAffixId second,
    MonsterAffixTier second_tier) noexcept {
    MonsterAffixSet result{};
    result.values[0] = {first, first_tier};
    result.values[1] = {second, second_tier};
    result.count = 2U;
    return result;
}

int ticks_in_phase(CombatWorld& world, MonsterAiPhase phase) noexcept {
    int ticks = 0;
    while (ticks < 300
           && world.snapshot().monsters[0].ai_phase == phase) {
        world.tick({});
        ++ticks;
    }
    return ticks;
}

bool has_abyss_phase_timing(
    CombatEncounterConfig config,
    std::uint16_t telegraph,
    std::uint16_t active,
    std::uint16_t recovery,
    std::uint16_t cooldown) noexcept {
    config.wave.spawns[0].position = Vec3{0.90F, 0.0F, 0.0F};
    CombatWorld world{config};
    world.tick({});
    return world.snapshot().monsters[0].ai_phase == MonsterAiPhase::telegraph
        && ticks_in_phase(world, MonsterAiPhase::telegraph) == telegraph
        && ticks_in_phase(world, MonsterAiPhase::active) == active
        && ticks_in_phase(world, MonsterAiPhase::recovery) == recovery
        && ticks_in_phase(world, MonsterAiPhase::cooldown) == cooldown;
}

arpg::test::Failure chaos_chaser_moves_then_stops_for_telegraph() noexcept {
    CombatWorld world{encounter_for(MonsterId::chaos_chaser, 2.0F)};
    const float initial_x = world.snapshot().monsters[0].position.x;
    world.tick(MovementInput{});
    const auto moving = world.snapshot();
    ARPG_REQUIRE(moving.monsters[0].ai_phase == MonsterAiPhase::move);
    ARPG_REQUIRE(moving.monsters[0].position.x < initial_x);

    bool reached_telegraph = false;
    for (int tick = 0; tick < 120; ++tick) {
        world.tick(MovementInput{});
        if (world.snapshot().monsters[0].ai_phase
            == MonsterAiPhase::telegraph) {
            reached_telegraph = true;
            break;
        }
    }
    ARPG_REQUIRE(reached_telegraph);
    const float telegraph_x = world.snapshot().monsters[0].position.x;
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().monsters[0].position.x == telegraph_x);
    return {};
}

arpg::test::Failure chaos_chaser_damages_only_once_per_active_serial() noexcept {
    CombatWorld world{encounter_for(MonsterId::chaos_chaser, 0.65F)};
    bool reached_active = false;
    for (int tick = 0; tick < 120; ++tick) {
        world.tick(MovementInput{});
        if (world.snapshot().monsters[0].ai_phase == MonsterAiPhase::active) {
            reached_active = true;
            break;
        }
    }
    ARPG_REQUIRE(reached_active);
    const int after_first_active_tick = world.snapshot().player.hp;
    tick_n(world, 3);
    const int after_active = world.snapshot().player.hp;
    ARPG_REQUIRE(after_active < after_first_active_tick);

    tick_n(world, 18 + 42);
    const int after_cooldown = world.snapshot().player.hp;
    ARPG_REQUIRE(after_cooldown == after_active);
    return {};
}

arpg::test::Failure unsupported_monsters_remain_inert() noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 2U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::fire_bomber, Vec3{2.0F, 0.0F, 0.0F}};
    config.wave.spawns[1] = MonsterSpawnSpec{
        MonsterId::lightning_dasher, Vec3{3.0F, 0.0F, 0.0F}};
    CombatWorld world{config};
    const auto before = world.snapshot();
    world.tick(MovementInput{});
    const auto after = world.snapshot();
    for (std::size_t index = 0; index < 2U; ++index) {
        ARPG_REQUIRE(after.monsters[index].ai_phase == MonsterAiPhase::idle);
        ARPG_REQUIRE(after.monsters[index].position.x
                     == before.monsters[index].position.x);
        ARPG_REQUIRE(after.monsters[index].position.y
                     == before.monsters[index].position.y);
        ARPG_REQUIRE(after.monsters[index].velocity.x == 0.0F);
        ARPG_REQUIRE(after.monsters[index].velocity.y == 0.0F);
        ARPG_REQUIRE(after.player.hp == after.player.max_hp);
    }
    return {};
}

arpg::test::Failure unsupported_monster_reaction_recovers() noexcept {
    CombatWorld world{encounter_for(MonsterId::fire_bomber, 1.20F)};
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    tick_n(world, 5);
    ARPG_REQUIRE(world.snapshot().monsters[0].reaction
                 == ReactionState::hitstun);
    tick_n(world, 20);
    ARPG_REQUIRE(world.snapshot().monsters[0].reaction == ReactionState::idle);
    ARPG_REQUIRE(world.snapshot().monsters[0].ai_phase == MonsterAiPhase::idle);
    return {};
}

arpg::test::Failure water_bulwark_is_slow_and_has_front_armor() noexcept {
    const MonsterDefinition* chaser =
        monster_definition(MonsterId::chaos_chaser);
    const MonsterDefinition* bulwark =
        monster_definition(MonsterId::water_bulwark);
    ARPG_REQUIRE(chaser != nullptr);
    ARPG_REQUIRE(bulwark != nullptr);
    ARPG_REQUIRE(bulwark->move_speed < chaser->move_speed);
    ARPG_REQUIRE(bulwark->max_hp > chaser->max_hp);
    ARPG_REQUIRE(bulwark->max_break > 0);

    CombatWorld world{encounter_for(MonsterId::water_bulwark, 1.2F)};
    const auto initial = world.snapshot().monsters[0];
    ARPG_REQUIRE(initial.armor == ArmorState::armored);
    ARPG_REQUIRE(initial.max_break == bulwark->max_break);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().monsters[0].position.x < initial.position.x);
    return {};
}

arpg::test::Failure bulwark_back_hit_bypasses_front_armor() noexcept {
    CombatWorld world{encounter_for(
        MonsterId::water_bulwark, 1.20F, 0.0F, 2.1F)};
    bool reached_active = false;
    for (int tick = 0; tick < 120; ++tick) {
        world.tick(MovementInput{});
        if (world.snapshot().monsters[0].ai_phase == MonsterAiPhase::active) {
            reached_active = true;
            break;
        }
    }
    ARPG_REQUIRE(reached_active);

    // Cross to the monster's rear while active/recovery holds its facing.
    tick_n(world, 4, MovementInput{1, 0});
    tick_n(world, 13, MovementInput{1, -1});
    world.tick(MovementInput{-1, 0});
    const auto before = world.snapshot().monsters[0];
    ARPG_REQUIRE(before.armor == ArmorState::armored);
    const int before_break = before.break_value;
    ARPG_REQUIRE(world.snapshot().player.position.x > before.position.x);
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    tick_n(world, 5);
    const auto after = world.snapshot().monsters[0];
    ARPG_REQUIRE(after.hp < before.hp);
    ARPG_REQUIRE(after.reaction == ReactionState::hitstun);
    ARPG_REQUIRE(after.armor == ArmorState::armored);
    ARPG_REQUIRE(after.break_value == before_break);
    return {};
}

arpg::test::Failure bulwark_accepts_normal_reaction_after_break() noexcept {
    CombatEncounterConfig config = encounter_for(
        MonsterId::water_bulwark, 0.90F);
    CombatWorld world{config};
    ARPG_REQUIRE(world.queue_action(Action::launcher));
    world.tick(MovementInput{});
    tick_n(world, 7);
    const auto before = world.snapshot().monsters[0];
    ARPG_REQUIRE(before.armor == ArmorState::armored);
    ARPG_REQUIRE(before.reaction != ReactionState::hitstun);
    for (int attack = 0; attack < 8; ++attack) {
        ARPG_REQUIRE(world.queue_action(Action::launcher));
        world.tick(MovementInput{});
        tick_n(world, 7);
        ARPG_REQUIRE(arpg::test::finish_attack(world, 100));
        for (int wait = 0; wait < 140; ++wait) {
            const auto target = world.snapshot().monsters[0];
            if (target.position.z == 0.0F
                && target.reaction != ReactionState::airborne
                && target.reaction != ReactionState::knockdown
                && target.reaction != ReactionState::rising) {
                break;
            }
            world.tick(MovementInput{});
        }
        if (world.snapshot().monsters[0].armor == ArmorState::broken) {
            break;
        }
    }
    ARPG_REQUIRE(world.snapshot().monsters[0].armor == ArmorState::broken);
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    tick_n(world, 5);
    ARPG_REQUIRE(world.snapshot().monsters[0].reaction == ReactionState::hitstun);
    return {};
}

arpg::test::Failure launcher_gives_stage4_monster_minimum_airtime() noexcept {
    CombatWorld world{encounter_for(MonsterId::chaos_chaser, 1.20F)};
    ARPG_REQUIRE(world.queue_action(Action::launcher));
    world.tick(MovementInput{});
    tick_n(world, 7);
    auto target = world.snapshot().monsters[0];
    ARPG_REQUIRE(target.reaction == ReactionState::airborne);
    tick_n(world, 60);
    target = world.snapshot().monsters[0];
    ARPG_REQUIRE(target.reaction == ReactionState::airborne);
    ARPG_REQUIRE(target.position.z > 0.0F);
    return {};
}

arpg::test::Failure swift_pursuit_composes_move_and_cooldown() noexcept {
    CombatEncounterConfig normal_config = encounter_for(
        MonsterId::chaos_chaser, 3.0F);
    CombatEncounterConfig swift_config = normal_config;
    swift_config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::swift_pursuit);
    CombatWorld normal{normal_config};
    CombatWorld swift{swift_config};
    const float normal_start = normal.snapshot().monsters[0].position.x;
    const float swift_start = swift.snapshot().monsters[0].position.x;
    normal.tick({});
    swift.tick({});
    ARPG_REQUIRE(arpg::test::near(
        swift_start - swift.snapshot().monsters[0].position.x,
        (normal_start - normal.snapshot().monsters[0].position.x) * 1.15F,
        1.0e-5));

    CombatEncounterConfig timing = encounter_for(
        MonsterId::chaos_chaser, 0.90F);
    timing.abyss = swift_config.abyss;
    ARPG_REQUIRE(has_abyss_phase_timing(timing, 12U, 4U, 18U, 36U));

    timing.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::swift, MonsterAffixTier::m3);
    ARPG_REQUIRE(has_abyss_phase_timing(timing, 12U, 4U, 18U, 28U));
    return {};
}

arpg::test::Failure abyss_bulwark_adds_to_stage9_profile() noexcept {
    CombatEncounterConfig config = encounter_for(
        MonsterId::chaos_chaser, 2.0F);
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::abyss_bulwark);
    config.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::armored, MonsterAffixTier::m3);
    CombatWorld armored{config};
    const MonsterAffixProfile armored_profile =
        arpg::test::CombatWorldTestAccess::monster_affix_profile(armored, 0U);
    ARPG_REQUIRE(armored_profile.armor_rating == 1007);
    ARPG_REQUIRE(armored.snapshot().monsters[0].max_shield == 126);
    ARPG_REQUIRE(armored.snapshot().monsters[0].shield == 36);

    config.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::mighty, MonsterAffixTier::m3);
    CombatWorld mighty{config};
    ARPG_REQUIRE(mighty.snapshot().monsters[0].max_hp == 240);
    ARPG_REQUIRE(mighty.snapshot().monsters[0].max_shield == 162);
    ARPG_REQUIRE(mighty.snapshot().monsters[0].shield == 72);

    config.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::shielding, MonsterAffixTier::m2);
    CombatWorld shielding{config};
    ARPG_REQUIRE(shielding.snapshot().monsters[0].max_shield == 78);
    ARPG_REQUIRE(shielding.snapshot().monsters[0].shield == 36);

    config.wave.spawns[0].affixes = {};
    CombatWorld zero_armor{config};
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_affix_profile(
        zero_armor, 0U).armor_rating == 0);
    return {};
}

arpg::test::Failure abyss_fury_scales_contact_once() noexcept {
    CombatEncounterConfig melee = encounter_for(
        MonsterId::chaos_chaser, 0.0F);
    melee.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::abyss_fury);
    CombatWorld contact{melee};
    arpg::test::CombatWorldTestAccess::arm_monster_active_attack(contact, 0U);
    const int contact_before = contact.snapshot().player.hp;
    arpg::test::CombatWorldTestAccess::simulate_monster(contact, 0U);
    ARPG_REQUIRE(contact_before - contact.snapshot().player.hp == 20);
    return {};
}

arpg::test::Failure abyss_fury_scales_projectile_once() noexcept {
    CombatEncounterConfig projectile = encounter_for(
        MonsterId::lightning_shooter, 4.0F);
    projectile.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::abyss_fury);
    CombatWorld shooter{projectile};
    arpg::test::CombatWorldTestAccess::arm_monster_active_attack(shooter, 0U);
    arpg::test::CombatWorldTestAccess::simulate_monster(shooter, 0U);
    ARPG_REQUIRE(shooter.snapshot().projectile_count == 1U);
    ARPG_REQUIRE(shooter.snapshot().projectiles[0].damage.amount[
        arpg::modifiers::damage_index(
            arpg::modifiers::DamageType::lightning)] == 12);
    return {};
}

arpg::test::Failure abyss_fury_scales_hazard_once() noexcept {
    CombatEncounterConfig hazard = encounter_for(
        MonsterId::chaos_hazard, 0.0F);
    hazard.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::abyss_fury);
    CombatWorld hazard_world{hazard};
    hazard_world.tick({});
    ARPG_REQUIRE(hazard_world.snapshot().hazard_count == 1U);
    ARPG_REQUIRE(hazard_world.snapshot().hazards[0].damage.amount[
        arpg::modifiers::damage_index(
            arpg::modifiers::DamageType::chaos)] == 15);
    return {};
}

arpg::test::Failure abyss_fury_scales_death_blast_once() noexcept {
    CombatEncounterConfig death = encounter_for(
        MonsterId::chaos_chaser, 2.0F);
    death.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::abyss_fury);
    death.wave.spawns[0].position = Vec3{2.0F, 0.0F, 0.0F};
    death.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::death_blast, MonsterAffixTier::m1);
    CombatWorld death_world{death};
    arpg::test::CombatWorldTestAccess::defeat_monster(death_world, 0U, true);
    ARPG_REQUIRE(death_world.snapshot().hazard_count == 1U);
    ARPG_REQUIRE(death_world.snapshot().hazards[0].damage.amount[
        arpg::modifiers::damage_index(
            arpg::modifiers::DamageType::physical)] == 174);
    return {};
}

arpg::test::Failure abyss_fury_composes_with_frenzy_once() noexcept {
    CombatEncounterConfig frenzy = encounter_for(
        MonsterId::chaos_chaser, 0.0F);
    frenzy.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::abyss_fury);
    frenzy.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::frenzy, MonsterAffixTier::m3);
    CombatWorld composed_damage{frenzy};
    arpg::test::CombatWorldTestAccess::arm_monster_active_attack(
        composed_damage, 0U);
    const int composed_before = composed_damage.snapshot().player.hp;
    arpg::test::CombatWorldTestAccess::simulate_monster(composed_damage, 0U);
    ARPG_REQUIRE(composed_before - composed_damage.snapshot().player.hp == 30);
    ARPG_REQUIRE(has_abyss_phase_timing(frenzy, 7U, 4U, 9U, 21U));
    return {};
}

arpg::test::Failure abyss_fury_scales_final_chilling_contact_and_projectile() noexcept {
    CombatEncounterConfig contact = encounter_for(
        MonsterId::chaos_chaser, 0.0F);
    contact.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::abyss_fury);
    contact.wave.spawns[0].affixes = two_affixes(
        MonsterAffixId::chilling, MonsterAffixTier::m1,
        MonsterAffixId::blink_assault, MonsterAffixTier::m1);
    CombatWorld contact_world{contact};
    arpg::test::CombatWorldTestAccess::set_blink_empowered(
        contact_world, 0U, true);
    arpg::test::CombatWorldTestAccess::arm_monster_active_attack(
        contact_world, 0U);
    const int contact_before = contact_world.snapshot().player.hp;
    arpg::test::CombatWorldTestAccess::simulate_monster(contact_world, 0U);
    ARPG_REQUIRE(contact_before - contact_world.snapshot().player.hp == 27);

    CombatEncounterConfig projectile = encounter_for(
        MonsterId::lightning_shooter, 0.0F);
    projectile.abyss = contact.abyss;
    projectile.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::chilling, MonsterAffixTier::m1);
    CombatWorld projectile_world{projectile};
    arpg::test::CombatWorldTestAccess::arm_monster_active_attack(
        projectile_world, 0U);
    arpg::test::CombatWorldTestAccess::simulate_monster(projectile_world, 0U);
    ARPG_REQUIRE(projectile_world.snapshot().projectile_count == 1U);
    ARPG_REQUIRE(projectile_world.snapshot().projectiles[0].damage.amount[
        arpg::modifiers::damage_index(
            arpg::modifiers::DamageType::lightning)] == 12);
    const int projectile_before = projectile_world.snapshot().player.hp;
    projectile_world.tick({});
    ARPG_REQUIRE(projectile_before - projectile_world.snapshot().player.hp == 19);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"chaser move and telegraph stop", &chaos_chaser_moves_then_stops_for_telegraph},
    {"chaser active serial cooldown", &chaos_chaser_damages_only_once_per_active_serial},
    {"bulwark slow armored profile", &water_bulwark_is_slow_and_has_front_armor},
    {"bulwark rear bypasses armor", &bulwark_back_hit_bypasses_front_armor},
    {"bulwark break reaction", &bulwark_accepts_normal_reaction_after_break},
    {"launcher minimum airtime", &launcher_gives_stage4_monster_minimum_airtime},
    {"swift pursuit composes move and cooldown",
     &swift_pursuit_composes_move_and_cooldown},
    {"abyss bulwark composes profile", &abyss_bulwark_adds_to_stage9_profile},
    {"abyss fury scales contact once", &abyss_fury_scales_contact_once},
    {"abyss fury scales projectile once", &abyss_fury_scales_projectile_once},
    {"abyss fury scales hazard once", &abyss_fury_scales_hazard_once},
    {"abyss fury scales death blast once", &abyss_fury_scales_death_blast_once},
    {"abyss fury composes frenzy once", &abyss_fury_composes_with_frenzy_once},
    {"abyss fury scales final chilling damage",
     &abyss_fury_scales_final_chilling_contact_and_projectile},
};

}  // namespace

arpg::test::TestSuite monster_melee_suite() noexcept {
    return arpg::test::make_suite("monster_melee", kCases);
}
