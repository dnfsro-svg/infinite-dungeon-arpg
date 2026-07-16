#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"

namespace {

using namespace arpg::combat;
using arpg::test::tick_n;

CombatEncounterConfig encounter_for(
    MonsterId id,
    Vec3 monster_position,
    Vec3 player_position = {}) noexcept {
    CombatEncounterConfig config{};
    config.player_spawn = player_position;
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{id, monster_position};
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

arpg::test::Failure bomber_warns_then_explodes_once_and_self_defeats() noexcept {
    CombatWorld world{encounter_for(
        MonsterId::fire_bomber, Vec3{1.20F, 0.0F, 0.0F})};
    const int initial_hp = world.snapshot().player.hp;
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::telegraph));
    tick_n(world, 30);
    ARPG_REQUIRE(world.snapshot().player.hp == initial_hp);
    ARPG_REQUIRE(world.snapshot().monsters[0].ai_phase == MonsterAiPhase::telegraph);

    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::active));
    world.tick(MovementInput{});
    const int after_explosion = world.snapshot().player.hp;
    ARPG_REQUIRE(after_explosion < initial_hp);
    ARPG_REQUIRE(world.snapshot().monsters[0].hp == 0);
    ARPG_REQUIRE(world.snapshot().monsters[0].reaction == ReactionState::defeated);
    bool saw_self_defeat = false;
    while (const auto event = world.try_pop_event()) {
        if (event->kind != CombatEventKind::defeated) continue;
        saw_self_defeat = true;
        ARPG_REQUIRE(event->monster_id == MonsterId::fire_bomber);
        ARPG_REQUIRE(!event->reward_eligible);
    }
    ARPG_REQUIRE(saw_self_defeat);
    tick_n(world, 90);
    ARPG_REQUIRE(world.snapshot().player.hp == after_explosion);
    return {};
}

arpg::test::Failure charger_captures_line_and_clamps_burst_to_room() noexcept {
    CombatWorld world{encounter_for(
        MonsterId::fire_charger, Vec3{11.90F, 0.0F, 0.0F},
        Vec3{-12.0F, 0.0F, 0.0F})};
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::telegraph, 800));
    const auto warning = world.snapshot().monsters[0];
    tick_n(world, 20, MovementInput{1, 1});
    ARPG_REQUIRE(world.snapshot().monsters[0].position.x == warning.position.x);
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::active));
    const float active_start_x = world.snapshot().monsters[0].position.x;
    for (int tick = 0; tick < 18; ++tick) {
        world.tick(MovementInput{1, 1});
        const auto charger = world.snapshot().monsters[0];
        ARPG_REQUIRE(charger.position.x >= -12.0F);
        ARPG_REQUIRE(charger.position.x <= 12.0F);
        ARPG_REQUIRE(charger.position.y >= -5.5F);
        ARPG_REQUIRE(charger.position.y <= 5.5F);
    }
    const auto complete = world.snapshot().monsters[0];
    ARPG_REQUIRE(complete.position.x < active_start_x);
    ARPG_REQUIRE(complete.position.y == 0.0F);
    return {};
}

arpg::test::Failure dasher_has_short_warning_burst_and_recovery_gap() noexcept {
    CombatWorld world{encounter_for(
        MonsterId::lightning_dasher, Vec3{3.0F, 0.0F, 0.0F})};
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::telegraph));
    tick_n(world, 24);
    ARPG_REQUIRE(world.snapshot().monsters[0].ai_phase == MonsterAiPhase::active);
    const float before_burst = world.snapshot().monsters[0].position.x;
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().monsters[0].position.x < before_burst);
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::recovery));
    const float recovery_x = world.snapshot().monsters[0].position.x;
    tick_n(world, 10);
    ARPG_REQUIRE(world.snapshot().monsters[0].ai_phase == MonsterAiPhase::recovery);
    ARPG_REQUIRE(world.snapshot().monsters[0].position.x == recovery_x);
    return {};
}

arpg::test::Failure hazard_warns_then_ticks_in_fixed_area_and_cleans_with_owner() noexcept {
    CombatWorld world{encounter_for(
        MonsterId::chaos_hazard, Vec3{4.0F, 0.0F, 0.0F})};
    const int initial_hp = world.snapshot().player.hp;
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::telegraph));
    ARPG_REQUIRE(world.snapshot().hazard_count == 1U);
    ARPG_REQUIRE(world.snapshot().hazards[0].telegraph_ticks > 0U);
    tick_n(world, 30);
    ARPG_REQUIRE(world.snapshot().player.hp == initial_hp);
    ARPG_REQUIRE(reach_phase(world, MonsterAiPhase::active));
    world.tick(MovementInput{});
    const auto active = world.snapshot();
    ARPG_REQUIRE(active.hazard_count == 1U);
    ARPG_REQUIRE(active.hazards[0].active);
    ARPG_REQUIRE(active.hazards[0].center.x == 0.0F);
    const int after_first_tick = active.player.hp;
    tick_n(world, 10);
    ARPG_REQUIRE(world.snapshot().player.hp == after_first_tick);
    tick_n(world, 20);
    ARPG_REQUIRE(world.snapshot().player.hp < after_first_tick);
    ARPG_REQUIRE(world.destroy_monster(MonsterHandle{
        0U, active.monsters[0].generation}));
    ARPG_REQUIRE(world.snapshot().hazard_count == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"bomber warning explosion self defeat", &bomber_warns_then_explodes_once_and_self_defeats},
    {"charger captured line and clamp", &charger_captures_line_and_clamps_burst_to_room},
    {"dasher warning burst recovery", &dasher_has_short_warning_burst_and_recovery_gap},
    {"hazard warning interval owner cleanup", &hazard_warns_then_ticks_in_fixed_area_and_cleans_with_owner},
};

}  // namespace

arpg::test::TestSuite monster_special_suite() noexcept {
    return arpg::test::make_suite("monster_special", kCases);
}
