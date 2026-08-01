#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_test_support.hpp"
#include "room_field_test_fixture.hpp"
#include "checkpoint/room_checkpoint_validation.hpp"
#include "combat/attack_catalog.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/room_combat_checkpoint.hpp"
#include "combat/combat_world.hpp"
#include "modifiers/effect_set.hpp"
#include "skills/active_skill_catalog.hpp"

#include <array>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

namespace {

using namespace arpg;

static_assert(std::is_same_v<combat::RoomCombatCheckpoint,
    checkpoint::RoomCombatCheckpoint>);
static_assert(!std::is_copy_constructible_v<checkpoint::RoomCombatCheckpoint>);
static_assert(!std::is_move_constructible_v<checkpoint::RoomCombatCheckpoint>);

#define ARPG_CHECKPOINT_ENUM_VALUE(runtime_type, checkpoint_type, value) \
    static_assert(static_cast<std::int64_t>(runtime_type::value) \
        == static_cast<std::int64_t>(checkpoint_type::value))

ARPG_CHECKPOINT_ENUM_VALUE(combat::AttackId, checkpoint::AttackId, j1);
ARPG_CHECKPOINT_ENUM_VALUE(combat::AttackId, checkpoint::AttackId, j2);
ARPG_CHECKPOINT_ENUM_VALUE(combat::AttackId, checkpoint::AttackId, j3);
ARPG_CHECKPOINT_ENUM_VALUE(combat::AttackId, checkpoint::AttackId, launcher);
ARPG_CHECKPOINT_ENUM_VALUE(combat::AttackId, checkpoint::AttackId, air_j);
ARPG_CHECKPOINT_ENUM_VALUE(combat::AttackId, checkpoint::AttackId, none);
ARPG_CHECKPOINT_ENUM_VALUE(combat::Facing, checkpoint::Facing, left);
ARPG_CHECKPOINT_ENUM_VALUE(combat::Facing, checkpoint::Facing, right);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerState, checkpoint::PlayerState, idle);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerState, checkpoint::PlayerState, move);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerState, checkpoint::PlayerState,
    attack_startup);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerState, checkpoint::PlayerState,
    attack_active);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerState, checkpoint::PlayerState,
    attack_recovery);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerState, checkpoint::PlayerState,
    jump_rise);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerState, checkpoint::PlayerState,
    jump_fall);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerState, checkpoint::PlayerState,
    landing);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterId, checkpoint::MonsterId,
    fire_bomber);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterId, checkpoint::MonsterId,
    fire_charger);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterId, checkpoint::MonsterId,
    water_bulwark);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterId, checkpoint::MonsterId,
    water_support);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterId, checkpoint::MonsterId,
    lightning_shooter);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterId, checkpoint::MonsterId,
    lightning_dasher);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterId, checkpoint::MonsterId,
    chaos_chaser);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterId, checkpoint::MonsterId,
    chaos_hazard);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterId, checkpoint::MonsterId, count);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, monster_attack);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, projectile);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, ground_hazard);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, monster_affix);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, abyss_environment);
ARPG_CHECKPOINT_ENUM_VALUE(combat::PlayerDamageSourceKind,
    checkpoint::PlayerDamageSourceKind, unknown);
ARPG_CHECKPOINT_ENUM_VALUE(modifiers::DamageType, checkpoint::DamageType,
    physical);
ARPG_CHECKPOINT_ENUM_VALUE(modifiers::DamageType, checkpoint::DamageType,
    fire);
ARPG_CHECKPOINT_ENUM_VALUE(modifiers::DamageType, checkpoint::DamageType,
    water);
ARPG_CHECKPOINT_ENUM_VALUE(modifiers::DamageType, checkpoint::DamageType,
    lightning);
ARPG_CHECKPOINT_ENUM_VALUE(modifiers::DamageType, checkpoint::DamageType,
    chaos);
ARPG_CHECKPOINT_ENUM_VALUE(modifiers::DamageType, checkpoint::DamageType,
    count);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, mighty);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, frenzy);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, swift);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, armored);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, shielding);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, multishot);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, burning_ground);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, chilling);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, chain_lightning);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, chaos_corrosion);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, blink_assault);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, death_blast);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixId,
    checkpoint::MonsterAffixId, count);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixTier,
    checkpoint::MonsterAffixTier, m1);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixTier,
    checkpoint::MonsterAffixTier, m2);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixTier,
    checkpoint::MonsterAffixTier, m3);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixTier,
    checkpoint::MonsterAffixTier, count);
ARPG_CHECKPOINT_ENUM_VALUE(combat::DummyKind, checkpoint::DummyKind, light);
ARPG_CHECKPOINT_ENUM_VALUE(combat::DummyKind, checkpoint::DummyKind, normal);
ARPG_CHECKPOINT_ENUM_VALUE(combat::DummyKind, checkpoint::DummyKind, heavy);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ReactionState,
    checkpoint::ReactionState, idle);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ReactionState,
    checkpoint::ReactionState, hitstun);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ReactionState,
    checkpoint::ReactionState, airborne);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ReactionState,
    checkpoint::ReactionState, knockdown);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ReactionState,
    checkpoint::ReactionState, rising);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ReactionState,
    checkpoint::ReactionState, defeated);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ReactionState,
    checkpoint::ReactionState, respawning);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ArmorState, checkpoint::ArmorState, none);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ArmorState, checkpoint::ArmorState,
    armored);
