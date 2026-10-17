#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"

#include <cmath>

namespace {

using namespace arpg::combat;
using arpg::test::tick_n;

CombatEncounterConfig ranged_encounter(
    MonsterId id,
    float monster_x,
    float player_x = 0.0F) noexcept {
    CombatEncounterConfig config{};
    config.player_spawn = Vec3{player_x, 0.0F, 0.0F};
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{id, Vec3{monster_x, 0.0F, 0.0F}};
    return config;
}

CombatEncounterConfig support_encounter() noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = 3U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::water_support, Vec3{4.0F, 0.0F, 0.0F}};
    config.wave.spawns[1] = MonsterSpawnSpec{
        MonsterId::water_bulwark, Vec3{2.0F, 0.0F, 0.0F}};
    config.wave.spawns[2] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{2.5F, 0.0F, 0.0F}};
    return config;
}

bool reach_phase(
    CombatWorld& world,
    MonsterAiPhase phase,
    int max_ticks = 400) noexcept {
    for (int tick = 0; tick < max_ticks; ++tick) {
        if (world.snapshot().monsters[0].ai_phase == phase) {
            return true;
        }
        world.tick(MovementInput{});
    }
    return world.snapshot().monsters[0].ai_phase == phase;
}

arpg::test::Failure shooter_maintains_range_and_freezes_telegraph() noexcept {
    CombatWorld world{ranged_encounter(MonsterId::lightning_shooter, 2.0F)};
    const float start_x = world.snapshot().monsters[0].position.x;
    world.tick(MovementInput{});
    const auto moving = world.snapshot().monsters[0];
    ARPG_REQUIRE(moving.position.x > start_x);
    ARPG_REQUIRE(moving.ai_phase == MonsterAiPhase::move);
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::telegraph));
    const auto warning = world.snapshot().monsters[0];
    tick_n(world, 1);
    const auto frozen = world.snapshot().monsters[0];
    ARPG_REQUIRE(frozen.position.x == warning.position.x);
    ARPG_REQUIRE(frozen.position.y == warning.position.y);
    return {};
}

arpg::test::Failure shooter_creates_deterministic_projectile() noexcept {
    const auto config = ranged_encounter(MonsterId::lightning_shooter, 4.5F);
    CombatWorld first{config};
    CombatWorld second{config};
    for (int tick = 0; tick < 100; ++tick) {
        first.tick(MovementInput{});
        second.tick(MovementInput{});
        if (first.snapshot().projectile_count != 0U) {
            break;
        }
    }
    const auto first_snapshot = first.snapshot();
    const auto second_snapshot = second.snapshot();
    ARPG_REQUIRE(first_snapshot.projectile_count == 1U);
    ARPG_REQUIRE(second_snapshot.projectile_count == 1U);
    ARPG_REQUIRE(first_snapshot.projectiles[0].active);
    ARPG_REQUIRE(first_snapshot.projectiles[0].owner.index == 0U);
    ARPG_REQUIRE(first_snapshot.projectiles[0].position.x
                 == second_snapshot.projectiles[0].position.x);
    ARPG_REQUIRE(first_snapshot.projectiles[0].velocity.x
                 == second_snapshot.projectiles[0].velocity.x);
    return {};
}

arpg::test::Failure projectile_lifetime_removes_active_slot() noexcept {
    CombatWorld world{ranged_encounter(MonsterId::lightning_shooter, 4.5F)};
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::active));
    tick_n(world, 1);
    ARPG_REQUIRE(world.snapshot().projectile_count == 1U);
    tick_n(world, 125);
    ARPG_REQUIRE(world.snapshot().projectile_count == 0U);
    return {};
}

arpg::test::Failure support_selects_lowest_legal_ally_and_shields() noexcept {
    CombatWorld world{support_encounter()};
    bool shielded = false;
    for (int tick = 0; tick < 500; ++tick) {
        world.tick(MovementInput{});
        const auto snapshot = world.snapshot();
        if (snapshot.monsters[1].shield > 0) {
            shielded = true;
            ARPG_REQUIRE(snapshot.monsters[2].shield == 0);
            break;
        }
    }
    ARPG_REQUIRE(shielded);
    ARPG_REQUIRE(world.snapshot().player.hp == world.snapshot().player.max_hp);
    return {};
}

arpg::test::Failure shield_absorbs_damage_and_refresh_does_not_stack() noexcept {
    CombatWorld world{support_encounter()};
    for (int tick = 0; tick < 500; ++tick) {
        world.tick(MovementInput{});
        if (world.snapshot().monsters[1].shield > 0) {
            break;
        }
    }
    const auto shielded = world.snapshot().monsters[1];
    ARPG_REQUIRE(shielded.shield > 0);
    ARPG_REQUIRE(shielded.shield <= shielded.max_shield);
    tick_n(world, 130);
    const auto refreshed = world.snapshot().monsters[1];
    ARPG_REQUIRE(refreshed.shield <= refreshed.max_shield);
    ARPG_REQUIRE(refreshed.shield_ticks <= refreshed.max_shield_ticks);
    return {};
}

arpg::test::Failure support_without_ally_repositions_and_short_cooldown() noexcept {
    CombatWorld world{ranged_encounter(MonsterId::water_support, 4.0F)};
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::active));
    world.tick(MovementInput{});
    const auto support = world.snapshot().monsters[0];
    ARPG_REQUIRE(support.ai_phase == MonsterAiPhase::cooldown);
    ARPG_REQUIRE(support.ai_phase != MonsterAiPhase::recovery);
    ARPG_REQUIRE(world.snapshot().player.hp == world.snapshot().player.max_hp);
    return {};
}

arpg::test::Failure projectile_saturation_is_reported_without_overwrite() noexcept {
    CombatEncounterConfig config = ranged_encounter(
        MonsterId::lightning_shooter, 4.5F);
    config.wave.spawn_count = static_cast<std::uint8_t>(kMonsterCapacity);
    for (std::size_t index = 0; index < kMonsterCapacity; ++index) {
        config.wave.spawns[index] = MonsterSpawnSpec{
            MonsterId::lightning_shooter, Vec3{4.5F, 0.0F, 0.0F}};
    }
    CombatWorld world{config};
    const auto initial = world.snapshot();
    arpg::test::CombatWorldTestAccess::fill_projectiles(
        world, MonsterHandle{0U, initial.monsters[0].generation});
    const auto snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.projectile_count <= kProjectileCapacity);
    ARPG_REQUIRE(snapshot.diagnostics.projectile_saturation_count > 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"shooter range and telegraph freeze", &shooter_maintains_range_and_freezes_telegraph},
    {"deterministic projectile", &shooter_creates_deterministic_projectile},
    {"projectile lifetime cleanup", &projectile_lifetime_removes_active_slot},
    {"support lowest legal ally", &support_selects_lowest_legal_ally_and_shields},
    {"shield absorbs and refresh clamps", &shield_absorbs_damage_and_refresh_does_not_stack},
    {"support no-target fallback", &support_without_ally_repositions_and_short_cooldown},
    {"projectile saturation diagnostic", &projectile_saturation_is_reported_without_overwrite},
};

}  // namespace

arpg::test::TestSuite monster_ranged_suite() noexcept {
    return arpg::test::make_suite("monster_ranged", kCases);
}
