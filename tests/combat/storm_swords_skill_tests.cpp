#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_test_support.hpp"

#include "combat/active_skill_runtime.hpp"
#include "combat/combat_world.hpp"
#include "skills/active_skill_catalog.hpp"
#include "skills/active_skill_types.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace {

using namespace arpg::combat;
using arpg::skills::ActiveSkillId;
using arpg::test::tick_n;

constexpr double kFloatTolerance = 1.0e-4;
constexpr std::uint16_t kStormStrikesTick = 72U;
constexpr std::uint16_t kStormFirstDamageTick = 108U;
constexpr std::uint16_t kStormDamageIntervalTicks = 18U;
constexpr std::uint16_t kStormFinisherTick = 324U;
constexpr std::uint16_t kStormRecoveryTick = 342U;

CombatLabConfig storm_config() noexcept {
    CombatLabConfig config{};
    config.dummy_spawns = {{{12.0F, 4.0F, 0.0F},
                            {12.0F, 0.0F, 0.0F},
                            {12.0F, -4.0F, 0.0F}}};
    return config;
}

template <typename Event, typename = void>
struct HasStormMetadata : std::false_type {};

template <typename Event>
struct HasStormMetadata<Event, std::void_t<
    decltype(std::declval<Event>().skill),
    decltype(std::declval<Event>().strike_index),
    decltype(std::declval<Event>().finisher)>> : std::true_type {};

template <typename Event>
bool carries_storm_metadata(
    const Event& event, std::uint8_t strike_index, bool finisher) noexcept {
    if constexpr (HasStormMetadata<Event>::value) {
        return event.skill == ActiveSkillId::storm_swords
            && event.strike_index == strike_index
            && event.finisher == finisher;
    } else {
        return false;
    }
}

arpg::test::Failure accepts_storm_and_locks_center_ahead_of_cast_facing() noexcept {
    CombatWorld world{storm_config()};
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);

    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.id == ActiveSkillId::storm_swords);
    ARPG_REQUIRE(snapshot.active_skill.phase == ActiveSkillPhase::startup);
    ARPG_REQUIRE(snapshot.active_skill.elapsed_ticks == 0U);
    ARPG_REQUIRE(snapshot.active_skill.strike_index == 0U);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.active_skill.locked_center.x, 3.5, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.active_skill.locked_center.y, 0.0, kFloatTolerance));
    ARPG_REQUIRE(snapshot.skill_cooldowns[1] == 1800U);

    tick_n(world, 8, MovementInput{-1, 1});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.position.x < 0.0F);
    ARPG_REQUIRE(snapshot.player.position.y > 0.0F);
    ARPG_REQUIRE(snapshot.player.facing == Facing::right);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.active_skill.locked_center.x, 3.5, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.active_skill.locked_center.y, 0.0, kFloatTolerance));
    return {};
}

