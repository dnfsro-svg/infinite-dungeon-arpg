#include "test_framework.hpp"

#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "checkpoint/room_checkpoint_validation.hpp"
#include "combat/monster_affix_types.hpp"
#include "dungeon/death_checkpoint.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_generation.hpp"
#include "../dungeon/dungeon_test_support.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace {

namespace abyss = arpg::abyss;
namespace checkpoint = arpg::checkpoint;
namespace combat = arpg::combat;
namespace dungeon = arpg::dungeon;

using checkpoint::DeathLifecycle;
using checkpoint::DeathSourceKind;
using dungeon::DungeonRules;
using dungeon::DungeonSession;
using dungeon::DungeonSnapshot;
using dungeon::RequestResult;
using dungeon::RoomPhase;
using dungeon::PendingSaveKind;
using dungeon::SaveDisposition;

combat::CombatDeathSnapshot combat_death(
    DeathSourceKind source = DeathSourceKind::monster_affix) noexcept {
    combat::CombatDeathSnapshot result{};
    result.source.monster = combat::MonsterId::fire_charger;
    switch (source) {
    case DeathSourceKind::monster_attack:
        result.source.kind = combat::PlayerDamageSourceKind::monster_attack;
        break;
    case DeathSourceKind::projectile:
        result.source.kind = combat::PlayerDamageSourceKind::projectile;
        break;
    case DeathSourceKind::ground_hazard:
        result.source.kind = combat::PlayerDamageSourceKind::ground_hazard;
        result.source.detail_id = static_cast<std::uint16_t>(
            combat::HazardKind::burning);
        break;
    case DeathSourceKind::monster_affix:
        result.source.kind = combat::PlayerDamageSourceKind::monster_affix;
        result.source.detail_id = static_cast<std::uint16_t>(
            combat::MonsterAffixId::burning_ground);
        break;
    case DeathSourceKind::abyss_environment:
        result.source.kind = combat::PlayerDamageSourceKind::abyss_environment;
        result.source.monster = combat::MonsterId::count;
        result.source.detail_id = static_cast<std::uint16_t>(
            abyss::AbyssRuleId::thunderstorm);
        break;
    case DeathSourceKind::unknown:
        result.source.kind = combat::PlayerDamageSourceKind::unknown;
        result.source.monster = combat::MonsterId::count;
        break;
    }
    result.primary_type = arpg::modifiers::DamageType::fire;
    result.raw_damage = 50U;
    result.barrier_loss = 10U;
    result.health_loss = 20U;
    result.final_damage = 30U;
    result.recent_damage = {{0U, 30U, 0U, 0U, 0U}};
    result.defense.max_hp = 100;
    result.defense.max_barrier = 50;
    result.defense.armor = 100;
    result.defense.evasion = 200;
    result.defense.armor_reduction_bp = 1000;
    result.defense.evasion_rate_bp = 2000;
    result.defense.damage_reduction = {{100, -200, 300, 400}};
    result.defense.damage_reduction_cap = {{7500, 7600, 7700, 7800}};
    return result;
}

checkpoint::DungeonRunState pending_state(
    std::uint64_t depth = 4U,
    bool abyss_death = false,
    DeathSourceKind source = DeathSourceKind::monster_affix) noexcept {
    DungeonRules rules{};
    checkpoint::DungeonRunState state =
        dungeon::make_initial_run_state(0x51A6E11U, rules).state;
    state.current_room.index = 17U;
    state.current_room.seed = abyss_death ? 0x150U : 0xABCDEFU;
    state.current_room.depth = depth;
    state.current_room.floor_room_index = 7U;
    state.current_room.entry = checkpoint::EntrySide::left;
    state.current_room.ecology = checkpoint::DungeonElement::lightning;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = abyss_death;
    state.commit_generation = 11U;
    state.death_sequence = 3U;
    state.biases = {};

    const auto target = dungeon::make_death_retreat_target(
        state, 4U, rules);
    if (target.fault != dungeon::DungeonFault::none) return {};
    state.death = dungeon::make_death_checkpoint(
        combat_death(source), state.current_room, target.room);

    if (abyss_death) {
        const auto selection = abyss::select_abyss_rule(
            state.current_room.seed, state.current_room.depth);
        if (!selection.has_value()) return {};
        state.abyss.lifecycle = abyss::AbyssLifecycle::failed;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
        const auto total = abyss::reward_profile_for(
            selection->danger, 1U).item_count;
        state.last_abyss_resolution = {
            true, state.current_room.seed, selection->rule,
            total, 0U, 0U, total, abyss::AbyssLifecycle::failed};
    }

    ++state.commit_generation;
    ++state.death_sequence;
    state.current_room.is_abyss = false;
    state.last_transition = checkpoint::TransitionKind::death_retreat;
    state.last_direction = checkpoint::ExitDirection::none;
    return state;
}

checkpoint::DungeonRunState unprotected_combat_state(
    const std::uint64_t seed) noexcept {
    checkpoint::DungeonRunState state = dungeon::make_initial_run_state(
        seed, DungeonRules{}).state;
    state.current_room.depth = 4U;
    return state;
}

bool death_equals(
    const checkpoint::DeathCheckpoint& lhs,
    const checkpoint::DeathCheckpoint& rhs) noexcept {
    checkpoint::DungeonRunState left{};
    checkpoint::DungeonRunState right{};
    left.death = lhs;
    right.death = rhs;
    return dungeon::same_run_state(left, right);
}

bool all_closed(const std::array<bool, 4>& exits) noexcept {
    for (const bool open : exits) {
        if (open) return false;
    }
    return true;
}

bool fail_closed(checkpoint::DungeonRunState state) noexcept {
    DungeonSession session{DungeonRules{}, std::move(state)};
    const DungeonSnapshot snapshot = session.snapshot();
    return snapshot.phase == RoomPhase::faulted
        && !snapshot.has_active_room
        && !snapshot.combat.has_value()
        && snapshot.ground_item_count == 0U
        && all_closed(snapshot.exits_open)
        && !snapshot.has_pending_transition;
}

arpg::test::Failure ordinary_pending_load_is_read_only() noexcept {
    const auto expected = pending_state();
    DungeonSession session{DungeonRules{}, expected};
    const DungeonSnapshot snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::death_pending);
    ARPG_REQUIRE(snapshot.death.has_value());
    ARPG_REQUIRE(!snapshot.death->saving);
    ARPG_REQUIRE(snapshot.death->can_continue);
    ARPG_REQUIRE(!snapshot.death->continue_failed);
    ARPG_REQUIRE(death_equals(snapshot.death->checkpoint, expected.death));
    ARPG_REQUIRE(!snapshot.has_active_room);
    ARPG_REQUIRE(!snapshot.combat.has_value());
    ARPG_REQUIRE(snapshot.ground_item_count == 0U);
    ARPG_REQUIRE(all_closed(snapshot.exits_open));
    ARPG_REQUIRE(!snapshot.has_pending_transition);
    ARPG_REQUIRE(!session.try_pop_event().has_value());
    ARPG_REQUIRE(!session.try_pop_combat_event().has_value());
    return {};
}

