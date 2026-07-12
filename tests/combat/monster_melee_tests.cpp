#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_catalog.hpp"

#include <cmath>

namespace {

using namespace arpg::combat;
using arpg::test::tick_n;

CombatEncounterConfig encounter_for(
    MonsterId id,
    float monster_x,
    float player_x = 0.0F) noexcept {
    CombatEncounterConfig config{};
    config.player_spawn = Vec3{player_x, 0.0F, 0.0F};
    config.initial_facing = Facing::right;
    config.wave.spawn_count = 1U;
    config.wave.spawns[0] = MonsterSpawnSpec{
        id, Vec3{monster_x, 0.0F, 0.0F}};
    return config;
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

constexpr arpg::test::TestCase kCases[] = {
    {"chaser move and telegraph stop", &chaos_chaser_moves_then_stops_for_telegraph},
    {"chaser active serial cooldown", &chaos_chaser_damages_only_once_per_active_serial},
    {"bulwark slow armored profile", &water_bulwark_is_slow_and_has_front_armor},
    {"bulwark break reaction", &bulwark_accepts_normal_reaction_after_break},
};

}  // namespace

arpg::test::TestSuite monster_melee_suite() noexcept {
    return arpg::test::make_suite("monster_melee", kCases);
}
