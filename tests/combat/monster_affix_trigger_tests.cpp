#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace arpg::combat;

MonsterAffixSet one_affix(
    MonsterAffixId id, MonsterAffixTier tier) noexcept {
    MonsterAffixSet result{};
    result.values[0] = MonsterAffixInstance{id, tier};
    result.count = 1U;
    return result;
}

MonsterAffixSet two_affixes(
    MonsterAffixId first_id,
    MonsterAffixTier first_tier,
    MonsterAffixId second_id,
    MonsterAffixTier second_tier) noexcept {
    MonsterAffixSet result{};
    result.values[0] = MonsterAffixInstance{first_id, first_tier};
    result.values[1] = MonsterAffixInstance{second_id, second_tier};
    result.count = 2U;
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

CombatEncounterConfig dual_affixed_encounter(
    MonsterId id,
    MonsterAffixId first_id,
    MonsterAffixTier first_tier,
    MonsterAffixId second_id,
    MonsterAffixTier second_tier,
    Vec3 position = Vec3{4.5F, 0.0F, 0.0F}) noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        id, position, two_affixes(first_id, first_tier, second_id, second_tier)};
    return config;
}

bool same_vec(Vec3 left, Vec3 right) noexcept {
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool same_packet(const DamagePacket& left, const DamagePacket& right) noexcept {
    return left.amount == right.amount;
}

bool same_projectile(
    const ProjectileSnapshot& left, const ProjectileSnapshot& right) noexcept {
    return left.active == right.active && left.generation == right.generation
        && left.owner.index == right.owner.index
        && left.owner.generation == right.owner.generation
        && same_vec(left.position, right.position)
        && same_vec(left.velocity, right.velocity)
        && left.lifetime_ticks == right.lifetime_ticks
        && same_packet(left.damage, right.damage) && left.radius == right.radius
        && left.trigger_chain_on_end == right.trigger_chain_on_end;
}

bool same_hazard(
    const HazardSnapshot& left, const HazardSnapshot& right) noexcept {
    return left.active == right.active && left.generation == right.generation
        && left.owner.index == right.owner.index
        && left.owner.generation == right.owner.generation
        && left.kind == right.kind && same_vec(left.center, right.center)
        && left.radius == right.radius
        && left.telegraph_ticks == right.telegraph_ticks
        && left.active_ticks == right.active_ticks
        && left.lifetime_ticks == right.lifetime_ticks
        && left.damage_interval_ticks == right.damage_interval_ticks
        && left.player_latched == right.player_latched
        && left.persists_after_owner_death == right.persists_after_owner_death
        && same_packet(left.damage, right.damage);
}

template <std::size_t Count>
bool same_projectiles(
    const std::array<ProjectileSnapshot, Count>& left,
    const std::array<ProjectileSnapshot, Count>& right) noexcept {
    for (std::size_t index = 0U; index < Count; ++index) {
        if (!same_projectile(left[index], right[index])) return false;
    }
    return true;
}

template <std::size_t Count>
bool same_hazards(
    const std::array<HazardSnapshot, Count>& left,
    const std::array<HazardSnapshot, Count>& right) noexcept {
    for (std::size_t index = 0U; index < Count; ++index) {
        if (!same_hazard(left[index], right[index])) return false;
    }
    return true;
}

int drain_events_of_kind(CombatWorld& world, CombatEventKind wanted) noexcept {
    int count = 0;
    while (const auto event = world.try_pop_event()) {
        if (event->kind == wanted) ++count;
    }
    return count;
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

arpg::test::Failure chain_projectile_end_paths_warn_once_without_recursion() noexcept {
    struct EndCase final {
        Vec3 position;
        Vec3 velocity;
        std::uint16_t lifetime_ticks;
    };
    constexpr std::array<EndCase, 3> kEndCases{{
        {{0.0F, 0.0F, 0.0F}, {}, 30U},       // hit player
        {{12.1F, 0.0F, 0.0F}, {}, 30U},      // leave room bounds
        {{8.0F, 0.0F, 0.0F}, {}, 1U},        // expire
    }};

    for (const EndCase& end_case : kEndCases) {
        CombatWorld world{affixed_encounter(
            MonsterId::chaos_chaser, MonsterAffixId::chain_lightning,
            MonsterAffixTier::m1, Vec3{8.0F, 0.0F, 0.0F})};
        const MonsterSnapshot owner_snapshot = world.snapshot().monsters[0];
        const MonsterHandle owner{0U, owner_snapshot.generation};
        ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_projectile(
            world, owner, end_case.position, end_case.velocity,
            end_case.lifetime_ticks, DamagePacket{1}, 0.10F, true,
            owner_snapshot.affixes));

        world.tick(MovementInput{});
        const CombatSnapshot warning = world.snapshot();
        ARPG_REQUIRE(warning.projectile_count == 0U);
        ARPG_REQUIRE(warning.hazard_count == 1U);
        ARPG_REQUIRE(warning.hazards[0].kind == HazardKind::chain_lightning);
        ARPG_REQUIRE(warning.hazards[0].telegraph_ticks == 42U);
        ARPG_REQUIRE(drain_events_of_kind(
            world, CombatEventKind::affix_chain_warning) == 1);

        arpg::test::tick_n(world, 60);
        ARPG_REQUIRE(world.snapshot().hazard_count == 0U);
        ARPG_REQUIRE(drain_events_of_kind(
            world, CombatEventKind::affix_chain_warning) == 0);
    }
    return {};
}

arpg::test::Failure burning_ground_deals_fire_every_30_ticks_and_expires_per_tier() noexcept {
    constexpr std::array<MonsterAffixTier, 3> kTiers{{
        MonsterAffixTier::m1, MonsterAffixTier::m2, MonsterAffixTier::m3}};
    constexpr std::array<int, 3> kIntervals{{180, 150, 120}};
    constexpr std::array<int, 3> kDurations{{120, 180, 240}};
    constexpr std::array<int, 3> kDamage{{25, 35, 45}};

    for (std::size_t index = 0U; index < kTiers.size(); ++index) {
        CombatWorld world{affixed_encounter(
            MonsterId::chaos_chaser, MonsterAffixId::burning_ground,
            kTiers[index], Vec3{})};
        arpg::test::CombatWorldTestAccess::freeze_monster_ai(world, 0U, 1000U);
        arpg::test::tick_n(world, kIntervals[index]);

        const CombatSnapshot spawned = world.snapshot();
        ARPG_REQUIRE(spawned.hazard_count == 1U);
        ARPG_REQUIRE(spawned.hazards[0].kind == HazardKind::burning);
        ARPG_REQUIRE(spawned.hazards[0].damage_interval_ticks == 30U);
        ARPG_REQUIRE(spawned.hazards[0].active_ticks
                     == static_cast<std::uint16_t>(kDurations[index] - 1));
        ARPG_REQUIRE(spawned.hazards[0].lifetime_ticks
                     == static_cast<std::uint16_t>(kDurations[index] - 1));
        ARPG_REQUIRE(spawned.player.hp == spawned.player.max_hp - kDamage[index]);

        arpg::test::tick_n(world, 29);
        const int hp_before_second_tick = world.snapshot().player.hp;
        world.tick(MovementInput{});
        ARPG_REQUIRE(world.snapshot().player.hp
                     == hp_before_second_tick - kDamage[index]);

        arpg::test::tick_n(world, kDurations[index] - 1 - 30);
        ARPG_REQUIRE(!world.snapshot().hazards[0].active);
    }
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
    const auto multishot_before = multishot_world.snapshot();
    arpg::test::CombatWorldTestAccess::arm_monster_active_attack(
        multishot_world, 0U);
    arpg::test::CombatWorldTestAccess::simulate_monster(multishot_world, 0U);
    const auto multishot_full = multishot_world.snapshot();
    ARPG_REQUIRE(multishot_full.projectile_count == kProjectileCapacity);
    ARPG_REQUIRE(multishot_full.diagnostics.projectile_saturation_count == 1U);
    ARPG_REQUIRE(same_projectiles(
        multishot_before.projectiles, multishot_full.projectiles));
    ARPG_REQUIRE(drain_events_of_kind(
        multishot_world, CombatEventKind::affix_chain_warning) == 0);

    CombatWorld burning_world{affixed_encounter(
        MonsterId::chaos_chaser, MonsterAffixId::burning_ground,
        MonsterAffixTier::m1)};
    const auto burning_initial = burning_world.snapshot();
    const MonsterHandle burning_owner{0U, burning_initial.monsters[0].generation};
    arpg::test::CombatWorldTestAccess::fill_hazards(burning_world, burning_owner);
    arpg::test::CombatWorldTestAccess::set_saturation_counts(
        burning_world, 0U, 0U);
    arpg::test::CombatWorldTestAccess::set_active_affix_ticks(
        burning_world, 0U, 179U, 0U);
    const auto burning_before = burning_world.snapshot();
    arpg::test::CombatWorldTestAccess::tick_active_affixes(burning_world, 0U);
    const auto burning_full = burning_world.snapshot();
    ARPG_REQUIRE(burning_full.hazard_count == kHazardCapacity);
    ARPG_REQUIRE(burning_full.diagnostics.hazard_saturation_count == 1U);
    ARPG_REQUIRE(same_hazards(burning_before.hazards, burning_full.hazards));
    ARPG_REQUIRE(drain_events_of_kind(
        burning_world, CombatEventKind::affix_chain_warning) == 0);

    CombatWorld chain_world{affixed_encounter(
        MonsterId::chaos_chaser, MonsterAffixId::chain_lightning,
        MonsterAffixTier::m1, Vec3{0.65F, 0.0F, 0.0F})};
    const auto chain_initial = chain_world.snapshot();
    const MonsterHandle chain_owner{0U, chain_initial.monsters[0].generation};
    arpg::test::CombatWorldTestAccess::fill_hazards(chain_world, chain_owner);
    arpg::test::CombatWorldTestAccess::set_saturation_counts(
        chain_world, 0U, 0U);
    const auto chain_before = chain_world.snapshot();
    arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
        chain_world, 0U, DamagePacket{1}, Vec3{}, FeedbackLevel::light);
    const auto chain_full = chain_world.snapshot();
    ARPG_REQUIRE(chain_full.hazard_count == kHazardCapacity);
    ARPG_REQUIRE(chain_full.diagnostics.hazard_saturation_count == 1U);
    ARPG_REQUIRE(same_hazards(chain_before.hazards, chain_full.hazards));
    ARPG_REQUIRE(chain_full.monsters[0].affix_warning == MonsterAffixWarning::none);
    ARPG_REQUIRE(drain_events_of_kind(
        chain_world, CombatEventKind::affix_chain_warning) == 0);
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

arpg::test::Failure blink_warning_pauses_ai_then_real_hit_consumes_frenzied_empowerment() noexcept {
    CombatWorld world{dual_affixed_encounter(
        MonsterId::chaos_chaser, MonsterAffixId::blink_assault,
        MonsterAffixTier::m3, MonsterAffixId::frenzy, MonsterAffixTier::m3,
        Vec3{11.9F, 0.0F, 0.0F})};
    arpg::test::tick_n(world, 240);
    const MonsterSnapshot warning = world.snapshot().monsters[0];
    const int hp_before_warning = world.snapshot().player.hp;
    ARPG_REQUIRE(warning.affix_warning == MonsterAffixWarning::blink);
    ARPG_REQUIRE(warning.affix_warning_ticks == 30U);

    arpg::test::tick_n(world, 29);
    const MonsterSnapshot paused = world.snapshot().monsters[0];
    ARPG_REQUIRE(paused.affix_warning == MonsterAffixWarning::blink);
    ARPG_REQUIRE(paused.affix_warning_ticks == 1U);
    ARPG_REQUIRE(paused.ai_phase == warning.ai_phase);
    ARPG_REQUIRE(same_vec(paused.position, warning.position));
    ARPG_REQUIRE(world.snapshot().player.hp == hp_before_warning);

    world.tick(MovementInput{});
    const MonsterSnapshot empowered = world.snapshot().monsters[0];
    ARPG_REQUIRE(empowered.affix_warning == MonsterAffixWarning::none);
    ARPG_REQUIRE(empowered.blink_empowered);
    ARPG_REQUIRE(world.snapshot().player.hp == hp_before_warning);

    bool actual_hit = false;
    for (int tick = 0; tick < 60; ++tick) {
        world.tick(MovementInput{});
        if (world.snapshot().player.hp < hp_before_warning) {
            actual_hit = true;
            break;
        }
    }
    const CombatSnapshot after_hit = world.snapshot();
    ARPG_REQUIRE(actual_hit);
    ARPG_REQUIRE(after_hit.player.hp == hp_before_warning - 100);
    ARPG_REQUIRE(!after_hit.monsters[0].blink_empowered);
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
    {"chain projectile end paths are once-only", &chain_projectile_end_paths_warn_once_without_recursion},
    {"burning ground damage interval and lifetime", &burning_ground_deals_fire_every_30_ticks_and_expires_per_tier},
    {"blink warning clamp empower", &blink_assault_warns_then_clamps_and_empowers},
    {"blink pause and real frenzied hit", &blink_warning_pauses_ai_then_real_hit_consumes_frenzied_empowerment},
    {"tiered multishot and burning values", &tiered_multishot_and_burning_values_are_frozen},
    {"tiered blink cooldowns and warnings", &tiered_blink_cooldowns_and_warnings_are_frozen},
    {"full pools degrade affix triggers", &full_pools_degrade_affix_triggers_once_without_overwrite},
};

}  // namespace

arpg::test::TestSuite monster_affix_trigger_suite() noexcept {
    return arpg::test::make_suite("monster_affix_triggers", kCases);
}