arpg::test::Failure depth_one_and_abyss_pending_loads_are_valid() noexcept {
    {
        auto state = pending_state(1U);
        state.current_room.floor_room_index = 0U;
        state.death.death_floor_room_index = 0U;
        DungeonSession session{DungeonRules{}, state};
        const auto snapshot = session.snapshot();
        ARPG_REQUIRE(snapshot.phase == RoomPhase::death_pending);
        ARPG_REQUIRE(snapshot.death->checkpoint.target_room.depth == 1U);
    }
    {
        const auto state = pending_state(9U, true,
            DeathSourceKind::abyss_environment);
        DungeonSession session{DungeonRules{}, state};
        const auto snapshot = session.snapshot();
        ARPG_REQUIRE(snapshot.phase == RoomPhase::death_pending);
        ARPG_REQUIRE(snapshot.death->checkpoint.death_was_abyss);
        ARPG_REQUIRE(snapshot.death->checkpoint.target_room.depth == 8U);
    }
    return {};
}

arpg::test::Failure none_lifecycle_constructs_the_existing_room() noexcept {
    const auto state = dungeon::make_initial_run_state(
        0x51A6E11U, DungeonRules{}).state;
    DungeonSession session{DungeonRules{}, state};
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::locked);
    ARPG_REQUIRE(snapshot.has_active_room);
    ARPG_REQUIRE(snapshot.combat.has_value());
    ARPG_REQUIRE(!snapshot.death.has_value());
    return {};
}

arpg::test::Failure target_regeneration_mutations_fail_closed() noexcept {
    for (std::uint8_t mutation = 0U; mutation < 8U; ++mutation) {
        auto state = pending_state();
        switch (mutation) {
        case 0U: ++state.death.target_room.seed; break;
        case 1U:
            state.death.target_room.ecology = checkpoint::DungeonElement::fire;
            break;
        case 2U: state.death.target_room.has_hole =
            !state.death.target_room.has_hole; break;
        case 3U: ++state.death.target_room.index; break;
        case 4U: ++state.death.target_room.depth; break;
        case 5U: state.death.target_room.entry = checkpoint::EntrySide::left;
            break;
        case 6U: state.death.target_room.floor_room_index = 1U; break;
        case 7U: state.death.target_room.is_abyss = true; break;
        }
        ARPG_REQUIRE(fail_closed(std::move(state)));
    }
    return {};
}

arpg::test::Failure sequence_generation_and_anchor_mutations_fail_closed() noexcept {
    for (std::uint8_t mutation = 0U; mutation < 11U; ++mutation) {
        auto state = pending_state();
        switch (mutation) {
        case 0U: state.death_sequence = 0U; break;
        case 1U: state.commit_generation = 0U; break;
        case 2U: state.last_transition = checkpoint::TransitionKind::door;
            break;
        case 3U: state.last_direction = checkpoint::ExitDirection::left;
            break;
        case 4U: state.biases[0] = 1U; break;
        case 5U: state.current_room.is_abyss = true; break;
        case 6U: ++state.current_room.depth; break;
        case 7U: ++state.current_room.floor_room_index; break;
        case 8U: state.current_room.ecology = checkpoint::DungeonElement::fire;
            break;
        case 9U: ++state.current_room.seed; break;
        case 10U:
            state.current_room.entry = static_cast<checkpoint::EntrySide>(
                0xFEU);
            break;
        }
        ARPG_REQUIRE(fail_closed(std::move(state)));
    }
    return {};
}

arpg::test::Failure first_generation_cannot_claim_a_committed_death() noexcept {
    auto state = pending_state();
    state.commit_generation = 1U;
    state.death_sequence = 1U;

    checkpoint::DungeonRunState pre_commit = state;
    pre_commit.commit_generation = 0U;
    pre_commit.death_sequence = 0U;
    pre_commit.death = {};
    const auto target = dungeon::make_death_retreat_target(
        pre_commit, 1U, DungeonRules{});
    ARPG_REQUIRE(target.fault == dungeon::DungeonFault::none);
    state.death.target_room = target.room;

    DungeonSession session{DungeonRules{}, state};
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(!session.snapshot().combat.has_value());
    return {};
}

arpg::test::Failure source_catalog_accepts_only_canonical_ids() noexcept {
    constexpr std::array<DeathSourceKind, 6U> kinds{{
        DeathSourceKind::monster_attack,
        DeathSourceKind::projectile,
        DeathSourceKind::ground_hazard,
        DeathSourceKind::monster_affix,
        DeathSourceKind::abyss_environment,
        DeathSourceKind::unknown,
    }};
    for (const DeathSourceKind kind : kinds) {
        DungeonSession session{DungeonRules{}, pending_state(
            4U, kind == DeathSourceKind::abyss_environment, kind)};
        ARPG_REQUIRE(session.snapshot().phase == RoomPhase::death_pending);
    }

    for (std::uint8_t mutation = 0U; mutation < 6U; ++mutation) {
        auto state = pending_state();
        switch (mutation) {
        case 0U:
            state.death.source_kind = DeathSourceKind::monster_attack;
            state.death.source_detail_id = 1U;
            break;
        case 1U:
            state.death.source_kind = DeathSourceKind::projectile;
            state.death.source_monster_id = static_cast<std::uint8_t>(
                combat::MonsterId::count);
            state.death.source_detail_id = 0U;
            break;
        case 2U:
            state.death.source_kind = DeathSourceKind::ground_hazard;
            state.death.source_detail_id = static_cast<std::uint16_t>(
                combat::HazardKind::chaos_expansion) + 1U;
            break;
        case 3U:
            state.death.source_kind = DeathSourceKind::monster_affix;
            state.death.source_detail_id = static_cast<std::uint16_t>(
                combat::MonsterAffixId::count);
            break;
        case 4U:
            state.death.source_kind = DeathSourceKind::abyss_environment;
            state.death.source_monster_id = 0xFFU;
            state.death.source_detail_id = static_cast<std::uint16_t>(
                abyss::AbyssRuleId::none);
            break;
        case 5U:
            state.death.source_kind = DeathSourceKind::unknown;
            state.death.source_monster_id = 0U;
            state.death.source_detail_id = 0U;
            break;
        }
        ARPG_REQUIRE(fail_closed(std::move(state)));
    }
    return {};
}

arpg::test::Failure abyss_failure_relationships_are_defended() noexcept {
    for (std::uint8_t mutation = 0U; mutation < 9U; ++mutation) {
        auto state = pending_state(9U, true,
            DeathSourceKind::abyss_environment);
        switch (mutation) {
        case 0U: state.abyss.lifecycle = abyss::AbyssLifecycle::none; break;
        case 1U: state.abyss.rule = abyss::AbyssRuleId::none; break;
        case 2U: ++state.abyss.rules_version; break;
        case 3U: state.last_abyss_resolution.valid = false; break;
        case 4U: ++state.last_abyss_resolution.room_seed; break;
        case 5U: state.last_abyss_resolution.rule = abyss::AbyssRuleId::none;
            break;
        case 6U: state.last_abyss_resolution.generated = 1U; break;
        case 7U: state.last_abyss_resolution.abandoned = 0U; break;
        case 8U:
            state.last_abyss_resolution.lifecycle =
                abyss::AbyssLifecycle::cleared;
            break;
        }
        ARPG_REQUIRE(fail_closed(std::move(state)));
    }
    return {};
}

