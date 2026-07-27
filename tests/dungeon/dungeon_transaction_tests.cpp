#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "abyss/abyss_rules.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace {

using arpg::combat::MovementInput;
using arpg::dungeon::DungeonEventKind;
using arpg::dungeon::DungeonFault;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::PendingSaveKind;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;

DungeonRunState initial_state(
    std::uint64_t seed = 0xA11CE5EEDULL) noexcept {
    return arpg::dungeon::make_initial_run_state(seed, DungeonRules{}).state;
}

DungeonRunState available_state(std::uint64_t seed = 1U) noexcept {
    DungeonRunState state = initial_state(0xAB155EEDULL);
    while (!arpg::abyss::is_abyss_roll(seed)) ++seed;
    state.current_room.seed = seed;
    state.current_room.depth = 40U;
    state.current_room.entry = arpg::dungeon::EntrySide::left;
    state.current_room.ecology = arpg::dungeon::DungeonElement::chaos;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = true;
    state.last_transition = arpg::dungeon::TransitionKind::door;
    state.last_direction = ExitDirection::right;
    const auto selection = arpg::abyss::select_abyss_rule(
        state.current_room.seed, state.current_room.depth);
    if (selection.has_value()) {
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::available;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
    }
    return state;
}

DungeonRunState available_state_for_rule(
    arpg::abyss::AbyssRuleId rule) noexcept {
    std::uint64_t seed = 1U;
    for (;;) {
        DungeonRunState state = available_state(seed);
        if (state.abyss.rule == rule) return state;
        seed = state.current_room.seed + 1U;
    }
}

arpg::test::Failure available_room_queues_start_without_population() noexcept {
    const DungeonRunState available = available_state();
    DungeonSession session{DungeonRules{}, available};
    const auto snapshot = session.snapshot();
    const auto pending = session.pending_save();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::committing);
    ARPG_REQUIRE(!snapshot.combat.has_value());
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == arpg::dungeon::PendingSaveKind::abyss_start);
    ARPG_REQUIRE(pending->next_state.current_room.seed
        == available.current_room.seed);
    ARPG_REQUIRE(pending->next_state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::started);
    ARPG_REQUIRE(pending->expected_generation
        == available.commit_generation + 1U);

    const auto& plan = arpg::test::DungeonSessionTestAccess::encounter_plan(
        session);
    ARPG_REQUIRE(plan.wave_count == 0U);
    ARPG_REQUIRE(snapshot.initial_monster_count == 0U);
    ARPG_REQUIRE(snapshot.monster_blueprint_hash == 0U);
    ARPG_REQUIRE(snapshot.environment_blueprint_hash == 0U);
    return {};
}

arpg::test::Failure abyss_start_not_committed_faults_without_combat() noexcept {
    DungeonSession session{DungeonRules{}, available_state()};
    session.resolve_pending_save({SaveDisposition::not_committed, 0U, {}});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(!session.snapshot().combat.has_value());
    return {};
}

arpg::test::Failure abyss_start_indeterminate_faults_without_combat() noexcept {
    DungeonSession session{DungeonRules{}, available_state()};
    session.resolve_pending_save({SaveDisposition::indeterminate, 0U, {}});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(!session.snapshot().combat.has_value());
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_commit_indeterminate);
    return {};
}

arpg::test::Failure abyss_start_generation_mismatch_faults() noexcept {
    DungeonSession session{DungeonRules{}, available_state()};
    const auto pending = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation + 1U, pending.next_state});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(!session.snapshot().combat.has_value());
    return {};
}

arpg::test::Failure abyss_start_state_mismatch_faults() noexcept {
    DungeonSession session{DungeonRules{}, available_state()};
    const auto pending = *session.pending_save();
    DungeonRunState wrong = pending.next_state;
    wrong.current_room.has_hole = !wrong.current_room.has_hole;
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, wrong});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(!session.snapshot().combat.has_value());
    return {};
}