ARPG_CHECKPOINT_ENUM_VALUE(combat::ArmorState, checkpoint::ArmorState, broken);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAiPhase,
    checkpoint::MonsterAiPhase, idle);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAiPhase,
    checkpoint::MonsterAiPhase, move);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAiPhase,
    checkpoint::MonsterAiPhase, telegraph);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAiPhase,
    checkpoint::MonsterAiPhase, active);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAiPhase,
    checkpoint::MonsterAiPhase, recovery);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAiPhase,
    checkpoint::MonsterAiPhase, cooldown);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAiPhase,
    checkpoint::MonsterAiPhase, defeated);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixWarning,
    checkpoint::MonsterAffixWarning, none);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixWarning,
    checkpoint::MonsterAffixWarning, blink);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixWarning,
    checkpoint::MonsterAffixWarning, chain_lightning);
ARPG_CHECKPOINT_ENUM_VALUE(combat::MonsterAffixWarning,
    checkpoint::MonsterAffixWarning, death_blast);
ARPG_CHECKPOINT_ENUM_VALUE(combat::HazardKind, checkpoint::HazardKind, native);
ARPG_CHECKPOINT_ENUM_VALUE(combat::HazardKind, checkpoint::HazardKind,
    burning);
ARPG_CHECKPOINT_ENUM_VALUE(combat::HazardKind, checkpoint::HazardKind,
    chain_lightning);
ARPG_CHECKPOINT_ENUM_VALUE(combat::HazardKind, checkpoint::HazardKind,
    death_blast);
ARPG_CHECKPOINT_ENUM_VALUE(combat::HazardKind, checkpoint::HazardKind,
    thunderstorm);
ARPG_CHECKPOINT_ENUM_VALUE(combat::HazardKind, checkpoint::HazardKind,
    hunting_flame);
ARPG_CHECKPOINT_ENUM_VALUE(combat::HazardKind, checkpoint::HazardKind,
    chaos_expansion);

#undef ARPG_CHECKPOINT_ENUM_VALUE

static_assert(combat::kMonsterOrdinalWordCount
    == checkpoint::kMonsterOrdinalWordCount);
static_assert(combat::kRoomEnvironmentCellCount
    == checkpoint::kRoomEnvironmentCellCount);
static_assert(combat::kFireRoomCrateCapacity
    == checkpoint::kFireRoomCrateCapacity);
static_assert(combat::kPlayerDamageHistoryTicks
    == checkpoint::kPlayerDamageHistoryTicks);
static_assert(modifiers::kDamageTypeCount == checkpoint::kDamageTypeCount);
static_assert(modifiers::kElementCount == checkpoint::kElementCount);

struct ExpectedAffixTiming final {
    std::uint16_t interval_ticks{};
    std::uint16_t duration_ticks{};
};

constexpr std::array<ExpectedAffixTiming, 36U> kExpectedAffixTimings{{
    {0U, 0U}, {0U, 0U}, {0U, 0U},
    {0U, 0U}, {0U, 0U}, {0U, 0U},
    {0U, 0U}, {0U, 0U}, {0U, 0U},
    {0U, 0U}, {0U, 0U}, {0U, 0U},
    {0U, 180U}, {0U, 150U}, {0U, 120U},
    {0U, 0U}, {0U, 0U}, {0U, 0U},
    {180U, 120U}, {150U, 180U}, {120U, 240U},
    {0U, 60U}, {0U, 90U}, {0U, 120U},
    {42U, 0U}, {42U, 0U}, {42U, 0U},
    {0U, 120U}, {0U, 180U}, {0U, 240U},
    {480U, 42U}, {360U, 36U}, {240U, 30U},
    {66U, 0U}, {54U, 0U}, {45U, 0U},
}};
static_assert(kExpectedAffixTimings.size()
    == static_cast<std::size_t>(checkpoint::MonsterAffixId::count)
        * static_cast<std::size_t>(checkpoint::MonsterAffixTier::count));

test::Failure runtime_catalogs_match_checkpoint_schema_exhaustively() noexcept {
    constexpr std::array<combat::AttackId, combat::kAttackCount> attacks{{
        combat::AttackId::j1,
        combat::AttackId::j2,
        combat::AttackId::j3,
        combat::AttackId::launcher,
        combat::AttackId::air_j,
    }};
    static_assert(attacks.size() == checkpoint::kAttackActiveTicks.size());
    for (std::size_t index = 0U; index < attacks.size(); ++index) {
        const combat::AttackDefinition* definition =
            combat::find_attack_definition(attacks[index]);
        ARPG_REQUIRE(definition != nullptr);
        ARPG_REQUIRE(definition->id == attacks[index]);
        ARPG_REQUIRE(definition->active_ticks
            == checkpoint::kAttackActiveTicks[index]);
    }

    const combat::MonsterAffixCatalog& catalog =
        combat::monster_affix_catalog();
    ARPG_REQUIRE(catalog.size()
        == static_cast<std::size_t>(checkpoint::MonsterAffixId::count));
    for (std::size_t affix = 0U; affix < catalog.size(); ++affix) {
        const auto runtime_id = static_cast<combat::MonsterAffixId>(affix);
        const combat::MonsterAffixDefinition* definition =
            combat::monster_affix_definition(runtime_id);
        ARPG_REQUIRE(definition != nullptr);
        ARPG_REQUIRE(definition == &catalog[affix]);
        ARPG_REQUIRE(static_cast<std::size_t>(definition->id) == affix);
        for (std::size_t tier = 0U; tier < definition->tiers.size(); ++tier) {
            const combat::MonsterAffixTierValues& values =
                definition->tiers[tier];
            const ExpectedAffixTiming& expected = kExpectedAffixTimings[
                affix * definition->tiers.size() + tier];
            ARPG_REQUIRE(values.interval_ticks == expected.interval_ticks);
            ARPG_REQUIRE(values.duration_ticks == expected.duration_ticks);
            if (runtime_id == combat::MonsterAffixId::burning_ground) {
                ARPG_REQUIRE(values.interval_ticks
                    == checkpoint::kBurningGroundTimings[tier].interval_ticks);
                ARPG_REQUIRE(values.duration_ticks
                    == checkpoint::kBurningGroundTimings[tier].duration_ticks);
            }
            if (runtime_id == combat::MonsterAffixId::blink_assault) {
                ARPG_REQUIRE(values.interval_ticks
                    == checkpoint::kBlinkAssaultTimings[tier].interval_ticks);
                ARPG_REQUIRE(values.duration_ticks
                    == checkpoint::kBlinkAssaultTimings[tier].duration_ticks);
            }
        }
    }
    return {};
}

