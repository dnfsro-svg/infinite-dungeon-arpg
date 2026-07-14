#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon_test_support.hpp"

#include "dungeon/room_generation.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/encounter_director.hpp"
#include "passives/passive_tree_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

using arpg::combat::CombatEvent;
using arpg::combat::CombatSnapshot;
using arpg::combat::MovementInput;
using arpg::dungeon::DungeonEvent;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::EncounterDirectorConfig;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;
namespace checkpoint = arpg::dungeon::checkpoint;

constexpr std::array<ExitDirection, 4> kRoute{{
    ExitDirection::up,
    ExitDirection::right,
    ExitDirection::down,
    ExitDirection::left,
}};

constexpr std::size_t kGoldenTraceRoomCount = 256U;
constexpr std::size_t kGoldenTraceMonsterCapacity =
    arpg::combat::kEncounterWaveCapacity * arpg::combat::kEncounterSpawnCapacity;
constexpr std::uint64_t kGoldenTraceHashOffset = 14695981039346656037ULL;
constexpr std::uint64_t kGoldenTraceHashPrime = 1099511628211ULL;

struct GoldenTraceEntry final {
    std::uint64_t depth{};
    checkpoint::DungeonElement ecology{};
    std::array<arpg::combat::MonsterId, kGoldenTraceMonsterCapacity> monster_ids{};
    std::uint8_t monster_count{};
    bool has_hole{};
    bool is_abyss{};
    std::uint64_t next_seed{};
};

void append_golden_trace_value(
    std::uint64_t& hash,
    std::uint64_t value) noexcept {
    for (std::size_t byte = 0U; byte < sizeof(value); ++byte) {
        hash ^= value & 0xFFULL;
        hash *= kGoldenTraceHashPrime;
        value >>= 8U;
    }
}

bool generate_golden_room_trace(
    std::array<GoldenTraceEntry, kGoldenTraceRoomCount>& trace) noexcept {
    constexpr std::uint64_t kRootSeed = 0x6d5a56da1234ULL;
    const DungeonRules rules{};
    auto built = arpg::dungeon::make_initial_run_state(kRootSeed, rules);
    if (built.fault != arpg::dungeon::DungeonFault::none) {
        return false;
    }
    for (std::size_t index = 0U; index < trace.size(); ++index) {
        const auto plan = arpg::dungeon::build_encounter_plan(
            built.state.current_room.seed,
            built.state.current_room.depth,
            built.state.current_room.ecology,
            rules.encounter);
        const auto next = arpg::dungeon::make_door_transition(
            built.state, kRoute[index % kRoute.size()], rules);
        if (plan.fault != arpg::dungeon::DungeonFault::none
                || next.fault != arpg::dungeon::DungeonFault::none) {
            return false;
        }
        GoldenTraceEntry& entry = trace[index];
        entry.depth = built.state.current_room.depth;
        entry.ecology = built.state.current_room.ecology;
        entry.has_hole = built.state.current_room.has_hole;
        entry.is_abyss = built.state.current_room.is_abyss;
        entry.next_seed = next.state.current_room.seed;
        for (std::size_t wave = 0U; wave < plan.plan.wave_count; ++wave) {
            for (std::size_t spawn = 0U;
                 spawn < plan.plan.waves[wave].spawn_count; ++spawn) {
                if (entry.monster_count >= entry.monster_ids.size()) {
                    return false;
                }
                entry.monster_ids[entry.monster_count++] =
                    plan.plan.waves[wave].spawns[spawn].id;
            }
        }
        built = next;
    }
    return true;
}

std::uint64_t golden_room_trace_hash(
    const std::array<GoldenTraceEntry, kGoldenTraceRoomCount>& trace) noexcept {
    std::uint64_t hash = kGoldenTraceHashOffset;
    for (const GoldenTraceEntry& entry : trace) {
        append_golden_trace_value(hash, entry.depth);
        append_golden_trace_value(hash,
            static_cast<std::uint64_t>(entry.ecology));
        for (std::size_t monster = 0U; monster < entry.monster_count; ++monster) {
            append_golden_trace_value(hash,
                static_cast<std::uint64_t>(entry.monster_ids[monster]));
        }
        append_golden_trace_value(hash, entry.monster_count);
        append_golden_trace_value(hash, entry.has_hole ? 1U : 0U);
        append_golden_trace_value(hash, entry.is_abyss ? 1U : 0U);
        append_golden_trace_value(hash, entry.next_seed);
    }
    return hash;
}

struct StressSummary final {
    std::uint32_t dungeon_events{};
    std::uint32_t combat_events{};
    std::uint32_t dungeon_overflow{};
    std::uint32_t relay_overflow{};
    std::uint32_t combat_overflow{};
    std::uint32_t input_overflow{};
};

bool same_vec(const arpg::combat::Vec3& lhs, const arpg::combat::Vec3& rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool same_combat(const CombatSnapshot& lhs, const CombatSnapshot& rhs) noexcept {
    if (lhs.tick != rhs.tick
            || !same_vec(lhs.player.position, rhs.player.position)
            || !same_vec(lhs.player.velocity, rhs.player.velocity)
            || lhs.player.facing != rhs.player.facing
            || lhs.player.state != rhs.player.state
            || lhs.player.active_attack != rhs.player.active_attack
            || lhs.player.attack_phase != rhs.player.attack_phase
            || lhs.player.attack_elapsed_ticks != rhs.player.attack_elapsed_ticks
            || lhs.player.combo_stage != rhs.player.combo_stage
            || lhs.player.hit_stop_ticks != rhs.player.hit_stop_ticks
            || lhs.player.air_attack_available != rhs.player.air_attack_available
            || lhs.player.hp != rhs.player.hp
            || lhs.player.max_hp != rhs.player.max_hp
            || lhs.player.hurt_ticks != rhs.player.hurt_ticks
            || lhs.player.invulnerability_ticks != rhs.player.invulnerability_ticks
            || lhs.monster_count != rhs.monster_count
            || lhs.diagnostics.input_size != rhs.diagnostics.input_size
            || lhs.diagnostics.input_expired_count != rhs.diagnostics.input_expired_count
            || lhs.diagnostics.input_overflow_count != rhs.diagnostics.input_overflow_count
            || lhs.diagnostics.event_overflow_count != rhs.diagnostics.event_overflow_count) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.monsters.size(); ++index) {
        const auto& a = lhs.monsters[index];
        const auto& b = rhs.monsters[index];
        if (!same_vec(a.position, b.position) || !same_vec(a.velocity, b.velocity)
                || a.active != b.active || a.generation != b.generation
                || a.id != b.id || !same_vec(a.spawn, b.spawn)
                || a.kind != b.kind || a.reaction != b.reaction
                || a.armor != b.armor || a.hp != b.hp || a.max_hp != b.max_hp
                || a.break_value != b.break_value || a.max_break != b.max_break
                || a.shield != b.shield || a.max_shield != b.max_shield
                || a.shield_ticks != b.shield_ticks
                || a.max_shield_ticks != b.max_shield_ticks
                || a.break_window_ticks != b.break_window_ticks
                || a.hit_stop_ticks != b.hit_stop_ticks
                || a.ai_phase != b.ai_phase
                || !same_vec(a.attack_target_position, b.attack_target_position)
                || !same_vec(a.attack_vector, b.attack_vector)) {
            return false;
        }
    }
    return true;
}

