#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/room_generation.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "abyss/abyss_rules.hpp"
#include "items/item_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

bool all_exits_are(
    const arpg::dungeon::DungeonSnapshot& state,
    bool expected) noexcept {
    for (const bool open : state.exits_open) {
        if (open != expected) {
            return false;
        }
    }
    return true;
}

arpg::dungeon::DungeonRunState lifecycle_available_state(
    std::uint64_t seed = 1U) noexcept {
    auto state = arpg::dungeon::make_initial_run_state(
        0xAB155EEDULL, arpg::dungeon::DungeonRules{}).state;
    while (!arpg::abyss::is_abyss_roll(seed)) ++seed;
    state.current_room.seed = seed;
    state.current_room.depth = 40U;
    state.current_room.entry = arpg::dungeon::EntrySide::left;
    state.current_room.ecology = arpg::dungeon::DungeonElement::water;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = true;
    state.last_transition = arpg::dungeon::TransitionKind::door;
    state.last_direction = arpg::dungeon::ExitDirection::right;
    const auto selected = arpg::abyss::select_abyss_rule(seed, 40U);
    if (selected.has_value()) {
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::available;
        state.abyss.danger = selected->danger;
        state.abyss.rule = selected->rule;
        state.abyss.rules_version = selected->rules_version;
    }
    return state;
}

bool commit_abyss_start(arpg::dungeon::DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()
            || pending->kind
                != arpg::dungeon::PendingSaveKind::abyss_start) return false;
    session.resolve_pending_save({arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation, pending->next_state});
    return session.snapshot().phase == arpg::dungeon::RoomPhase::locked
        && session.snapshot().combat.has_value();
}

arpg::dungeon::DungeonRunState lifecycle_state_for_rule(
    arpg::abyss::AbyssRuleId rule) noexcept {
    std::uint64_t seed = 1U;
    for (;;) {
        auto state = lifecycle_available_state(seed);
        if (state.abyss.rule == rule) return state;
        seed = state.current_room.seed + 1U;
    }
}

bool drive_to_abyss_clear_pending(
    arpg::dungeon::DungeonSession& session,
    arpg::test::EventSummary& events) noexcept {
    if (session.snapshot().phase == arpg::dungeon::RoomPhase::locked) {
        session.tick({});
        arpg::test::drain_all_events(session, events);
    }
    for (int tick = 0; tick < 4096; ++tick) {
        const auto state = session.snapshot();
        if (state.phase == arpg::dungeon::RoomPhase::committing) {
            const auto pending = session.pending_save();
            return pending.has_value()
                && pending->kind
                    == arpg::dungeon::PendingSaveKind::abyss_clear;
        }
        if (state.phase == arpg::dungeon::RoomPhase::combat) {
            arpg::test::force_defeat_current_wave(session);
        }
        session.tick({});
        arpg::test::drain_all_events(session, events);
    }
    return false;
}

bool drive_to_final_abyss_wave(
    arpg::dungeon::DungeonSession& session) noexcept {
    arpg::test::EventSummary ignored{};
    if (session.snapshot().phase == arpg::dungeon::RoomPhase::locked) {
        session.tick({});
        arpg::test::drain_all_events(session, ignored);
    }
    for (int tick = 0; tick < 4096; ++tick) {
        const auto state = session.snapshot();
        if (state.phase == arpg::dungeon::RoomPhase::combat
                && state.wave_index + 1U == state.wave_count) {
            return true;
        }
        if (state.phase == arpg::dungeon::RoomPhase::combat) {
            arpg::test::force_defeat_current_wave(session);
        }
        session.tick({});
        arpg::test::drain_all_events(session, ignored);
    }
    return false;
}

std::size_t environment_hazard_count(
    const arpg::combat::CombatSnapshot& state) noexcept {
    std::size_t count = 0U;
    for (const auto& hazard : state.hazards) {
        if (hazard.active
                && hazard.source
                    == arpg::combat::HazardSource::abyss_environment) {
            ++count;
        }
    }
    return count;
}

