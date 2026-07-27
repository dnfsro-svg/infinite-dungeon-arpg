#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_test_support.hpp"
#include "room_field_test_fixture.hpp"
#include "combat/room_combat_checkpoint.hpp"
#include "combat/combat_world.hpp"
#include "modifiers/effect_set.hpp"
#include "skills/active_skill_catalog.hpp"

#include <memory>
#include <limits>
#include <type_traits>

namespace {

using namespace arpg;

static_assert(!std::is_copy_constructible_v<combat::RoomCombatCheckpoint>);
static_assert(!std::is_move_constructible_v<combat::RoomCombatCheckpoint>);

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

    std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
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
    std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
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
        std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
            new (std::nothrow) combat::RoomCombatCheckpoint{}};
        ARPG_REQUIRE(checkpoint != nullptr);
        ARPG_REQUIRE(ground.capture_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(checkpoint->attack.id == combat::AttackId::none);
        ARPG_REQUIRE(checkpoint->player.state == combat::PlayerState::idle);
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
            == combat::PlayerState::jump_rise);
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

        std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
            new (std::nothrow) combat::RoomCombatCheckpoint{}};
        ARPG_REQUIRE(checkpoint != nullptr);
        ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
        ARPG_REQUIRE(checkpoint->has_death_snapshot);
        ARPG_REQUIRE(checkpoint->attack.id == combat::AttackId::none);
        ARPG_REQUIRE(checkpoint->player.state == combat::PlayerState::idle);
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
    std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
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

test::Failure late_legacy_lookup_failure_is_atomic() noexcept {
    combat::CombatWorld world{};
    std::unique_ptr<combat::RoomCombatCheckpoint> candidate{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
    std::unique_ptr<combat::RoomCombatCheckpoint> before{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
    std::unique_ptr<combat::RoomCombatCheckpoint> after{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
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
    std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
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
    checkpoint->monsters[0U].armor = combat::ArmorState::broken;
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
    checkpoint->monsters[breakable_index].armor = combat::ArmorState::none;
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
    std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
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
    std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
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
        std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
            new (std::nothrow) combat::RoomCombatCheckpoint{}};
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
    std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
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
    std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
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

test::Failure legacy_latch_rejects_absent_monster_ordinal() noexcept {
    combat::CombatWorld world{};
    ARPG_REQUIRE(world.queue_action(combat::Action::light));
    world.tick({});
    std::unique_ptr<combat::RoomCombatCheckpoint> checkpoint{
        new (std::nothrow) combat::RoomCombatCheckpoint{}};
    ARPG_REQUIRE(checkpoint != nullptr);
    ARPG_REQUIRE(world.capture_room_checkpoint(*checkpoint));
    checkpoint->attack.elapsed_ticks = checkpoint->attack.startup_ticks;
    checkpoint->player.state = combat::PlayerState::attack_active;
    ARPG_REQUIRE(checkpoint->attack.hit_targets.insert(1000U));
    checkpoint->attack.connected = true;
    checkpoint->attack.impact_event_emitted = true;
    ARPG_REQUIRE(!world.restore_room_checkpoint(*checkpoint));
    return {};
}

constexpr test::TestCase kCases[] = {
    {"effect checkpoint fields", &effect_checkpoint_preserves_commands_and_diagnostics},
    {"effect checkpoint logical queue", &effect_checkpoint_canonicalizes_the_logical_queue},
    {"room checkpoint transients", &room_checkpoint_round_trip_clears_only_transients},
    {"room checkpoint invalid rng", &malformed_rng_state_rejects_without_mutating_world},
    {"active skill checkpoint normalization", &active_skills_normalize_ground_and_air_player_state},
    {"attack death checkpoint", &attack_state_death_checkpoint_round_trips},
    {"room resident authority", &room_resident_restores_persistent_and_active_authority},
    {"late failure atomicity", &late_legacy_lookup_failure_is_atomic},
    {"cross field tampering", &malformed_cross_field_authority_is_rejected},
    {"attack and fire tampering", &attack_latch_and_fire_crate_tampering_is_rejected},
    {"cleared abyss round trip", &cleared_abyss_checkpoint_round_trips},
    {"active abyss timer round trip", &active_abyss_timers_and_post_death_gap_round_trip},
    {"near landing air attack", &near_landing_air_attack_checkpoint_round_trips},
    {"malformed death authority", &malformed_death_history_and_extreme_defense_are_rejected},
    {"legacy absent latch", &legacy_latch_rejects_absent_monster_ordinal},
};

}  // namespace

arpg::test::TestSuite room_combat_checkpoint_suite() noexcept {
    return arpg::test::make_suite("room_combat_checkpoint", kCases);
}