arpg::test::Failure ordinary_death_preserves_valid_historical_resolution() noexcept {
    auto state = pending_state();
    const auto selection = abyss::select_abyss_rule(0x1234U, 20U);
    ARPG_REQUIRE(selection.has_value());
    const auto total = abyss::reward_profile_for(
        selection->danger, 1U).item_count;
    state.last_abyss_resolution = {
        true, 0x1234U, selection->rule, total, 0U, 0U, total};
    DungeonSession session{DungeonRules{}, state};
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::death_pending);
    return {};
}

arpg::test::Failure death_pending_rejects_every_gameplay_request() noexcept {
    const auto expected = pending_state();
    DungeonSession session{DungeonRules{}, expected};
    const auto before = session.snapshot();
    ARPG_REQUIRE(!session.queue_action(combat::Action::light));
    ARPG_REQUIRE(!session.request_descent(true));
    ARPG_REQUIRE(!session.request_passive_allocation(0U));
    ARPG_REQUIRE(!session.request_passive_refund(0U));
    ARPG_REQUIRE(session.request_equip(1U) == RequestResult::rejected);
    ARPG_REQUIRE(session.request_unequip(arpg::items::ItemSlot::weapon)
        == RequestResult::rejected);
    ARPG_REQUIRE(session.request_recipe({{1U, 2U, 3U}})
        == RequestResult::rejected);
    ARPG_REQUIRE(session.request_pickup(0U) == RequestResult::rejected);
    session.request_nearby_pickups({0.0F, 0.0F, 0.0F});
    ARPG_REQUIRE(session.reset_current_room() == RequestResult::rejected);
    session.tick({1, 1});
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.phase == RoomPhase::death_pending);
    ARPG_REQUIRE(after.room_index == before.room_index);
    ARPG_REQUIRE(after.room_seed == before.room_seed);
    ARPG_REQUIRE(after.commit_generation == before.commit_generation);
    ARPG_REQUIRE(after.death.has_value());
    ARPG_REQUIRE(death_equals(after.death->checkpoint, expected.death));
    ARPG_REQUIRE(!after.pending_save_kind.has_value());
    ARPG_REQUIRE(!after.has_pending_transition);
    ARPG_REQUIRE(after.ground_item_count == 0U);
    ARPG_REQUIRE(!after.combat.has_value());
    return {};
}

bool drive_ordinary_death(DungeonSession& session) noexcept {
    session.tick({});
    if (!arpg::test::kill_current_player_through_combat(session)) return false;
    session.tick({});
    return session.snapshot().phase == RoomPhase::committing;
}

checkpoint::DungeonRunState available_abyss_state() noexcept {
    checkpoint::DungeonRunState state = dungeon::make_initial_run_state(
        0xA8175EEDU, DungeonRules{}).state;
    std::uint64_t seed = 1U;
    while (!abyss::is_abyss_roll(seed)) ++seed;
    state.current_room.index = 29U;
    state.current_room.seed = seed;
    state.current_room.depth = 12U;
    state.current_room.floor_room_index = 4U;
    state.current_room.entry = checkpoint::EntrySide::right;
    state.current_room.ecology = checkpoint::DungeonElement::chaos;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = true;
    state.last_transition = checkpoint::TransitionKind::door;
    state.last_direction = checkpoint::ExitDirection::left;
    const auto selection = abyss::select_abyss_rule(
        state.current_room.seed, state.current_room.depth);
    if (!selection.has_value()) return {};
    state.abyss.lifecycle = abyss::AbyssLifecycle::available;
    state.abyss.danger = selection->danger;
    state.abyss.rule = selection->rule;
    state.abyss.rules_version = selection->rules_version;
    return state;
}

bool drive_started_abyss_death(
    DungeonSession& session,
    checkpoint::DungeonRunState& started) noexcept {
    const auto start = session.pending_save();
    if (!start.has_value()
            || start->kind != PendingSaveKind::abyss_start
            || start->next_state.abyss.lifecycle
                != abyss::AbyssLifecycle::started) {
        return false;
    }
    started = start->next_state;
    session.resolve_pending_save({SaveDisposition::committed,
        start->expected_generation, start->next_state});
    if (session.snapshot().phase != RoomPhase::locked
            || !session.snapshot().combat.has_value()) {
        return false;
    }
    session.tick({});
    if (!arpg::test::kill_current_player_through_combat(session)) return false;
    session.tick({});
    return session.snapshot().phase == RoomPhase::committing;
}

arpg::test::Failure abyss_death_prepares_one_atomic_retreat() noexcept {
    DungeonSession session{DungeonRules{}, available_abyss_state()};
    checkpoint::DungeonRunState started{};
    ARPG_REQUIRE(drive_started_abyss_death(session, started));
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == PendingSaveKind::death_retreat);
    ARPG_REQUIRE(pending->death_snapshot.has_value());
    ARPG_REQUIRE(pending->expected_generation
        == started.commit_generation + 1U);
    ARPG_REQUIRE(pending->next_state.death_sequence
        == started.death_sequence + 1U);
    ARPG_REQUIRE(pending->next_state.death.lifecycle
        == DeathLifecycle::pending_continue);
    ARPG_REQUIRE(pending->next_state.death.death_was_abyss);
    ARPG_REQUIRE(pending->next_state.death.death_depth
        == started.current_room.depth);
    ARPG_REQUIRE(pending->next_state.death.death_floor_room_index
        == started.current_room.floor_room_index);
    ARPG_REQUIRE(pending->next_state.death.death_ecology
        == started.current_room.ecology);
    ARPG_REQUIRE(!pending->next_state.current_room.is_abyss);
    ARPG_REQUIRE(pending->next_state.abyss.lifecycle
        == abyss::AbyssLifecycle::failed);
    const auto total = abyss::reward_profile_for(
        started.abyss.danger, 1U).item_count;
    const auto& resolution = pending->next_state.last_abyss_resolution;
    ARPG_REQUIRE(resolution.valid);
    ARPG_REQUIRE(resolution.lifecycle
        == abyss::AbyssLifecycle::failed);
    ARPG_REQUIRE(resolution.room_seed == started.current_room.seed);
    ARPG_REQUIRE(resolution.rule == started.abyss.rule);
    ARPG_REQUIRE(resolution.total == total);
    ARPG_REQUIRE(resolution.generated == 0U);
    ARPG_REQUIRE(resolution.claimed == 0U);
    ARPG_REQUIRE(resolution.abandoned == total);
    ARPG_REQUIRE(dungeon::same_run_state(
        arpg::test::stable_state(session), started));
    return {};
}