arpg::test::Failure abyss_clear_is_atomic_and_restores_life_resources() noexcept {
    using namespace arpg;
    dungeon::DungeonRunState state = lifecycle_state_for_rule(
        abyss::AbyssRuleId::life_sacrifice);
    items::ItemInstance barrier_item{};
    barrier_item.id = 0xBABB1EULL;
    barrier_item.base_id = 3U;
    barrier_item.rarity = items::ItemRarity::normal;
    barrier_item.item_level = 40U;
    barrier_item.required_level = 1U;
    state.item_ownership.items.push_back(barrier_item);
    state.item_ownership.equipment.equipped_ids[2] = barrier_item.id;
    const auto stable_before = state;

    dungeon::DungeonSession session{{}, state};
    ARPG_REQUIRE(commit_abyss_start(session));
    session.tick({});
    while (session.try_pop_event().has_value()) {
    }
    const auto entered = session.snapshot();
    ARPG_REQUIRE(entered.combat->player.max_hp < 1000);
    ARPG_REQUIRE(entered.combat->player.max_barrier > 0);
    test::damage_current_player(session,
        entered.combat->player.max_barrier + 100);
    const auto damaged = session.snapshot();
    ARPG_REQUIRE(damaged.combat->player.hp < damaged.combat->player.max_hp);
    ARPG_REQUIRE(damaged.combat->player.barrier == 0);

    test::EventSummary events{};
    ARPG_REQUIRE(drive_to_abyss_clear_pending(session, events));
    const auto frozen = session.snapshot();
    const auto pending = session.pending_save();
    ARPG_REQUIRE(frozen.phase == dungeon::RoomPhase::committing);
    ARPG_REQUIRE(frozen.pending_save_kind == dungeon::PendingSaveKind::abyss_clear);
    ARPG_REQUIRE(frozen.has_hole);
    ARPG_REQUIRE(all_exits_are(frozen, false));
    ARPG_REQUIRE(!session.request_descent(true));
    ARPG_REQUIRE(!session.queue_action(combat::Action::light));
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->next_state.current_room.is_abyss);
    ARPG_REQUIRE(pending->next_state.current_room.has_hole);
    ARPG_REQUIRE(pending->next_state.abyss.lifecycle
        == abyss::AbyssLifecycle::cleared);
    ARPG_REQUIRE(pending->next_state.abyss.rule == stable_before.abyss.rule);
    ARPG_REQUIRE(pending->next_state.abyss.danger == stable_before.abyss.danger);
    ARPG_REQUIRE(pending->next_state.abyss.rules_version
        == stable_before.abyss.rules_version);
    ARPG_REQUIRE(pending->next_state.abyss.generated_mask == 0U);
    ARPG_REQUIRE(pending->next_state.abyss.claimed_mask == 0U);
    ARPG_REQUIRE(pending->next_state.abyss.abandoned_mask == 0U);
    ARPG_REQUIRE(pending->next_state.abyss.reward_revision == 0U);
    ARPG_REQUIRE(pending->next_state.item_ownership.next_item_sequence
        == stable_before.item_ownership.next_item_sequence);
    ARPG_REQUIRE(pending->next_state.item_ownership.items.size()
        == stable_before.item_ownership.items.size());
    ARPG_REQUIRE(events.room_cleared_count == 0U);
    ARPG_REQUIRE(events.exits_opened_count == 0U);
    ARPG_REQUIRE(test::stable_state(session).progression.experience
        == stable_before.progression.experience);
    ARPG_REQUIRE(frozen.pending_room_experience > 0U);
    const auto* active_rule = test::DungeonSessionTestAccess::active_abyss_config(
        session);
    ARPG_REQUIRE(active_rule != nullptr);
    ARPG_REQUIRE(active_rule->rule == abyss::AbyssRuleId::life_sacrifice);

    const int frozen_hp = frozen.combat->player.hp;
    const int frozen_max_hp = frozen.combat->player.max_hp;
    const int frozen_barrier = frozen.combat->player.barrier;
    const auto expected_hp = abyss::map_resource_ratio(
        frozen_hp, frozen_max_hp, 1000, frozen_hp > 0);
    ARPG_REQUIRE(expected_hp.has_value());
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending->expected_generation, pending->next_state});

    const auto cleared = session.snapshot();
    ARPG_REQUIRE(cleared.phase == dungeon::RoomPhase::cleared);
    ARPG_REQUIRE(cleared.is_abyss);
    ARPG_REQUIRE(cleared.has_hole);
    ARPG_REQUIRE(all_exits_are(cleared, true));
    ARPG_REQUIRE(cleared.combat->player.max_hp == 1000);
    ARPG_REQUIRE(cleared.combat->player.hp == *expected_hp);
    ARPG_REQUIRE(cleared.combat->player.barrier == frozen_barrier);
    ARPG_REQUIRE(cleared.combat->player.barrier
        < cleared.combat->player.max_barrier);
    ARPG_REQUIRE(cleared.pending_room_experience == 0U);
    ARPG_REQUIRE(cleared.last_room_experience > 0U);
    ARPG_REQUIRE(cleared.progression.experience
        == pending->next_state.progression.experience);
    ARPG_REQUIRE(test::stable_state(session).abyss.lifecycle
        == abyss::AbyssLifecycle::cleared);
    const auto* cleared_rule = test::DungeonSessionTestAccess::active_abyss_config(
        session);
    ARPG_REQUIRE(cleared_rule != nullptr);
    ARPG_REQUIRE(cleared_rule->rule == abyss::AbyssRuleId::none);
    ARPG_REQUIRE(cleared_rule->monster_damage_bp == 10000U);
    ARPG_REQUIRE(cleared_rule->monster_armor_bp == 10000U);
    test::drain_all_events(session, events);
    ARPG_REQUIRE(events.room_cleared_count == 1U);
    ARPG_REQUIRE(events.exits_opened_count == 1U);
    ARPG_REQUIRE(events.dungeon_count >= 2U);
    ARPG_REQUIRE(events.dungeon_kinds[events.dungeon_count - 2U]
        == dungeon::DungeonEventKind::room_cleared);
    ARPG_REQUIRE(events.dungeon_kinds[events.dungeon_count - 1U]
        == dungeon::DungeonEventKind::exits_opened);
    session.tick({});
    if (session.snapshot().phase == dungeon::RoomPhase::committing) {
        ARPG_REQUIRE(session.pending_save()->kind
            == dungeon::PendingSaveKind::abyss_reward_materialized);
        ARPG_REQUIRE(test::commit_pending(session));
    }
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::awaiting_exit);
    return {};
}

