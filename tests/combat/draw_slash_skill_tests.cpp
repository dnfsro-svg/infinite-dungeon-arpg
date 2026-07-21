#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/active_skill_runtime.hpp"
#include "combat/combat_world.hpp"
#include "skills/active_skill_types.hpp"

namespace {

using namespace arpg::combat;
using arpg::skills::ActiveSkillId;
using arpg::test::tick_n;

constexpr double kFloatTolerance = 1.0e-4;

CombatLabConfig draw_slash_config() noexcept {
    CombatLabConfig config{};
    config.dummy_spawns = {{{2.0F, 0.0F, 0.0F},
                            {3.0F, 0.75F, 0.0F},
                            {4.0F, -1.75F, 0.0F}}};
    return config;
}

arpg::test::Failure accepts_draw_slash_and_locks_facing() noexcept {
    CombatWorld world{draw_slash_config()};

    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::accepted);
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.id == ActiveSkillId::draw_slash);
    ARPG_REQUIRE(snapshot.active_skill.phase == ActiveSkillPhase::startup);
    ARPG_REQUIRE(snapshot.active_skill.elapsed_ticks == 0U);
    ARPG_REQUIRE(snapshot.skill_cooldowns[0] == 240U);

    world.tick(MovementInput{-1, 0});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.facing == Facing::right);
    ARPG_REQUIRE(arpg::test::near(snapshot.player.position.x, -0.09,
                                  kFloatTolerance));
    return {};
}

arpg::test::Failure rejects_invalid_and_conflicting_skill_requests() noexcept {
    CombatWorld world{draw_slash_config()};
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::none)
                 == SkillCastResult::none);
    ARPG_REQUIRE(world.request_active_skill(
                     static_cast<ActiveSkillId>(99U))
                 == SkillCastResult::invalid_skill);
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::accepted);
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::cooling_down);
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::skill_active);
    return {};
}

arpg::test::Failure rejects_skill_when_player_is_unavailable_or_attacking() noexcept {
    CombatWorld basic_attack{draw_slash_config()};
    ARPG_REQUIRE(basic_attack.queue_action(Action::light));
    basic_attack.tick(MovementInput{});
    ARPG_REQUIRE(basic_attack.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::basic_attack_active);

    CombatWorld hurt{draw_slash_config()};
    arpg::test::CombatWorldTestAccess::apply_damage(
        hurt, 1, Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::light);
    ARPG_REQUIRE(hurt.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::player_unavailable);

    CombatWorld defeated{draw_slash_config()};
    arpg::test::CombatWorldTestAccess::set_player_resources(defeated, 0, 0);
    ARPG_REQUIRE(defeated.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::player_unavailable);
    return {};
}

arpg::test::Failure startup_has_no_damage_and_draw_slash_hits_each_front_target_once() noexcept {
    CombatWorld world{draw_slash_config()};
    const CombatSnapshot before = world.snapshot();
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::accepted);

    tick_n(world, kDrawSlashStartupTicks - 1U);
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].hp == before.monsters[0].hp);
    ARPG_REQUIRE(snapshot.monsters[1].hp == before.monsters[1].hp);
    ARPG_REQUIRE(snapshot.monsters[2].hp == before.monsters[2].hp);

    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].hp < before.monsters[0].hp);
    ARPG_REQUIRE(snapshot.monsters[1].hp < before.monsters[1].hp);
    ARPG_REQUIRE(snapshot.monsters[2].hp < before.monsters[2].hp);
    ARPG_REQUIRE(snapshot.monsters[0].velocity.x > 0.0F);
    ARPG_REQUIRE(snapshot.monsters[1].velocity.x > 0.0F);

    const auto after_hit = snapshot;
    tick_n(world, 30);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].hp == after_hit.monsters[0].hp);
    ARPG_REQUIRE(snapshot.monsters[1].hp == after_hit.monsters[1].hp);
    ARPG_REQUIRE(snapshot.monsters[2].hp == after_hit.monsters[2].hp);
    return {};
}

arpg::test::Failure draw_slash_does_not_hit_behind_the_player() noexcept {
    CombatLabConfig config = draw_slash_config();
    config.dummy_spawns[2] = {-1.0F, 0.0F, 0.0F};
    CombatWorld world{config};
    const int hp = world.snapshot().monsters[2].hp;
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::accepted);
    tick_n(world, kDrawSlashStartupTicks);
    ARPG_REQUIRE(world.snapshot().monsters[2].hp == hp);
    return {};
}

arpg::test::Failure basic_actions_remain_buffered_while_skill_is_active() noexcept {
    CombatWorld world{draw_slash_config()};
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::accepted);
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.active_attack == AttackId::none);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 1U);
    return {};
}

arpg::test::Failure cooldown_counts_down_without_underflow_and_resets_on_world_reset() noexcept {
    CombatWorld world{draw_slash_config()};
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::accepted);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().skill_cooldowns[0] == 239U);
    tick_n(world, 500);
    ARPG_REQUIRE(world.snapshot().skill_cooldowns[0] == 0U);
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::accepted);
    world.reset();
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.id == ActiveSkillId::none);
    ARPG_REQUIRE(snapshot.skill_cooldowns[0] == 0U);

    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::draw_slash)
                 == SkillCastResult::accepted);
    EncounterWave empty{};
    ARPG_REQUIRE(world.load_wave(empty));
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.id == ActiveSkillId::none);
    ARPG_REQUIRE(snapshot.skill_cooldowns[0] == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"accepts draw slash and locks facing", &accepts_draw_slash_and_locks_facing},
    {"rejects invalid and conflicting skill requests",
     &rejects_invalid_and_conflicting_skill_requests},
    {"rejects unavailable and attacking player", &rejects_skill_when_player_is_unavailable_or_attacking},
    {"startup delays damage and hits front targets once",
     &startup_has_no_damage_and_draw_slash_hits_each_front_target_once},
    {"does not hit behind player", &draw_slash_does_not_hit_behind_the_player},
    {"basic actions stay buffered during skill", &basic_actions_remain_buffered_while_skill_is_active},
    {"cooldown clamps and resets", &cooldown_counts_down_without_underflow_and_resets_on_world_reset},
};

}  // namespace

arpg::test::TestSuite draw_slash_skill_suite() noexcept {
    return arpg::test::make_suite("draw_slash_skill", kCases);
}