arpg::test::Failure abyss_start_commit_creates_combat_after_receipt() noexcept {
    const DungeonRunState available = available_state_for_rule(
        arpg::abyss::AbyssRuleId::abyss_fury);
    DungeonSession session{DungeonRules{}, available};
    const auto* pending_config =
        arpg::test::DungeonSessionTestAccess::pending_abyss_config(session);
    ARPG_REQUIRE(pending_config != nullptr);
    ARPG_REQUIRE(pending_config->rule == available.abyss.rule);
    ARPG_REQUIRE(pending_config->danger == available.abyss.danger);
    ARPG_REQUIRE(pending_config->monster_damage_bp == 14500U);
    ARPG_REQUIRE(pending_config->monster_attack_speed_bp == 14500U);
    const auto pending = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, pending.next_state});
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::locked);
    ARPG_REQUIRE(snapshot.combat.has_value());
    ARPG_REQUIRE(snapshot.commit_generation == pending.expected_generation);
    ARPG_REQUIRE(arpg::test::DungeonSessionTestAccess::stable_state(session)
        .abyss.lifecycle == arpg::abyss::AbyssLifecycle::started);
    const auto* active_config =
        arpg::test::DungeonSessionTestAccess::active_abyss_config(session);
    ARPG_REQUIRE(active_config != nullptr);
    ARPG_REQUIRE(active_config->rule == available.abyss.rule);
    ARPG_REQUIRE(active_config->monster_damage_bp == 14500U);
    ARPG_REQUIRE(active_config->monster_attack_speed_bp == 14500U);
    const auto* const plan = arpg::test::room_monster_plan(session);
    ARPG_REQUIRE(plan != nullptr);
    ARPG_REQUIRE(plan->monster_count == snapshot.initial_monster_count);
    const auto minimum = arpg::abyss::minimum_abyss_affixes(
        available.current_room.depth);
    for (std::uint16_t ordinal = 0U;
         ordinal < plan->monster_count; ++ordinal) {
        ARPG_REQUIRE(plan->monsters[ordinal].affixes.count >= minimum);
    }
    return {};
}

bool drive_to_abyss_clear_pending(DungeonSession& session) noexcept {
    session.tick({});
    arpg::test::EventSummary events{};
    arpg::test::drain_all_events(session, events);
    for (int tick = 0; tick < 4096; ++tick) {
        const auto state = session.snapshot();
        if (state.phase == RoomPhase::committing) {
            const auto pending = session.pending_save();
            if (!pending.has_value()) return false;
            if (pending->kind == PendingSaveKind::room_unlock) {
                session.resolve_pending_save({SaveDisposition::committed,
                    pending->expected_generation,
                    pending->next_state,
                    pending->kind});
                if (session.snapshot().phase != RoomPhase::combat
                        || !session.snapshot().exits_unlocked) {
                    return false;
                }
                arpg::test::drain_all_events(session, events);
                continue;
            }
            return pending->kind == PendingSaveKind::abyss_clear;
        }
        if (state.phase == RoomPhase::combat) {
            arpg::test::force_defeat_current_wave(session);
            arpg::test::clear_all_ground_health_potions(session);
        }
        session.tick({});
        arpg::test::drain_all_events(session, events);
    }
    return false;
}

bool commit_abyss_start_for_clear(DungeonSession& session) noexcept {
    if (!session.pending_save().has_value()) return false;
    const auto start = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::committed,
        start.expected_generation, start.next_state});
    return session.snapshot().phase == RoomPhase::locked;
}

arpg::test::Failure abyss_clear_reward_total_matches_danger() noexcept {
    using arpg::abyss::AbyssDanger;
    constexpr std::array<AbyssDanger, 3> dangers{{
        AbyssDanger::low, AbyssDanger::medium, AbyssDanger::high}};
    for (const AbyssDanger danger : dangers) {
        std::uint64_t seed = 1U;
        DungeonRunState state{};
        do {
            state = available_state(seed);
            seed = state.current_room.seed + 1U;
        } while (state.abyss.danger != danger);
        DungeonSession session{DungeonRules{}, state};
        const auto start = *session.pending_save();
        session.resolve_pending_save({SaveDisposition::committed,
            start.expected_generation, start.next_state});
        ARPG_REQUIRE(drive_to_abyss_clear_pending(session));
        const auto pending = *session.pending_save();
        ARPG_REQUIRE(pending.next_state.abyss.reward_total
            == static_cast<std::uint8_t>(danger) + 1U);
        ARPG_REQUIRE(pending.next_state.abyss.generated_mask == 0U);
        ARPG_REQUIRE(pending.next_state.abyss.claimed_mask == 0U);
        ARPG_REQUIRE(pending.next_state.abyss.abandoned_mask == 0U);
        ARPG_REQUIRE(pending.next_state.abyss.reward_revision == 0U);
    }
    return {};
}