arpg::test::Failure abyss_clear_removes_environment_hazard_after_commit() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session{{}, lifecycle_state_for_rule(
        abyss::AbyssRuleId::chaos_expansion)};
    ARPG_REQUIRE(commit_abyss_start(session));
    session.tick({});
    session.tick({});
    ARPG_REQUIRE(environment_hazard_count(*session.snapshot().combat) > 0U);

    test::EventSummary events{};
    ARPG_REQUIRE(drive_to_abyss_clear_pending(session, events));
    ARPG_REQUIRE(environment_hazard_count(*session.snapshot().combat) > 0U);
    const auto pending = *session.pending_save();
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending.expected_generation, pending.next_state});
    ARPG_REQUIRE(environment_hazard_count(*session.snapshot().combat) == 0U);
    ARPG_REQUIRE(test::DungeonSessionTestAccess::active_abyss_config(session)
        ->rule == abyss::AbyssRuleId::none);
    return {};
}

arpg::test::Failure abyss_clear_requires_two_reserved_event_slots() noexcept {
    using namespace arpg;
    for (const std::size_t free_slots : {std::size_t{0U}, std::size_t{1U}}) {
        dungeon::DungeonSession session{{}, lifecycle_state_for_rule(
            abyss::AbyssRuleId::chaos_expansion)};
        ARPG_REQUIRE(commit_abyss_start(session));
        session.tick({});
        ARPG_REQUIRE(drive_to_final_abyss_wave(session));
        const auto before = session.snapshot();
        ARPG_REQUIRE(environment_hazard_count(*before.combat) > 0U);
        test::force_defeat_current_wave(session);
        ARPG_REQUIRE(test::fill_dungeon_events(session,
            dungeon::DungeonSession::kDungeonEventCapacity - free_slots)
            == dungeon::DungeonSession::kDungeonEventCapacity - free_slots);

        session.tick({});
        const auto faulted = session.snapshot();
        ARPG_REQUIRE(faulted.phase == dungeon::RoomPhase::faulted);
        ARPG_REQUIRE(faulted.diagnostics.fault
            == dungeon::DungeonFault::event_overflow);
        ARPG_REQUIRE(!session.pending_save().has_value());
        ARPG_REQUIRE(test::stable_state(session).abyss.lifecycle
            == abyss::AbyssLifecycle::started);
        ARPG_REQUIRE(test::stable_state(session).progression.experience
            == before.progression.experience);
        ARPG_REQUIRE(faulted.pending_room_experience > 0U);
        ARPG_REQUIRE(environment_hazard_count(*faulted.combat)
            == environment_hazard_count(*before.combat));
        ARPG_REQUIRE(test::DungeonSessionTestAccess::active_abyss_config(session)
            ->rule == abyss::AbyssRuleId::chaos_expansion);
        ARPG_REQUIRE(all_exits_are(faulted, false));
    }
    return {};
}