bool same_snapshot(const DungeonSnapshot& lhs, const DungeonSnapshot& rhs) noexcept {
    if (lhs.session_tick != rhs.session_tick || lhs.root_seed != rhs.root_seed
            || lhs.commit_generation != rhs.commit_generation
            || lhs.room_index != rhs.room_index
            || lhs.room_seed != rhs.room_seed || lhs.phase != rhs.phase
            || lhs.depth != rhs.depth
            || lhs.floor_room_index != rhs.floor_room_index
            || lhs.biases != rhs.biases
            || lhs.has_active_room != rhs.has_active_room
            || lhs.exits_open != rhs.exits_open
            || lhs.wave_index != rhs.wave_index
            || lhs.wave_count != rhs.wave_count
            || lhs.wave_delay_ticks != rhs.wave_delay_ticks
            || lhs.remaining_targets != rhs.remaining_targets
            || lhs.entry_side != rhs.entry_side || lhs.last_exit != rhs.last_exit
            || lhs.last_transition != rhs.last_transition
            || lhs.ecology != rhs.ecology
            || lhs.has_hole != rhs.has_hole
            || lhs.is_abyss != rhs.is_abyss
            || lhs.has_pending_transition != rhs.has_pending_transition
            || lhs.encounter.total_budget != rhs.encounter.total_budget
            || lhs.encounter.current_wave_budget
                != rhs.encounter.current_wave_budget
            || lhs.encounter.current_wave_spawn_count
                != rhs.encounter.current_wave_spawn_count
            || lhs.encounter.plan_valid != rhs.encounter.plan_valid
            || lhs.diagnostics.event_overflow_count
                != rhs.diagnostics.event_overflow_count
            || lhs.diagnostics.combat_relay_overflow_count
                != rhs.diagnostics.combat_relay_overflow_count
            || lhs.diagnostics.rejected_exit_count
                != rhs.diagnostics.rejected_exit_count
            || lhs.diagnostics.save_failure_count
                != rhs.diagnostics.save_failure_count
            || lhs.diagnostics.fault != rhs.diagnostics.fault
            || lhs.diagnostics.room_index_overflow
                != rhs.diagnostics.room_index_overflow
            || lhs.combat.has_value() != rhs.combat.has_value()) {
        return false;
    }
    return !lhs.combat.has_value() || same_combat(*lhs.combat, *rhs.combat);
}

bool same_event(const DungeonEvent& lhs, const DungeonEvent& rhs) noexcept {
    return lhs.kind == rhs.kind && lhs.session_tick == rhs.session_tick
        && lhs.room_index == rhs.room_index && lhs.room_seed == rhs.room_seed
        && lhs.destination_room_index == rhs.destination_room_index
        && lhs.destination_room_seed == rhs.destination_room_seed
        && lhs.transition == rhs.transition
        && lhs.direction == rhs.direction;
}

bool same_event(const CombatEvent& lhs, const CombatEvent& rhs) noexcept {
    return lhs.kind == rhs.kind && lhs.tick == rhs.tick
        && lhs.attack == rhs.attack && lhs.feedback == rhs.feedback
        && lhs.target_index == rhs.target_index
        && lhs.hit_count == rhs.hit_count && lhs.value == rhs.value
        && same_vec(lhs.position, rhs.position);
}

void sample_diagnostics(const DungeonSnapshot& state, StressSummary& summary) noexcept {
    summary.dungeon_overflow = state.diagnostics.event_overflow_count;
    summary.relay_overflow = state.diagnostics.combat_relay_overflow_count;
    if (state.combat.has_value()) {
        summary.combat_overflow += state.combat->diagnostics.event_overflow_count;
        summary.input_overflow += state.combat->diagnostics.input_overflow_count;
    }
}

void drain(DungeonSession& session, StressSummary& summary) noexcept {
    while (session.try_pop_event().has_value()) {
        ++summary.dungeon_events;
    }
    while (session.try_pop_combat_event().has_value()) {
        ++summary.combat_events;
    }
    sample_diagnostics(session.snapshot(), summary);
}

bool tick_equal(
    DungeonSession& lhs,
    DungeonSession& rhs,
    MovementInput movement) noexcept {
    lhs.tick(movement);
    rhs.tick(movement);
    if (!same_snapshot(lhs.snapshot(), rhs.snapshot())) {
        return false;
    }
    for (;;) {
        const auto a = lhs.try_pop_event();
        const auto b = rhs.try_pop_event();
        if (a.has_value() != b.has_value()) {
            return false;
        }
        if (!a.has_value()) {
            break;
        }
        if (!same_event(*a, *b)) {
            return false;
        }
    }
    for (;;) {
        const auto a = lhs.try_pop_combat_event();
        const auto b = rhs.try_pop_combat_event();
        if (a.has_value() != b.has_value()) {
            return false;
        }
        if (!a.has_value()) {
            break;
        }
        if (!same_event(*a, *b)) {
            return false;
        }
    }
    return true;
}

