#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"

namespace {

using namespace arpg::combat;

MonsterAffixSet one_affix(
    MonsterAffixId id, MonsterAffixTier tier) noexcept {
    MonsterAffixSet result{};
    result.values[0] = MonsterAffixInstance{id, tier};
    result.count = 1U;
    return result;
}

CombatEncounterConfig affixed_encounter(
    MonsterId id, MonsterAffixId affix, MonsterAffixTier tier,
    Vec3 position = Vec3{4.5F, 0.0F, 0.0F}) noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        id, position, one_affix(affix, tier)};
    return config;
}

arpg::test::Failure multishot_spawns_four_fanned_projectiles() noexcept {
    CombatWorld world{affixed_encounter(
        MonsterId::lightning_shooter, MonsterAffixId::multishot,
        MonsterAffixTier::m3)};
    for (int tick = 0; tick < 100; ++tick) {
        world.tick(MovementInput{});
        if (world.snapshot().projectile_count != 0U) break;
    }
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.projectile_count == 4U);
    ARPG_REQUIRE(snapshot.projectiles[0].damage.amount[
        arpg::modifiers::damage_index(arpg::modifiers::DamageType::lightning)] == 20);
    ARPG_REQUIRE(snapshot.projectiles[1].damage.amount[
        arpg::modifiers::damage_index(arpg::modifiers::DamageType::lightning)] == 20);
    ARPG_REQUIRE(snapshot.projectiles[0].velocity.y
                 != snapshot.projectiles[1].velocity.y);
    return {};
}

arpg::test::Failure burning_ground_spawns_periodic_hazard() noexcept {
    CombatWorld world{affixed_encounter(
        MonsterId::chaos_chaser, MonsterAffixId::burning_ground,
        MonsterAffixTier::m1, Vec3{0.8F, 0.0F, 0.0F})};
    arpg::test::tick_n(world, 180);
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.hazard_count == 1U);
    ARPG_REQUIRE(snapshot.hazards[0].kind == HazardKind::burning);
    ARPG_REQUIRE(snapshot.hazards[0].radius == 0.75F);
    ARPG_REQUIRE(snapshot.hazards[0].damage_interval_ticks == 30U);
    return {};
}

arpg::test::Failure chain_lightning_warns_after_a_direct_hit() noexcept {
    CombatWorld world{affixed_encounter(
        MonsterId::chaos_chaser, MonsterAffixId::chain_lightning,
        MonsterAffixTier::m1, Vec3{0.65F, 0.0F, 0.0F})};
    for (int tick = 0; tick < 100; ++tick) {
        world.tick(MovementInput{});
        if (world.snapshot().player.hp < world.snapshot().player.max_hp) break;
    }
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.hazard_count == 1U);
    ARPG_REQUIRE(snapshot.hazards[0].kind == HazardKind::chain_lightning);
    ARPG_REQUIRE(snapshot.hazards[0].telegraph_ticks == 42U);
    return {};
}

arpg::test::Failure full_pools_degrade_affix_triggers_once_without_overwrite() noexcept {
    CombatWorld multishot_world{affixed_encounter(
        MonsterId::lightning_shooter, MonsterAffixId::multishot,
        MonsterAffixTier::m3)};
    const auto multishot_initial = multishot_world.snapshot();
    const MonsterHandle multishot_owner{0U, multishot_initial.monsters[0].generation};
    arpg::test::CombatWorldTestAccess::fill_projectiles(
        multishot_world, multishot_owner);
    arpg::test::CombatWorldTestAccess::set_saturation_counts(
        multishot_world, 0U, 0U);
    arpg::test::tick_n(multishot_world, 100);
    const auto multishot_full = multishot_world.snapshot();
    ARPG_REQUIRE(multishot_full.projectile_count == kProjectileCapacity);
    ARPG_REQUIRE(multishot_full.diagnostics.projectile_saturation_count == 1U);

    CombatWorld burning_world{affixed_encounter(
        MonsterId::chaos_chaser, MonsterAffixId::burning_ground,
        MonsterAffixTier::m1)};
    const auto burning_initial = burning_world.snapshot();
    const MonsterHandle burning_owner{0U, burning_initial.monsters[0].generation};
    arpg::test::CombatWorldTestAccess::fill_hazards(burning_world, burning_owner);
    arpg::test::CombatWorldTestAccess::set_saturation_counts(
        burning_world, 0U, 0U);
    arpg::test::tick_n(burning_world, 180);
    const auto burning_full = burning_world.snapshot();
    ARPG_REQUIRE(burning_full.hazard_count == kHazardCapacity);
    ARPG_REQUIRE(burning_full.diagnostics.hazard_saturation_count == 1U);
    ARPG_REQUIRE(burning_full.hazards[0].kind == HazardKind::native);

    CombatWorld chain_world{affixed_encounter(
        MonsterId::chaos_chaser, MonsterAffixId::chain_lightning,
        MonsterAffixTier::m1, Vec3{0.65F, 0.0F, 0.0F})};
    const auto chain_initial = chain_world.snapshot();
    const MonsterHandle chain_owner{0U, chain_initial.monsters[0].generation};
    arpg::test::CombatWorldTestAccess::fill_hazards(chain_world, chain_owner);
    arpg::test::CombatWorldTestAccess::set_saturation_counts(
        chain_world, 0U, 0U);
    arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
        chain_world, 0U, DamagePacket{1}, Vec3{}, FeedbackLevel::light);
    const auto chain_full = chain_world.snapshot();
    ARPG_REQUIRE(chain_full.hazard_count == kHazardCapacity);
    ARPG_REQUIRE(chain_full.diagnostics.hazard_saturation_count == 1U);
    ARPG_REQUIRE(chain_full.monsters[0].affix_warning == MonsterAffixWarning::none);
    return {};
}