arpg::test::Failure abyss_clear_exactly_two_slots_publish_both_events() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session{{}, lifecycle_state_for_rule(
        abyss::AbyssRuleId::chaos_expansion)};
    ARPG_REQUIRE(commit_abyss_start(session));
    session.tick({});
    ARPG_REQUIRE(drive_to_final_abyss_wave(session));
    test::force_defeat_current_wave(session);
    constexpr std::size_t kReservedSlots = 2U;
    ARPG_REQUIRE(test::fill_dungeon_events(session,
        dungeon::DungeonSession::kDungeonEventCapacity - kReservedSlots)
        == dungeon::DungeonSession::kDungeonEventCapacity - kReservedSlots);

    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::committing);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::abyss_clear);
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending->expected_generation, pending->next_state});

    const auto cleared = session.snapshot();
    ARPG_REQUIRE(cleared.phase == dungeon::RoomPhase::cleared);
    ARPG_REQUIRE(cleared.diagnostics.fault == dungeon::DungeonFault::none);
    test::EventSummary events{};
    test::drain_all_events(session, events);
    ARPG_REQUIRE(events.room_cleared_count == 1U);
    ARPG_REQUIRE(events.exits_opened_count == 1U);
    ARPG_REQUIRE(events.dungeon_count
        == dungeon::DungeonSession::kDungeonEventCapacity);
    ARPG_REQUIRE(events.dungeon_kinds[events.dungeon_count - 2U]
        == dungeon::DungeonEventKind::room_cleared);
    ARPG_REQUIRE(events.dungeon_kinds[events.dungeon_count - 1U]
        == dungeon::DungeonEventKind::exits_opened);
    return {};
}

arpg::test::Failure abyss_reset_queues_fail_and_rejects_reentry() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session{{}, lifecycle_available_state()};
    ARPG_REQUIRE(commit_abyss_start(session));
    const auto before = session.snapshot();
    ARPG_REQUIRE(session.reset_current_room()
        == dungeon::RequestResult::accepted);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::abyss_fail);
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::committing);
    ARPG_REQUIRE(session.snapshot().combat.has_value());
    ARPG_REQUIRE(pending->next_state.current_room.seed == before.room_seed);
    ARPG_REQUIRE(pending->next_state.current_room.ecology == before.ecology);
    ARPG_REQUIRE(pending->next_state.current_room.has_hole == before.has_hole);
    ARPG_REQUIRE(!pending->next_state.current_room.is_abyss);
    ARPG_REQUIRE(pending->next_state.abyss.lifecycle
        == abyss::AbyssLifecycle::failed);
    ARPG_REQUIRE(session.reset_current_room()
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(session.pending_save()->expected_generation
        == pending->expected_generation);
    return {};
}

arpg::test::Failure abyss_fail_not_committed_faults() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session{{}, lifecycle_available_state()};
    ARPG_REQUIRE(commit_abyss_start(session));
    ARPG_REQUIRE(session.reset_current_room()
        == dungeon::RequestResult::accepted);
    session.resolve_pending_save({dungeon::SaveDisposition::not_committed, 0U, {}});
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::faulted);
    return {};
}

arpg::test::Failure abyss_fail_receipt_mismatch_faults() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session{{}, lifecycle_available_state()};
    ARPG_REQUIRE(commit_abyss_start(session));
    ARPG_REQUIRE(session.reset_current_room()
        == dungeon::RequestResult::accepted);
    const auto pending = *session.pending_save();
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending.expected_generation + 1U, pending.next_state});
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure abyss_fail_commit_rebuilds_same_normal_room() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session{{}, lifecycle_available_state()};
    ARPG_REQUIRE(commit_abyss_start(session));
    const auto before = session.snapshot();
    ARPG_REQUIRE(session.reset_current_room()
        == dungeon::RequestResult::accepted);
    const auto pending = *session.pending_save();
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending.expected_generation, pending.next_state});
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.phase == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(after.combat.has_value());
    ARPG_REQUIRE(after.room_seed == before.room_seed);
    ARPG_REQUIRE(after.ecology == before.ecology);
    ARPG_REQUIRE(after.has_hole == before.has_hole);
    ARPG_REQUIRE(!after.is_abyss);
    ARPG_REQUIRE(test::DungeonSessionTestAccess::stable_state(session)
        .abyss.lifecycle == abyss::AbyssLifecycle::failed);
    const auto expected = dungeon::build_encounter_plan(after.room_seed,
        after.depth, after.ecology, dungeon::DungeonRules{}.encounter);
    ARPG_REQUIRE(after.encounter.total_budget == expected.plan.total_budget);
    return {};
}