arpg::test::Failure twelve_strikes_follow_timeline_events_then_finish_and_recover() noexcept {
    CombatWorld world{storm_config()};
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);

    tick_n(world, kStormStrikesTick - 1U);
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.phase == ActiveSkillPhase::startup);
    ARPG_REQUIRE(snapshot.active_skill.elapsed_ticks == kStormStrikesTick - 1U);
    ARPG_REQUIRE(snapshot.active_skill.strike_index == 0U);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.phase == ActiveSkillPhase::strikes);
    ARPG_REQUIRE(snapshot.active_skill.elapsed_ticks == kStormStrikesTick);
    tick_n(world, 24U);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.elapsed_ticks == 96U);
    ARPG_REQUIRE(snapshot.active_skill.frame_index == 38U);
    ARPG_REQUIRE(snapshot.active_skill.spawned_sword_count == 15U);
    ARPG_REQUIRE(snapshot.active_skill.player_invulnerable);

    for (std::uint8_t strike = 1U; strike <= 12U; ++strike) {
        const std::uint16_t boundary = static_cast<std::uint16_t>(
            kStormFirstDamageTick
            + static_cast<std::uint16_t>(strike - 1U) * kStormDamageIntervalTicks);
        const std::uint16_t elapsed = snapshot.active_skill.elapsed_ticks;
        tick_n(world, static_cast<int>(boundary - elapsed));
        snapshot = world.snapshot();
        ARPG_REQUIRE(snapshot.active_skill.phase == ActiveSkillPhase::strikes);
        ARPG_REQUIRE(snapshot.active_skill.elapsed_ticks == boundary);
        ARPG_REQUIRE(snapshot.active_skill.strike_index == strike);
        if (strike != 12U) {
            tick_n(world, kStormDamageIntervalTicks - 1U);
            snapshot = world.snapshot();
            ARPG_REQUIRE(snapshot.active_skill.phase
                         == ActiveSkillPhase::strikes);
            ARPG_REQUIRE(snapshot.active_skill.strike_index == strike);
        }
    }

    tick_n(world, kStormFinisherTick - 1U - snapshot.active_skill.elapsed_ticks);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.elapsed_ticks == kStormFinisherTick - 1U);
    ARPG_REQUIRE(snapshot.active_skill.strike_index == 12U);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.phase == ActiveSkillPhase::finisher);
    ARPG_REQUIRE(snapshot.active_skill.elapsed_ticks == kStormFinisherTick);
    ARPG_REQUIRE(snapshot.active_skill.strike_index == 12U);

    tick_n(world, kStormRecoveryTick - kStormFinisherTick - 1U);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.elapsed_ticks == kStormRecoveryTick - 1U);
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.phase == ActiveSkillPhase::recovery);
    ARPG_REQUIRE(!snapshot.active_skill.player_invulnerable);
    ARPG_REQUIRE(snapshot.active_skill.spawned_sword_count == 24U);
    tick_n(world, 18U);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.id == ActiveSkillId::none);
    ARPG_REQUIRE(snapshot.active_skill.phase == ActiveSkillPhase::none);
    return {};
}

arpg::test::Failure each_strike_hits_each_target_once_and_latch_resets_next_strike() noexcept {
    CombatLabConfig config = storm_config();
    config.dummy_spawns = {{{3.5F, 0.0F, 0.0F},
                            {3.0F, 0.5F, 0.0F},
                            {4.0F, -0.5F, 0.0F}}};
    CombatWorld world{config};
    const CombatSnapshot before = world.snapshot();
    arpg::test::drain_events(world);
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);

    tick_n(world, kStormFirstDamageTick);
    CombatSnapshot snapshot = world.snapshot();
    for (std::size_t index = 0U; index < 3U; ++index) {
        ARPG_REQUIRE(before.monsters[index].hp - snapshot.monsters[index].hp == 42);
    }
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::hitstun);

    std::uint8_t first_strike_hits{};
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::hit) {
            ARPG_REQUIRE(event->attack == AttackId::none);
            ARPG_REQUIRE(event->value == 42);
            ARPG_REQUIRE(carries_storm_metadata(*event, 1U, false));
            ++first_strike_hits;
        }
    }
    ARPG_REQUIRE(first_strike_hits == 3U);

    tick_n(world, kStormDamageIntervalTicks - 1U);
    snapshot = world.snapshot();
    for (std::size_t index = 0U; index < 3U; ++index) {
        ARPG_REQUIRE(before.monsters[index].hp - snapshot.monsters[index].hp == 42);
    }
    world.tick(MovementInput{});
    snapshot = world.snapshot();
    for (std::size_t index = 0U; index < 3U; ++index) {
        ARPG_REQUIRE(before.monsters[index].hp - snapshot.monsters[index].hp == 84);
    }
    return {};
}

