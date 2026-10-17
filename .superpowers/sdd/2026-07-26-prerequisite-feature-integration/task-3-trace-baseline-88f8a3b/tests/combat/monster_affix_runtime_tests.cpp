#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_affix_runtime.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/monster_pool.hpp"
#include "abyss/abyss_rules.hpp"
#include "modifiers/damage_types.hpp"

#include <array>
#include <cmath>
#include <type_traits>

namespace {

using namespace arpg::combat;

static_assert(std::is_trivially_copyable_v<MonsterAffixProfile>);
static_assert(std::is_trivially_copyable_v<MonsterRuntime>);
static_assert(std::is_trivially_copyable_v<MonsterSnapshot>);

MonsterAffixSet one_affix(
    MonsterAffixId id, MonsterAffixTier tier) noexcept {
    MonsterAffixSet result{};
    result.values[0] = {id, tier};
    result.count = 1U;
    return result;
}

CombatEncounterConfig affixed_encounter(
    MonsterAffixId id,
    MonsterAffixTier tier,
    MonsterId monster = MonsterId::chaos_chaser,
    float monster_x = 1.0F) noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        monster, Vec3{monster_x, 0.0F, 0.0F}, one_affix(id, tier)};
    return config;
}

CombatEncounterConfig normal_encounter(
    MonsterId monster = MonsterId::chaos_chaser,
    float monster_x = 1.0F) noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        monster, Vec3{monster_x, 0.0F, 0.0F}};
    return config;
}

CombatEncounterConfig dual_affixed_encounter(
    MonsterAffixId id,
    MonsterAffixTier first_tier,
    MonsterAffixTier second_tier) noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 2U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{1.0F, 0.0F, 0.0F},
        one_affix(id, first_tier)};
    config.wave.spawns[1] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{2.0F, 0.0F, 0.0F},
        one_affix(id, second_tier)};
    return config;
}

void resolve_player_attack(
    CombatWorld& world, Action action = Action::light) noexcept {
    static_cast<void>(world.queue_action(action));
    arpg::test::tick_n(world, 20);
}

bool hit_monster_once(CombatWorld& world) noexcept {
    const int hp_before = world.snapshot().monsters[0].hp;
    if (!world.queue_action(Action::light)) return false;
    for (int tick = 0; tick < 40; ++tick) {
        world.tick(MovementInput{});
        if (world.snapshot().monsters[0].hp < hp_before) return true;
    }
    return false;
}

bool has_phase_timing(
    MonsterAffixId id,
    MonsterAffixTier tier,
    std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks,
    std::uint16_t recovery_ticks,
    std::uint16_t cooldown_ticks) noexcept {
    CombatWorld world{affixed_encounter(id, tier, MonsterId::chaos_chaser, 0.90F)};
    world.tick(MovementInput{});
    const auto ticks_in_phase = [&world](MonsterAiPhase expected) noexcept {
        int ticks = 0;
        while (ticks < 200
               && world.snapshot().monsters[0].ai_phase == expected) {
            world.tick(MovementInput{});
            ++ticks;
        }
        return ticks;
    };
    if (world.snapshot().monsters[0].ai_phase != MonsterAiPhase::telegraph
        || ticks_in_phase(MonsterAiPhase::telegraph) != telegraph_ticks
        || world.snapshot().monsters[0].ai_phase != MonsterAiPhase::active) {
        return false;
    }
    if (ticks_in_phase(MonsterAiPhase::active) != active_ticks
        || world.snapshot().monsters[0].ai_phase != MonsterAiPhase::recovery) {
        return false;
    }
    if (ticks_in_phase(MonsterAiPhase::recovery) != recovery_ticks
        || world.snapshot().monsters[0].ai_phase != MonsterAiPhase::cooldown) {
        return false;
    }
    return ticks_in_phase(MonsterAiPhase::cooldown) == cooldown_ticks
        && world.snapshot().monsters[0].ai_phase == MonsterAiPhase::move;
}

bool shielding_refills_after_delay(
    MonsterAffixTier tier, int delay) noexcept {
    CombatWorld world{affixed_encounter(MonsterAffixId::shielding, tier)};
    if (!hit_monster_once(world) || world.snapshot().monsters[0].shield != 0) {
        return false;
    }
    arpg::test::tick_n(world, 50);
    if (!hit_monster_once(world)) return false;
    arpg::test::tick_n(world, delay - 1);
    if (world.snapshot().monsters[0].shield != 0) return false;
    world.tick(MovementInput{});
    return world.snapshot().monsters[0].shield
        == world.snapshot().monsters[0].max_shield;
}

