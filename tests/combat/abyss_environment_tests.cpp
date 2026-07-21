#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_test_support.hpp"

#include "abyss/abyss_rules.hpp"
#include "combat/combat_world.hpp"
#include "modifiers/damage_types.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace {

using namespace arpg::combat;

const HazardSnapshot* find_environment_hazard(
    const CombatSnapshot& state, HazardKind kind) noexcept {
    for (const HazardSnapshot& hazard : state.hazards) {
        if (hazard.active && hazard.source == HazardSource::abyss_environment
            && hazard.kind == kind) {
            return &hazard;
        }
    }
    return nullptr;
}

std::size_t environment_hazard_count(const CombatSnapshot& state) noexcept {
    std::size_t count = 0U;
    for (const HazardSnapshot& hazard : state.hazards) {
        if (hazard.active && hazard.source == HazardSource::abyss_environment) {
            ++count;
        }
    }
    return count;
}

std::size_t drain_player_hits(
    CombatWorld& world,
    std::array<std::uint64_t, 16U>& ticks) noexcept {
    std::size_t count = 0U;
    while (const auto event = world.try_pop_event()) {
        if (event->kind != CombatEventKind::player_hit) continue;
        if (count < ticks.size()) ticks[count] = event->tick;
        ++count;
    }
    return count;
}

std::size_t drain_player_hit_values(
    CombatWorld& world,
    std::array<int, 16U>& values) noexcept {
    std::size_t count = 0U;
    while (const auto event = world.try_pop_event()) {
        if (event->kind != CombatEventKind::player_hit) continue;
        if (count < values.size()) values[count] = event->value;
        ++count;
    }
    return count;
}

PlayerCombatBuild build_for_max_hp(int maximum) noexcept {
    PlayerCombatBuild build{};
    build.values.max_health = static_cast<std::int64_t>(maximum - 1000)
        * arpg::modifiers::kFixedOne;
    return build;
}

CombatEncounterConfig environment_encounter(
    arpg::abyss::AbyssRuleId rule) noexcept {
    CombatEncounterConfig config{};
    config.abyss = arpg::abyss::combat_config_for(rule);
    config.abyss.player_max_health_bp = 1000U;
    return config;
}

arpg::test::Failure chaos_region_exists_when_challenge_begins() noexcept {
    CombatEncounterConfig config{};
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::chaos_expansion);
    config.abyss.player_max_health_bp = 1000U;
    CombatWorld world{config};

    const CombatSnapshot state = world.snapshot();
    ARPG_REQUIRE(state.tick == 0U);
    ARPG_REQUIRE(state.hazard_count == 1U);
    const HazardSnapshot* hazard = find_environment_hazard(
        state, HazardKind::chaos_expansion);
    ARPG_REQUIRE(hazard != nullptr);
    ARPG_REQUIRE(hazard->owner.index == 0xFFFFU);
    ARPG_REQUIRE(hazard->owner.generation == 0U);
    ARPG_REQUIRE(hazard->center.x == 0.0F);
    ARPG_REQUIRE(hazard->center.y == 0.0F);
    ARPG_REQUIRE(hazard->radius == 1.0F);
    ARPG_REQUIRE(hazard->telegraph_ticks == 0U);
    ARPG_REQUIRE(hazard->active_ticks
        == (std::numeric_limits<std::uint16_t>::max)());
    ARPG_REQUIRE(hazard->lifetime_ticks
        == (std::numeric_limits<std::uint16_t>::max)());
    ARPG_REQUIRE(hazard->damage_interval_ticks == 60U);
    ARPG_REQUIRE(hazard->environment_damage_bp == 800U);
    ARPG_REQUIRE(hazard->environment_damage_type
        == arpg::modifiers::DamageType::chaos);
    const std::size_t chaos = arpg::modifiers::damage_index(
        arpg::modifiers::DamageType::chaos);
    for (std::size_t index = 0U; index < hazard->damage.amount.size(); ++index) {
        ARPG_REQUIRE(hazard->damage.amount[index]
            == (index == chaos ? 8 : 0));
    }
    static_assert(std::is_trivially_copyable_v<AbyssEnvironmentRuntime>);
    return {};
}