arpg::test::Failure abyss_death_retry_and_reload_preserve_exact_state() noexcept {
    DungeonSession session{DungeonRules{}, available_abyss_state()};
    checkpoint::DungeonRunState started{};
    ARPG_REQUIRE(drive_started_abyss_death(session, started));
    const auto first = *session.pending_save();
    ARPG_REQUIRE(first.death_snapshot.has_value());

    session.resolve_pending_save({SaveDisposition::not_committed, 0U, {},
        PendingSaveKind::death_retreat});
    auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::combat);
    ARPG_REQUIRE(snapshot.combat.has_value());
    ARPG_REQUIRE(snapshot.combat->player.hp == 0);
    ARPG_REQUIRE(snapshot.is_abyss);
    ARPG_REQUIRE(dungeon::same_run_state(
        arpg::test::stable_state(session), started));

    session.tick({});
    const auto retry = *session.pending_save();
    ARPG_REQUIRE(retry.kind == PendingSaveKind::death_retreat);
    ARPG_REQUIRE(retry.expected_generation == first.expected_generation);
    ARPG_REQUIRE(dungeon::same_run_state(retry.next_state, first.next_state));
    ARPG_REQUIRE(retry.death_snapshot.has_value());
    ARPG_REQUIRE(retry.death_snapshot->tick == first.death_snapshot->tick);
    ARPG_REQUIRE(retry.death_snapshot->source.kind
        == first.death_snapshot->source.kind);
    ARPG_REQUIRE(retry.death_snapshot->source.monster
        == first.death_snapshot->source.monster);
    ARPG_REQUIRE(retry.death_snapshot->source.detail_id
        == first.death_snapshot->source.detail_id);
    ARPG_REQUIRE(retry.death_snapshot->final_damage
        == first.death_snapshot->final_damage);
    ARPG_REQUIRE(retry.death_snapshot->recent_damage
        == first.death_snapshot->recent_damage);
    ARPG_REQUIRE(retry.death_snapshot->defense.damage_reduction
        == first.death_snapshot->defense.damage_reduction);

    std::unique_ptr<checkpoint::SaveCheckpointSlot> saved{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(saved != nullptr);
    saved->state.item_ownership.items.reserve(
        retry.next_state.item_ownership.items.size());
    ARPG_REQUIRE(session.capture_save_checkpoint(
        *saved, retry.expected_generation, &retry.next_state));
    ARPG_REQUIRE(saved->state.death.lifecycle
        == checkpoint::DeathLifecycle::pending_continue);
    ARPG_REQUIRE(saved->room_progress.lifecycle
        == checkpoint::RoomProgressLifecycle::death_pending);

    session.resolve_pending_save({SaveDisposition::committed,
        retry.expected_generation, retry.next_state,
        PendingSaveKind::death_retreat});
    snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::death_pending);
    ARPG_REQUIRE(snapshot.death.has_value());
    ARPG_REQUIRE(snapshot.death->checkpoint.death_was_abyss);
    ARPG_REQUIRE(!snapshot.combat.has_value());
    ARPG_REQUIRE(snapshot.ground_item_count == 0U);
    ARPG_REQUIRE(snapshot.abyss_pending_rewards == 0U);
    ARPG_REQUIRE(snapshot.abyss_unpicked_rewards == 0U);
    ARPG_REQUIRE(snapshot.pending_room_experience == 0U);

    DungeonSession rejected{DungeonRules{}, saved->state};
    const auto rejected_before = rejected.snapshot();
    saved->room_progress.environment_blueprint_hash ^= 1U;
    ARPG_REQUIRE(!rejected.restore_room_progress_checkpoint(*saved));
    const auto rejected_after = rejected.snapshot();
    ARPG_REQUIRE(rejected_after.phase == rejected_before.phase);
    ARPG_REQUIRE(rejected_after.phase == RoomPhase::death_pending);
    ARPG_REQUIRE(rejected_after.death.has_value());
    ARPG_REQUIRE(!rejected_after.combat.has_value());
    ARPG_REQUIRE(rejected_after.ground_item_count == 0U);
    ARPG_REQUIRE(dungeon::same_run_state(
        arpg::test::stable_state(rejected), saved->state));
    saved->room_progress.environment_blueprint_hash ^= 1U;

    DungeonSession reloaded{DungeonRules{}, saved->state};
    ARPG_REQUIRE(reloaded.restore_room_progress_checkpoint(*saved));
    const auto restored = reloaded.snapshot();
    ARPG_REQUIRE(restored.phase == RoomPhase::death_pending);
    ARPG_REQUIRE(restored.death.has_value());
    ARPG_REQUIRE(death_equals(
        restored.death->checkpoint, retry.next_state.death));
    ARPG_REQUIRE(!restored.combat.has_value());
    ARPG_REQUIRE(restored.ground_item_count == 0U);
    ARPG_REQUIRE(!reloaded.pending_save().has_value());
    return {};
}

arpg::test::Failure abyss_death_receipt_mutations_fail_closed() noexcept {
    for (std::uint8_t mutation = 0U; mutation < 4U; ++mutation) {
        DungeonSession session{DungeonRules{}, available_abyss_state()};
        checkpoint::DungeonRunState started{};
        ARPG_REQUIRE(drive_started_abyss_death(session, started));
        const auto pending = *session.pending_save();
        auto verified = pending.next_state;
        switch (mutation) {
        case 0U:
            verified.abyss.lifecycle = abyss::AbyssLifecycle::started;
            break;
        case 1U:
            ++verified.last_abyss_resolution.room_seed;
            break;
        case 2U:
            verified.death.death_was_abyss = false;
            break;
        case 3U:
            verified.current_room.is_abyss = true;
            break;
        }
        session.resolve_pending_save({SaveDisposition::committed,
            pending.expected_generation, std::move(verified),
            PendingSaveKind::death_retreat});
        ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
        ARPG_REQUIRE(session.snapshot().diagnostics.fault
            == dungeon::DungeonFault::save_receipt_mismatch);
    }
    return {};
}

arpg::test::Failure ordinary_death_prepares_atomic_retreat() noexcept {
    auto initial = unprotected_combat_state(0xD34D6U);
    initial.item_ownership.materials[0] =
        (std::numeric_limits<std::uint64_t>::max)();
    DungeonSession session{DungeonRules{}, std::move(initial)};
    const auto stable_before = arpg::test::stable_state(session);
    ARPG_REQUIRE(drive_ordinary_death(session));
    const auto* pending = session.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    ARPG_REQUIRE(pending->kind == PendingSaveKind::death_retreat);
    ARPG_REQUIRE(pending->death_snapshot.has_value());
    ARPG_REQUIRE(pending->expected_generation
        == stable_before.commit_generation + 1U);
    ARPG_REQUIRE(pending->next_state.death_sequence
        == stable_before.death_sequence + 1U);
    ARPG_REQUIRE(pending->next_state.item_ownership.materials
        == stable_before.item_ownership.materials);
    ARPG_REQUIRE(arpg::test::stable_state(session).death_sequence
        == stable_before.death_sequence);
    ARPG_REQUIRE(pending->next_state.death.lifecycle
        == DeathLifecycle::pending_continue);
    ARPG_REQUIRE(pending->next_state.current_room.index
        == stable_before.current_room.index);
    ARPG_REQUIRE(!pending->next_state.current_room.is_abyss);
    ARPG_REQUIRE((pending->next_state.biases
        == std::array<std::uint32_t, 4>{}));
    ARPG_REQUIRE(pending->next_state.last_transition
        == checkpoint::TransitionKind::death_retreat);
    ARPG_REQUIRE(pending->next_state.last_direction
        == checkpoint::ExitDirection::none);
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.death.has_value());
    ARPG_REQUIRE(snapshot.death->saving);
    ARPG_REQUIRE(!snapshot.death->can_continue);
    ARPG_REQUIRE(snapshot.combat.has_value());

    auto saved = std::make_unique<checkpoint::SaveCheckpointSlot>();
    ARPG_REQUIRE(saved != nullptr);
    saved->state.item_ownership.items.reserve(
        pending->next_state.item_ownership.items.size());
    ARPG_REQUIRE(session.capture_save_checkpoint(
        *saved, pending->expected_generation, &pending->next_state));
    ARPG_REQUIRE(saved->room_progress.lifecycle
        == checkpoint::RoomProgressLifecycle::death_pending);
    return {};
}