bool wait_for_player_hit(CombatWorld& world, int maximum_ticks) noexcept {
    for (int tick = 0; tick < maximum_ticks; ++tick) {
        const PlayerSnapshot before = world.snapshot().player;
        world.tick(MovementInput{});
        const PlayerSnapshot after = world.snapshot().player;
        if (after.hp < before.hp || after.barrier < before.barrier) return true;
    }
    return false;
}

arpg::test::Failure mighty_scales_only_horizontal_launch_impulse() noexcept {
    CombatWorld normal{normal_encounter()};
    CombatWorld strong{affixed_encounter(
        MonsterAffixId::mighty, MonsterAffixTier::m3)};
    ARPG_REQUIRE(normal.queue_action(Action::launcher));
    ARPG_REQUIRE(strong.queue_action(Action::launcher));
    arpg::test::tick_n(normal, 8);
    arpg::test::tick_n(strong, 8);

    const MonsterSnapshot normal_after = normal.snapshot().monsters[0];
    const MonsterSnapshot strong_after = strong.snapshot().monsters[0];
    ARPG_REQUIRE(strong_after.velocity.z == normal_after.velocity.z);
    ARPG_REQUIRE(arpg::test::near(std::fabs(strong_after.velocity.x),
        std::fabs(normal_after.velocity.x) * 0.55F, 1.0e-4));
    return {};
}

arpg::test::Failure frenzy_scales_damage_and_only_non_active_timing() noexcept {
    const MonsterDefinition* const chaser = monster_definition(
        MonsterId::chaos_chaser);
    ARPG_REQUIRE(chaser != nullptr);
    const MonsterAffixProfile m1 = evaluate_monster_affixes(
        *chaser, one_affix(MonsterAffixId::frenzy, MonsterAffixTier::m1));
    const MonsterAffixProfile m2 = evaluate_monster_affixes(
        *chaser, one_affix(MonsterAffixId::frenzy, MonsterAffixTier::m2));
    const MonsterAffixProfile m3 = evaluate_monster_affixes(
        *chaser, one_affix(MonsterAffixId::frenzy, MonsterAffixTier::m3));
    ARPG_REQUIRE(scaled_monster_damage(100, m1) == 115);
    ARPG_REQUIRE(scaled_monster_damage(100, m2) == 130);
    ARPG_REQUIRE(scaled_monster_damage(100, m3) == 150);
    ARPG_REQUIRE(has_phase_timing(
        MonsterAffixId::frenzy, MonsterAffixTier::m1, 11U, 4U, 17U, 38U));
    ARPG_REQUIRE(has_phase_timing(
        MonsterAffixId::frenzy, MonsterAffixTier::m2, 10U, 4U, 15U, 34U));
    ARPG_REQUIRE(has_phase_timing(
        MonsterAffixId::frenzy, MonsterAffixTier::m3, 9U, 4U, 13U, 30U));
    return {};
}

arpg::test::Failure swift_scales_only_move_and_cooldown() noexcept {
    constexpr std::array<MonsterAffixTier, 3> kTiers{{
        MonsterAffixTier::m1, MonsterAffixTier::m2, MonsterAffixTier::m3}};
    constexpr std::array<float, 3> kMoveMultipliers{{1.15F, 1.30F, 1.45F}};
    constexpr std::array<std::uint16_t, 3> kCooldownTicks{{39U, 36U, 32U}};
    for (std::size_t index = 0U; index < kTiers.size(); ++index) {
        CombatWorld normal{normal_encounter(MonsterId::chaos_chaser, 3.0F)};
        CombatWorld swift{affixed_encounter(
            MonsterAffixId::swift, kTiers[index], MonsterId::chaos_chaser,
            3.0F)};
        const float normal_start = normal.snapshot().monsters[0].position.x;
        const float swift_start = swift.snapshot().monsters[0].position.x;
        normal.tick(MovementInput{});
        swift.tick(MovementInput{});
        const MonsterSnapshot normal_after = normal.snapshot().monsters[0];
        const MonsterSnapshot swift_after = swift.snapshot().monsters[0];
        ARPG_REQUIRE(arpg::test::near(
            swift_start - swift_after.position.x,
            (normal_start - normal_after.position.x) * kMoveMultipliers[index],
            1.0e-4));
        ARPG_REQUIRE(normal_after.ai_phase == MonsterAiPhase::move);
        ARPG_REQUIRE(swift_after.ai_phase == MonsterAiPhase::move);
        ARPG_REQUIRE(has_phase_timing(
            MonsterAffixId::swift, kTiers[index], 12U, 4U, 18U,
            kCooldownTicks[index]));
    }
    return {};
}