bool commit_equal(
    DungeonSession& lhs,
    DungeonSession& rhs) noexcept {
    const auto a = lhs.pending_transition();
    const auto b = rhs.pending_transition();
    if (!a.has_value() || !b.has_value()
            || a->kind != b->kind || a->direction != b->direction
            || a->expected_generation != b->expected_generation
            || !arpg::dungeon::same_run_state(a->next_state, b->next_state)) {
        return false;
    }
    lhs.resolve_pending_transition({
        arpg::dungeon::SaveDisposition::committed,
        a->expected_generation,
        a->next_state,
    });
    rhs.resolve_pending_transition({
        arpg::dungeon::SaveDisposition::committed,
        b->expected_generation,
        b->next_state,
    });
    if (!same_snapshot(lhs.snapshot(), rhs.snapshot())) {
        return false;
    }
    for (;;) {
        const auto x = lhs.try_pop_event();
        const auto y = rhs.try_pop_event();
        if (x.has_value() != y.has_value()) {
            return false;
        }
        if (!x.has_value()) {
            break;
        }
        if (!same_event(*x, *y)) {
            return false;
        }
    }
    return true;
}

MovementInput outward(ExitDirection direction) noexcept {
    switch (direction) {
    case ExitDirection::up: return {0, -1};
    case ExitDirection::down: return {0, 1};
    case ExitDirection::left: return {-1, 0};
    case ExitDirection::right: return {1, 0};
    case ExitDirection::none: return {};
    }
    return {};
}

MovementInput align_center(const DungeonSnapshot& state, ExitDirection direction) noexcept {
    constexpr float kTolerance = 0.10F;
    MovementInput movement{};
    const auto position = state.combat->player.position;
    if (direction == ExitDirection::left || direction == ExitDirection::right) {
        movement.y = position.y > kTolerance ? -1
            : (position.y < -kTolerance ? 1 : 0);
    } else {
        movement.x = position.x > kTolerance ? -1
            : (position.x < -kTolerance ? 1 : 0);
    }
    return movement;
}

bool drive_clear(DungeonSession& session, StressSummary& summary) noexcept {
    for (int tick = 0; tick < 8192; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        if (state.phase == RoomPhase::cleared) {
            session.tick(MovementInput{});
            drain(session, summary);
            return session.snapshot().phase == RoomPhase::awaiting_exit;
        }
        if (state.phase == RoomPhase::combat && state.combat.has_value()) {
            arpg::test::force_defeat_current_wave(session);
        }
        session.tick({});
        drain(session, summary);
    }
    return false;
}

bool confirm_pending_save(DungeonSession& session) noexcept {
    const auto pending = session.pending_transition();
    if (!pending.has_value()) {
        return false;
    }
    session.resolve_pending_transition({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
    });
    const DungeonSnapshot saved = session.snapshot();
    return saved.phase == RoomPhase::transitioning
        && !saved.has_pending_transition
        && saved.commit_generation == pending->expected_generation
        && saved.room_index == pending->next_state.current_room.index
        && saved.room_seed == pending->next_state.current_room.seed
        && !saved.has_active_room && !saved.combat.has_value();
}

bool drive_exit(
    DungeonSession& session,
    ExitDirection direction,
    StressSummary& summary,
    bool verify_phases) noexcept {
    for (int tick = 0; tick < 512; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        const MovementInput movement = align_center(state, direction);
        if (movement.x == 0 && movement.y == 0) {
            break;
        }
        session.tick(movement);
        drain(session, summary);
    }
    for (int tick = 0; tick < 512; ++tick) {
        session.tick(outward(direction));
        drain(session, summary);
        if (session.snapshot().phase == RoomPhase::committing) {
            if (!confirm_pending_save(session)) {
                return false;
            }
            drain(session, summary);
        }
        if (session.snapshot().phase == RoomPhase::transitioning) {
            break;
        }
    }
    const DungeonSnapshot transition = session.snapshot();
    if (transition.phase != RoomPhase::transitioning
            || transition.has_active_room || transition.combat.has_value()) {
        return false;
    }
    session.tick(outward(direction));
    drain(session, summary);
    const DungeonSnapshot locked = session.snapshot();
    if (locked.phase != RoomPhase::locked || !locked.has_active_room
            || !locked.combat.has_value() || locked.combat->tick != 0U) {
        return false;
    }
    session.tick(outward(direction));
    drain(session, summary);
    const DungeonSnapshot combat = session.snapshot();
    return combat.phase == RoomPhase::combat && combat.has_active_room
        && combat.combat.has_value() && combat.combat->tick == 0U
        && (!verify_phases || combat.room_index == transition.room_index);
}

bool drive_rooms(
    DungeonSession& session,
    std::size_t exits,
    StressSummary& summary,
    bool verify_phases = false) noexcept {
    drain(session, summary);
    for (std::size_t index = 0; index < exits; ++index) {
        if (!drive_clear(session, summary)
                || !drive_exit(session, kRoute[index % kRoute.size()], summary,
                    verify_phases)) {
            return false;
        }
    }
    return true;
}

constexpr std::array<arpg::passives::PassiveNodeId, 12U> kPassiveRouteCycle{{
    8U, 9U, 10U, 22U, 23U, 24U, 36U, 37U, 38U, 50U, 51U, 52U,
}};

struct PassiveStressRecord final {
    std::uint64_t room_seed{};
    std::uint64_t allocated_bits{};
    std::uint64_t generation{};
    int hp{};
    int barrier{};
};

bool same_record(const PassiveStressRecord& left,
    const PassiveStressRecord& right) noexcept {
    return left.room_seed == right.room_seed
        && left.allocated_bits == right.allocated_bits
        && left.generation == right.generation
        && left.hp == right.hp && left.barrier == right.barrier;
}

bool node_is_allocated(std::uint64_t bits,
    arpg::passives::PassiveNodeId node) noexcept {
    return (bits & (std::uint64_t{1U} << node)) != 0U;
}