arpg::test::Failure ordinary_death_receipts_are_bound_to_kind() noexcept {
    DungeonSession session{
        DungeonRules{}, unprotected_combat_state(0xD34D7U)};
    ARPG_REQUIRE(drive_ordinary_death(session));
    const auto pending = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, pending.next_state,
        PendingSaveKind::transition});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure ordinary_death_commits_then_duplicate_faults() noexcept {
    DungeonSession session{
        DungeonRules{}, unprotected_combat_state(0xD34D8U)};
    ARPG_REQUIRE(drive_ordinary_death(session));
    const auto pending = *session.pending_save();
    const dungeon::PendingSaveResult receipt{SaveDisposition::committed,
        pending.expected_generation, pending.next_state,
        PendingSaveKind::death_retreat};
    session.resolve_pending_save(receipt);
    auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::death_pending);
    ARPG_REQUIRE(snapshot.death.has_value());
    ARPG_REQUIRE(!snapshot.death->saving);
    ARPG_REQUIRE(snapshot.death->can_continue);
    ARPG_REQUIRE(!snapshot.combat.has_value());
    ARPG_REQUIRE(snapshot.ground_item_count == 0U);
    session.resolve_pending_save(receipt);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    return {};
}

arpg::test::Failure ordinary_death_not_committed_retries_identically() noexcept {
    DungeonSession session{
        DungeonRules{}, unprotected_combat_state(0xD34D9U)};
    ARPG_REQUIRE(drive_ordinary_death(session));
    const auto first = *session.pending_save();
    std::uint32_t detected = 0U;
    while (const auto event = session.try_pop_event()) {
        if (event->kind == dungeon::DungeonEventKind::death_detected) ++detected;
    }
    ARPG_REQUIRE(detected == 1U);
    session.resolve_pending_save({SaveDisposition::not_committed, 0U, {},
        PendingSaveKind::death_retreat});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(session.snapshot().combat.has_value());
    ARPG_REQUIRE(!session.snapshot().death->continue_failed);
    ARPG_REQUIRE(!session.pending_save().has_value());
    session.tick({});
    const auto second = *session.pending_save();
    ARPG_REQUIRE(second.kind == first.kind);
    ARPG_REQUIRE(second.expected_generation == first.expected_generation);
    ARPG_REQUIRE(dungeon::same_run_state(second.next_state, first.next_state));
    ARPG_REQUIRE(second.death_snapshot.has_value());
    ARPG_REQUIRE(second.death_snapshot->recent_damage
        == first.death_snapshot->recent_damage);
    while (const auto event = session.try_pop_event()) {
        if (event->kind == dungeon::DungeonEventKind::death_detected) ++detected;
    }
    ARPG_REQUIRE(detected == 1U);
    return {};
}

arpg::test::Failure ordinary_death_receipt_fault_matrix() noexcept {
    for (std::uint8_t mutation = 0U; mutation < 4U; ++mutation) {
        DungeonSession session{DungeonRules{},
            unprotected_combat_state(0xD34DA0U + mutation)};
        ARPG_REQUIRE(drive_ordinary_death(session));
        const auto pending = *session.pending_save();
        dungeon::PendingSaveResult receipt{SaveDisposition::committed,
            pending.expected_generation, pending.next_state,
            PendingSaveKind::death_retreat};
        switch (mutation) {
        case 0U:
            receipt.disposition = SaveDisposition::indeterminate;
            break;
        case 1U: receipt.kind.reset(); break;
        case 2U: ++receipt.generation; break;
        case 3U: ++receipt.verified_state.death.raw_damage; break;
        }
        session.resolve_pending_save(receipt);
        ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
        ARPG_REQUIRE(session.snapshot().diagnostics.fault
            == (mutation == 0U
                ? dungeon::DungeonFault::save_commit_indeterminate
                : dungeon::DungeonFault::save_receipt_mismatch));
    }
    return {};
}

arpg::test::Failure death_overflows_fault_before_pending_publication() noexcept {
    for (std::uint8_t mutation = 0U; mutation < 3U; ++mutation) {
        auto state = unprotected_combat_state(0xD34DB0U + mutation);
        if (mutation == 0U) {
            state.commit_generation =
                (std::numeric_limits<std::uint64_t>::max)();
        } else if (mutation == 1U) {
            state.death_sequence =
                (std::numeric_limits<std::uint64_t>::max)();
        } else {
            state.current_room.index =
                (std::numeric_limits<std::uint64_t>::max)();
        }
        DungeonSession session{DungeonRules{}, std::move(state)};
        session.tick({});
        ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
        session.tick({});
        ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
        ARPG_REQUIRE(!session.pending_save().has_value());
        ARPG_REQUIRE(arpg::test::stable_state(session).death.lifecycle
            == DeathLifecycle::none);
    }
    return {};
}

arpg::test::Failure death_wins_same_tick_and_events_are_exactly_once() noexcept {
    DungeonSession session{
        DungeonRules{}, unprotected_combat_state(0xD34DC0U)};
    session.tick({});
    arpg::test::EventSummary ignored{};
    arpg::test::drain_all_events(session, ignored);
    arpg::test::force_defeat_current_wave(session);
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    ARPG_REQUIRE(session.snapshot().progression.experience == 0U);
    std::uint32_t detected = 0U;
    std::uint32_t cleared = 0U;
    while (const auto event = session.try_pop_event()) {
        if (event->kind == dungeon::DungeonEventKind::death_detected) ++detected;
        if (event->kind == dungeon::DungeonEventKind::room_cleared) ++cleared;
    }
    ARPG_REQUIRE(detected == 1U);
    ARPG_REQUIRE(cleared == 0U);
    const auto pending = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, pending.next_state,
        PendingSaveKind::death_retreat});
    std::uint32_t committed = 0U;
    while (const auto event = session.try_pop_event()) {
        if (event->kind
            == dungeon::DungeonEventKind::death_retreat_committed) ++committed;
    }
    ARPG_REQUIRE(committed == 1U);
    ARPG_REQUIRE(session.snapshot().pending_room_experience == 0U);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);
    return {};
}

arpg::test::Failure death_event_overflow_publishes_no_pending_save() noexcept {
    DungeonSession session{
        DungeonRules{}, unprotected_combat_state(0xD34DD0U)};
    session.tick({});
    arpg::test::EventSummary ignored{};
    arpg::test::drain_all_events(session, ignored);
    ARPG_REQUIRE(arpg::test::fill_dungeon_events(
        session, DungeonSession::kDungeonEventCapacity)
        == DungeonSession::kDungeonEventCapacity);
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::event_overflow);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(arpg::test::stable_state(session).death.lifecycle
        == DeathLifecycle::none);
    return {};
}