arpg::test::Failure armored_reduces_only_physical_hp_damage() noexcept {
    CombatEncounterConfig physical_config = normal_encounter();
    physical_config.player_build.weapon_physical = 17;
    CombatEncounterConfig physical_armored_config = physical_config;
    physical_armored_config.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::armored, MonsterAffixTier::m3);
    CombatWorld physical_normal{physical_config};
    CombatWorld physical_armored{physical_armored_config};
    resolve_player_attack(physical_normal);
    resolve_player_attack(physical_armored);
    const MonsterSnapshot normal_after = physical_normal.snapshot().monsters[0];
    const MonsterSnapshot armored_after = physical_armored.snapshot().monsters[0];
    ARPG_REQUIRE(normal_after.max_hp - normal_after.hp == 45);
    ARPG_REQUIRE(armored_after.max_hp - armored_after.hp == 30);
    ARPG_REQUIRE(armored_after.break_value == normal_after.break_value);
    ARPG_REQUIRE(armored_after.hp == normal_after.hp + 15);

    CombatEncounterConfig break_config = normal_encounter(MonsterId::water_bulwark);
    break_config.player_build.weapon_physical = 17;
    CombatEncounterConfig break_armored_config = break_config;
    break_armored_config.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::armored, MonsterAffixTier::m3);
    CombatWorld break_normal{break_config};
    CombatWorld break_armored{break_armored_config};
    resolve_player_attack(break_normal);
    resolve_player_attack(break_armored);
    ARPG_REQUIRE(break_normal.snapshot().monsters[0].break_value == 110);
    ARPG_REQUIRE(break_armored.snapshot().monsters[0].break_value == 110);

    CombatEncounterConfig elemental_config = physical_config;
    elemental_config.player_build.values.flat_damage[
        arpg::modifiers::damage_index(arpg::modifiers::DamageType::fire)] = 100000;
    CombatEncounterConfig elemental_armored_config = elemental_config;
    elemental_armored_config.wave.spawns[0].affixes = one_affix(
        MonsterAffixId::armored, MonsterAffixTier::m3);
    CombatWorld elemental_normal{elemental_config};
    CombatWorld elemental_armored{elemental_armored_config};
    resolve_player_attack(elemental_normal);
    resolve_player_attack(elemental_armored);
    ARPG_REQUIRE(elemental_normal.snapshot().monsters[0].max_hp
                 - elemental_normal.snapshot().monsters[0].hp == 55);
    ARPG_REQUIRE(elemental_armored.snapshot().monsters[0].max_hp
                 - elemental_armored.snapshot().monsters[0].hp == 40);
    ARPG_REQUIRE(elemental_normal.snapshot().monsters[0].break_value
                 == elemental_armored.snapshot().monsters[0].break_value);
    return {};
}

arpg::test::Failure shielding_recharges_once_after_tier_delay_and_hit_resets_timer() noexcept {
    ARPG_REQUIRE(shielding_refills_after_delay(MonsterAffixTier::m1, 180));
    ARPG_REQUIRE(shielding_refills_after_delay(MonsterAffixTier::m2, 150));
    ARPG_REQUIRE(shielding_refills_after_delay(MonsterAffixTier::m3, 120));
    return {};
}

arpg::test::Failure frozen_catalog_projects_static_affix_values() noexcept {
    const MonsterDefinition* const bulwark =
        monster_definition(MonsterId::water_bulwark);
    ARPG_REQUIRE(bulwark != nullptr);

    const MonsterAffixProfile mighty = evaluate_monster_affixes(
        *bulwark, one_affix(MonsterAffixId::mighty, MonsterAffixTier::m3));
    ARPG_REQUIRE(mighty.max_hp == 630);
    ARPG_REQUIRE(mighty.horizontal_impulse_bp == 5500);

    const MonsterAffixProfile frenzy = evaluate_monster_affixes(
        *bulwark, one_affix(MonsterAffixId::frenzy, MonsterAffixTier::m3));
    ARPG_REQUIRE(frenzy.damage_bp == 15000);
    ARPG_REQUIRE(frenzy.attack_timing_bp == 7000);

    const MonsterAffixProfile swift = evaluate_monster_affixes(
        *bulwark, one_affix(MonsterAffixId::swift, MonsterAffixTier::m3));
    ARPG_REQUIRE(swift.move_bp == 14500);
    ARPG_REQUIRE(swift.cooldown_bp == 7600);

    const MonsterAffixProfile armored = evaluate_monster_affixes(
        *bulwark, one_affix(MonsterAffixId::armored, MonsterAffixTier::m3));
    ARPG_REQUIRE(armored.armor_rating == 775);

    const MonsterAffixProfile shielding = evaluate_monster_affixes(
        *bulwark, one_affix(MonsterAffixId::shielding, MonsterAffixTier::m2));
    ARPG_REQUIRE(shielding.max_shield == 110);
    ARPG_REQUIRE(shielding.shield_recharge_delay_ticks == 150U);
    return {};
}