arpg::test::Failure abyss_clear_not_committed_faults_atomically() noexcept {
    DungeonSession session{DungeonRules{}, available_state()};
    ARPG_REQUIRE(commit_abyss_start_for_clear(session));
    ARPG_REQUIRE(drive_to_abyss_clear_pending(session));
    const auto before = session.snapshot();
    session.resolve_pending_save({SaveDisposition::not_committed, 0U, {}});
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.phase == RoomPhase::faulted);
    ARPG_REQUIRE(after.diagnostics.fault == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(after.commit_generation == before.commit_generation);
    ARPG_REQUIRE(after.pending_room_experience == before.pending_room_experience);
    ARPG_REQUIRE(after.progression.experience == before.progression.experience);
    ARPG_REQUIRE(after.exits_unlocked);
    ARPG_REQUIRE(after.exits_open[0]);
    ARPG_REQUIRE(arpg::test::DungeonSessionTestAccess::active_abyss_config(session)
        ->rule != arpg::abyss::AbyssRuleId::none);
    arpg::test::EventSummary events{};
    arpg::test::drain_all_events(session, events);
    ARPG_REQUIRE(events.room_cleared_count == 0U);
    ARPG_REQUIRE(events.exits_opened_count == 0U);
    return {};
}

arpg::test::Failure abyss_clear_indeterminate_faults_atomically() noexcept {
    DungeonSession session{DungeonRules{}, available_state()};
    ARPG_REQUIRE(commit_abyss_start_for_clear(session));
    ARPG_REQUIRE(drive_to_abyss_clear_pending(session));
    const auto before = session.snapshot();
    session.resolve_pending_save({SaveDisposition::indeterminate, 0U, {}});
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.phase == RoomPhase::faulted);
    ARPG_REQUIRE(after.diagnostics.fault == DungeonFault::save_commit_indeterminate);
    ARPG_REQUIRE(after.commit_generation == before.commit_generation);
    ARPG_REQUIRE(after.pending_room_experience == before.pending_room_experience);
    ARPG_REQUIRE(after.progression.experience == before.progression.experience);
    ARPG_REQUIRE(after.exits_unlocked);
    ARPG_REQUIRE(after.exits_open[0]);
    return {};
}

arpg::test::Failure abyss_clear_generation_mismatch_faults_atomically() noexcept {
    DungeonSession session{DungeonRules{}, available_state()};
    ARPG_REQUIRE(commit_abyss_start_for_clear(session));
    ARPG_REQUIRE(drive_to_abyss_clear_pending(session));
    const auto pending = *session.pending_save();
    const auto before = session.snapshot();
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation + 1U, pending.next_state});
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.phase == RoomPhase::faulted);
    ARPG_REQUIRE(after.diagnostics.fault == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(after.commit_generation == before.commit_generation);
    ARPG_REQUIRE(after.pending_room_experience == before.pending_room_experience);
    ARPG_REQUIRE(after.progression.experience == before.progression.experience);
    ARPG_REQUIRE(after.exits_unlocked);
    ARPG_REQUIRE(after.exits_open[0]);
    return {};
}