bool has_allocated_neighbor(std::uint64_t bits,
    const arpg::passives::PassiveNode& node) noexcept {
    for (std::size_t index = 0U; index < node.neighbor_count; ++index) {
        if (node_is_allocated(bits, node.neighbors[index])) {
            return true;
        }
    }
    return false;
}

bool commit_passive_receipt(DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()
            || pending->kind != arpg::dungeon::PendingSaveKind::passive_tree) {
        return false;
    }
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
    });
    const DungeonSnapshot committed = session.snapshot();
    return !committed.passive_save_pending
        && committed.commit_generation == pending->expected_generation
        && committed.passive_tree.allocated_bits
            == pending->next_state.passive_tree.allocated_bits;
}

bool allocate_route_node(DungeonSession& session,
    arpg::passives::PassiveNodeId requested) noexcept {
    const DungeonSnapshot before = session.snapshot();
    const auto& nodes = arpg::passives::passive_nodes();
    const auto& target = nodes[requested];
    arpg::passives::PassiveNodeId chosen = requested;
    bool found = !node_is_allocated(before.passive_tree.allocated_bits, requested)
        && has_allocated_neighbor(before.passive_tree.allocated_bits, target);
    if (!found) {
        bool has_candidate = false;
        for (std::size_t index = 0U; index < target.neighbor_count; ++index) {
            const auto neighbor = target.neighbors[index];
            if (node_is_allocated(before.passive_tree.allocated_bits, neighbor)
                    || !has_allocated_neighbor(before.passive_tree.allocated_bits,
                        nodes[neighbor])) {
                continue;
            }
            if (!has_candidate || neighbor < chosen) {
                chosen = neighbor;
                has_candidate = true;
            }
        }
        found = has_candidate;
    }
    if (!found) {
        return true;
    }
    return session.request_passive_allocation(chosen)
        && commit_passive_receipt(session);
}

DungeonSession passive_trace_session(std::uint64_t root_seed) noexcept {
    auto built = arpg::dungeon::make_initial_run_state(root_seed, DungeonRules{});
    built.state.progression = {64U, 0U, 63U, 63U};
    return DungeonSession(DungeonRules{}, built.state);
}

bool generate_passive_stress_trace(
    std::array<PassiveStressRecord, 1000U>& trace) noexcept {
    DungeonSession session = passive_trace_session(0x7A6E4F1B2C3DULL);
    StressSummary summary{};
    for (std::size_t room_index = 0U; room_index < trace.size(); ++room_index) {
        if (!drive_clear(session, summary)) {
            return false;
        }
        if (room_index % 25U == 0U) {
            const auto next = kPassiveRouteCycle[(room_index / 25U)
                % kPassiveRouteCycle.size()];
            if (!allocate_route_node(session, next)) {
                return false;
            }
        }
        const DungeonSnapshot snapshot = session.snapshot();
        if (!snapshot.combat.has_value()
                || snapshot.passive_tree.allocated_bits == 0U
                || snapshot.commit_generation == 0U) {
            return false;
        }
        trace[room_index] = {
            snapshot.room_seed,
            snapshot.passive_tree.allocated_bits,
            snapshot.commit_generation,
            snapshot.combat->player.hp,
            snapshot.combat->player.barrier,
        };
        if (!drive_exit(session, kRoute[room_index % kRoute.size()], summary, true)) {
            return false;
        }
    }
    return true;
}

DungeonRules single_chaser_rules() noexcept {
    DungeonRules rules{};
    rules.encounter.base_budget = 2U;
    rules.encounter.max_budget = 2U;
    rules.encounter.two_wave_threshold = 2U;
    return rules;
}

MovementInput launcher_robot_movement(
    const arpg::combat::PlayerSnapshot& player,
    const arpg::combat::MonsterSnapshot& target) noexcept {
    MovementInput movement = arpg::test::movement_toward(
        player.position, target.position);
    const float delta_x = target.position.x - player.position.x;
    const bool target_is_left = delta_x < 0.0F;
    const bool target_is_right = delta_x > 0.0F;
    const bool inside_horizontal_deadband = delta_x >= -1.00F
        && delta_x <= 1.00F;
    const bool facing_away = (target_is_left
            && player.facing == arpg::combat::Facing::right)
        || (target_is_right && player.facing == arpg::combat::Facing::left);
    if (movement.x == 0 && inside_horizontal_deadband && facing_away) {
        movement.x = target_is_left ? -1 : 1;
    }
    return movement;
}

struct RealInputTrace final {
    std::uint64_t room_index{};
    std::uint64_t session_tick{};
    RoomPhase phase{RoomPhase::locked};
    std::uint8_t remaining_targets{};
    std::uint32_t dungeon_overflow{};
    std::uint32_t relay_overflow{};
    std::uint32_t combat_overflow{};
    std::uint32_t input_overflow{};
    arpg::combat::Vec3 player_position{};
    int player_hp{};
    std::uint16_t player_hurt_ticks{};
    arpg::combat::AttackId player_attack{arpg::combat::AttackId::none};
    std::size_t input_size{};
    arpg::combat::Vec3 target_position{};
    int target_hp{};
    arpg::combat::MonsterAiPhase target_phase{
        arpg::combat::MonsterAiPhase::idle};
    arpg::combat::ReactionState target_reaction{
        arpg::combat::ReactionState::idle};
    arpg::combat::Facing player_facing{arpg::combat::Facing::right};
    arpg::combat::Vec3 initial_player_position{};
    arpg::combat::Vec3 initial_target_position{};
    int initial_target_hp{};
    bool initial_combat_seen{};
    std::uint32_t action_attempts{};
    std::uint32_t action_accepted{};
    std::uint32_t action_rejected{};
    std::uint32_t swings{};
    std::uint32_t active_samples{};
    std::uint32_t monster_hits{};
    std::uint32_t defeated{};
    std::uint32_t player_hits{};
    std::uint32_t player_hurt_started{};
    bool attack_in_flight{};
    bool attack_had_hit{};
    bool first_whiff_seen{};
    std::uint64_t first_whiff_tick{};
    float first_whiff_dx{};
    float first_whiff_dy{};
    arpg::combat::Facing first_whiff_facing{arpg::combat::Facing::right};
    arpg::combat::MonsterAiPhase first_whiff_ai{
        arpg::combat::MonsterAiPhase::idle};
    arpg::combat::ReactionState first_whiff_reaction{
        arpg::combat::ReactionState::idle};
};