arpg::test::Failure chaos_damage_tracks_current_actual_max_hp() noexcept {
    CombatEncounterConfig config{};
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::chaos_expansion);
    config.player_build = build_for_max_hp(1001);
    CombatWorld world{config};
    std::array<int, 16U> values{};

    world.tick({});
    ARPG_REQUIRE(drain_player_hit_values(world, values) == 1U);
    ARPG_REQUIRE(values[0] == 81);

    world.apply_player_build(build_for_max_hp(1101));
    const HazardSnapshot* raised = find_environment_hazard(
        world.snapshot(), HazardKind::chaos_expansion);
    ARPG_REQUIRE(raised != nullptr);
    ARPG_REQUIRE(raised->damage.amount[arpg::modifiers::damage_index(
        arpg::modifiers::DamageType::chaos)] == 89);
    arpg::test::tick_n(world, 59);
    world.tick({});
    ARPG_REQUIRE(drain_player_hit_values(world, values) == 1U);
    ARPG_REQUIRE(values[0] == 89);

    world.apply_player_build(build_for_max_hp(1001));
    arpg::test::tick_n(world, 59);
    world.tick({});
    ARPG_REQUIRE(drain_player_hit_values(world, values) == 1U);
    ARPG_REQUIRE(values[0] == 81);
    return {};
}

arpg::test::Failure warning_damage_tracks_activation_max_hp() noexcept {
    CombatEncounterConfig hunting{};
    hunting.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::hunting_flames);
    hunting.player_build = build_for_max_hp(1001);
    CombatWorld hunting_world{hunting};
    arpg::test::tick_n(hunting_world, 240);
    hunting_world.tick({});
    hunting_world.apply_player_build(build_for_max_hp(1101));
    const HazardSnapshot* hunting_warning = find_environment_hazard(
        hunting_world.snapshot(), HazardKind::hunting_flame);
    ARPG_REQUIRE(hunting_warning != nullptr);
    ARPG_REQUIRE(hunting_warning->environment_damage_bp == 1000U);
    ARPG_REQUIRE(hunting_warning->environment_damage_type
        == arpg::modifiers::DamageType::fire);
    ARPG_REQUIRE(hunting_warning->damage.amount[
        arpg::modifiers::damage_index(arpg::modifiers::DamageType::fire)]
        == 111);
    arpg::test::tick_n(hunting_world, 44);
    hunting_world.tick({});
    std::array<int, 16U> values{};
    ARPG_REQUIRE(drain_player_hit_values(hunting_world, values) == 1U);
    ARPG_REQUIRE(values[0] == 111);

    CombatEncounterConfig thunder{};
    thunder.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::thunderstorm);
    thunder.player_build = build_for_max_hp(1101);
    CombatWorld thunder_world{thunder};
    arpg::test::tick_n(thunder_world, 180);
    thunder_world.tick({});
    thunder_world.apply_player_build(build_for_max_hp(1001));
    const HazardSnapshot* thunder_warning = find_environment_hazard(
        thunder_world.snapshot(), HazardKind::thunderstorm);
    ARPG_REQUIRE(thunder_warning != nullptr);
    ARPG_REQUIRE(thunder_warning->environment_damage_bp == 1500U);
    ARPG_REQUIRE(thunder_warning->environment_damage_type
        == arpg::modifiers::DamageType::lightning);
    ARPG_REQUIRE(thunder_warning->damage.amount[
        arpg::modifiers::damage_index(arpg::modifiers::DamageType::lightning)]
        == 151);
    arpg::test::tick_n(thunder_world, 44);
    thunder_world.tick({});
    ARPG_REQUIRE(drain_player_hit_values(thunder_world, values) == 1U);
    ARPG_REQUIRE(values[0] == 151);
    return {};
}

bool environment_point_hits(float radius, Vec3 point) noexcept {
    CombatEncounterConfig config{};
    config.player_spawn = point;
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::chaos_expansion);
    config.abyss.player_max_health_bp = 1000U;
    config.abyss.environment.radius_milliunits[0] =
        static_cast<std::uint16_t>(radius * 1000.0F);
    CombatWorld world{config};
    world.tick({});
    std::array<int, 16U> values{};
    return drain_player_hit_values(world, values) == 1U;
}

arpg::test::Failure environment_uses_ground_plane_circle() noexcept {
    struct RadiusCase final {
        float radius;
        float diagonal;
    };
    constexpr std::array<RadiusCase, 3U> cases{{
        {0.8F, 0.6F}, {1.0F, 0.8F}, {6.2F, 4.4F}}};
    for (const RadiusCase test : cases) {
        ARPG_REQUIRE(environment_point_hits(
            test.radius, Vec3{test.radius - 0.01F, 0.0F, 0.0F}));
        ARPG_REQUIRE(!environment_point_hits(
            test.radius, Vec3{test.radius + 0.01F, 0.0F, 0.0F}));
        ARPG_REQUIRE(!environment_point_hits(
            test.radius, Vec3{test.diagonal, test.diagonal, 0.0F}));
    }
    return {};
}