arpg::test::Failure abyss_clear_state_mismatch_faults_atomically() noexcept {
    DungeonSession session{DungeonRules{}, available_state()};
    ARPG_REQUIRE(commit_abyss_start_for_clear(session));
    ARPG_REQUIRE(drive_to_abyss_clear_pending(session));
    const auto pending = *session.pending_save();
    DungeonRunState wrong = pending.next_state;
    wrong.abyss.reward_total = static_cast<std::uint8_t>(
        wrong.abyss.reward_total == 3U ? 2U : wrong.abyss.reward_total + 1U);
    const auto before = session.snapshot();
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, wrong});
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.phase == RoomPhase::faulted);
    ARPG_REQUIRE(after.diagnostics.fault == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(after.commit_generation == before.commit_generation);
    ARPG_REQUIRE(after.pending_room_experience == before.pending_room_experience);
    ARPG_REQUIRE(after.progression.experience == before.progression.experience);
    ARPG_REQUIRE(after.exits_unlocked);
    ARPG_REQUIRE(after.exits_open[0]);
    return {};
}

bool clear_and_await(DungeonSession& session) noexcept {
    arpg::test::EventSummary summary;
    if (!arpg::test::drive_until_cleared(session, summary)) {
        return false;
    }
    if (session.snapshot().phase == RoomPhase::cleared) {
        session.tick(MovementInput{});
    }
    return session.snapshot().phase == RoomPhase::awaiting_exit;
}

bool drive_to_open_door(
    DungeonSession& session,
    ExitDirection direction) noexcept {
    if (!clear_and_await(session)) {
        return false;
    }
    MovementInput outward{};
    switch (direction) {
    case ExitDirection::up:
        outward = {0, -1};
        break;
    case ExitDirection::down:
        outward = {0, 1};
        break;
    case ExitDirection::left:
        outward = {-1, 0};
        break;
    case ExitDirection::right:
        outward = {1, 0};
        break;
    case ExitDirection::none:
        return false;
    }
    for (int tick = 0; tick < 4096; ++tick) {
        const auto state = session.snapshot();
        MovementInput movement{};
        if (!state.combat.has_value()) {
            return false;
        }
        if (direction == ExitDirection::left
                || direction == ExitDirection::right) {
            movement.y = state.combat->player.position.y > 0.1F ? -1
                : (state.combat->player.position.y < -0.1F ? 1 : 0);
        } else {
            movement.x = state.combat->player.position.x > 0.1F ? -1
                : (state.combat->player.position.x < -0.1F ? 1 : 0);
        }
        if (movement.x == 0 && movement.y == 0) {
            break;
        }
        session.tick(movement);
        arpg::test::EventSummary ignored;
        arpg::test::drain_all_events(session, ignored);
    }
    for (int tick = 0; tick < 4096; ++tick) {
        session.tick(outward);
        arpg::test::EventSummary ignored;
        arpg::test::drain_all_events(session, ignored);
        if (session.snapshot().phase == RoomPhase::committing) {
            return true;
        }
    }
    return false;
}

arpg::test::Failure constructor_uses_saved_room_descriptor() noexcept {
    DungeonRules rules;
    DungeonRunState saved = initial_state(0x5555U);
    saved.current_room.index = 37U;
    saved.current_room.seed = 0x123456789ABCDEF0ULL;
    saved.current_room.depth = 9U;
    saved.current_room.floor_room_index = 4U;
    saved.current_room.entry = arpg::dungeon::EntrySide::left;
    saved.current_room.ecology = arpg::dungeon::DungeonElement::chaos;
    saved.current_room.has_hole = true;
    saved.current_room.is_abyss = true;

    DungeonSession session{rules, saved};
    const DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.room_index == saved.current_room.index);
    ARPG_REQUIRE(state.room_seed == saved.current_room.seed);
    ARPG_REQUIRE(state.depth == saved.current_room.depth);
    ARPG_REQUIRE(state.floor_room_index == saved.current_room.floor_room_index);
    ARPG_REQUIRE(state.entry_side == saved.current_room.entry);
    ARPG_REQUIRE(state.ecology == saved.current_room.ecology);
    ARPG_REQUIRE(state.has_hole == saved.current_room.has_hole);
    ARPG_REQUIRE(state.is_abyss == saved.current_room.is_abyss);
    return {};
}

arpg::test::Failure door_request_freezes_old_combat_until_commit() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    const DungeonSnapshot before = session.snapshot();
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->expected_generation
        == pending->next_state.commit_generation);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    ARPG_REQUIRE(session.snapshot().exits_open[0]);
    ARPG_REQUIRE(session.snapshot().combat.has_value());
    ARPG_REQUIRE(session.snapshot().room_index == before.room_index);
    const auto frozen = session.snapshot().combat->tick;
    session.tick({0, -1});
    ARPG_REQUIRE(session.snapshot().combat->tick == frozen);
    return {};
}