arpg::test::Failure ordinary_reset_returns_accepted() noexcept {
    arpg::dungeon::DungeonSession session;
    ARPG_REQUIRE(session.reset_current_room()
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(session.snapshot().phase == arpg::dungeon::RoomPhase::locked);
    return {};
}

arpg::test::Failure ordinary_player_defeat_uses_normal_reset() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    session.tick({});
    while (session.try_pop_event().has_value()) {
    }
    test::DungeonSessionTestAccess::damage_current_player(
        session, session.snapshot().combat->player.max_hp);
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(session.snapshot().combat->player.hp
        == session.snapshot().combat->player.max_hp);
    ARPG_REQUIRE(!session.snapshot().is_abyss);
    ARPG_REQUIRE(!session.pending_save().has_value());
    return {};
}

arpg::test::Failure abyss_player_defeat_queues_one_fail() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session{{}, lifecycle_available_state()};
    ARPG_REQUIRE(commit_abyss_start(session));
    session.tick({});
    while (session.try_pop_event().has_value()) {
    }
    while (session.try_pop_combat_event().has_value()) {
    }
    test::DungeonSessionTestAccess::damage_current_player(
        session, session.snapshot().combat->player.max_hp);
    session.tick({});
    const auto first = session.pending_save();
    ARPG_REQUIRE(first.has_value());
    ARPG_REQUIRE(first->kind == dungeon::PendingSaveKind::abyss_fail);
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::committing);
    ARPG_REQUIRE(session.reset_current_room() == dungeon::RequestResult::rejected);
    session.tick({});
    ARPG_REQUIRE(session.pending_save()->expected_generation
        == first->expected_generation);
    std::uint32_t defeated = 0U;
    while (const auto event = session.try_pop_combat_event()) {
        if (event->kind == combat::CombatEventKind::player_defeated) ++defeated;
    }
    ARPG_REQUIRE(defeated == 1U);
    return {};
}

arpg::test::Failure abyss_player_defeat_does_not_depend_on_event_delivery() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session{{}, lifecycle_available_state()};
    ARPG_REQUIRE(commit_abyss_start(session));
    session.tick({});
    while (session.try_pop_event().has_value()) {
    }
    while (session.try_pop_combat_event().has_value()) {
    }
    test::DungeonSessionTestAccess::fill_current_combat_events(session, 62U);
    test::DungeonSessionTestAccess::damage_current_player(
        session, session.snapshot().combat->player.max_hp);
    ARPG_REQUIRE(session.snapshot().combat->player.hp == 0);
    session.tick({});
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::abyss_fail);
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::committing);
    std::uint32_t defeat_events = 0U;
    while (const auto event = session.try_pop_combat_event()) {
        if (event->kind == combat::CombatEventKind::player_defeated) {
            ++defeat_events;
        }
    }
    ARPG_REQUIRE(defeat_events == 0U);
    return {};
}

arpg::test::Failure relay_overflow_fault_precedes_durable_defeat() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session{{}, lifecycle_available_state()};
    ARPG_REQUIRE(commit_abyss_start(session));
    session.tick({});
    while (session.try_pop_event().has_value()) {
    }
    test::DungeonSessionTestAccess::damage_current_player(
        session, session.snapshot().combat->player.max_hp);
    test::DungeonSessionTestAccess::force_fault(
        session, dungeon::DungeonFault::combat_relay_overflow);
    test::DungeonSessionTestAccess::handle_player_defeat(session);
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::combat_relay_overflow);
    ARPG_REQUIRE(!session.pending_save().has_value());
    return {};
}

arpg::test::Failure invalid_available_checkpoint_faults() noexcept {
    auto invalid = lifecycle_available_state();
    invalid.abyss.rule = invalid.abyss.rule == arpg::abyss::AbyssRuleId::thunderstorm
        ? arpg::abyss::AbyssRuleId::swift_pursuit
        : arpg::abyss::AbyssRuleId::thunderstorm;
    arpg::dungeon::DungeonSession session{{}, invalid};
    ARPG_REQUIRE(session.snapshot().phase == arpg::dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::invalid_abyss_state);
    ARPG_REQUIRE(!session.snapshot().combat.has_value());
    return {};
}