arpg::test::Failure blink_assault_warns_then_clamps_and_empowers() noexcept {
    CombatWorld world{affixed_encounter(
        MonsterId::chaos_chaser, MonsterAffixId::blink_assault,
        MonsterAffixTier::m3, Vec3{11.9F, 0.0F, 0.0F})};
    arpg::test::tick_n(world, 240);
    const CombatSnapshot warning = world.snapshot();
    ARPG_REQUIRE(warning.monsters[0].affix_warning == MonsterAffixWarning::blink);
    arpg::test::tick_n(world, 30);
    ARPG_REQUIRE(world.snapshot().monsters[0].position.x <= 12.0F);
    return {};
}

arpg::test::Failure tiered_multishot_and_burning_values_are_frozen() noexcept {
    constexpr MonsterAffixTier kTiers[] = {
        MonsterAffixTier::m1, MonsterAffixTier::m2, MonsterAffixTier::m3};
    constexpr std::size_t kProjectileCounts[] = {2U, 3U, 4U};
    constexpr int kProjectileDamage[] = {30, 24, 20};
    constexpr int kBurnIntervals[] = {180, 150, 120};
    constexpr float kBurnRadii[] = {0.75F, 0.90F, 1.05F};
    constexpr int kBurnDamage[] = {25, 35, 45};
    for (std::size_t index = 0U; index < 3U; ++index) {
        CombatWorld multishot_world{affixed_encounter(
            MonsterId::lightning_shooter, MonsterAffixId::multishot,
            kTiers[index])};
        for (int tick = 0; tick < 100; ++tick) {
            multishot_world.tick(MovementInput{});
            if (multishot_world.snapshot().projectile_count != 0U) break;
        }
        const auto multishot = multishot_world.snapshot();
        ARPG_REQUIRE(multishot.projectile_count == kProjectileCounts[index]);
        ARPG_REQUIRE(multishot.projectiles[0].damage.amount[
            arpg::modifiers::damage_index(
                arpg::modifiers::DamageType::lightning)] == kProjectileDamage[index]);
        ARPG_REQUIRE(multishot.projectiles[0].velocity.y
                     + multishot.projectiles[kProjectileCounts[index] - 1U].velocity.y
                     == 0.0F);

        CombatWorld burning_world{affixed_encounter(
            MonsterId::chaos_chaser, MonsterAffixId::burning_ground,
            kTiers[index])};
        arpg::test::tick_n(burning_world, kBurnIntervals[index]);
        const auto burning = burning_world.snapshot();
        ARPG_REQUIRE(burning.hazard_count == 1U);
        ARPG_REQUIRE(burning.hazards[0].radius == kBurnRadii[index]);
        ARPG_REQUIRE(burning.hazards[0].damage.amount[
            arpg::modifiers::damage_index(
                arpg::modifiers::DamageType::fire)] == kBurnDamage[index]);
    }
    return {};
}

arpg::test::Failure tiered_blink_cooldowns_and_warnings_are_frozen() noexcept {
    constexpr MonsterAffixTier kTiers[] = {
        MonsterAffixTier::m1, MonsterAffixTier::m2, MonsterAffixTier::m3};
    constexpr int kCooldowns[] = {480, 360, 240};
    constexpr int kWarnings[] = {42, 36, 30};
    for (std::size_t index = 0U; index < 3U; ++index) {
        CombatWorld world{affixed_encounter(
            MonsterId::chaos_chaser, MonsterAffixId::blink_assault,
            kTiers[index], Vec3{11.9F, 0.0F, 0.0F})};
        arpg::test::tick_n(world, kCooldowns[index]);
        const auto warning = world.snapshot().monsters[0];
        ARPG_REQUIRE(warning.affix_warning == MonsterAffixWarning::blink);
        ARPG_REQUIRE(warning.affix_warning_ticks == kWarnings[index]);
        arpg::test::tick_n(world, kWarnings[index]);
        const auto blinked = world.snapshot().monsters[0];
        ARPG_REQUIRE(blinked.affix_warning == MonsterAffixWarning::none);
        ARPG_REQUIRE(blinked.blink_empowered);
        ARPG_REQUIRE(blinked.position.x >= -12.0F);
        ARPG_REQUIRE(blinked.position.x <= 12.0F);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"multishot fanned projectiles", &multishot_spawns_four_fanned_projectiles},
    {"burning ground periodic hazard", &burning_ground_spawns_periodic_hazard},
    {"chain lightning direct-hit warning", &chain_lightning_warns_after_a_direct_hit},
    {"blink warning clamp empower", &blink_assault_warns_then_clamps_and_empowers},
    {"tiered multishot and burning values", &tiered_multishot_and_burning_values_are_frozen},
    {"tiered blink cooldowns and warnings", &tiered_blink_cooldowns_and_warnings_are_frozen},
    {"full pools degrade affix triggers", &full_pools_degrade_affix_triggers_once_without_overwrite},
};

}  // namespace

arpg::test::TestSuite monster_affix_trigger_suite() noexcept {
    return arpg::test::make_suite("monster_affix_triggers", kCases);
}