arpg::test::Failure exact_commit_adopts_verified_state_and_destroys_old_room() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    session.resolve_pending_transition({
        SaveDisposition::committed,
        pending->next_state.commit_generation,
        pending->next_state,
    });
    const DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.phase == RoomPhase::transitioning);
    ARPG_REQUIRE(!state.combat.has_value());
    ARPG_REQUIRE(state.room_index == pending->next_state.current_room.index);
    ARPG_REQUIRE(state.room_seed == pending->next_state.current_room.seed);
    ARPG_REQUIRE(!session.pending_transition().has_value());
    return {};
}

arpg::test::Failure not_committed_discards_pending_and_retries_identically() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::right));
    const auto first = session.pending_transition();
    ARPG_REQUIRE(first.has_value());
    session.resolve_pending_transition({SaveDisposition::not_committed, 0U, {}});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(!session.pending_transition().has_value());
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::right));
    const auto second = session.pending_transition();
    ARPG_REQUIRE(second.has_value());
    ARPG_REQUIRE(second->kind == first->kind);
    ARPG_REQUIRE(second->direction == first->direction);
    ARPG_REQUIRE(second->expected_generation == first->expected_generation);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        second->next_state, first->next_state));
    return {};
}

arpg::test::Failure indeterminate_faults_and_blocks_all_actions() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    session.resolve_pending_transition({SaveDisposition::indeterminate, 0U, {}});
    const DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.phase == RoomPhase::faulted);
    ARPG_REQUIRE(state.diagnostics.fault == DungeonFault::save_commit_indeterminate);
    ARPG_REQUIRE(!session.queue_action(arpg::combat::Action::light));
    ARPG_REQUIRE(!session.request_descent(true));
    session.tick({1, 0});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    return {};
}

arpg::test::Failure generation_mismatch_faults_with_receipt_mismatch() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    session.resolve_pending_transition({
        SaveDisposition::committed,
        pending->expected_generation + 1U,
        pending->next_state,
    });
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure verified_state_mismatch_faults_with_receipt_mismatch() noexcept {
    DungeonSession session{DungeonRules{}, initial_state()};
    ARPG_REQUIRE(drive_to_open_door(session, ExitDirection::up));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    DungeonRunState wrong = pending->next_state;
    ++wrong.current_room.index;
    session.resolve_pending_transition({
        SaveDisposition::committed,
        pending->expected_generation,
        wrong,
    });
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure descent_requires_clear_hole_range_and_edge() noexcept {
    DungeonRunState saved = initial_state();
    saved.current_room.has_hole = true;
    DungeonSession session{DungeonRules{}, saved};
    ARPG_REQUIRE(!session.request_descent(false));
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::locked);
    session.tick({});
    ARPG_REQUIRE(!session.request_descent(false));
    ARPG_REQUIRE(!session.request_descent(true));
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(clear_and_await(session));
    ARPG_REQUIRE(session.request_descent(true));
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    ARPG_REQUIRE(session.pending_transition()->kind
        == arpg::dungeon::TransitionKind::descent);

    DungeonRunState max_index = initial_state();
    max_index.current_room.index =
        (std::numeric_limits<std::uint64_t>::max)();
    max_index.current_room.has_hole = true;
    DungeonSession overflow_session{DungeonRules{}, max_index};
    ARPG_REQUIRE(clear_and_await(overflow_session));
    ARPG_REQUIRE(!overflow_session.request_descent(true));
    const DungeonSnapshot overflow_state = overflow_session.snapshot();
    ARPG_REQUIRE(overflow_state.phase == RoomPhase::faulted);
    ARPG_REQUIRE(overflow_state.diagnostics.fault
        == DungeonFault::room_index_overflow);
    ARPG_REQUIRE(overflow_state.diagnostics.room_index_overflow);
    return {};
}