arpg::test::Failure normal_and_finisher_radii_are_independent_and_finisher_launches() noexcept {
    CombatLabConfig config = storm_config();
    config.dummy_spawns = {{{3.5F, 2.8F, 0.0F},
                            {3.5F, 3.0F, 0.0F},
                            {3.5F, 3.8001F, 0.0F}}};
    CombatWorld world{config};
    const CombatSnapshot before = world.snapshot();
    arpg::test::drain_events(world);
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);

    tick_n(world, kStormFirstDamageTick);
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(before.monsters[0].hp - snapshot.monsters[0].hp == 42);
    ARPG_REQUIRE(before.monsters[1].hp == snapshot.monsters[1].hp);
    ARPG_REQUIRE(before.monsters[2].hp == snapshot.monsters[2].hp);

    arpg::test::drain_events(world);
    tick_n(world, kStormFinisherTick - kStormFirstDamageTick);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.phase == ActiveSkillPhase::finisher);
    ARPG_REQUIRE(before.monsters[1].hp - snapshot.monsters[1].hp == 360);
    ARPG_REQUIRE(before.monsters[2].hp == snapshot.monsters[2].hp);
    ARPG_REQUIRE(snapshot.monsters[1].reaction == ReactionState::airborne);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[1].velocity.x, 0.26, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.monsters[1].velocity.z, 0.24, kFloatTolerance));

    bool saw_finisher = false;
    while (const auto event = world.try_pop_event()) {
        saw_finisher = saw_finisher
            || (event->kind == CombatEventKind::hit
                && event->target_ordinal == 1U && event->value == 360
                && event->attack == AttackId::none
                && carries_storm_metadata(*event, 12U, true));
    }
    ARPG_REQUIRE(saw_finisher);

    CombatLabConfig left_config = storm_config();
    left_config.initial_facing = Facing::left;
    left_config.dummy_spawns = {{{12.0F, 4.0F, 0.0F},
                                 {-3.5F, 3.0F, 0.0F},
                                 {12.0F, -4.0F, 0.0F}}};
    CombatWorld left{left_config};
    ARPG_REQUIRE(left.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);
    tick_n(left, kStormFinisherTick);
    const CombatSnapshot left_snapshot = left.snapshot();
    ARPG_REQUIRE(left_snapshot.monsters[1].reaction
                 == ReactionState::airborne);
    ARPG_REQUIRE(arpg::test::near(
        left_snapshot.monsters[1].velocity.x, -0.26, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        left_snapshot.monsters[1].velocity.z, 0.24, kFloatTolerance));
    return {};
}

arpg::test::Failure reset_cancels_remaining_storm_damage_and_cooldown() noexcept {
    CombatLabConfig config = storm_config();
    config.dummy_spawns[0] = {3.5F, 0.0F, 0.0F};
    CombatWorld world{config};
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);
    tick_n(world, kStormFirstDamageTick);
    ARPG_REQUIRE(world.snapshot().monsters[0].hp == 258);

    world.reset();
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.active_skill.id == ActiveSkillId::none);
    ARPG_REQUIRE(snapshot.skill_cooldowns[1] == 0U);
    ARPG_REQUIRE(snapshot.monsters[0].hp == 300);
    arpg::test::drain_events(world);
    tick_n(world, 150);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].hp == 300);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::hit);
    }
    return {};
}

arpg::test::Failure load_wave_cancels_remaining_storm_without_late_hits() noexcept {
    CombatLabConfig config = storm_config();
    config.dummy_spawns[0] = {3.5F, 0.0F, 0.0F};
    CombatWorld world{config};
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);
    tick_n(world, kStormFirstDamageTick);

    EncounterWave empty{};
    ARPG_REQUIRE(world.load_wave(empty));
    ARPG_REQUIRE(world.snapshot().active_skill.id == ActiveSkillId::none);
    ARPG_REQUIRE(world.snapshot().skill_cooldowns[1] == 0U);
    tick_n(world, 150);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::hit);
    }
    return {};
}