test::Failure compatibility_alias_uses_neutral_checkpoint_path() noexcept {
    combat::CombatWorld world{};
    ARPG_REQUIRE(world.queue_action(combat::Action::light));
    world.tick({});
    ARPG_REQUIRE(world.snapshot().player.active_attack == combat::AttackId::j1);

    std::unique_ptr<checkpoint::RoomCombatCheckpoint> before{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> after{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(before != nullptr && after != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*before));
    ARPG_REQUIRE(before->attack.id == checkpoint::AttackId::j1);
    ARPG_REQUIRE(world.restore_room_checkpoint(*before));
    ARPG_REQUIRE(world.capture_room_checkpoint(*after));
    ARPG_REQUIRE(checkpoint::same_room_combat_checkpoint(*before, *after));

    ARPG_REQUIRE(after->monster_count != 0U);
    checkpoint::MonsterAffixSet& inactive_tail = after->monsters[0U].affixes;
    ARPG_REQUIRE(inactive_tail.count < inactive_tail.values.size());
    inactive_tail.values[inactive_tail.count].id =
        checkpoint::MonsterAffixId::count;
    ARPG_REQUIRE(checkpoint::valid_room_combat_checkpoint_structural(
        *after, static_cast<std::uint32_t>(limits::kRoomMonsterCapacity)));
    ARPG_REQUIRE(!world.restore_room_checkpoint(*after));
    ARPG_REQUIRE(world.capture_room_checkpoint(*after));
    ARPG_REQUIRE(checkpoint::same_room_combat_checkpoint(*before, *after));
    return {};
}

void populate_maximum_obstacle_blueprint(
    combat::RoomEnvironmentBlueprint& blueprint) noexcept {
    blueprint.record_count = static_cast<std::uint16_t>(
        combat::kRoomEnvironmentCellCount);
    blueprint.obstacle_count = blueprint.record_count;
    for (std::size_t cell = 0U;
            cell < combat::kRoomEnvironmentCellCount; ++cell) {
        blueprint.cell_offsets[cell] = static_cast<std::uint16_t>(cell);
        blueprint.cell_counts[cell] = 1U;
        const combat::Vec3 center =
            test::room_field_fixture::cell_center(
                cell % combat::room_spatial::columns,
                cell / combat::room_spatial::columns);
        combat::RoomEnvironmentRecord& record = blueprint.records[cell];
        record.ordinal = static_cast<std::uint16_t>(cell);
        record.home_cell = static_cast<std::uint16_t>(cell);
        record.prop = combat::RoomPropKind::crate;
        record.anchor = center;
        const float obstacle_y = center.y + 2.5F;
        record.obstacle = {
            {{center.x - 0.25F, obstacle_y - 0.25F, 0.0F},
                {center.x + 0.25F, obstacle_y + 0.25F, 2.0F}},
            combat::RoomObstacleKind::breakable,
            20U,
        };
    }
    blueprint.cell_offsets.back() = blueprint.record_count;
}

test::Failure neutral_checkpoint_round_trips_maximum_room_authority() noexcept {
    namespace fixture = test::room_field_fixture;
    std::unique_ptr<combat::RoomMonsterField> field{
        new (std::nothrow) combat::RoomMonsterField{}};
    std::unique_ptr<combat::RoomEnvironmentBlueprint> environment{
        new (std::nothrow) combat::RoomEnvironmentBlueprint{}};
    ARPG_REQUIRE(field != nullptr && environment != nullptr);
    ARPG_REQUIRE(fixture::seal_test_plan(*field,
        static_cast<std::uint16_t>(limits::kRoomMonsterCapacity))
        == combat::RoomMonsterFieldFault::none);
    populate_maximum_obstacle_blueprint(*environment);

    combat::CombatEncounterConfig config{};
    config.player_spawn = fixture::cell_center(10U, 10U);
    config.player_spawn.x += 2.0F;
    config.abyss.rule = abyss::AbyssRuleId::chaos_expansion;
    config.abyss.environment.active = true;
    config.abyss.environment.damage_type = modifiers::DamageType::chaos;
    config.abyss.environment.damage_bp = 100U;
    config.abyss.environment.damage_interval_ticks = 30U;
    config.abyss.environment.expansion_interval_ticks = 1000U;
    config.abyss.environment.radius_count = 3U;
    config.abyss.environment.radius_milliunits =
        {{1000U, 2000U, 3000U, 0U, 0U}};
    combat::CombatWorld world{config, std::move(field),
        combat::room_obstacle_plan_view(*environment)};
    ARPG_REQUIRE(world.fault() == combat::CombatFault::none);
    combat::RoomMonsterField* active_field = world.room_monster_field();
    ARPG_REQUIRE(active_field != nullptr);
    ARPG_REQUIRE(active_field->total_count() == limits::kRoomMonsterCapacity);
    const combat::RoomResidentOrdinals residents =
        active_field->resident_ordinals();
    ARPG_REQUIRE(residents.count
        == combat::room_spatial::maximum_streaming_monsters);
    const combat::MonsterOrdinal effect_owner = residents.ordinals[0U];
    modifiers::EffectDefinition monster_effect{};
    monster_effect.id = 0xC001U;
    monster_effect.duration_ticks = 97;
    ARPG_REQUIRE(active_field->active_effects(effect_owner)->apply(
        monster_effect) == modifiers::ApplyResult::applied);

    combat::RoomObstacleRuntime* obstacles =
        const_cast<combat::RoomObstacleRuntime*>(world.room_obstacles());
    ARPG_REQUIRE(obstacles != nullptr);
    ARPG_REQUIRE(obstacles->obstacle_count()
        == combat::kRoomEnvironmentCellCount);
    modifiers::EffectDefinition obstacle_effect{};
    obstacle_effect.id = 0x0B57U;
    obstacle_effect.duration_ticks = 83;
    ARPG_REQUIRE(obstacles->effects(0U) != nullptr);
    ARPG_REQUIRE(obstacles->effects(0U)->apply(obstacle_effect)
        == modifiers::ApplyResult::applied);
    ARPG_REQUIRE(obstacles->apply_damage(0U, 3U, 41U));

    for (std::size_t slot = 0U; slot < residents.count; ++slot) {
        test::CombatWorldTestAccess::freeze_monster_ai(world, slot, 1000U);
    }
    ARPG_REQUIRE(world.queue_action(combat::Action::light));
    world.tick({});
    ARPG_REQUIRE(world.snapshot().player.active_attack == combat::AttackId::j1);

    std::unique_ptr<checkpoint::RoomCombatCheckpoint> before{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> after{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(before != nullptr && after != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*before));
    ARPG_REQUIRE(before->monster_count == residents.count);
    ARPG_REQUIRE(before->obstacle_count
        == checkpoint::kRoomEnvironmentCellCount);
    ARPG_REQUIRE(before->attack.id == checkpoint::AttackId::j1);
    ARPG_REQUIRE(before->abyss_environment.active);
    ARPG_REQUIRE(before->abyss_environment.rule
        == abyss::AbyssRuleId::chaos_expansion);
    ARPG_REQUIRE(before->player_damage_history.initialized);
    bool found_effect_owner = false;
    for (std::uint16_t index = 0U; index < before->monster_count; ++index) {
        if (before->monsters[index].ordinal != effect_owner) continue;
        found_effect_owner =
            before->monsters[index].effects.effects[0U].occupied;
        break;
    }
    ARPG_REQUIRE(found_effect_owner);
    ARPG_REQUIRE(before->obstacles[0U].effects.effects[0U].occupied);
    ARPG_REQUIRE(before->obstacles[0U].hp == 17U);
    ARPG_REQUIRE(world.restore_room_checkpoint(*before));
    ARPG_REQUIRE(world.capture_room_checkpoint(*after));
    ARPG_REQUIRE(checkpoint::same_room_combat_checkpoint(*before, *after));

    test::CombatWorldTestAccess::set_player_resources(world, 1, 0);
    test::CombatWorldTestAccess::apply_damage(world, 100,
        {1.0F, 0.0F, 0.0F}, combat::FeedbackLevel::heavy);
    ARPG_REQUIRE(world.player_defeated());
    ARPG_REQUIRE(world.capture_room_checkpoint(*before));
    ARPG_REQUIRE(before->has_death_snapshot);
    ARPG_REQUIRE(before->player_damage_history.initialized);
    ARPG_REQUIRE(before->death_snapshot.final_damage != 0U);
    ARPG_REQUIRE(before->death_snapshot.recent_damage[
        static_cast<std::size_t>(checkpoint::DamageType::physical)] != 0U);
    ARPG_REQUIRE(world.restore_room_checkpoint(*before));
    ARPG_REQUIRE(world.capture_room_checkpoint(*after));
    ARPG_REQUIRE(checkpoint::same_room_combat_checkpoint(*before, *after));
    return {};
}

test::Failure effect_checkpoint_preserves_commands_and_diagnostics() noexcept {
    modifiers::EffectSet source{};
    modifiers::EffectDefinition definition{};
    definition.id = 17U;
    definition.duration_ticks = 23;
    definition.refresh_rule = modifiers::RefreshRule::add_stack;
    definition.max_stacks = 2U;
    definition.strength = 42;
    definition.has_modifier = true;
    definition.modifier.id = 91U;
    definition.modifier.stat = modifiers::StatId::shield;
    definition.on_apply = {modifiers::EffectCommandKind::set_shield, 42};
    ARPG_REQUIRE(source.apply(definition) == modifiers::ApplyResult::applied);

    modifiers::EffectSetCheckpoint checkpoint{};
    source.capture_checkpoint(checkpoint);
    modifiers::EffectSet restored{};
    ARPG_REQUIRE(restored.restore_checkpoint(checkpoint));
    ARPG_REQUIRE(restored.same_state(source));
    return {};
}

test::Failure effect_checkpoint_canonicalizes_the_logical_queue() noexcept {
    modifiers::EffectSet source{};
    modifiers::EffectDefinition definition{};
    definition.id = 31U;
    definition.duration_ticks = 50;
    definition.refresh_rule = modifiers::RefreshRule::refresh_duration;
    definition.on_apply = {modifiers::EffectCommandKind::set_shield, 7};
    definition.on_refresh = {modifiers::EffectCommandKind::set_shield, 8};
    ARPG_REQUIRE(source.apply(definition) == modifiers::ApplyResult::applied);
    modifiers::EffectCommand popped{};
    ARPG_REQUIRE(source.pop_command(popped));
    for (std::size_t index = 0U; index < 12U; ++index) {
        ARPG_REQUIRE(source.apply(definition)
            == modifiers::ApplyResult::refreshed);
        if (index < 5U) ARPG_REQUIRE(source.pop_command(popped));
    }

    modifiers::EffectSetCheckpoint checkpoint{};
    source.capture_checkpoint(checkpoint);
    ARPG_REQUIRE(checkpoint.command_head == 0U);
    ARPG_REQUIRE(checkpoint.command_count == source.queued_command_count());
    for (std::size_t index = checkpoint.command_count;
            index < checkpoint.commands.size(); ++index) {
        ARPG_REQUIRE(checkpoint.commands[index].kind
            == modifiers::EffectCommandKind::none);
        ARPG_REQUIRE(checkpoint.commands[index].value == 0);
        ARPG_REQUIRE(checkpoint.commands[index].effect_id == 0U);
    }
    modifiers::EffectSet restored{};
    ARPG_REQUIRE(restored.restore_checkpoint(checkpoint));
    ARPG_REQUIRE(restored.same_state(source));

    checkpoint.effects[7].strength = 1;
    ARPG_REQUIRE(!restored.restore_checkpoint(checkpoint));
    checkpoint.effects[7] = {};
    checkpoint.commands[checkpoint.command_count].value = 1;
    ARPG_REQUIRE(!restored.restore_checkpoint(checkpoint));
    return {};
}

test::Failure room_checkpoint_round_trip_clears_only_transients() noexcept {
    combat::CombatWorld world{};
    world.tick({1, 0});
    const combat::CombatSnapshot frozen = world.snapshot();
    const combat::MonsterHandle owner =
        test::CombatWorldTestAccess::first_active_monster(world);
    ARPG_REQUIRE(test::CombatWorldTestAccess::spawn_projectile(world, owner));
    ARPG_REQUIRE(test::CombatWorldTestAccess::spawn_hazard(world, owner));
    test::CombatWorldTestAccess::set_player_hit_stop(world, 5U);
    ARPG_REQUIRE(world.queue_action(combat::Action::light));
    test::CombatWorldTestAccess::fill_event_queue(world, 3U);
    ARPG_REQUIRE(world.snapshot().projectile_count != 0U);
    ARPG_REQUIRE(world.snapshot().hazard_count != 0U);
    ARPG_REQUIRE(world.snapshot().diagnostics.input_size != 0U);

    std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    const std::uint64_t before = test::allocation_count();
    ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
    const std::uint64_t after = test::allocation_count();
    ARPG_REQUIRE(after == before);

    ARPG_REQUIRE(world.restore_room_checkpoint(*checkpoint));
    const combat::CombatSnapshot restored = world.snapshot();
    ARPG_REQUIRE(restored.tick == frozen.tick);
    ARPG_REQUIRE(restored.player.position.x == frozen.player.position.x);
    ARPG_REQUIRE(restored.player.position.y == frozen.player.position.y);
    ARPG_REQUIRE(restored.player.hp == frozen.player.hp);
    ARPG_REQUIRE(restored.player.active_attack == frozen.player.active_attack);
    ARPG_REQUIRE(restored.player.hit_stop_ticks == 0U);
    ARPG_REQUIRE(restored.active_skill.id == skills::ActiveSkillId::none);
    ARPG_REQUIRE(restored.projectile_count == 0U);
    ARPG_REQUIRE(restored.hazard_count == 0U);
    ARPG_REQUIRE(restored.diagnostics.input_size == 0U);
    ARPG_REQUIRE(!world.try_pop_event().has_value());
    return {};
}

test::Failure malformed_rng_state_rejects_without_mutating_world() noexcept {
    combat::CombatWorld world{};
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
    checkpoint->evasion_rng_state = {};
    const combat::CombatSnapshot before = world.snapshot();
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    const combat::CombatSnapshot after = world.snapshot();
    ARPG_REQUIRE(after.tick == before.tick);
    ARPG_REQUIRE(after.player.position.x == before.player.position.x);
    ARPG_REQUIRE(after.player.hp == before.player.hp);
    return {};
}

test::Failure active_skills_normalize_ground_and_air_player_state() noexcept {
    const skills::ActiveSkillId skills_to_test[]{
        skills::ActiveSkillId::draw_slash,
        skills::ActiveSkillId::storm_swords,
    };
    for (const skills::ActiveSkillId skill : skills_to_test) {
        combat::CombatWorld ground{};
        ARPG_REQUIRE(ground.request_active_skill(skill)
            == combat::SkillCastResult::accepted);
        ground.tick({1, 0});
        std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
            new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
        ARPG_REQUIRE(checkpoint != nullptr);
        ARPG_REQUIRE(ground.capture_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(checkpoint->attack.id
            == arpg::checkpoint::AttackId::none);
        ARPG_REQUIRE(checkpoint->player.state
            == arpg::checkpoint::PlayerState::idle);
        const auto cooldowns = checkpoint->player.skill_cooldowns;
        ARPG_REQUIRE(ground.restore_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(ground.snapshot().active_skill.id
            == skills::ActiveSkillId::none);
        ARPG_REQUIRE(ground.snapshot().player.state
            == combat::PlayerState::idle);
        ARPG_REQUIRE(ground.snapshot().skill_cooldowns == cooldowns);
        ARPG_REQUIRE(ground.queue_action(combat::Action::light));
        ground.tick({});
        ARPG_REQUIRE(ground.snapshot().player.active_attack
            != combat::AttackId::none);

        combat::CombatWorld airborne{};
        ARPG_REQUIRE(airborne.queue_action(combat::Action::jump));
        airborne.tick({});
        ARPG_REQUIRE(airborne.snapshot().player.position.z > 0.0F);
        ARPG_REQUIRE(airborne.request_active_skill(skill)
            == combat::SkillCastResult::accepted);
        ARPG_REQUIRE(airborne.capture_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(checkpoint->player.state
            == arpg::checkpoint::PlayerState::jump_rise);
        ARPG_REQUIRE(airborne.restore_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(airborne.snapshot().active_skill.id
            == skills::ActiveSkillId::none);
        ARPG_REQUIRE(airborne.snapshot().player.state
            == combat::PlayerState::jump_rise);
        ARPG_REQUIRE(airborne.queue_action(combat::Action::light));
        airborne.tick({});
        ARPG_REQUIRE(airborne.snapshot().player.active_attack
            == combat::AttackId::air_j);
    }
    return {};
}

test::Failure attack_state_death_checkpoint_round_trips() noexcept {
    for (std::size_t mode = 0U; mode < 2U; ++mode) {
        combat::CombatWorld world{};
        test::CombatWorldTestAccess::set_player_resources(world, 1, 0);
        if (mode == 0U) {
            ARPG_REQUIRE(world.queue_action(combat::Action::light));
            world.tick({});
        } else {
            ARPG_REQUIRE(world.request_active_skill(
                skills::ActiveSkillId::draw_slash)
                == combat::SkillCastResult::accepted);
        }
        ARPG_REQUIRE(world.snapshot().player.state
            == combat::PlayerState::attack_startup);
        test::CombatWorldTestAccess::apply_damage(world, 100,
            {1.0F, 0.0F, 0.0F}, combat::FeedbackLevel::heavy);
        ARPG_REQUIRE(world.snapshot().player.hp == 0);

        std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
            new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
        ARPG_REQUIRE(checkpoint != nullptr);
        ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(checkpoint->has_death_snapshot);
        ARPG_REQUIRE(checkpoint->attack.id
            == arpg::checkpoint::AttackId::none);
        ARPG_REQUIRE(checkpoint->player.state
            == arpg::checkpoint::PlayerState::idle);
        ARPG_REQUIRE(world.restore_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(world.snapshot().player.hp == 0);
        ARPG_REQUIRE(world.snapshot().player.state
            == combat::PlayerState::idle);
    }
    return {};
}

test::Failure room_resident_restores_persistent_and_active_authority() noexcept {
    namespace fixture = test::room_field_fixture;
    auto field = std::make_unique<combat::RoomMonsterField>();
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 20U)
        == combat::RoomMonsterFieldFault::none);
    combat::CombatEncounterConfig config{};
    config.player_spawn = fixture::cell_center(1U, 0U);
    combat::CombatWorld world{config, std::move(field), {}};
    combat::RoomMonsterField* active_field = world.room_monster_field();
    ARPG_REQUIRE(active_field != nullptr);
    const auto residents = active_field->resident_ordinals();
    ARPG_REQUIRE(residents.count != 0U);
    const combat::MonsterOrdinal ordinal = residents.ordinals[0U];
    combat::MonsterRuntime* runtime = active_field->active_runtime(ordinal);
    ARPG_REQUIRE(runtime != nullptr);
    runtime->hp -= 1;
    runtime->position.x += 0.1F;
    modifiers::EffectDefinition effect{};
    effect.id = 71U;
    effect.duration_ticks = 99;
    ARPG_REQUIRE(active_field->active_effects(ordinal)->apply(effect)
        == modifiers::ApplyResult::applied);
    const int expected_hp = runtime->hp;
    const float expected_x = runtime->position.x;
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));

    runtime->hp -= 5;
    runtime->position.x += 1.0F;
    runtime->effects.clear();
    ARPG_REQUIRE(world.restore_room_checkpoint(*checkpoint));
    const combat::MonsterPersistentState* persistent =
        active_field->persistent_state(ordinal);
    runtime = active_field->active_runtime(ordinal);
    ARPG_REQUIRE(persistent != nullptr && runtime != nullptr);
    ARPG_REQUIRE(persistent->hp == expected_hp && runtime->hp == expected_hp);
    ARPG_REQUIRE(persistent->position.x == expected_x
        && runtime->position.x == expected_x);
    ARPG_REQUIRE(persistent->effects.active_count() == 1U);
    ARPG_REQUIRE(runtime->effects.active_count() == 1U);
    return {};
}

test::Failure late_checkpoint_lookup_failure_is_atomic() noexcept {
    combat::CombatWorld world{};
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> candidate{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> before{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> after{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(candidate && before && after);
    ARPG_REQUIRE(world.capture_room_checkpoint(*candidate));
    ARPG_REQUIRE(world.capture_room_checkpoint(*before));
    ARPG_REQUIRE(candidate->monster_count != 0U);
    candidate->monsters[0U].ordinal = 1000U;
    ARPG_REQUIRE(!world.restore_room_checkpoint(*candidate));
    ARPG_REQUIRE(world.capture_room_checkpoint(*after));
    ARPG_REQUIRE(after->tick == before->tick);
    ARPG_REQUIRE(after->evasion_rng_state == before->evasion_rng_state);
    ARPG_REQUIRE(after->player.hp == before->player.hp);
    ARPG_REQUIRE(after->player.position.x == before->player.position.x);
    ARPG_REQUIRE(after->player_damage_history.buckets
        == before->player_damage_history.buckets);
    ARPG_REQUIRE(after->monster_count == before->monster_count);
    ARPG_REQUIRE(after->monsters[0U].ordinal
        == before->monsters[0U].ordinal);
    ARPG_REQUIRE(after->monsters[0U].hp == before->monsters[0U].hp);
    return {};
}

test::Failure malformed_cross_field_authority_is_rejected() noexcept {
    combat::CombatWorld world{};
    world.tick({});
    world.tick({});
    world.tick({});
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
    const std::uint64_t history_tick =
        checkpoint->player_damage_history.active_tick;
    checkpoint->player_damage_history.active_tick = 0U;
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    checkpoint->player_damage_history.active_tick = history_tick;

    checkpoint->player.skill_cooldowns[0U] =
        static_cast<std::uint16_t>(skills::kDrawSlashCooldownTicks + 1U);
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    checkpoint->player.skill_cooldowns[0U] = 0U;

    checkpoint->abyss_environment.locked_center.x = 1.0F;
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    checkpoint->abyss_environment.locked_center = {};

    ARPG_REQUIRE(checkpoint->monster_count != 0U);
    checkpoint->monsters[0U].shield_recharge_ticks =
        static_cast<std::uint16_t>(
            checkpoint->monsters[0U].affix_profile
                .shield_recharge_delay_ticks + 1U);
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    checkpoint->monsters[0U].shield_recharge_ticks = 0U;
    const auto armor = checkpoint->monsters[0U].armor;
    const auto break_value = checkpoint->monsters[0U].break_value;
    checkpoint->monsters[0U].armor =
        arpg::checkpoint::ArmorState::broken;
    checkpoint->monsters[0U].break_value = 0;
    checkpoint->monsters[0U].break_window_ticks = 0U;
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    checkpoint->monsters[0U].armor = armor;
    checkpoint->monsters[0U].break_value = break_value;
    checkpoint->monsters[0U].break_window_ticks = 0U;
    std::size_t breakable_index = checkpoint->monster_count;
    for (std::size_t index = 0U; index < checkpoint->monster_count; ++index) {
        if (checkpoint->monsters[index].max_break > 0) {
            breakable_index = index;
            break;
        }
    }
    ARPG_REQUIRE(breakable_index < checkpoint->monster_count);
    const auto breakable_armor = checkpoint->monsters[breakable_index].armor;
    checkpoint->monsters[breakable_index].armor =
        arpg::checkpoint::ArmorState::none;
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    checkpoint->monsters[breakable_index].armor = breakable_armor;
    checkpoint->monsters[0U].burning_ground_ticks = 0xFFFFU;
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    checkpoint->monsters[0U].burning_ground_ticks = 0U;
    checkpoint->monsters[0U].effects.effects[0U].occupied = true;
    checkpoint->monsters[0U].effects.effects[0U].id = 99U;
    checkpoint->monsters[0U].effects.effects[0U].remaining_ticks = 10;
    checkpoint->monsters[0U].effects.effects[0U].stacks = 1U;
    checkpoint->monsters[0U].effects.effects[0U].max_stacks = 1U;
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    return {};
}

test::Failure attack_latch_and_fire_crate_tampering_is_rejected() noexcept {
    combat::CombatWorld attack_world{};
    ARPG_REQUIRE(attack_world.queue_action(combat::Action::light));
    attack_world.tick({});
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    ARPG_REQUIRE(attack_world.capture_room_checkpoint(*checkpoint));
    ARPG_REQUIRE(checkpoint->attack.elapsed_ticks
        < checkpoint->attack.startup_ticks);
    ARPG_REQUIRE(checkpoint->attack.hit_targets.insert(0U));
    checkpoint->attack.connected = true;
    checkpoint->attack.impact_event_emitted = true;
    ARPG_REQUIRE(!attack_world.restore_room_checkpoint(*checkpoint));

    combat::CombatEncounterConfig fire_config{};
    fire_config.fire_room_obstacles = true;
    combat::CombatWorld fire_world{fire_config};
    ARPG_REQUIRE(fire_world.capture_room_checkpoint(*checkpoint));
    ARPG_REQUIRE(checkpoint->fire_crate_count == 2U);
    checkpoint->fire_crates[0U].position.x += 1.0F;
    ARPG_REQUIRE(!fire_world.restore_room_checkpoint(*checkpoint));
    return {};
}

test::Failure cleared_abyss_checkpoint_round_trips() noexcept {
    combat::CombatWorld world{};
    world.clear_abyss_rule_preserving_resources();
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
    ARPG_REQUIRE(checkpoint->abyss_environment.expansion_stage == 0xFFU);
    ARPG_REQUIRE(world.restore_room_checkpoint(*checkpoint));
    return {};
}

test::Failure active_abyss_timers_and_post_death_gap_round_trip() noexcept {
    const bool expansion_modes[]{true, false};
    for (const bool expansion : expansion_modes) {
        combat::CombatWorld world{};
        abyss::AbyssCombatConfig config{};
        config.rule = expansion ? abyss::AbyssRuleId::chaos_expansion
                                : abyss::AbyssRuleId::thunderstorm;
        config.environment.active = true;
        config.environment.radius_count = expansion ? 3U : 1U;
        config.environment.radius_milliunits = {{1000U, 2000U, 3000U, 0U, 0U}};
        config.environment.expansion_interval_ticks = expansion ? 5U : 0U;
        config.environment.cycle_ticks = expansion ? 0U : 5U;
        config.environment.warning_ticks = 1U;
        config.environment.duration_ticks = 2U;
        config.environment.damage_interval_ticks = 1U;
        test::CombatWorldTestAccess::activate_abyss_environment(world, config);
        for (int tick = 0; tick < 8; ++tick) world.tick({});
        std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
            new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
        ARPG_REQUIRE(checkpoint != nullptr);
        ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(world.restore_room_checkpoint(*checkpoint));

        test::CombatWorldTestAccess::set_player_resources(world, 1, 0);
        test::CombatWorldTestAccess::apply_damage(world, 100,
            {1.0F, 0.0F, 0.0F}, combat::FeedbackLevel::heavy);
        ARPG_REQUIRE(world.snapshot().player.hp == 0);
        for (int tick = 0; tick < 31; ++tick) world.tick({});
        ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(world.restore_room_checkpoint(*checkpoint));
    }
    return {};
}

test::Failure near_landing_air_attack_checkpoint_round_trips() noexcept {
    combat::CombatWorld world{};
    ARPG_REQUIRE(world.queue_action(combat::Action::jump));
    world.tick({});
    while (world.snapshot().player.velocity.z > -5.0F) world.tick({});
    ARPG_REQUIRE(world.queue_action(combat::Action::light));
    world.tick({});
    for (int tick = 0; tick < 30
            && !(world.snapshot().player.state == combat::PlayerState::landing
                && world.snapshot().player.active_attack
                    == combat::AttackId::air_j); ++tick) {
        world.tick({});
    }
    ARPG_REQUIRE(world.snapshot().player.state == combat::PlayerState::landing);
    ARPG_REQUIRE(world.snapshot().player.active_attack == combat::AttackId::air_j);
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
    ARPG_REQUIRE(world.restore_room_checkpoint(*checkpoint));
    return {};
}

test::Failure malformed_death_history_and_extreme_defense_are_rejected() noexcept {
    combat::CombatWorld world{};
    test::CombatWorldTestAccess::set_player_resources(world, 1, 0);
    test::CombatWorldTestAccess::apply_damage(world, 100,
        {1.0F, 0.0F, 0.0F}, combat::FeedbackLevel::heavy);
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
    const auto recent = checkpoint->death_snapshot.recent_damage;
    checkpoint->death_snapshot.recent_damage = {};
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    checkpoint->death_snapshot.recent_damage = recent;
    checkpoint->death_snapshot.defense.max_barrier =
        (std::numeric_limits<int>::min)();
    checkpoint->death_snapshot.defense.barrier =
        (std::numeric_limits<int>::max)();
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    return {};
}

test::Failure checkpoint_latch_rejects_absent_monster_ordinal() noexcept {
    combat::CombatWorld world{};
    ARPG_REQUIRE(world.queue_action(combat::Action::light));
    world.tick({});
    std::unique_ptr<checkpoint::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) checkpoint::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
    checkpoint->attack.elapsed_ticks = checkpoint->attack.startup_ticks;
    checkpoint->player.state =
        arpg::checkpoint::PlayerState::attack_active;
    ARPG_REQUIRE(checkpoint->attack.hit_targets.insert(1000U));
    checkpoint->attack.connected = true;
    checkpoint->attack.impact_event_emitted = true;
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    return {};
}

constexpr test::TestCase kCases[] = {
    {"checkpoint runtime catalog parity",
        &runtime_catalogs_match_checkpoint_schema_exhaustively},
    {"checkpoint compatibility alias uses neutral path",
        &compatibility_alias_uses_neutral_checkpoint_path},
    {"neutral checkpoint maximum room authority",
        &neutral_checkpoint_round_trips_maximum_room_authority},
    {"effect checkpoint fields", &effect_checkpoint_preserves_commands_and_diagnostics},
    {"effect checkpoint logical queue", &effect_checkpoint_canonicalizes_the_logical_queue},
    {"room checkpoint transients", &room_checkpoint_round_trip_clears_only_transients},
    {"room checkpoint invalid rng", &malformed_rng_state_rejects_without_mutating_world},
    {"active skill checkpoint normalization", &active_skills_normalize_ground_and_air_player_state},
    {"attack death checkpoint", &attack_state_death_checkpoint_round_trips},
    {"room resident authority", &room_resident_restores_persistent_and_active_authority},
    {"late failure atomicity", &late_checkpoint_lookup_failure_is_atomic},
    {"cross field tampering", &malformed_cross_field_authority_is_rejected},
    {"attack and fire tampering", &attack_latch_and_fire_crate_tampering_is_rejected},
    {"cleared abyss round trip", &cleared_abyss_checkpoint_round_trips},
    {"active abyss timer round trip", &active_abyss_timers_and_post_death_gap_round_trip},
    {"near landing air attack", &near_landing_air_attack_checkpoint_round_trips},
    {"malformed death authority", &malformed_death_history_and_extreme_defense_are_rejected},
    {"checkpoint absent latch",
        &checkpoint_latch_rejects_absent_monster_ordinal},
};

}  // namespace

arpg::test::TestSuite room_combat_checkpoint_suite() noexcept {
    return arpg::test::make_suite("room_combat_checkpoint", kCases);
}
