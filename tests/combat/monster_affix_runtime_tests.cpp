#include "test_framework.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_affix_runtime.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/monster_pool.hpp"

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

arpg::test::Failure frozen_catalog_projects_static_affix_values() noexcept {
    const MonsterDefinition* const bulwark =
        monster_definition(MonsterId::water_bulwark);
    ARPG_REQUIRE(bulwark != nullptr);

    const MonsterAffixProfile mighty = evaluate_monster_affixes(
        *bulwark, one_affix(MonsterAffixId::mighty, MonsterAffixTier::m3));
    ARPG_REQUIRE(mighty.max_hp == 1400);
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
    ARPG_REQUIRE(shielding.max_shield == 245);
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
    ARPG_REQUIRE(runtime->affix_profile.max_hp == 1400);
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
    ARPG_REQUIRE(snapshot.monsters[0].max_hp == 1400);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"frozen static affix projection", &frozen_catalog_projects_static_affix_values},
    {"spawn spec runtime snapshot copy", &spawn_spec_is_copied_to_runtime_and_snapshot},
};

}  // namespace

arpg::test::TestSuite monster_affix_runtime_suite() noexcept {
    return arpg::test::make_suite("monster_affix_runtime", kCases);
}