arpg::test::Failure lethal_damage_cancels_remaining_storm_without_late_hits() noexcept {
    CombatLabConfig config = storm_config();
    config.dummy_spawns[0] = {3.5F, 0.0F, 0.0F};
    CombatWorld world{config};
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);
    tick_n(world, kStormStrikesTick);
    const int hp_after_first_strike = world.snapshot().monsters[0].hp;

    arpg::test::CombatWorldTestAccess::apply_damage(
        world, DamagePacket{(std::numeric_limits<int>::max)()},
        DamageDelivery::direct, Vec3{1.0F, 0.0F, 0.0F}, FeedbackLevel::heavy);
    ARPG_REQUIRE(world.player_defeated());
    ARPG_REQUIRE(world.snapshot().active_skill.id == ActiveSkillId::none);
    ARPG_REQUIRE(world.snapshot().skill_cooldowns[1] == 0U);
    tick_n(world, 150);
    ARPG_REQUIRE(world.snapshot().monsters[0].hp == hp_after_first_strike);
    return {};
}

arpg::test::Failure cooldown_counts_1800_world_ticks_without_underflow() noexcept {
    CombatWorld world{storm_config()};
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);
    ARPG_REQUIRE(world.snapshot().skill_cooldowns[1] == 1800U);
    world.tick(MovementInput{});
    ARPG_REQUIRE(world.snapshot().skill_cooldowns[1] == 1799U);
    tick_n(world, 1799);
    ARPG_REQUIRE(world.snapshot().skill_cooldowns[1] == 0U);
    tick_n(world, 50);
    ARPG_REQUIRE(world.snapshot().skill_cooldowns[1] == 0U);
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);
    return {};
}

arpg::test::Failure storm_hot_paths_allocate_nothing() noexcept {
    CombatLabConfig config = storm_config();
    config.dummy_spawns[0] = {3.5F, 0.0F, 0.0F};
    config.dummy_spawns[1] = {3.5F, 3.0F, 0.0F};
    CombatWorld world{config};

    const std::uint64_t request_before = arpg::test::allocation_count();
    ARPG_REQUIRE(world.request_active_skill(ActiveSkillId::storm_swords)
                 == SkillCastResult::accepted);
    ARPG_REQUIRE(arpg::test::allocation_count() == request_before);

    tick_n(world, kStormFirstDamageTick - 1U);
    const std::uint64_t strike_before = arpg::test::allocation_count();
    world.tick(MovementInput{});
    ARPG_REQUIRE(arpg::test::allocation_count() == strike_before);

    tick_n(world, kStormFinisherTick - kStormFirstDamageTick - 1U);
    const std::uint64_t finisher_before = arpg::test::allocation_count();
    world.tick(MovementInput{});
    ARPG_REQUIRE(arpg::test::allocation_count() == finisher_before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"accepts storm and locks center ahead of facing",
     &accepts_storm_and_locks_center_ahead_of_cast_facing},
    {"twelve strikes use timeline events then finisher and recovery",
     &twelve_strikes_follow_timeline_events_then_finish_and_recover},
    {"per-strike latch resets only for next strike",
     &each_strike_hits_each_target_once_and_latch_resets_next_strike},
    {"normal and finisher radii are independent and finisher launches",
     &normal_and_finisher_radii_are_independent_and_finisher_launches},
    {"reset cancels remaining storm damage", &reset_cancels_remaining_storm_damage_and_cooldown},
    {"load wave cancels remaining storm damage", &load_wave_cancels_remaining_storm_without_late_hits},
    {"lethal damage cancels remaining storm damage", &lethal_damage_cancels_remaining_storm_without_late_hits},
    {"cooldown counts 1800 ticks without underflow", &cooldown_counts_1800_world_ticks_without_underflow},
    {"storm hot paths allocate nothing", &storm_hot_paths_allocate_nothing},
};

}  // namespace

arpg::test::TestSuite storm_swords_skill_suite() noexcept {
    return arpg::test::make_suite("storm_swords_skill", kCases);
}