void capture_trace(
    const DungeonSnapshot& state,
    RealInputTrace& trace) noexcept {
    trace.room_index = state.room_index;
    trace.session_tick = state.session_tick;
    trace.phase = state.phase;
    trace.remaining_targets = state.remaining_targets;
    trace.dungeon_overflow = state.diagnostics.event_overflow_count;
    trace.relay_overflow = state.diagnostics.combat_relay_overflow_count;
    if (state.combat.has_value()) {
        trace.combat_overflow = state.combat->diagnostics.event_overflow_count;
        trace.input_overflow = state.combat->diagnostics.input_overflow_count;
        trace.player_position = state.combat->player.position;
        trace.player_hp = state.combat->player.hp;
        trace.player_hurt_ticks = state.combat->player.hurt_ticks;
        trace.player_attack = state.combat->player.active_attack;
        trace.player_facing = state.combat->player.facing;
        trace.input_size = state.combat->diagnostics.input_size;
        if (const auto* target = arpg::test::nearest_living_monster(
                *state.combat)) {
            trace.target_position = target->position;
            trace.target_hp = target->hp;
            trace.target_phase = target->ai_phase;
            trace.target_reaction = target->reaction;
            if (!trace.initial_combat_seen) {
                trace.initial_combat_seen = true;
                trace.initial_player_position = state.combat->player.position;
                trace.initial_target_position = target->position;
                trace.initial_target_hp = target->hp;
            }
        }
    }
}

void drain_real_input_events(
    DungeonSession& session,
    StressSummary& summary,
    RealInputTrace& trace) noexcept {
    while (session.try_pop_event().has_value()) {
        ++summary.dungeon_events;
    }
    while (const auto event = session.try_pop_combat_event()) {
        ++summary.combat_events;
        if (event->kind == arpg::combat::CombatEventKind::swing) {
            ++trace.swings;
            trace.attack_in_flight = true;
            trace.attack_had_hit = false;
        } else if (event->kind == arpg::combat::CombatEventKind::hit) {
            ++trace.monster_hits;
            trace.attack_had_hit = true;
        } else if (event->kind == arpg::combat::CombatEventKind::defeated) {
            ++trace.defeated;
        } else if (event->kind == arpg::combat::CombatEventKind::player_hit) {
            ++trace.player_hits;
        } else if (event->kind
                == arpg::combat::CombatEventKind::player_hurt_started) {
            ++trace.player_hurt_started;
        }
    }
    sample_diagnostics(session.snapshot(), summary);
}

bool drive_real_input_clear(
    DungeonSession& session,
    StressSummary& summary,
    RealInputTrace& trace,
    arpg::combat::Action action) noexcept {
    for (int tick = 0; tick < 4096; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        capture_trace(state, trace);
        if (state.combat.has_value()) {
            const auto& player = state.combat->player;
            if (player.active_attack != arpg::combat::AttackId::none
                    && player.attack_phase == arpg::combat::AttackPhase::active) {
                ++trace.active_samples;
            }
            if (trace.attack_in_flight
                    && player.active_attack == arpg::combat::AttackId::none) {
                if (!trace.attack_had_hit && !trace.first_whiff_seen) {
                    trace.first_whiff_seen = true;
                    trace.first_whiff_tick = state.session_tick;
                    trace.first_whiff_dx = trace.target_position.x
                        - trace.player_position.x;
                    trace.first_whiff_dy = trace.target_position.y
                        - trace.player_position.y;
                    trace.first_whiff_facing = trace.player_facing;
                    trace.first_whiff_ai = trace.target_phase;
                    trace.first_whiff_reaction = trace.target_reaction;
                }
                trace.attack_in_flight = false;
            }
        }
        if (state.phase == RoomPhase::cleared) {
            session.tick({});
            drain_real_input_events(session, summary, trace);
            capture_trace(session.snapshot(), trace);
            return session.snapshot().phase == RoomPhase::awaiting_exit;
        }
        if (state.phase == RoomPhase::faulted || !state.combat.has_value()) {
            return false;
        }

        MovementInput movement{};
        if (state.phase == RoomPhase::combat) {
            const auto* target = arpg::test::nearest_living_monster(*state.combat);
            if (target != nullptr) {
                movement = launcher_robot_movement(
                    state.combat->player, *target);
                if (state.combat->player.hurt_ticks == 0U
                        && state.combat->player.active_attack
                            == arpg::combat::AttackId::none
                        && state.combat->diagnostics.input_size == 0U
                        && arpg::test::in_light_attack_lane(
                            state.combat->player, *target)) {
                    ++trace.action_attempts;
                    if (!session.queue_action(action)) {
                        ++trace.action_rejected;
                        return false;
                    }
                    ++trace.action_accepted;
                }
            }
        }
        session.tick(movement);
        drain_real_input_events(session, summary, trace);
    }
    capture_trace(session.snapshot(), trace);
    return false;
}

void print_real_input_trace(
    const char* label,
    const RealInputTrace& trace,
    bool cleared) noexcept {
    std::printf("[real-input] %s room=%llu cleared=%u initial-player=%.2f,%.2f "
        "initial-target=%.2f,%.2f hp=%d queues=%u/%u rejected=%u swings=%u "
        "active=%u hits=%u defeated=%u player-hit/hurt=%u/%u first-whiff=%u "
        "whiff-tick=%llu dx=%.2f dy=%.2f facing=%d ai/reaction=%u/%u\n",
        label, static_cast<unsigned long long>(trace.room_index),
        static_cast<unsigned>(cleared), trace.initial_player_position.x,
        trace.initial_player_position.y, trace.initial_target_position.x,
        trace.initial_target_position.y, trace.initial_target_hp,
        trace.action_accepted, trace.action_attempts, trace.action_rejected,
        trace.swings, trace.active_samples, trace.monster_hits, trace.defeated,
        trace.player_hits, trace.player_hurt_started,
        static_cast<unsigned>(trace.first_whiff_seen),
        static_cast<unsigned long long>(trace.first_whiff_tick),
        trace.first_whiff_dx, trace.first_whiff_dy,
        static_cast<int>(trace.first_whiff_facing),
        static_cast<unsigned>(trace.first_whiff_ai),
        static_cast<unsigned>(trace.first_whiff_reaction));
}