arpg::test::Failure hazard_pool_rejects_invalid_source_enum() noexcept {
    HazardPool pool{};
    const auto handle = pool.spawn(
        static_cast<HazardSource>(0xFFU), MonsterHandle{},
        HazardKind::native, Vec3{}, 1.0F, 0U, 1U, 1U,
        DamagePacket{1}, false);
    ARPG_REQUIRE(!handle.has_value());
    ARPG_REQUIRE(pool.active_count() == 0U);
    return {};
}

arpg::test::Failure chaos_expands_and_damages_on_locked_ticks() noexcept {
    CombatWorld world{environment_encounter(
        arpg::abyss::AbyssRuleId::chaos_expansion)};
    const int maximum = world.snapshot().player.max_hp;

    world.tick({});
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 8);
    arpg::test::tick_n(world, 59);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 8);
    world.tick({});
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 16);

    arpg::test::tick_n(world, 119);
    const auto* before = find_environment_hazard(
        world.snapshot(), HazardKind::chaos_expansion);
    ARPG_REQUIRE(before != nullptr);
    ARPG_REQUIRE(before->radius == 1.0F);
    world.tick({});
    const auto* first = find_environment_hazard(
        world.snapshot(), HazardKind::chaos_expansion);
    ARPG_REQUIRE(first != nullptr);
    ARPG_REQUIRE(first->radius == 2.3F);

    constexpr std::array<float, 3U> remaining{{3.6F, 4.9F, 6.2F}};
    for (const float radius : remaining) {
        arpg::test::tick_n(world, 179);
        world.tick({});
        const auto* expanded = find_environment_hazard(
            world.snapshot(), HazardKind::chaos_expansion);
        ARPG_REQUIRE(expanded != nullptr);
        ARPG_REQUIRE(expanded->radius == radius);
    }
    arpg::test::tick_n(world, 180);
    const auto* final = find_environment_hazard(
        world.snapshot(), HazardKind::chaos_expansion);
    ARPG_REQUIRE(final != nullptr);
    ARPG_REQUIRE(final->radius == 6.2F);
    return {};
}

arpg::test::Failure thunderstorm_locks_warns_hits_and_ends() noexcept {
    CombatEncounterConfig config = environment_encounter(
        arpg::abyss::AbyssRuleId::thunderstorm);
    config.player_spawn = Vec3{2.0F, -1.0F, 0.0F};
    config.player_build.values.evasion = 1000000;
    config.player_build.values.damage_reduction[
        arpg::modifiers::element_index(
            arpg::modifiers::DamageType::lightning)] = 5000;
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::set_player_evasion_rate_bp(
        world, 10000);
    const int maximum = world.snapshot().player.max_hp;

    arpg::test::tick_n(world, 180);
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 0U);
    world.tick({});
    const auto* warning = find_environment_hazard(
        world.snapshot(), HazardKind::thunderstorm);
    ARPG_REQUIRE(warning != nullptr);
    ARPG_REQUIRE(warning->center.x == 2.0F);
    ARPG_REQUIRE(warning->center.y == -1.0F);
    ARPG_REQUIRE(warning->radius == 0.8F);
    ARPG_REQUIRE(warning->telegraph_ticks == 45U);
    ARPG_REQUIRE(warning->active_ticks == 1U);
    const std::size_t lightning = arpg::modifiers::damage_index(
        arpg::modifiers::DamageType::lightning);
    ARPG_REQUIRE(warning->damage.amount[lightning] == 15);
    for (std::size_t index = 0U; index < warning->damage.amount.size(); ++index) {
        ARPG_REQUIRE(warning->damage.amount[index]
            == (index == lightning ? 15 : 0));
    }

    arpg::test::tick_n(world, 44);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum);
    world.tick({});
    ARPG_REQUIRE(find_environment_hazard(
        world.snapshot(), HazardKind::thunderstorm) == nullptr);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 8);
    std::array<std::uint64_t, 16U> hit_ticks{};
    ARPG_REQUIRE(drain_player_hits(world, hit_ticks) == 1U);
    ARPG_REQUIRE(hit_ticks[0] == 225U);
    ARPG_REQUIRE(world.snapshot().player.corrosion_ticks == 0U);
    return {};
}