arpg::test::Failure first_death_reserves_detected_and_terminal_events() noexcept {
    DungeonSession session{
        DungeonRules{}, unprotected_combat_state(0xD34DE0U)};
    session.tick({});
    arpg::test::EventSummary ignored{};
    arpg::test::drain_all_events(session, ignored);
    ARPG_REQUIRE(arpg::test::fill_dungeon_events(
        session, DungeonSession::kDungeonEventCapacity - 1U)
        == DungeonSession::kDungeonEventCapacity - 1U);
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::event_overflow);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(arpg::test::stable_state(session).death.lifecycle
        == DeathLifecycle::none);
    return {};
}

arpg::test::Failure death_commit_consumes_reserved_terminal_slot() noexcept {
    DungeonSession session{
        DungeonRules{}, unprotected_combat_state(0xD34DE1U)};
    session.tick({});
    arpg::test::EventSummary ignored{};
    arpg::test::drain_all_events(session, ignored);
    ARPG_REQUIRE(arpg::test::fill_dungeon_events(
        session, DungeonSession::kDungeonEventCapacity - 2U)
        == DungeonSession::kDungeonEventCapacity - 2U);
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    const auto pending = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, pending.next_state,
        PendingSaveKind::death_retreat});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::death_pending);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::none);
    return {};
}

arpg::test::Failure death_not_committed_and_retry_reserve_terminal_slot() noexcept {
    DungeonSession session{
        DungeonRules{}, unprotected_combat_state(0xD34DE2U)};
    session.tick({});
    arpg::test::EventSummary ignored{};
    arpg::test::drain_all_events(session, ignored);
    ARPG_REQUIRE(arpg::test::fill_dungeon_events(
        session, DungeonSession::kDungeonEventCapacity - 2U)
        == DungeonSession::kDungeonEventCapacity - 2U);
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
    session.tick({});
    const auto first = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::not_committed, 0U, {},
        PendingSaveKind::death_retreat});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::none);
    ARPG_REQUIRE(!session.pending_save().has_value());

    arpg::test::drain_all_events(session, ignored);
    ARPG_REQUIRE(arpg::test::fill_dungeon_events(
        session, DungeonSession::kDungeonEventCapacity - 1U)
        == DungeonSession::kDungeonEventCapacity - 1U);
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    const auto retry = *session.pending_save();
    ARPG_REQUIRE(retry.expected_generation == first.expected_generation);
    ARPG_REQUIRE(dungeon::same_run_state(retry.next_state, first.next_state));
    session.resolve_pending_save({SaveDisposition::not_committed, 0U, {},
        PendingSaveKind::death_retreat});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::none);
    ARPG_REQUIRE(!session.pending_save().has_value());
    return {};
}

arpg::test::Failure death_continue_prepares_exact_target_without_reroll() noexcept {
    const auto persisted = pending_state(9U);
    DungeonSession session{DungeonRules{}, persisted};
    const auto stable_before = arpg::test::stable_state(session);

    ARPG_REQUIRE(session.request_death_continue() == RequestResult::accepted);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == PendingSaveKind::death_continue);
    ARPG_REQUIRE(pending->expected_generation
        == stable_before.commit_generation + 1U);
    ARPG_REQUIRE(pending->next_state.current_room.seed
        == stable_before.death.target_room.seed);
    ARPG_REQUIRE(pending->next_state.current_room.index
        == stable_before.death.target_room.index);
    ARPG_REQUIRE(pending->next_state.current_room.depth == 8U);
    ARPG_REQUIRE(pending->next_state.current_room.floor_room_index == 0U);
    ARPG_REQUIRE(pending->next_state.current_room.entry
        == checkpoint::EntrySide::initial);
    ARPG_REQUIRE(!pending->next_state.current_room.is_abyss);
    ARPG_REQUIRE(pending->next_state.current_room.seed != 0U);
    ARPG_REQUIRE(pending->next_state.current_room.seed
        != stable_before.current_room.seed);
    ARPG_REQUIRE(pending->next_state.death.lifecycle == DeathLifecycle::none);
    ARPG_REQUIRE(checkpoint::valid_death_checkpoint_structural(
        pending->next_state.death));
    ARPG_REQUIRE(pending->next_state.death_sequence
        == stable_before.death_sequence);
    ARPG_REQUIRE((pending->next_state.biases
        == std::array<std::uint32_t, 4>{}));
    ARPG_REQUIRE(pending->next_state.progression.level
        == stable_before.progression.level);
    ARPG_REQUIRE(pending->next_state.passive_tree.allocated_bits
        == stable_before.passive_tree.allocated_bits);
    ARPG_REQUIRE(pending->next_state.item_ownership.next_item_sequence
        == stable_before.item_ownership.next_item_sequence);
    ARPG_REQUIRE(pending->next_state.last_abyss_resolution.valid
        == stable_before.last_abyss_resolution.valid);
    auto expected = stable_before;
    ++expected.commit_generation;
    expected.current_room = stable_before.death.target_room;
    expected.abyss = {};
    expected.death = {};
    ARPG_REQUIRE(dungeon::same_run_state(pending->next_state, expected));
    ARPG_REQUIRE(dungeon::same_run_state(
        arpg::test::stable_state(session), stable_before));
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::committing);
    ARPG_REQUIRE(snapshot.death.has_value());
    ARPG_REQUIRE(snapshot.death->saving);
    ARPG_REQUIRE(!snapshot.death->can_continue);
    ARPG_REQUIRE(!snapshot.combat.has_value());
    return {};
}

arpg::test::Failure depth_one_continue_clamps_and_constructs_saved_room() noexcept {
    const auto persisted = pending_state(1U);
    DungeonSession session{DungeonRules{}, persisted};
    ARPG_REQUIRE(session.request_death_continue() == RequestResult::accepted);
    const auto pending = *session.pending_save();
    ARPG_REQUIRE(pending.next_state.current_room.depth == 1U);
    ARPG_REQUIRE(pending.next_state.current_room.seed
        == persisted.death.target_room.seed);
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, pending.next_state,
        PendingSaveKind::death_continue});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::transitioning);
    ARPG_REQUIRE(!session.snapshot().death.has_value());
    ARPG_REQUIRE(!session.snapshot().combat.has_value());
    session.tick({});
    const auto entered = session.snapshot();
    ARPG_REQUIRE(entered.phase == RoomPhase::locked);
    ARPG_REQUIRE(entered.combat.has_value());
    ARPG_REQUIRE(!entered.death.has_value());
    ARPG_REQUIRE(entered.room_seed == persisted.death.target_room.seed);
    ARPG_REQUIRE(entered.room_index == persisted.death.target_room.index);
    ARPG_REQUIRE(entered.depth == 1U);
    ARPG_REQUIRE(entered.floor_room_index == 0U);
    ARPG_REQUIRE(!entered.is_abyss);
    return {};
}