arpg::test::Failure launcher_input_robot_clears_ten_minimal_committed_rooms() noexcept {
    constexpr std::uint64_t kSeed = 0x5245414C494E5055ULL;
    const DungeonRules rules = single_chaser_rules();
    const auto initial = arpg::dungeon::make_initial_run_state(kSeed, rules);
    ARPG_REQUIRE(initial.fault == arpg::dungeon::DungeonFault::none);
    DungeonSession session{rules, initial.state};
    const DungeonSnapshot first = session.snapshot();
    std::printf("[real-input] seed=%llu initial-room=%llu plan=%u/%u/%u\n",
        static_cast<unsigned long long>(kSeed),
        static_cast<unsigned long long>(first.room_index),
        static_cast<unsigned>(first.wave_count),
        static_cast<unsigned>(first.encounter.total_budget),
        static_cast<unsigned>(first.encounter.current_wave_spawn_count));
    ARPG_REQUIRE(first.wave_count == 1U);
    ARPG_REQUIRE(first.encounter.total_budget == 2U);
    ARPG_REQUIRE(first.encounter.current_wave_spawn_count == 1U);

    StressSummary summary{};
    for (std::size_t room = 0; room < 10U; ++room) {
        RealInputTrace trace{};
        const bool cleared = drive_real_input_clear(
            session, summary, trace, arpg::combat::Action::launcher);
        print_real_input_trace(
            room == 0U ? "room0-launcher" : "roomN-launcher", trace, cleared);
        if (!cleared) {
            std::printf("[real-input] clear-failed room=%llu tick=%llu phase=%u "
                "remaining=%u overflow=%u/%u/%u/%u player=%.2f,%.2f hp=%d "
                "hurt=%u attack=%u input=%llu target=%.2f,%.2f hp=%d ai=%u "
                "actions=%u hits=%u player_hits=%u\n",
                static_cast<unsigned long long>(trace.room_index),
                static_cast<unsigned long long>(trace.session_tick),
                static_cast<unsigned>(trace.phase),
                static_cast<unsigned>(trace.remaining_targets),
                trace.dungeon_overflow, trace.relay_overflow,
                trace.combat_overflow, trace.input_overflow,
                trace.player_position.x, trace.player_position.y,
                trace.player_hp, static_cast<unsigned>(trace.player_hurt_ticks),
                static_cast<unsigned>(trace.player_attack),
                static_cast<unsigned long long>(trace.input_size),
                trace.target_position.x, trace.target_position.y, trace.target_hp,
                static_cast<unsigned>(trace.target_phase), trace.action_accepted,
                trace.monster_hits, trace.player_hits);
        }
        ARPG_REQUIRE(cleared);
        ARPG_REQUIRE(drive_exit(session, kRoute[room % kRoute.size()], summary,
            true));
    }
    ARPG_REQUIRE(session.snapshot().room_index == 10U);
    ARPG_REQUIRE(summary.dungeon_overflow == 0U);
    ARPG_REQUIRE(summary.relay_overflow == 0U);
    ARPG_REQUIRE(summary.combat_overflow == 0U);
    ARPG_REQUIRE(summary.input_overflow == 0U);
    return {};
}

arpg::test::Failure launcher_input_robot_clears_thousand_minimal_committed_rooms() noexcept {
    constexpr std::uint64_t kSeed = 0x5245414C494E5055ULL;
    const DungeonRules rules = single_chaser_rules();
    const auto initial = arpg::dungeon::make_initial_run_state(kSeed, rules);
    ARPG_REQUIRE(initial.fault == arpg::dungeon::DungeonFault::none);
    DungeonSession session{rules, initial.state};
    StressSummary summary{};
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    for (std::size_t room = 0; room < 1000U; ++room) {
        RealInputTrace trace{};
        const bool cleared = drive_real_input_clear(
            session, summary, trace, arpg::combat::Action::launcher);
        if (!cleared) {
            print_real_input_trace("launcher-1000-failure", trace, false);
        }
        ARPG_REQUIRE(cleared);
        ARPG_REQUIRE(drive_exit(session, kRoute[room % kRoute.size()], summary,
            true));
    }
    const DungeonSnapshot final = session.snapshot();
    std::printf("[real-input] launcher-1000 rooms=%llu allocation-delta=%llu "
        "overflow=%u/%u/%u/%u\n",
        static_cast<unsigned long long>(final.room_index),
        static_cast<unsigned long long>(arpg::test::allocation_count()
            - allocations_before),
        summary.dungeon_overflow, summary.relay_overflow,
        summary.combat_overflow, summary.input_overflow);
    ARPG_REQUIRE(final.room_index == 1000U);
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    ARPG_REQUIRE(summary.dungeon_overflow == 0U);
    ARPG_REQUIRE(summary.relay_overflow == 0U);
    ARPG_REQUIRE(summary.combat_overflow == 0U);
    ARPG_REQUIRE(summary.input_overflow == 0U);
    return {};
}

arpg::test::Failure launcher_robot_clears_room38_reverse_deadband_regression() noexcept {
    constexpr std::uint64_t kSeed = 0x5245414C494E5055ULL;
    const DungeonRules rules = single_chaser_rules();
    auto built = arpg::dungeon::make_initial_run_state(kSeed, rules);
    ARPG_REQUIRE(built.fault == arpg::dungeon::DungeonFault::none);
    for (std::size_t room = 0; room < 38U; ++room) {
        built = arpg::dungeon::make_door_transition(
            built.state, kRoute[room % kRoute.size()], rules);
        ARPG_REQUIRE(built.fault == arpg::dungeon::DungeonFault::none);
    }
    DungeonSession session{rules, built.state};
    StressSummary summary{};
    RealInputTrace trace{};
    const bool cleared = drive_real_input_clear(
        session, summary, trace, arpg::combat::Action::launcher);
    if (!cleared) {
        print_real_input_trace("room38-reverse-deadband-red", trace, false);
    }
    ARPG_REQUIRE(cleared);
    ARPG_REQUIRE(trace.action_accepted > 0U);
    ARPG_REQUIRE(trace.swings > 0U);
    ARPG_REQUIRE(trace.monster_hits > 0U);
    return {};
}