arpg::test::Failure spawn_spec_is_copied_to_runtime_and_snapshot() noexcept {
    MonsterSpawnSpec spec{};
    spec.id = MonsterId::water_bulwark;
    spec.position = Vec3{2.0F, 0.0F, 0.0F};
    spec.affixes = one_affix(MonsterAffixId::mighty, MonsterAffixTier::m3);
    spec.spawn_ordinal = 97U;

    MonsterPool pool;
    const auto handle = pool.spawn(spec);
    ARPG_REQUIRE(handle.has_value());
    const MonsterRuntime* const runtime = pool.get(*handle);
    ARPG_REQUIRE(runtime != nullptr);
    ARPG_REQUIRE(runtime->affixes == spec.affixes);
    ARPG_REQUIRE(runtime->spawn_ordinal == 97U);
    ARPG_REQUIRE(runtime->affix_profile.max_hp == 630);
    ARPG_REQUIRE(runtime->affix_warning == MonsterAffixWarning::none);
    ARPG_REQUIRE(runtime->affix_warning_ticks == 0U);

    EncounterWave wave{};
    wave.spawn_count = 1U;
    wave.spawns[0] = spec;
    CombatEncounterConfig config{};
    config.wave = wave;
    CombatWorld world{config};
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monster_count == 1U);
    ARPG_REQUIRE(snapshot.monsters[0].affixes == spec.affixes);
    ARPG_REQUIRE(snapshot.monsters[0].spawn_ordinal == 97U);
    ARPG_REQUIRE(snapshot.monsters[0].affix_warning == MonsterAffixWarning::none);
    ARPG_REQUIRE(snapshot.monsters[0].affix_warning_ticks == 0U);
    ARPG_REQUIRE(snapshot.monsters[0].max_hp == 630);
    return {};
}

arpg::test::Failure chilling_direct_hit_adds_water_damage_and_slows_movement() noexcept {
    CombatWorld world{affixed_encounter(
        MonsterAffixId::chilling, MonsterAffixTier::m3,
        MonsterId::chaos_chaser, 0.90F)};
    const int maximum_hp = world.snapshot().player.max_hp;
    ARPG_REQUIRE(wait_for_player_hit(world, 30));
    ARPG_REQUIRE(world.snapshot().player.hp == maximum_hp - 19);
    ARPG_REQUIRE(world.snapshot().player.slow_bp == 3500);
    ARPG_REQUIRE(world.snapshot().player.slow_ticks == 120U);

    arpg::test::tick_n(world, 30);
    const float start_x = world.snapshot().player.position.x;
    world.tick(MovementInput{1, 0});
    ARPG_REQUIRE(arpg::test::near(
        world.snapshot().player.position.x - start_x, 0.0585F, 1.0e-4F));

    CombatWorld refresh{dual_affixed_encounter(
        MonsterAffixId::chilling, MonsterAffixTier::m1,
        MonsterAffixTier::m3)};
    arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
        refresh, 0U, DamagePacket{45}, Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(refresh.snapshot().player.slow_bp == 1500);
    ARPG_REQUIRE(refresh.snapshot().player.slow_ticks == 60U);
    arpg::test::tick_n(refresh, 30);
    arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
        refresh, 1U, DamagePacket{45}, Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(refresh.snapshot().player.slow_bp == 3500);
    ARPG_REQUIRE(refresh.snapshot().player.slow_ticks == 120U);
    arpg::test::tick_n(refresh, 30);
    arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
        refresh, 0U, DamagePacket{45}, Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(refresh.snapshot().player.slow_bp == 3500);
    ARPG_REQUIRE(refresh.snapshot().player.slow_ticks == 90U);
    return {};
}