arpg::test::Failure death_continue_not_committed_retries_exactly() noexcept {
    const auto persisted = pending_state(12U);
    DungeonSession session{DungeonRules{}, persisted};
    ARPG_REQUIRE(session.request_death_continue() == RequestResult::accepted);
    const auto first = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::not_committed, 0U, {},
        PendingSaveKind::death_continue});
    auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::death_pending);
    ARPG_REQUIRE(snapshot.death.has_value());
    ARPG_REQUIRE(snapshot.death->can_continue);
    ARPG_REQUIRE(snapshot.death->continue_failed);
    ARPG_REQUIRE(snapshot.diagnostics.save_failure_count == 1U);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(dungeon::same_run_state(
        arpg::test::stable_state(session), persisted));
    DungeonSession restarted{DungeonRules{}, persisted};
    ARPG_REQUIRE(!restarted.snapshot().death->continue_failed);

    ARPG_REQUIRE(session.request_death_continue() == RequestResult::accepted);
    ARPG_REQUIRE(!session.snapshot().death->continue_failed);
    const auto second = *session.pending_save();
    ARPG_REQUIRE(second.kind == first.kind);
    ARPG_REQUIRE(second.expected_generation == first.expected_generation);
    ARPG_REQUIRE(dungeon::same_run_state(second.next_state, first.next_state));
    return {};
}

arpg::test::Failure death_continue_receipt_fault_matrix() noexcept {
    for (std::uint8_t mutation = 0U; mutation < 6U; ++mutation) {
        DungeonSession session{DungeonRules{}, pending_state(6U)};
        ARPG_REQUIRE(session.request_death_continue()
            == RequestResult::accepted);
        const auto pending = *session.pending_save();
        dungeon::PendingSaveResult receipt{SaveDisposition::committed,
            pending.expected_generation, pending.next_state,
            PendingSaveKind::death_continue};
        switch (mutation) {
        case 0U: receipt.disposition = SaveDisposition::indeterminate; break;
        case 1U: receipt.kind.reset(); break;
        case 2U: receipt.kind = PendingSaveKind::death_retreat; break;
        case 3U: ++receipt.generation; break;
        case 4U: ++receipt.verified_state.current_room.seed; break;
        case 5U: ++receipt.verified_state.death_sequence; break;
        }
        session.resolve_pending_save(receipt);
        ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
        ARPG_REQUIRE(session.snapshot().diagnostics.fault
            == (mutation == 0U
                ? dungeon::DungeonFault::save_commit_indeterminate
                : dungeon::DungeonFault::save_receipt_mismatch));
    }

    DungeonSession session{DungeonRules{}, pending_state(6U)};
    ARPG_REQUIRE(session.request_death_continue() == RequestResult::accepted);
    const auto pending = *session.pending_save();
    const dungeon::PendingSaveResult receipt{SaveDisposition::committed,
        pending.expected_generation, pending.next_state,
        PendingSaveKind::death_continue};
    session.resolve_pending_save(receipt);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::transitioning);
    session.resolve_pending_save(receipt);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    return {};
}