arpg::test::Failure hunting_flame_first_hit_and_duration_are_exact() noexcept {
    CombatWorld world{environment_encounter(
        arpg::abyss::AbyssRuleId::hunting_flames)};
    const int maximum = world.snapshot().player.max_hp;
    arpg::test::tick_n(world, 240);
    world.tick({});
    const auto* warning = find_environment_hazard(
        world.snapshot(), HazardKind::hunting_flame);
    ARPG_REQUIRE(warning != nullptr);
    ARPG_REQUIRE(warning->radius == 1.0F);
    ARPG_REQUIRE(warning->telegraph_ticks == 45U);
    ARPG_REQUIRE(warning->active_ticks == 180U);
    ARPG_REQUIRE(warning->damage_interval_ticks == 60U);
    const std::size_t fire = arpg::modifiers::damage_index(
        arpg::modifiers::DamageType::fire);
    ARPG_REQUIRE(warning->damage.amount[fire] == 10);

    arpg::test::tick_n(world, 44);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum);
    world.tick({});
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 10);
    const auto* active = find_environment_hazard(
        world.snapshot(), HazardKind::hunting_flame);
    ARPG_REQUIRE(active != nullptr);
    ARPG_REQUIRE(active->active_ticks == 179U);

    arpg::test::tick_n(world, 179);
    ARPG_REQUIRE(find_environment_hazard(
        world.snapshot(), HazardKind::hunting_flame) == nullptr);
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 30);
    std::array<std::uint64_t, 16U> hit_ticks{};
    ARPG_REQUIRE(drain_player_hits(world, hit_ticks) == 3U);
    ARPG_REQUIRE(hit_ticks[0] == 285U);
    ARPG_REQUIRE(hit_ticks[1] == 345U);
    ARPG_REQUIRE(hit_ticks[2] == 405U);
    return {};
}

arpg::test::Failure hunting_flame_respects_shared_invulnerability() noexcept {
    CombatWorld world{environment_encounter(
        arpg::abyss::AbyssRuleId::hunting_flames)};
    const int maximum = world.snapshot().player.max_hp;
    arpg::test::tick_n(world, 285);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{1}, DamageDelivery::direct, Vec3{},
        FeedbackLevel::light);
    world.tick({});
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 1);
    std::array<std::uint64_t, 16U> hit_ticks{};
    ARPG_REQUIRE(drain_player_hits(world, hit_ticks) == 1U);
    ARPG_REQUIRE(hit_ticks[0] == 285U);

    arpg::test::tick_n(world, 59);
    world.tick({});
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 11);
    ARPG_REQUIRE(drain_player_hits(world, hit_ticks) == 1U);
    ARPG_REQUIRE(hit_ticks[0] == 345U);
    return {};
}

arpg::test::Failure runtime_consumes_environment_config_values() noexcept {
    CombatEncounterConfig config{};
    config.abyss.rule = arpg::abyss::AbyssRuleId::thunderstorm;
    config.abyss.player_max_health_bp = 1000U;
    auto& environment = config.abyss.environment;
    environment.active = true;
    environment.damage_type = arpg::modifiers::DamageType::fire;
    environment.damage_bp = 333U;
    environment.cycle_ticks = 3U;
    environment.warning_ticks = 2U;
    environment.radius_milliunits[0] = 1234U;
    environment.radius_count = 1U;
    CombatWorld world{config};
    const int maximum = world.snapshot().player.max_hp;

    arpg::test::tick_n(world, 3);
    world.tick({});
    const auto* warning = find_environment_hazard(
        world.snapshot(), HazardKind::thunderstorm);
    ARPG_REQUIRE(warning != nullptr);
    ARPG_REQUIRE(warning->radius == 1.234F);
    ARPG_REQUIRE(warning->telegraph_ticks == 2U);
    const std::size_t fire = arpg::modifiers::damage_index(
        arpg::modifiers::DamageType::fire);
    ARPG_REQUIRE(warning->damage.amount[fire] == 4);
    world.tick({});
    world.tick({});
    ARPG_REQUIRE(world.snapshot().player.hp == maximum - 4);
    return {};
}