arpg::test::Failure impossible_available_abyss_origin_faults_before_start() noexcept {
    using namespace arpg;
    for (const auto transition : std::array<dungeon::TransitionKind, 2U>{{
             dungeon::TransitionKind::none,
             dungeon::TransitionKind::descent}}) {
        auto state = lifecycle_available_state();
        state.current_room.entry = dungeon::EntrySide::initial;
        state.last_transition = transition;
        state.last_direction = dungeon::ExitDirection::none;
        dungeon::DungeonSession session{{}, state};
        ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::faulted);
        ARPG_REQUIRE(session.snapshot().diagnostics.fault
            == dungeon::DungeonFault::invalid_abyss_state);
        ARPG_REQUIRE(!session.pending_save().has_value());
    }
    return {};
}

arpg::test::Failure deep_abyss_snapshot_uses_expanded_budget_legality() noexcept {
    using namespace arpg;
    auto state = lifecycle_available_state();
    state.current_room.depth = 1000U;
    const auto selected = abyss::select_abyss_rule(
        state.current_room.seed, state.current_room.depth);
    ARPG_REQUIRE(selected.has_value());
    state.abyss.danger = selected->danger;
    state.abyss.rule = selected->rule;
    state.abyss.rules_version = selected->rules_version;
    dungeon::DungeonSession session{{}, state};
    ARPG_REQUIRE(commit_abyss_start(session));
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.encounter.total_budget
        > dungeon::DungeonRules{}.encounter.max_budget);
    ARPG_REQUIRE(snapshot.encounter.plan_valid);
    return {};
}

arpg::test::Failure construction_and_first_tick_are_staged() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;

    const dungeon::DungeonSnapshot constructed = session.snapshot();
    ARPG_REQUIRE(constructed.phase == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(constructed.has_active_room);
    ARPG_REQUIRE(constructed.combat.has_value());
    ARPG_REQUIRE(constructed.combat->tick == 0U);
    ARPG_REQUIRE(constructed.remaining_targets > 0U);
    ARPG_REQUIRE(constructed.remaining_targets
        == constructed.combat->monster_count);
    ARPG_REQUIRE(all_exits_are(constructed, false));

    session.tick(combat::MovementInput{});
    const dungeon::DungeonSnapshot started = session.snapshot();
    ARPG_REQUIRE(started.phase == dungeon::RoomPhase::combat);
    ARPG_REQUIRE(started.session_tick == 1U);
    ARPG_REQUIRE(started.combat.has_value());
    ARPG_REQUIRE(started.combat->tick == 0U);

    const auto entered = session.try_pop_event();
    ARPG_REQUIRE(entered.has_value());
    ARPG_REQUIRE(entered->kind == dungeon::DungeonEventKind::room_entered);
    ARPG_REQUIRE(entered->session_tick == 0U);
    const auto started_event = session.try_pop_event();
    ARPG_REQUIRE(started_event.has_value());
    ARPG_REQUIRE(started_event->kind
        == dungeon::DungeonEventKind::combat_started);
    ARPG_REQUIRE(started_event->session_tick == 0U);
    ARPG_REQUIRE(!session.try_pop_event().has_value());
    ARPG_REQUIRE(!session.try_pop_combat_event().has_value());
    return {};
}

arpg::test::Failure closed_doors_ignore_pre_clear_contact() noexcept {
    using namespace arpg;
    constexpr std::array<combat::MovementInput, 4> kOutwardMovements{{
        {0, -1},
        {0, 1},
        {-1, 0},
        {1, 0},
    }};

    for (const combat::MovementInput movement : kOutwardMovements) {
        dungeon::DungeonSession session;
        session.tick(combat::MovementInput{});
        for (int tick = 0; tick < 128; ++tick) {
            session.tick(movement);
        }
        const dungeon::DungeonSnapshot state = session.snapshot();
        ARPG_REQUIRE(state.phase == dungeon::RoomPhase::combat);
        ARPG_REQUIRE(state.room_index == 0U);
        ARPG_REQUIRE(state.remaining_targets > 0U);
        ARPG_REQUIRE(state.remaining_targets == state.combat->monster_count);
        ARPG_REQUIRE(all_exits_are(state, false));
        ARPG_REQUIRE(state.diagnostics.rejected_exit_count == 0U);
    }
    return {};
}