arpg::test::Failure corrosion_dot_bypasses_evasion_and_uses_chaos_reduction() noexcept {
    CombatWorld world{affixed_encounter(
        MonsterAffixId::chaos_corrosion, MonsterAffixTier::m3,
        MonsterId::chaos_chaser, 0.90F)};
    const int maximum_hp = world.snapshot().player.max_hp;
    ARPG_REQUIRE(wait_for_player_hit(world, 30));
    ARPG_REQUIRE(world.snapshot().player.hp == maximum_hp - 14);
    ARPG_REQUIRE(world.snapshot().player.corrosion_damage_per_second == 45);
    ARPG_REQUIRE(world.snapshot().player.corrosion_ticks == 240U);

    PlayerCombatBuild protected_build{};
    protected_build.values.evasion = 1000000;
    protected_build.values.damage_reduction[
        arpg::modifiers::element_index(arpg::modifiers::DamageType::chaos)] = 5000;
    world.apply_player_build(protected_build);
    arpg::test::tick_n(world, 60);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum_hp - 37);

    CombatWorld refresh{dual_affixed_encounter(
        MonsterAffixId::chaos_corrosion, MonsterAffixTier::m1,
        MonsterAffixTier::m3)};
    arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
        refresh, 0U, DamagePacket{45}, Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(refresh.snapshot().player.corrosion_damage_per_second == 20);
    ARPG_REQUIRE(refresh.snapshot().player.corrosion_ticks == 120U);
    arpg::test::tick_n(refresh, 30);
    arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
        refresh, 1U, DamagePacket{45}, Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(refresh.snapshot().player.corrosion_damage_per_second == 45);
    ARPG_REQUIRE(refresh.snapshot().player.corrosion_ticks == 240U);
    arpg::test::tick_n(refresh, 30);
    arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
        refresh, 0U, DamagePacket{45}, Vec3{}, FeedbackLevel::light);
    ARPG_REQUIRE(refresh.snapshot().player.corrosion_damage_per_second == 45);
    ARPG_REQUIRE(refresh.snapshot().player.corrosion_ticks == 210U);
    return {};
}

arpg::test::Failure abyss_fury_scales_final_corrosion_dot_once() noexcept {
    constexpr std::array<MonsterAffixTier, 3> tiers{{
        MonsterAffixTier::m1,
        MonsterAffixTier::m2,
        MonsterAffixTier::m3,
    }};
    constexpr std::array<int, 3> expected{{29, 43, 65}};
    constexpr std::array<int, 3> unscaled{{20, 30, 45}};
    for (std::size_t index = 0U; index < tiers.size(); ++index) {
        CombatEncounterConfig config = affixed_encounter(
            MonsterAffixId::chaos_corrosion, tiers[index]);
        CombatWorld normal{config};
        arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
            normal, 0U, DamagePacket{2}, Vec3{}, FeedbackLevel::light);
        ARPG_REQUIRE(normal.snapshot().player.corrosion_damage_per_second
                     == unscaled[index]);

        config.abyss = arpg::abyss::combat_config_for(
            arpg::abyss::AbyssRuleId::abyss_fury);
        CombatWorld world{config};
        arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
            world, 0U, DamagePacket{2}, Vec3{}, FeedbackLevel::light);
        ARPG_REQUIRE(world.snapshot().player.corrosion_damage_per_second
                     == expected[index]);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"mighty horizontal launch only", &mighty_scales_only_horizontal_launch_impulse},
    {"frenzy damage and non-active timing", &frenzy_scales_damage_and_only_non_active_timing},
    {"swift move and cooldown only", &swift_scales_only_move_and_cooldown},
    {"armored physical hp only", &armored_reduces_only_physical_hp_damage},
    {"shielding recharge delay and reset", &shielding_recharges_once_after_tier_delay_and_hit_resets_timer},
    {"frozen static affix projection", &frozen_catalog_projects_static_affix_values},
    {"spawn spec runtime snapshot copy", &spawn_spec_is_copied_to_runtime_and_snapshot},
    {"chilling direct water and slow", &chilling_direct_hit_adds_water_damage_and_slows_movement},
    {"corrosion dot delivery and reduction", &corrosion_dot_bypasses_evasion_and_uses_chaos_reduction},
    {"abyss fury final corrosion dot once",
     &abyss_fury_scales_final_corrosion_dot_once},
};

}  // namespace

arpg::test::TestSuite monster_affix_runtime_suite() noexcept {
    return arpg::test::make_suite("monster_affix_runtime", kCases);
}
