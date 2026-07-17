#include "test_framework.hpp"

#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "combat/monster_affix_types.hpp"
#include "dungeon/death_checkpoint.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_generation.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace {

namespace abyss = arpg::abyss;
namespace checkpoint = arpg::dungeon::checkpoint;
namespace combat = arpg::combat;
namespace dungeon = arpg::dungeon;

using checkpoint::DeathLifecycle;
using checkpoint::DeathSourceKind;
using dungeon::DungeonRules;
using dungeon::DungeonSession;
using dungeon::DungeonSnapshot;
using dungeon::RequestResult;
using dungeon::RoomPhase;

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
            total, 0U, 0U, total};
    }

    ++state.commit_generation;
    ++state.death_sequence;
    state.current_room.is_abyss = false;
    state.last_transition = checkpoint::TransitionKind::death_retreat;
    state.last_direction = checkpoint::ExitDirection::none;
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
    for (std::uint8_t mutation = 0U; mutation < 8U; ++mutation) {
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
};

}  // namespace

arpg::test::TestSuite dungeon_death_lifecycle_suite() noexcept {
    return arpg::test::make_suite("dungeon_death_lifecycle", kCases);
}