arpg::test::Failure real_combat_clears_once_without_respawn() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    test::EventSummary events;
    const std::uint8_t initial_targets = session.snapshot().remaining_targets;

    session.tick(combat::MovementInput{});
    test::drain_all_events(session, events);
    ARPG_REQUIRE(test::drive_until_cleared(session, events));

    const dungeon::DungeonSnapshot cleared = session.snapshot();
    ARPG_REQUIRE(cleared.phase == dungeon::RoomPhase::cleared);
    ARPG_REQUIRE(cleared.remaining_targets == 0U);
    ARPG_REQUIRE(all_exits_are(cleared, true));
    ARPG_REQUIRE(events.room_cleared_count == 1U);
    ARPG_REQUIRE(events.exits_opened_count == 1U);
    ARPG_REQUIRE(events.defeated_count == initial_targets);
    ARPG_REQUIRE(events.dungeon_count >= 2U);
    ARPG_REQUIRE(events.dungeon_kinds[events.dungeon_count - 2U]
        == dungeon::DungeonEventKind::room_cleared);
    ARPG_REQUIRE(events.dungeon_kinds[events.dungeon_count - 1U]
        == dungeon::DungeonEventKind::exits_opened);

    const std::uint64_t cleared_session_tick = cleared.session_tick;
    const std::uint64_t combat_tick = cleared.combat->tick;
    const float player_x = cleared.combat->player.position.x;
    session.tick(combat::MovementInput{-1, 0});
    test::drain_all_events(session, events);
    const dungeon::DungeonSnapshot transitioned = session.snapshot();
    ARPG_REQUIRE(transitioned.phase == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(transitioned.session_tick == cleared_session_tick + 1U);
    ARPG_REQUIRE(transitioned.combat->tick == cleared.combat->tick);
    ARPG_REQUIRE(transitioned.combat->player.position.x
        == cleared.combat->player.position.x);
    ARPG_REQUIRE(transitioned.combat->player.position.y
        == cleared.combat->player.position.y);
    ARPG_REQUIRE(transitioned.combat->player.position.z
        == cleared.combat->player.position.z);
    ARPG_REQUIRE(transitioned.combat->player.state
        == cleared.combat->player.state);
    ARPG_REQUIRE(transitioned.combat->player.active_attack
        == cleared.combat->player.active_attack);
    ARPG_REQUIRE(transitioned.combat->player.attack_phase
        == cleared.combat->player.attack_phase);
    ARPG_REQUIRE(transitioned.combat->player.attack_elapsed_ticks
        == cleared.combat->player.attack_elapsed_ticks);
    ARPG_REQUIRE(transitioned.combat->player.combo_stage
        == cleared.combat->player.combo_stage);
    ARPG_REQUIRE(transitioned.combat->player.hit_stop_ticks
        == cleared.combat->player.hit_stop_ticks);

    bool moved_while_awaiting_exit = false;
    for (int tick = 0; tick < 64; ++tick) {
        session.tick(combat::MovementInput{-1, 0});
        test::drain_all_events(session, events);
        const dungeon::DungeonSnapshot awaiting = session.snapshot();
        ARPG_REQUIRE(awaiting.phase == dungeon::RoomPhase::awaiting_exit);
        if (awaiting.combat->player.position.x < player_x) {
            moved_while_awaiting_exit = true;
            break;
        }
    }
    const dungeon::DungeonSnapshot awaiting = session.snapshot();
    ARPG_REQUIRE(awaiting.combat->tick > combat_tick);
    ARPG_REQUIRE(moved_while_awaiting_exit);
    ARPG_REQUIRE(!session.queue_action(combat::Action::light));

    for (int tick = 0; tick < 180; ++tick) {
        session.tick(combat::MovementInput{});
        test::drain_all_events(session, events);
    }
    const dungeon::DungeonSnapshot stable = session.snapshot();
    ARPG_REQUIRE(stable.phase == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(stable.remaining_targets == 0U);
    ARPG_REQUIRE(all_exits_are(stable, true));
    ARPG_REQUIRE(events.room_cleared_count == 1U);
    ARPG_REQUIRE(events.exits_opened_count == 1U);
    ARPG_REQUIRE(events.defeated_count == initial_targets);
    return {};
}

arpg::test::Failure reset_reconstructs_same_room_and_clears_queues() noexcept {
    using namespace arpg;
    dungeon::DungeonSessionConfig config;
    config.root_seed = 0xA5A55A5AF00DFACEULL;
    config.initial_room_index = 19U;
    dungeon::DungeonSession session{config};

    session.tick(combat::MovementInput{});
    ARPG_REQUIRE(session.queue_action(combat::Action::light));
    session.tick(combat::MovementInput{});
    const dungeon::DungeonSnapshot before = session.snapshot();
    ARPG_REQUIRE(before.combat->tick == 1U);

    static_cast<void>(session.reset_current_room());
    const dungeon::DungeonSnapshot reset = session.snapshot();
    ARPG_REQUIRE(reset.session_tick == before.session_tick);
    ARPG_REQUIRE(reset.room_index == before.room_index);
    ARPG_REQUIRE(reset.room_seed == before.room_seed);
    ARPG_REQUIRE(reset.entry_side == before.entry_side);
    ARPG_REQUIRE(reset.last_exit == before.last_exit);
    ARPG_REQUIRE(reset.phase == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(reset.combat.has_value());
    ARPG_REQUIRE(reset.combat->tick == 0U);
    ARPG_REQUIRE(reset.remaining_targets > 0U);
    ARPG_REQUIRE(reset.remaining_targets == reset.combat->monster_count);
    ARPG_REQUIRE(all_exits_are(reset, false));
    ARPG_REQUIRE(reset.diagnostics.event_overflow_count
        == before.diagnostics.event_overflow_count);
    ARPG_REQUIRE(reset.diagnostics.combat_relay_overflow_count
        == before.diagnostics.combat_relay_overflow_count);

    const auto reset_event = session.try_pop_event();
    ARPG_REQUIRE(reset_event.has_value());
    ARPG_REQUIRE(reset_event->kind == dungeon::DungeonEventKind::room_reset);
    ARPG_REQUIRE(!session.try_pop_event().has_value());
    ARPG_REQUIRE(!session.try_pop_combat_event().has_value());
    return {};
}

arpg::test::Failure combat_events_relay_in_source_order() noexcept {
    using namespace arpg;
    dungeon::DungeonSession session;
    test::EventSummary relayed;
    const std::uint8_t initial_targets = session.snapshot().remaining_targets;

    session.tick(combat::MovementInput{});
    test::EventSummary dungeon_events;
    test::drain_all_events(session, dungeon_events);

    test::force_defeat_current_wave(session);
    session.tick(combat::MovementInput{});
    test::drain_all_events(session, relayed);

    ARPG_REQUIRE(relayed.combat_count == initial_targets);
    ARPG_REQUIRE(relayed.defeated_count == initial_targets);
    ARPG_REQUIRE(relayed.room_cleared_count == 1U);
    ARPG_REQUIRE(relayed.exits_opened_count == 1U);
    const dungeon::DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.diagnostics.combat_relay_overflow_count == 0U);
    ARPG_REQUIRE(state.diagnostics.event_overflow_count == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"abyss clear requires two reserved event slots", &abyss_clear_requires_two_reserved_event_slots},
    {"abyss clear exactly two slots publish both events", &abyss_clear_exactly_two_slots_publish_both_events},
    {"abyss clear is atomic and restores life resources", &abyss_clear_is_atomic_and_restores_life_resources},
    {"abyss clear removes environment hazard after commit", &abyss_clear_removes_environment_hazard_after_commit},
    {"abyss reset queues fail and rejects reentry", &abyss_reset_queues_fail_and_rejects_reentry},
    {"abyss fail not committed faults", &abyss_fail_not_committed_faults},
    {"abyss fail receipt mismatch faults", &abyss_fail_receipt_mismatch_faults},
    {"abyss fail commit rebuilds same normal room", &abyss_fail_commit_rebuilds_same_normal_room},
    {"ordinary reset returns accepted", &ordinary_reset_returns_accepted},
    {"ordinary player defeat uses normal reset", &ordinary_player_defeat_uses_normal_reset},
    {"abyss player defeat queues one fail", &abyss_player_defeat_queues_one_fail},
    {"abyss defeat does not depend on event delivery", &abyss_player_defeat_does_not_depend_on_event_delivery},
    {"relay overflow fault precedes durable defeat", &relay_overflow_fault_precedes_durable_defeat},
    {"invalid available checkpoint faults", &invalid_available_checkpoint_faults},
    {"impossible available origin faults", &impossible_available_abyss_origin_faults_before_start},
    {"deep abyss snapshot expanded legality", &deep_abyss_snapshot_uses_expanded_budget_legality},
    {"construction and first tick are staged", &construction_and_first_tick_are_staged},
    {"closed doors ignore pre-clear contact", &closed_doors_ignore_pre_clear_contact},
    {"real combat clears once without respawn", &real_combat_clears_once_without_respawn},
    {"reset reconstructs same room and clears queues", &reset_reconstructs_same_room_and_clears_queues},
    {"combat events relay in source order", &combat_events_relay_in_source_order},
};

}  // namespace

arpg::test::TestSuite dungeon_lifecycle_suite() noexcept {
    return arpg::test::make_suite("dungeon_lifecycle", kCases);
}