arpg::test::Failure death_continue_rejects_invalid_or_duplicate_requests() noexcept {
    DungeonSession live{DungeonRules{}, dungeon::make_initial_run_state(
        0xC071A0U, DungeonRules{}).state};
    ARPG_REQUIRE(live.request_death_continue() == RequestResult::rejected);
    ARPG_REQUIRE(!live.pending_save().has_value());

    DungeonSession pending{DungeonRules{}, pending_state()};
    ARPG_REQUIRE(pending.request_death_continue() == RequestResult::accepted);
    ARPG_REQUIRE(pending.request_death_continue() == RequestResult::rejected);

    auto invalid = pending_state();
    ++invalid.death.target_room.seed;
    DungeonSession faulted{DungeonRules{}, std::move(invalid)};
    ARPG_REQUIRE(faulted.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(faulted.request_death_continue() == RequestResult::faulted);
    ARPG_REQUIRE(!faulted.pending_save().has_value());

    auto overflow = pending_state();
    overflow.commit_generation =
        (std::numeric_limits<std::uint64_t>::max)();
    auto previous = overflow;
    --previous.commit_generation;
    --previous.death_sequence;
    previous.death = {};
    const auto target = dungeon::make_death_retreat_target(
        previous, overflow.death_sequence, DungeonRules{});
    ARPG_REQUIRE(target.fault == dungeon::DungeonFault::none);
    overflow.death.target_room = target.room;
    DungeonSession exhausted{DungeonRules{}, std::move(overflow)};
    ARPG_REQUIRE(exhausted.snapshot().phase == RoomPhase::death_pending);
    ARPG_REQUIRE(exhausted.request_death_continue() == RequestResult::faulted);
    ARPG_REQUIRE(exhausted.snapshot().diagnostics.fault
        == dungeon::DungeonFault::commit_generation_overflow);
    ARPG_REQUIRE(!exhausted.pending_save().has_value());
    return {};
}

arpg::test::Failure death_continue_events_and_capacity_are_atomic() noexcept {
    {
        DungeonSession session{DungeonRules{}, pending_state()};
        ARPG_REQUIRE(arpg::test::fill_dungeon_events(
            session, DungeonSession::kDungeonEventCapacity - 2U)
            == DungeonSession::kDungeonEventCapacity - 2U);
        ARPG_REQUIRE(session.request_death_continue() == RequestResult::accepted);
        const auto pending = *session.pending_save();
        session.resolve_pending_save({SaveDisposition::committed,
            pending.expected_generation, pending.next_state,
            PendingSaveKind::death_continue});
        std::uint32_t requested = 0U;
        std::uint32_t continued = 0U;
        while (const auto event = session.try_pop_event()) {
            if (event->kind
                    == dungeon::DungeonEventKind::death_continue_requested) {
                ++requested;
            }
            if (event->kind == dungeon::DungeonEventKind::death_continued) {
                ++continued;
            }
        }
        ARPG_REQUIRE(requested == 1U);
        ARPG_REQUIRE(continued == 1U);
    }
    {
        DungeonSession session{DungeonRules{}, pending_state()};
        ARPG_REQUIRE(arpg::test::fill_dungeon_events(
            session, DungeonSession::kDungeonEventCapacity - 2U)
            == DungeonSession::kDungeonEventCapacity - 2U);
        ARPG_REQUIRE(session.request_death_continue() == RequestResult::accepted);
        const auto first = *session.pending_save();
        session.resolve_pending_save({SaveDisposition::not_committed, 0U, {},
            PendingSaveKind::death_continue});
        ARPG_REQUIRE(session.snapshot().phase == RoomPhase::death_pending);
        ARPG_REQUIRE(session.snapshot().diagnostics.fault
            == dungeon::DungeonFault::none);
        std::uint32_t requested = 0U;
        std::uint32_t failed = 0U;
        while (const auto event = session.try_pop_event()) {
            if (event->kind
                    == dungeon::DungeonEventKind::death_continue_requested) {
                ++requested;
            }
            if (event->kind == dungeon::DungeonEventKind::save_failed) ++failed;
        }
        ARPG_REQUIRE(requested == 1U);
        ARPG_REQUIRE(failed == 1U);
        ARPG_REQUIRE(session.request_death_continue() == RequestResult::accepted);
        const auto retry = *session.pending_save();
        ARPG_REQUIRE(dungeon::same_run_state(retry.next_state, first.next_state));
        requested = 0U;
        while (const auto event = session.try_pop_event()) {
            if (event->kind
                    == dungeon::DungeonEventKind::death_continue_requested) {
                ++requested;
            }
        }
        ARPG_REQUIRE(requested == 1U);
    }
    for (std::size_t occupied = DungeonSession::kDungeonEventCapacity - 1U;
         occupied <= DungeonSession::kDungeonEventCapacity; ++occupied) {
        DungeonSession session{DungeonRules{}, pending_state()};
        const auto stable_before = arpg::test::stable_state(session);
        ARPG_REQUIRE(arpg::test::fill_dungeon_events(session, occupied)
            == occupied);
        ARPG_REQUIRE(session.request_death_continue() == RequestResult::faulted);
        ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
        ARPG_REQUIRE(session.snapshot().diagnostics.fault
            == dungeon::DungeonFault::event_overflow);
        ARPG_REQUIRE(!session.pending_save().has_value());
        ARPG_REQUIRE(dungeon::same_run_state(
            arpg::test::stable_state(session), stable_before));
    }
    return {};
}

arpg::test::Failure abyss_continue_clears_active_abyss_for_next_real_death() noexcept {
    DungeonSession session{DungeonRules{}, available_abyss_state()};
    checkpoint::DungeonRunState started{};
    ARPG_REQUIRE(drive_started_abyss_death(session, started));
    const auto first_retreat = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::committed,
        first_retreat.expected_generation, first_retreat.next_state,
        PendingSaveKind::death_retreat});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::death_pending);
    const auto history = first_retreat.next_state.last_abyss_resolution;
    ARPG_REQUIRE(history.valid);

    ARPG_REQUIRE(session.request_death_continue() == RequestResult::accepted);
    const auto continued = *session.pending_save();
    ARPG_REQUIRE(continued.kind == PendingSaveKind::death_continue);
    ARPG_REQUIRE(continued.next_state.abyss.lifecycle
        == abyss::AbyssLifecycle::none);
    ARPG_REQUIRE(continued.next_state.abyss.rule == abyss::AbyssRuleId::none);
    ARPG_REQUIRE(continued.next_state.abyss.reward_total == 0U);
    ARPG_REQUIRE(continued.next_state.abyss.generated_mask == 0U);
    ARPG_REQUIRE(continued.next_state.abyss.claimed_mask == 0U);
    ARPG_REQUIRE(continued.next_state.abyss.abandoned_mask == 0U);
    ARPG_REQUIRE(continued.next_state.last_abyss_resolution.valid);
    ARPG_REQUIRE(continued.next_state.last_abyss_resolution.room_seed
        == history.room_seed);
    ARPG_REQUIRE(continued.next_state.last_abyss_resolution.rule
        == history.rule);
    ARPG_REQUIRE(continued.next_state.last_abyss_resolution.total
        == history.total);
    ARPG_REQUIRE(continued.next_state.last_abyss_resolution.abandoned
        == history.abandoned);

    session.resolve_pending_save({SaveDisposition::committed,
        continued.expected_generation, continued.next_state,
        PendingSaveKind::death_continue});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::transitioning);
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::locked);
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
    session.tick({});
    const auto second_retreat = *session.pending_save();
    ARPG_REQUIRE(second_retreat.kind == PendingSaveKind::death_retreat);
    ARPG_REQUIRE(!second_retreat.next_state.death.death_was_abyss);
    ARPG_REQUIRE(second_retreat.next_state.last_abyss_resolution.valid);
    ARPG_REQUIRE(second_retreat.next_state.last_abyss_resolution.room_seed
        == history.room_seed);
    ARPG_REQUIRE(second_retreat.next_state.last_abyss_resolution.rule
        == history.rule);
    ARPG_REQUIRE(second_retreat.next_state.last_abyss_resolution.total
        == history.total);
    ARPG_REQUIRE(second_retreat.next_state.last_abyss_resolution.abandoned
        == history.abandoned);
    session.resolve_pending_save({SaveDisposition::committed,
        second_retreat.expected_generation, second_retreat.next_state,
        PendingSaveKind::death_retreat});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::death_pending);
    DungeonSession reloaded{DungeonRules{}, second_retreat.next_state};
    ARPG_REQUIRE(reloaded.snapshot().phase == RoomPhase::death_pending);
    ARPG_REQUIRE(reloaded.snapshot().death.has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"ordinary pending load is read only", &ordinary_pending_load_is_read_only},
    {"depth one and abyss pending loads are valid", &depth_one_and_abyss_pending_loads_are_valid},
    {"none lifecycle constructs existing room", &none_lifecycle_constructs_the_existing_room},
    {"target regeneration mutations fail closed", &target_regeneration_mutations_fail_closed},
    {"sequence generation and anchor mutations fail closed", &sequence_generation_and_anchor_mutations_fail_closed},
    {"first generation cannot claim committed death", &first_generation_cannot_claim_a_committed_death},
    {"source catalog accepts canonical ids", &source_catalog_accepts_only_canonical_ids},
    {"abyss failure relationships are defended", &abyss_failure_relationships_are_defended},
    {"ordinary death preserves historical resolution", &ordinary_death_preserves_valid_historical_resolution},
    {"death pending rejects gameplay requests", &death_pending_rejects_every_gameplay_request},
    {"ordinary death prepares atomic retreat", &ordinary_death_prepares_atomic_retreat},
    {"abyss death prepares one atomic retreat", &abyss_death_prepares_one_atomic_retreat},
    {"abyss death retry and reload preserve exact state", &abyss_death_retry_and_reload_preserve_exact_state},
    {"abyss death receipt mutations fail closed", &abyss_death_receipt_mutations_fail_closed},
    {"ordinary death receipts bind kind", &ordinary_death_receipts_are_bound_to_kind},
    {"ordinary death commits and duplicate faults", &ordinary_death_commits_then_duplicate_faults},
    {"ordinary death not committed retries identically", &ordinary_death_not_committed_retries_identically},
    {"ordinary death receipt fault matrix", &ordinary_death_receipt_fault_matrix},
    {"death overflows fault before pending", &death_overflows_fault_before_pending_publication},
    {"death wins same tick and events exactly once", &death_wins_same_tick_and_events_are_exactly_once},
    {"death event overflow publishes no pending", &death_event_overflow_publishes_no_pending_save},
    {"first death reserves two event slots", &first_death_reserves_detected_and_terminal_events},
    {"death commit consumes reserved slot", &death_commit_consumes_reserved_terminal_slot},
    {"death retry reserves terminal slot", &death_not_committed_and_retry_reserve_terminal_slot},
    {"death continue prepares exact target", &death_continue_prepares_exact_target_without_reroll},
    {"depth one continue constructs saved room", &depth_one_continue_clamps_and_constructs_saved_room},
    {"death continue retry is exact", &death_continue_not_committed_retries_exactly},
    {"death continue receipt fault matrix", &death_continue_receipt_fault_matrix},
    {"death continue rejects invalid requests", &death_continue_rejects_invalid_or_duplicate_requests},
    {"death continue events and capacity are atomic", &death_continue_events_and_capacity_are_atomic},
    {"abyss continue permits next real death", &abyss_continue_clears_active_abyss_for_next_real_death},
};

}  // namespace

arpg::test::TestSuite dungeon_death_lifecycle_suite() noexcept {
    return arpg::test::make_suite("dungeon_death_lifecycle", kCases);
}