arpg::test::Failure queue_overflow_faults_without_release_propagation() noexcept {
#if defined(NDEBUG)
    DungeonRules rules;
    rules.hole_threshold = 10000U;
    DungeonRunState saved = arpg::dungeon::make_initial_run_state(
        0xF00DULL, rules).state;
    saved.current_room.has_hole = true;
    DungeonSession session{rules, saved};
    for (int room = 0; room < 16
            && session.snapshot().phase != RoomPhase::faulted; ++room) {
        for (int tick = 0; tick < 4096
                && session.snapshot().phase != RoomPhase::cleared
                && session.snapshot().phase != RoomPhase::faulted; ++tick) {
            const auto state = session.snapshot();
            MovementInput movement{};
            if (state.phase == RoomPhase::combat && state.combat.has_value()) {
                const auto* target = arpg::test::nearest_living_monster(
                    *state.combat);
                if (target != nullptr) {
                    movement = arpg::test::movement_toward(
                        state.combat->player.position, target->position);
                    if (arpg::test::in_light_attack_lane(
                            state.combat->player, *target)
                            && state.combat->player.active_attack
                                == arpg::combat::AttackId::none) {
                        static_cast<void>(session.queue_action(
                            arpg::combat::Action::light));
                    }
                }
            }
            session.tick(movement);
        }
        if (session.snapshot().phase == RoomPhase::cleared) {
            session.tick({});
        }
        if (session.snapshot().phase != RoomPhase::awaiting_exit) {
            break;
        }
        if (!session.request_descent(true)
                || !arpg::test::commit_pending(session)) {
            break;
        }
        session.tick({});
        session.tick({});
    }
    const DungeonSnapshot state = session.snapshot();
    ARPG_REQUIRE(state.diagnostics.event_overflow_count > 0U
        || state.diagnostics.combat_relay_overflow_count > 0U);
    ARPG_REQUIRE(state.phase == RoomPhase::faulted);
#else
    // The overflow branch intentionally asserts in Debug; Release is the
    // build where this no-propagation contract is exercised.
    ARPG_REQUIRE(true);
#endif
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"abyss clear reward total matches danger", &abyss_clear_reward_total_matches_danger},
    {"abyss clear not committed faults atomically", &abyss_clear_not_committed_faults_atomically},
    {"abyss clear indeterminate faults atomically", &abyss_clear_indeterminate_faults_atomically},
    {"abyss clear generation mismatch faults atomically", &abyss_clear_generation_mismatch_faults_atomically},
    {"abyss clear state mismatch faults atomically", &abyss_clear_state_mismatch_faults_atomically},
    {"available room queues start without population",
        &available_room_queues_start_without_population},
    {"abyss start not committed faults without combat", &abyss_start_not_committed_faults_without_combat},
    {"abyss start indeterminate faults without combat", &abyss_start_indeterminate_faults_without_combat},
    {"abyss start generation mismatch faults", &abyss_start_generation_mismatch_faults},
    {"abyss start state mismatch faults", &abyss_start_state_mismatch_faults},
    {"abyss start commit creates combat after receipt", &abyss_start_commit_creates_combat_after_receipt},
    {"constructor uses saved room descriptor", &constructor_uses_saved_room_descriptor},
    {"door request freezes old combat until commit", &door_request_freezes_old_combat_until_commit},
    {"exact commit adopts verified state and destroys old room", &exact_commit_adopts_verified_state_and_destroys_old_room},
    {"not committed discards pending and retries identically", &not_committed_discards_pending_and_retries_identically},
    {"indeterminate faults and blocks all actions", &indeterminate_faults_and_blocks_all_actions},
    {"generation mismatch faults with receipt mismatch", &generation_mismatch_faults_with_receipt_mismatch},
    {"verified state mismatch faults with receipt mismatch", &verified_state_mismatch_faults_with_receipt_mismatch},
    {"descent requires clear hole range and edge", &descent_requires_clear_hole_range_and_edge},
    {"queue overflow faults without release propagation", &queue_overflow_faults_without_release_propagation},
};

}  // namespace

arpg::test::TestSuite dungeon_transaction_suite() noexcept {
    return arpg::test::make_suite("dungeon_transaction", kCases);
}