arpg::test::Failure full_pool_rejects_once_then_next_cycle_spawns() noexcept {
    CombatEncounterConfig config = environment_encounter(
        arpg::abyss::AbyssRuleId::thunderstorm);
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{8.0F, 3.0F, 0.0F}};
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::freeze_monster_ai(world, 0U, 1000U);
    const MonsterSnapshot monster = world.snapshot().monsters[0];
    const MonsterHandle owner{0U, monster.generation};
    arpg::test::CombatWorldTestAccess::fill_hazards(world, owner);
    const CombatSnapshot filled = world.snapshot();
    ARPG_REQUIRE(filled.hazard_count == kHazardCapacity);
    const std::uint16_t first_generation = filled.hazards[0].generation;
    arpg::test::drain_events(world);

    arpg::test::tick_n(world, 180);
    world.tick({});
    const CombatSnapshot rejected = world.snapshot();
    ARPG_REQUIRE(rejected.hazard_count == kHazardCapacity);
    ARPG_REQUIRE(rejected.hazards[0].generation == first_generation);
    ARPG_REQUIRE(rejected.hazards[0].source == HazardSource::monster);
    ARPG_REQUIRE(rejected.diagnostics.hazard_saturation_count == 1U);
    ARPG_REQUIRE(environment_hazard_count(rejected) == 0U);
    std::array<std::uint64_t, 16U> hit_ticks{};
    ARPG_REQUIRE(drain_player_hits(world, hit_ticks) == 0U);

    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::destroy_hazard_at(
        world, 0U));
    arpg::test::tick_n(world, 179);
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 0U);
    world.tick({});
    const CombatSnapshot recovered = world.snapshot();
    ARPG_REQUIRE(recovered.diagnostics.hazard_saturation_count == 1U);
    ARPG_REQUIRE(environment_hazard_count(recovered) == 1U);
    ARPG_REQUIRE(find_environment_hazard(
        recovered, HazardKind::thunderstorm) != nullptr);
    return {};
}

void fill_monster_hazard_world(CombatWorld& world) noexcept {
    arpg::test::CombatWorldTestAccess::freeze_monster_ai(world, 0U, 1000U);
    const MonsterSnapshot monster = world.snapshot().monsters[0];
    arpg::test::CombatWorldTestAccess::fill_hazards(
        world, MonsterHandle{0U, monster.generation});
}

arpg::test::Failure chaos_full_at_zero_retries_at_first_expansion() noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{8.0F, 3.0F, 0.0F}};
    CombatWorld world{config};
    fill_monster_hazard_world(world);
    arpg::test::CombatWorldTestAccess::activate_abyss_environment(
        world, arpg::abyss::combat_config_for(
            arpg::abyss::AbyssRuleId::chaos_expansion));
    ARPG_REQUIRE(world.snapshot().hazard_count == kHazardCapacity);
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 0U);
    ARPG_REQUIRE(world.snapshot().diagnostics.hazard_saturation_count == 1U);

    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::destroy_hazard_at(
        world, 0U));
    arpg::test::tick_n(world, 180);
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 0U);
    world.tick({});
    const CombatSnapshot recovered = world.snapshot();
    ARPG_REQUIRE(recovered.diagnostics.hazard_saturation_count == 1U);
    const HazardSnapshot* chaos = find_environment_hazard(
        recovered, HazardKind::chaos_expansion);
    ARPG_REQUIRE(chaos != nullptr);
    ARPG_REQUIRE(chaos->radius == 2.3F);
    return {};
}

arpg::test::Failure hunting_full_at_240_recovers_at_480() noexcept {
    CombatEncounterConfig config = environment_encounter(
        arpg::abyss::AbyssRuleId::hunting_flames);
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{8.0F, 3.0F, 0.0F}};
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::freeze_monster_ai(world, 0U, 1000U);
    const MonsterSnapshot monster = world.snapshot().monsters[0];
    arpg::test::CombatWorldTestAccess::fill_hazards(
        world, MonsterHandle{0U, monster.generation});

    arpg::test::tick_n(world, 240);
    world.tick({});
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 0U);
    ARPG_REQUIRE(world.snapshot().diagnostics.hazard_saturation_count == 1U);
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::destroy_hazard_at(
        world, 0U));
    arpg::test::tick_n(world, 239);
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 0U);
    world.tick({});
    const CombatSnapshot recovered = world.snapshot();
    ARPG_REQUIRE(recovered.diagnostics.hazard_saturation_count == 1U);
    ARPG_REQUIRE(find_environment_hazard(
        recovered, HazardKind::hunting_flame) != nullptr);
    return {};
}