arpg::test::Failure ten_thousand_director_plans_are_legal_deterministic_and_allocation_free() noexcept {
    const EncounterDirectorConfig config{};
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    for (std::uint64_t index = 0; index < 10000U; ++index) {
        const checkpoint::DungeonElement ecology =
            static_cast<checkpoint::DungeonElement>(index % 4U);
        const std::uint64_t seed = 0xD1EC70A000000000ULL + index * 7919U;
        const std::uint64_t depth = 1U + index % 1000U;
        const auto first = arpg::dungeon::build_encounter_plan(
            seed, depth, ecology, config);
        const auto second = arpg::dungeon::build_encounter_plan(
            seed, depth, ecology, config);
        ARPG_REQUIRE(first.fault == arpg::dungeon::DungeonFault::none);
        ARPG_REQUIRE(second.fault == arpg::dungeon::DungeonFault::none);
        ARPG_REQUIRE(arpg::test::same_encounter_plan(first.plan, second.plan));
        ARPG_REQUIRE(arpg::dungeon::encounter_plan_legal(first.plan, config));
        for (std::size_t wave = 0; wave < first.plan.wave_count; ++wave) {
            ARPG_REQUIRE(first.plan.waves[wave].spawn_count
                <= arpg::combat::kEncounterSpawnCapacity);
        }
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    return {};
}

arpg::test::Failure identical_seed_and_route_are_field_equal() noexcept {
    arpg::dungeon::DungeonSessionConfig config;
    config.root_seed = 0x1020304050607080ULL;
    DungeonSession lhs{config};
    DungeonSession rhs{config};
    ARPG_REQUIRE(same_snapshot(lhs.snapshot(), rhs.snapshot()));

    std::size_t exits = 0;
    for (int tick = 0; tick < 100000 && exits < 4U; ++tick) {
        const DungeonSnapshot state = lhs.snapshot();
        MovementInput movement{};
        if (state.phase == RoomPhase::combat && state.combat.has_value()) {
            arpg::test::force_defeat_current_wave(lhs);
            arpg::test::force_defeat_current_wave(rhs);
        } else if (state.phase == RoomPhase::awaiting_exit) {
            const ExitDirection direction = kRoute[exits];
            movement = align_center(state, direction);
            if (movement.x == 0 && movement.y == 0) {
                movement = outward(direction);
            }
        }
        const std::uint64_t before_index = state.room_index;
        ARPG_REQUIRE(tick_equal(lhs, rhs, movement));
        if (lhs.snapshot().phase == RoomPhase::committing) {
            ARPG_REQUIRE(commit_equal(lhs, rhs));
        }
        if (lhs.snapshot().room_index == before_index + 1U) {
            ++exits;
        }
    }
    ARPG_REQUIRE(exits == 4U);
    return {};
}

arpg::test::Failure one_changed_direction_changes_only_committed_room() noexcept {
    arpg::dungeon::DungeonSessionConfig config;
    config.root_seed = 0x9988776655443322ULL;
    DungeonSession up{config};
    DungeonSession right{config};
    StressSummary up_summary{};
    StressSummary right_summary{};
    ARPG_REQUIRE(drive_clear(up, up_summary));
    ARPG_REQUIRE(drive_clear(right, right_summary));
    ARPG_REQUIRE(drive_exit(up, ExitDirection::up, up_summary, true));
    ARPG_REQUIRE(drive_exit(right, ExitDirection::right, right_summary, true));
    const DungeonSnapshot a = up.snapshot();
    const DungeonSnapshot b = right.snapshot();
    ARPG_REQUIRE(a.room_index == 1U && b.room_index == 1U);
    ARPG_REQUIRE(a.room_seed != b.room_seed);
    ARPG_REQUIRE(a.room_seed == arpg::dungeon::derive_next_room_seed(
        arpg::dungeon::derive_initial_room_seed(config.root_seed, 0U),
        1U, ExitDirection::up));
    ARPG_REQUIRE(b.room_seed == arpg::dungeon::derive_next_room_seed(
        arpg::dungeon::derive_initial_room_seed(config.root_seed, 0U),
        1U, ExitDirection::right));
    ARPG_REQUIRE(a.entry_side == arpg::dungeon::EntrySide::bottom);
    ARPG_REQUIRE(b.entry_side == arpg::dungeon::EntrySide::left);
    ARPG_REQUIRE(!same_vec(a.combat->player.position, b.combat->player.position));
    return {};
}

arpg::test::Failure fixed_seed_room_trace_matches_baseline_golden() noexcept {
    std::array<GoldenTraceEntry, kGoldenTraceRoomCount> trace{};
    ARPG_REQUIRE(generate_golden_room_trace(trace));
    ARPG_REQUIRE(golden_room_trace_hash(trace) == 0xb0233e1750ad0926ULL);

    const GoldenTraceEntry& first = trace[0U];
    ARPG_REQUIRE(first.depth == 1U);
    ARPG_REQUIRE(first.ecology == checkpoint::DungeonElement::chaos);
    ARPG_REQUIRE(first.monster_count == 3U);
    ARPG_REQUIRE(first.monster_ids[0U] == arpg::combat::MonsterId::chaos_chaser);
    ARPG_REQUIRE(first.monster_ids[1U] == arpg::combat::MonsterId::chaos_chaser);
    ARPG_REQUIRE(first.monster_ids[2U] == arpg::combat::MonsterId::lightning_shooter);
    ARPG_REQUIRE(!first.has_hole);
    ARPG_REQUIRE(!first.is_abyss);
    ARPG_REQUIRE(first.next_seed == 0x163aec04f68d8227ULL);

    const GoldenTraceEntry& middle = trace[127U];
    ARPG_REQUIRE(middle.depth == 1U);
    ARPG_REQUIRE(middle.ecology == checkpoint::DungeonElement::fire);
    ARPG_REQUIRE(middle.monster_count == 3U);
    ARPG_REQUIRE(middle.monster_ids[0U] == arpg::combat::MonsterId::chaos_chaser);
    ARPG_REQUIRE(middle.monster_ids[1U] == arpg::combat::MonsterId::fire_charger);
    ARPG_REQUIRE(middle.monster_ids[2U] == arpg::combat::MonsterId::chaos_chaser);
    ARPG_REQUIRE(!middle.has_hole);
    ARPG_REQUIRE(!middle.is_abyss);
    ARPG_REQUIRE(middle.next_seed == 0x67e35554c0918040ULL);

    const GoldenTraceEntry& last = trace[255U];
    ARPG_REQUIRE(last.depth == 1U);
    ARPG_REQUIRE(last.ecology == checkpoint::DungeonElement::lightning);
    ARPG_REQUIRE(last.monster_count == 3U);
    ARPG_REQUIRE(last.monster_ids[0U] == arpg::combat::MonsterId::chaos_chaser);
    ARPG_REQUIRE(last.monster_ids[1U] == arpg::combat::MonsterId::lightning_shooter);
    ARPG_REQUIRE(last.monster_ids[2U] == arpg::combat::MonsterId::lightning_dasher);
    ARPG_REQUIRE(!last.has_hole);
    ARPG_REQUIRE(!last.is_abyss);
    ARPG_REQUIRE(last.next_seed == 0x6c0eca473cb86cf2ULL);
    return {};
}

arpg::test::Failure thousand_real_rooms_preserve_single_world_invariants() noexcept {
    DungeonSession session;
    StressSummary summary{};
    ARPG_REQUIRE(drive_rooms(session, 1000U, summary, true));
    const DungeonSnapshot final = session.snapshot();
    ARPG_REQUIRE(final.room_index == 1000U);
    ARPG_REQUIRE(final.phase == RoomPhase::combat);
    ARPG_REQUIRE(final.has_active_room);
    ARPG_REQUIRE(final.combat.has_value());
    ARPG_REQUIRE(final.combat->tick == 0U);
    return {};
}

arpg::test::Failure measured_thousand_rooms_allocate_nothing_and_never_overflow() noexcept {
    {
        DungeonSession warm_up;
        StressSummary warm_summary{};
        ARPG_REQUIRE(drive_rooms(warm_up, 1U, warm_summary));
    }

    DungeonSession measured;
    StressSummary summary{};
    drain(measured, summary);
    const std::uint64_t before = arpg::test::allocation_count();
    ARPG_REQUIRE(drive_rooms(measured, 1000U, summary));
    const std::uint64_t after = arpg::test::allocation_count();
    const DungeonSnapshot final = measured.snapshot();
    std::printf(
        "[stress] exits=1000 index=%llu allocation_before=%llu "
        "allocation_after=%llu delta=%llu dungeon_overflow=%u "
        "relay_overflow=%u combat_overflow=%u input_overflow=%u\n",
        static_cast<unsigned long long>(final.room_index),
        static_cast<unsigned long long>(before),
        static_cast<unsigned long long>(after),
        static_cast<unsigned long long>(after - before),
        summary.dungeon_overflow,
        summary.relay_overflow,
        summary.combat_overflow,
        summary.input_overflow);
    ARPG_REQUIRE(final.room_index == 1000U);
    ARPG_REQUIRE(after == before);
    ARPG_REQUIRE(summary.dungeon_overflow == 0U);
    ARPG_REQUIRE(summary.relay_overflow == 0U);
    ARPG_REQUIRE(summary.combat_overflow == 0U);
    ARPG_REQUIRE(summary.input_overflow == 0U);
    ARPG_REQUIRE(final.diagnostics.event_overflow_count == 0U);
    ARPG_REQUIRE(final.diagnostics.combat_relay_overflow_count == 0U);
    ARPG_REQUIRE(final.combat->diagnostics.event_overflow_count == 0U);
    return {};
}

arpg::test::Failure passive_star_chart_end_to_end_trace_is_deterministic() noexcept {
    std::array<PassiveStressRecord, 1000U> first{};
    std::array<PassiveStressRecord, 1000U> second{};
    ARPG_REQUIRE(generate_passive_stress_trace(first));
    ARPG_REQUIRE(generate_passive_stress_trace(second));
    for (std::size_t index = 0U; index < first.size(); ++index) {
        ARPG_REQUIRE(same_record(first[index], second[index]));
        ARPG_REQUIRE(first[index].allocated_bits != 0U);
        ARPG_REQUIRE(first[index].generation != 0U);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"launcher robot clears room38 reverse deadband regression",
     &launcher_robot_clears_room38_reverse_deadband_regression},
    {"launcher input robot clears ten minimal committed rooms",
     &launcher_input_robot_clears_ten_minimal_committed_rooms},
    {"launcher input robot clears thousand minimal committed rooms",
     &launcher_input_robot_clears_thousand_minimal_committed_rooms},
    {"ten thousand director plans are legal deterministic and allocation free",
     &ten_thousand_director_plans_are_legal_deterministic_and_allocation_free},
    {"identical seed and route are field equal", &identical_seed_and_route_are_field_equal},
    {"one changed direction changes only committed room", &one_changed_direction_changes_only_committed_room},
    {"fixed seed room trace matches baseline golden", &fixed_seed_room_trace_matches_baseline_golden},
    {"thousand real rooms preserve single world invariants", &thousand_real_rooms_preserve_single_world_invariants},
    {"measured thousand rooms allocate nothing and never overflow", &measured_thousand_rooms_allocate_nothing_and_never_overflow},
    {"passive star chart end to end trace is deterministic", &passive_star_chart_end_to_end_trace_is_deterministic},
};

}  // namespace

arpg::test::TestSuite dungeon_stress_suite() noexcept {
    return arpg::test::make_suite("dungeon_stress", kCases);
}