arpg::test::Failure monster_cleanup_keeps_environment_hazard() noexcept {
    CombatEncounterConfig config = environment_encounter(
        arpg::abyss::AbyssRuleId::chaos_expansion);
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{7.0F, 2.0F, 0.0F}};
    CombatWorld world{config};
    const MonsterSnapshot monster = world.snapshot().monsters[0];
    const MonsterHandle owner{0U, monster.generation};
    ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::spawn_hazard(world, owner));
    ARPG_REQUIRE(world.snapshot().hazard_count == 2U);

    ARPG_REQUIRE(world.destroy_monster(owner));
    const CombatSnapshot after = world.snapshot();
    ARPG_REQUIRE(after.hazard_count == 1U);
    ARPG_REQUIRE(environment_hazard_count(after) == 1U);
    ARPG_REQUIRE(find_environment_hazard(
        after, HazardKind::chaos_expansion) != nullptr);
    return {};
}

arpg::test::Failure wave_reload_keeps_environment_hazard() noexcept {
    CombatEncounterConfig config = environment_encounter(
        arpg::abyss::AbyssRuleId::chaos_expansion);
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{7.0F, 2.0F, 0.0F}};
    CombatWorld world{config};
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 1U);

    const EncounterWave empty_wave{};
    ARPG_REQUIRE(world.load_wave(empty_wave, false));
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 1U);
    ARPG_REQUIRE(find_environment_hazard(
        world.snapshot(), HazardKind::chaos_expansion) != nullptr);
    return {};
}

arpg::test::Failure clear_api_stops_and_removes_environment() noexcept {
    CombatWorld world{environment_encounter(
        arpg::abyss::AbyssRuleId::chaos_expansion)};
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 1U);
    world.clear_abyss_rule_preserving_resources();
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 0U);
    arpg::test::tick_n(world, 600);
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 0U);
    return {};
}

arpg::test::Failure six_hundred_ticks_allocate_nothing() noexcept {
    CombatEncounterConfig config = environment_encounter(
        arpg::abyss::AbyssRuleId::hunting_flames);
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{8.0F, 3.0F, 0.0F}};
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::freeze_monster_ai(world, 0U, 1000U);
    const MonsterSnapshot monster = world.snapshot().monsters[0];
    arpg::test::CombatWorldTestAccess::fill_hazards(
        world, MonsterHandle{0U, monster.generation}, kHazardCapacity - 1U);
    const std::uint64_t before = arpg::test::allocation_count();
    for (int tick = 0; tick < 600; ++tick) {
        world.tick({});
        static_cast<void>(world.snapshot());
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    ARPG_REQUIRE(world.snapshot().hazard_count == kHazardCapacity);
    ARPG_REQUIRE(environment_hazard_count(world.snapshot()) == 1U);
    ARPG_REQUIRE(world.snapshot().diagnostics.hazard_saturation_count == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"chaos region exists at tick zero",
     &chaos_region_exists_when_challenge_begins},
    {"chaos tracks current actual max hp",
     &chaos_damage_tracks_current_actual_max_hp},
    {"warning tracks activation max hp",
     &warning_damage_tracks_activation_max_hp},
    {"environment ground plane circle",
     &environment_uses_ground_plane_circle},
    {"hazard pool rejects invalid source",
     &hazard_pool_rejects_invalid_source_enum},
    {"chaos expands and damages on fixed ticks",
     &chaos_expands_and_damages_on_locked_ticks},
    {"thunderstorm fixed warning hit and end",
     &thunderstorm_locks_warns_hits_and_ends},
    {"hunting flame first hit and duration",
     &hunting_flame_first_hit_and_duration_are_exact},
    {"hunting flame shared invulnerability",
     &hunting_flame_respects_shared_invulnerability},
    {"runtime consumes environment config",
     &runtime_consumes_environment_config_values},
    {"full pool rejects then next cycle spawns",
     &full_pool_rejects_once_then_next_cycle_spawns},
    {"chaos full at zero retries at expansion",
     &chaos_full_at_zero_retries_at_first_expansion},
    {"hunting full at 240 recovers at 480",
     &hunting_full_at_240_recovers_at_480},
    {"monster cleanup keeps environment",
     &monster_cleanup_keeps_environment_hazard},
    {"wave reload keeps environment",
     &wave_reload_keeps_environment_hazard},
    {"clear api removes environment",
     &clear_api_stops_and_removes_environment},
    {"environment six hundred tick zero allocation",
     &six_hundred_ticks_allocate_nothing},
};

}  // namespace

arpg::test::TestSuite abyss_environment_suite() noexcept {
    return arpg::test::make_suite("abyss_environment", kCases);
}
