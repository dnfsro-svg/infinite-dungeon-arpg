#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon_test_support.hpp"

#include "abyss/abyss_rules.hpp"
#include "checkpoint/room_checkpoint_validation.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_render_snapshot.hpp"
#include "dungeon/room_affix.hpp"
#include "dungeon/room_monster_plan_builder.hpp"
#include "persistence/room_progress_codec.hpp"
#include "persistence/save_commit_worker.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

namespace checkpoint = arpg::checkpoint;
namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;

constexpr std::uint64_t kRootSeed = 0x4C41524745524F4FULL;
constexpr std::uint64_t kFinalRevision = 0x1100U;
constexpr std::array<std::size_t, 4U> kReloadSchedules{{0U, 1U, 7U, 31U}};

struct Scenario final {
    const char* name{};
    std::uint16_t population{};
    bool abyss{};
};

constexpr std::array<Scenario, 3U> kScenarios{{
    {"normal-min", 300U, false},
    {"normal-max", 750U, false},
    {"abyss-max", 1125U, true},
}};

struct Trace final {
    std::uint16_t population{};
    std::uint64_t monster_blueprint_hash{};
    std::uint64_t environment_blueprint_hash{};
    std::uint64_t exit_open_tick{};
    std::vector<dungeon::DungeonEvent> dungeon_events{};
    std::vector<arpg::combat::CombatEvent> combat_events{};
    std::vector<std::vector<std::uint8_t>> reload_checkpoint_bytes{};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> final_checkpoint{};
    std::unique_ptr<std::uint8_t[]> checkpoint_bytes{};
    std::size_t checkpoint_size{};
    dungeon::DungeonRunState transition_target{};
    dungeon::TransitionKind transition{dungeon::TransitionKind::none};
    dungeon::ExitDirection direction{dungeon::ExitDirection::none};
    std::size_t reloads{};
};

bool same_event(const dungeon::DungeonEvent& left,
    const dungeon::DungeonEvent& right) noexcept {
    return left.kind == right.kind && left.session_tick == right.session_tick
        && left.room_index == right.room_index
        && left.room_seed == right.room_seed
        && left.destination_room_index == right.destination_room_index
        && left.destination_room_seed == right.destination_room_seed
        && left.transition == right.transition
        && left.direction == right.direction
        && left.abyss_pending_rewards == right.abyss_pending_rewards
        && left.abyss_unpicked_rewards == right.abyss_unpicked_rewards;
}

bool same_combat_event(const arpg::combat::CombatEvent& left,
    const arpg::combat::CombatEvent& right) noexcept {
    return left.kind == right.kind && left.tick == right.tick
        && left.attack == right.attack && left.skill == right.skill
        && left.strike_index == right.strike_index
        && left.finisher == right.finisher
        && left.target_ordinal == right.target_ordinal
        && left.hit_count == right.hit_count && left.feedback == right.feedback
        && left.position.x == right.position.x
        && left.position.y == right.position.y
        && left.position.z == right.position.z && left.value == right.value
        && left.monster_id == right.monster_id
        && left.spawn_ordinal == right.spawn_ordinal
        && left.affix_score == right.affix_score
        && left.reward_eligible == right.reward_eligible;
}

bool has_exact_full_defeat_bits(
    const checkpoint::RoomProgressCheckpoint& room,
    std::uint32_t population) noexcept {
    for (std::size_t word = 0U; word < room.defeat_bits.size(); ++word) {
        const std::size_t begin = word * 64U;
        const std::size_t remaining = begin >= population ? 0U
            : (std::min)(std::size_t{64U}, population - begin);
        const std::uint64_t expected = remaining == 64U
            ? (std::numeric_limits<std::uint64_t>::max)()
            : remaining == 0U ? 0U
            : (std::uint64_t{1U} << remaining) - 1U;
        if (room.defeat_bits[word] != expected) return false;
    }
    return true;
}

void drain_events(dungeon::DungeonSession& session, Trace& trace) {
    while (const auto event = session.try_pop_event()) {
        if (event->kind == dungeon::DungeonEventKind::exits_opened
                && trace.exit_open_tick == 0U) {
            trace.exit_open_tick = event->session_tick;
        }
        trace.dungeon_events.push_back(*event);
    }
    while (const auto event = session.try_pop_combat_event()) {
        trace.combat_events.push_back(*event);
    }
}

std::uint64_t scenario_seed(const Scenario& scenario) noexcept {
    for (std::uint64_t seed = 1U; seed < 2'000'000U; ++seed) {
        const dungeon::RoomDensityRoll density =
            dungeon::roll_room_density(seed, scenario.abyss);
        if (density.total_count == scenario.population
                && (!scenario.abyss || arpg::abyss::is_abyss_roll(seed))) {
            return seed;
        }
    }
    return 0U;
}

std::unique_ptr<dungeon::DungeonSession> make_scenario_session(
    const Scenario& scenario, const dungeon::DungeonRules& rules) {
    const std::uint64_t seed = scenario_seed(scenario);
    if (seed == 0U) return {};
    auto built = dungeon::make_initial_run_state(kRootSeed, rules);
    if (built.fault != dungeon::DungeonFault::none) return {};
    auto& state = built.state;
    state.current_room.seed = seed;
    state.current_room.depth = 40U;
    state.current_room.entry = dungeon::EntrySide::left;
    state.current_room.ecology = scenario.abyss
        ? dungeon::DungeonElement::chaos : dungeon::DungeonElement::fire;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = scenario.abyss;
    state.last_transition = dungeon::TransitionKind::door;
    state.last_direction = dungeon::ExitDirection::right;
    if (scenario.abyss) {
        const auto selection = arpg::abyss::select_abyss_rule(seed, 40U);
        if (!selection.has_value()) return {};
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::available;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
    }
    return std::unique_ptr<dungeon::DungeonSession>{
        new (std::nothrow) dungeon::DungeonSession{rules, state}};
}

bool commit_current(dungeon::DungeonSession& session) noexcept {
    const dungeon::PendingSave* const pending = session.pending_save_view();
    if (pending == nullptr) return false;
    const dungeon::PendingSaveKind kind = pending->kind;
    const std::uint64_t generation = pending->expected_generation;
    auto durable = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    if (durable == nullptr) return false;
    try {
        durable->state.item_ownership.items.reserve(
            pending->next_state.item_ownership.items.size());
    } catch (...) {
        return false;
    }
    if (!session.capture_save_checkpoint(
            *durable, generation, &pending->next_state)) {
        return false;
    }
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        generation, durable->state, kind});
    return session.snapshot().phase != dungeon::RoomPhase::faulted;
}

bool enter_combat(dungeon::DungeonSession& session, Trace& trace) {
    for (std::size_t step = 0U; step < 4U; ++step) {
        const dungeon::DungeonSnapshot state = session.snapshot();
        if (state.phase == dungeon::RoomPhase::combat) {
            arpg::test::set_player_health(session, 1'000'000, 1'000'000);
            drain_events(session, trace);
            return true;
        }
        if (state.phase == dungeon::RoomPhase::committing) {
            if (!commit_current(session)) return false;
        } else {
            session.tick({});
        }
        drain_events(session, trace);
    }
    return false;
}

bool reload_session(std::unique_ptr<dungeon::DungeonSession>& session,
    const dungeon::DungeonRules& rules, Trace& trace) {
    const auto fail = [](const char* stage) noexcept {
        std::fprintf(stderr, "[task11-reload-fail] stage=%s\n", stage);
        return false;
    };
    auto source = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    auto decoded = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    auto bytes = std::unique_ptr<std::uint8_t[]>{
        new (std::nothrow)
            std::uint8_t[persistence::kMaximumEncodedCheckpointBytes]};
    if (source == nullptr || decoded == nullptr || bytes == nullptr) {
        return fail("allocate");
    }
    try {
        const std::size_t item_count = session->item_state().items.size();
        source->state.item_ownership.items.reserve(item_count);
        decoded->state.item_ownership.items.reserve(item_count);
    } catch (...) {
        return fail("reserve");
    }
    const std::uint64_t revision = session->snapshot().commit_generation;
    if (!session->capture_save_checkpoint(*source, revision)) {
        return fail("capture");
    }
    std::size_t written{};
    if (persistence::encode_checkpoint_v10_into(*source, bytes.get(),
            persistence::kMaximumEncodedCheckpointBytes, written)
            != persistence::CodecError::none) {
        return fail("encode");
    }
    try {
        trace.reload_checkpoint_bytes.emplace_back(
            bytes.get(), bytes.get() + written);
    } catch (...) {
        return fail("record-bytes");
    }
    bool migrated = true;
    if (persistence::decode_checkpoint_v10_into(bytes.get(), written,
            *decoded, migrated) != persistence::CodecError::none
            || migrated
            || !dungeon::same_run_state(source->state, decoded->state)
            || !checkpoint::same_room_progress_checkpoint(
                source->room_progress, decoded->room_progress)) {
        return fail("decode-compare");
    }
    auto restarted = std::unique_ptr<dungeon::DungeonSession>{
        new (std::nothrow) dungeon::DungeonSession{rules, decoded->state}};
    if (restarted != nullptr) {
        arpg::test::set_player_health(*restarted,
            decoded->room_progress.combat.player.hp,
            decoded->room_progress.combat.player.max_hp);
    }
    if (restarted == nullptr
            || !restarted->restore_room_progress_checkpoint(*decoded)) {
        return fail("restore");
    }
    session = std::move(restarted);
    ++trace.reloads;
    drain_events(*session, trace);
    return session->snapshot().phase != dungeon::RoomPhase::faulted
        ? true : fail("faulted");
}

bool settle_full_clear(dungeon::DungeonSession& session, Trace& trace) {
    for (std::size_t step = 0U; step < 8U; ++step) {
        const dungeon::DungeonSnapshot state = session.snapshot();
        if (state.phase == dungeon::RoomPhase::cleared
                || state.phase == dungeon::RoomPhase::awaiting_exit) {
            return true;
        }
        if (state.phase == dungeon::RoomPhase::committing) {
            if (!commit_current(session)) return false;
        } else {
            session.tick({});
        }
        drain_events(session, trace);
    }
    return false;
}

bool capture_final_checkpoint(dungeon::DungeonSession& session,
    Trace& trace) {
    trace.final_checkpoint.reset(
        new (std::nothrow) checkpoint::SaveCheckpointSlot{});
    trace.checkpoint_bytes.reset(new (std::nothrow)
        std::uint8_t[persistence::kMaximumEncodedCheckpointBytes]);
    if (trace.final_checkpoint == nullptr || trace.checkpoint_bytes == nullptr
            || !session.capture_save_checkpoint(
                *trace.final_checkpoint, kFinalRevision)) {
        return false;
    }
    return persistence::encode_checkpoint_v10_into(
        *trace.final_checkpoint, trace.checkpoint_bytes.get(),
        persistence::kMaximumEncodedCheckpointBytes, trace.checkpoint_size)
        == persistence::CodecError::none;
}

bool reach_transition(dungeon::DungeonSession& session, Trace& trace) {
    const auto fail = [&session](const char* stage) noexcept {
        const dungeon::DungeonSnapshot state = session.snapshot();
        const dungeon::PendingSave* pending = session.pending_save_view();
        std::fprintf(stderr,
            "[task11-transition-fail] stage=%s phase=%u pending=%d kind=%u "
            "armed=%d rewards=%u/%u player=%.3f,%.3f\n",
            stage, static_cast<unsigned int>(state.phase), pending != nullptr,
            pending == nullptr ? 255U
                : static_cast<unsigned int>(pending->kind),
            state.abyss_exit_confirmation_armed,
            static_cast<unsigned int>(state.abyss_pending_rewards),
            static_cast<unsigned int>(state.abyss_unpicked_rewards),
            state.combat.has_value() ? state.combat->player.position.x : 0.0F,
            state.combat.has_value() ? state.combat->player.position.y : 0.0F);
        return false;
    };
    for (std::size_t settle = 0U; settle < 24U; ++settle) {
        const dungeon::DungeonSnapshot state = session.snapshot();
        if (state.phase == dungeon::RoomPhase::awaiting_exit
                && session.pending_save_view() == nullptr
                && state.abyss_pending_rewards == 0U) {
            break;
        }
        if (state.phase == dungeon::RoomPhase::committing) {
            if (!commit_current(session)) return fail("settle-commit");
        } else {
            session.tick({});
        }
        drain_events(session, trace);
    }
    if (session.snapshot().phase != dungeon::RoomPhase::awaiting_exit
            || session.pending_save_view() != nullptr) {
        return fail("settle");
    }

    constexpr dungeon::ExitDirection kDirection =
        dungeon::ExitDirection::left;
    arpg::test::set_player_position(
        session, arpg::test::exit_boundary_position(kDirection));
    arpg::test::attempt_exit(session, kDirection);
    drain_events(session, trace);
    if (session.pending_save_view() == nullptr
            && session.snapshot().abyss_exit_confirmation_armed) {
        session.tick({});
        drain_events(session, trace);
        session.tick({-1, 0});
        drain_events(session, trace);
    }
    const dungeon::PendingSave* const pending = session.pending_save_view();
    if (pending == nullptr
            || (pending->kind != dungeon::PendingSaveKind::transition
                && pending->kind != dungeon::PendingSaveKind::abyss_abandon)) {
        return fail("pending-transition");
    }
    trace.transition_target = pending->next_state;
    trace.transition = pending->transition;
    trace.direction = pending->direction;
    return true;
}

bool generate_trace(const Scenario& scenario, std::size_t reload_count,
    Trace& trace) {
    const auto fail = [&scenario, reload_count](
                          const char* stage, std::size_t ordinal) noexcept {
        std::fprintf(stderr,
            "[task11-trace-fail] scenario=%s reloads=%zu stage=%s ordinal=%zu\n",
            scenario.name, reload_count, stage, ordinal);
        return false;
    };
    const dungeon::DungeonRules rules{};
    auto session = make_scenario_session(scenario, rules);
    if (session == nullptr || !enter_combat(*session, trace)) {
        return fail("enter-combat", 0U);
    }
    const arpg::combat::RoomMonsterPlan* plan =
        arpg::test::room_monster_plan(*session);
    const arpg::combat::RoomEnvironmentBlueprint* environment =
        arpg::test::room_environment_blueprint(*session);
    if (plan == nullptr || environment == nullptr
            || plan->monster_count != scenario.population) {
        return fail("blueprint", 0U);
    }
    trace.population = plan->monster_count;
    trace.monster_blueprint_hash = plan->blueprint_hash;
    trace.environment_blueprint_hash = environment->blueprint_hash;
    const std::uint32_t required = dungeon::required_kills(trace.population);
    try {
        trace.combat_events.reserve(
            static_cast<std::size_t>(trace.population) * 2U);
        trace.dungeon_events.reserve(
            static_cast<std::size_t>(trace.population) * 2U);
        trace.reload_checkpoint_bytes.reserve(reload_count);
    } catch (...) {
        return fail("reserve-trace", 0U);
    }

    std::size_t completed_reloads{};
    for (std::uint16_t ordinal = 0U; ordinal < trace.population; ++ordinal) {
        std::array<arpg::combat::CombatEvent, 2U> observed_events{};
        std::size_t observed_count{};
        const auto defeat_event = arpg::test::defeat_room_monster_by_ordinal(
            *session, ordinal, observed_events.data(), observed_events.size(),
            observed_count);
        if (!defeat_event.has_value()) return fail("defeat", ordinal);
        for (std::size_t index{}; index < observed_count; ++index) {
            trace.combat_events.push_back(observed_events[index]);
        }
        drain_events(*session, trace);
        const std::uint32_t defeated_count = ordinal + 1U;
        if (defeated_count == required) {
            session->tick({});
            drain_events(*session, trace);
            if (session->pending_save_view() == nullptr
                    || session->pending_save_view()->kind
                        != dungeon::PendingSaveKind::room_unlock
                    || !commit_current(*session)) {
                return fail("unlock-commit", ordinal);
            }
            drain_events(*session, trace);
        }

        const std::size_t next_reload = completed_reloads + 1U;
        const std::size_t reload_defeat = reload_count == 0U ? 0U
            : next_reload * trace.population / (reload_count + 1U);
        if (completed_reloads < reload_count
                && defeated_count == reload_defeat) {
            if (!reload_session(session, rules, trace)) {
                return fail("reload", ordinal);
            }
            ++completed_reloads;
            plan = arpg::test::room_monster_plan(*session);
            environment = arpg::test::room_environment_blueprint(*session);
            if (plan == nullptr
                    || environment == nullptr
                    || plan->blueprint_hash != trace.monster_blueprint_hash
                    || environment->blueprint_hash
                        != trace.environment_blueprint_hash) {
                return fail("reload-blueprint", ordinal);
            }
        }
    }
    if (completed_reloads != reload_count) return fail("reload-count", 0U);
    if (!settle_full_clear(*session, trace)) return fail("full-clear", 0U);
    if (!capture_final_checkpoint(*session, trace)) {
        return fail("final-checkpoint", 0U);
    }
    if (!reach_transition(*session, trace)) return fail("transition", 0U);
    const auto& room = trace.final_checkpoint->room_progress;
    const bool any_secondary_claim = std::any_of(
        room.secondary_claim_bits.begin(), room.secondary_claim_bits.end(),
        [](std::uint64_t word) noexcept { return word != 0U; });
    const bool any_material = std::any_of(
        trace.final_checkpoint->state.item_ownership.materials.begin(),
        trace.final_checkpoint->state.item_ownership.materials.end(),
        [](std::uint64_t count) noexcept { return count != 0U; });
    const bool valid = trace.reloads == reload_count
        && room.generated_monsters == trace.population
        && room.defeated_monsters == trace.population
        && room.required_kills == required && room.exits_unlocked
        && room.full_clear && room.reward_committed
        && any_secondary_claim && any_material
        && has_exact_full_defeat_bits(room, trace.population)
        && trace.exit_open_tick != 0U
        && trace.transition == dungeon::TransitionKind::door
        && trace.direction == dungeon::ExitDirection::left;
    return valid ? true : fail("final-contract", 0U);
}

bool same_trace(const Trace& left, const Trace& right) noexcept {
    if (left.population != right.population
            || left.monster_blueprint_hash != right.monster_blueprint_hash
            || left.environment_blueprint_hash
                != right.environment_blueprint_hash
            || left.exit_open_tick != right.exit_open_tick
            || left.reloads != right.reloads
            || left.dungeon_events.size() != right.dungeon_events.size()
            || left.combat_events.size() != right.combat_events.size()
            || left.reload_checkpoint_bytes
                != right.reload_checkpoint_bytes
            || left.final_checkpoint == nullptr
            || right.final_checkpoint == nullptr
            || !checkpoint::same_room_progress_checkpoint(
                left.final_checkpoint->room_progress,
                right.final_checkpoint->room_progress)
            || !dungeon::same_run_state(left.final_checkpoint->state,
                right.final_checkpoint->state)
            || !dungeon::same_run_state(
                left.transition_target, right.transition_target)
            || left.transition != right.transition
            || left.direction != right.direction
            || left.checkpoint_size != right.checkpoint_size
            || std::memcmp(left.checkpoint_bytes.get(),
                right.checkpoint_bytes.get(), left.checkpoint_size) != 0) {
        return false;
    }
    for (std::size_t index = 0U; index < left.dungeon_events.size(); ++index) {
        if (!same_event(left.dungeon_events[index],
                right.dungeon_events[index])) {
            return false;
        }
    }
    for (std::size_t index = 0U; index < left.combat_events.size(); ++index) {
        if (!same_combat_event(left.combat_events[index],
                right.combat_events[index])) {
            return false;
        }
    }
    return true;
}

bool same_cross_schedule_result(
    const Trace& left, const Trace& right) noexcept {
    if (left.final_checkpoint == nullptr
            || right.final_checkpoint == nullptr) {
        return false;
    }
    const checkpoint::SaveCheckpointSlot& a = *left.final_checkpoint;
    const checkpoint::SaveCheckpointSlot& b = *right.final_checkpoint;
    return left.population == right.population
        && left.monster_blueprint_hash == right.monster_blueprint_hash
        && left.environment_blueprint_hash
            == right.environment_blueprint_hash
        && a.persistence_revision == b.persistence_revision
        && dungeon::same_run_state(a.state, b.state)
        && checkpoint::same_room_progress_checkpoint(
            a.room_progress, b.room_progress)
        && dungeon::same_run_state(
            left.transition_target, right.transition_target)
        && left.transition == right.transition
        && left.direction == right.direction;
}

void report_run_state_difference(const dungeon::DungeonRunState& left,
    const dungeon::DungeonRunState& right) noexcept {
    std::fprintf(stderr,
        "[task11-state-diff] commit=%llu/%llu room=%llu/%llu seed=%llu/%llu "
        "level=%u/%u xp=%llu/%llu items=%zu/%zu next_item=%llu/%llu "
        "materials=%d equipment=%d claims=%d passive=%d skills=%d "
        "last=%u,%u/%u,%u\n",
        static_cast<unsigned long long>(left.commit_generation),
        static_cast<unsigned long long>(right.commit_generation),
        static_cast<unsigned long long>(left.current_room.index),
        static_cast<unsigned long long>(right.current_room.index),
        static_cast<unsigned long long>(left.current_room.seed),
        static_cast<unsigned long long>(right.current_room.seed),
        static_cast<unsigned int>(left.progression.level),
        static_cast<unsigned int>(right.progression.level),
        static_cast<unsigned long long>(left.progression.experience),
        static_cast<unsigned long long>(right.progression.experience),
        left.item_ownership.items.size(), right.item_ownership.items.size(),
        static_cast<unsigned long long>(left.item_ownership.next_item_sequence),
        static_cast<unsigned long long>(right.item_ownership.next_item_sequence),
        left.item_ownership.materials == right.item_ownership.materials,
        left.item_ownership.equipment.equipped_ids
            == right.item_ownership.equipment.equipped_ids,
        left.item_ownership.claimed_drop_bits
                == right.item_ownership.claimed_drop_bits
            && left.item_ownership.material_claimed_drop_bits
                == right.item_ownership.material_claimed_drop_bits,
        left.passive_tree.allocated_bits == right.passive_tree.allocated_bits,
        left.skill_loadout.owned_active_bits
            == right.skill_loadout.owned_active_bits,
        static_cast<unsigned int>(left.last_transition),
        static_cast<unsigned int>(left.last_direction),
        static_cast<unsigned int>(right.last_transition),
        static_cast<unsigned int>(right.last_direction));
}

void report_room_checkpoint_difference(
    const checkpoint::RoomProgressCheckpoint& left,
    const checkpoint::RoomProgressCheckpoint& right) noexcept {
    const auto& a = left.combat;
    const auto& b = right.combat;
    std::fprintf(stderr,
        "[task11-room-diff] pending=%llu/%llu bits=%d,%d,%d "
        "ground=%u,%u/%u,%u combat_same=%d tick=%llu/%llu rng=%d "
        "player=%.3f,%.3f,%u/%0.3f,%.3f,%u cooldown=%d "
        "attack=%u,%u/%u,%u monsters=%u/%u obstacles=%u/%u "
        "history=%llu/%llu\n",
        static_cast<unsigned long long>(left.pending_room_experience),
        static_cast<unsigned long long>(right.pending_room_experience),
        left.defeat_bits == right.defeat_bits,
        left.equipment_claim_bits == right.equipment_claim_bits,
        left.secondary_claim_bits == right.secondary_claim_bits,
        static_cast<unsigned int>(left.equipment_ground_count),
        static_cast<unsigned int>(left.secondary_ground_count),
        static_cast<unsigned int>(right.equipment_ground_count),
        static_cast<unsigned int>(right.secondary_ground_count),
        checkpoint::same_room_combat_checkpoint(a, b),
        static_cast<unsigned long long>(a.tick),
        static_cast<unsigned long long>(b.tick),
        a.evasion_rng_state == b.evasion_rng_state,
        a.player.position.x, a.player.position.y,
        static_cast<unsigned int>(a.player.state),
        b.player.position.x, b.player.position.y,
        static_cast<unsigned int>(b.player.state),
        a.player.skill_cooldowns == b.player.skill_cooldowns,
        static_cast<unsigned int>(a.attack.id),
        static_cast<unsigned int>(a.attack.elapsed_ticks),
        static_cast<unsigned int>(b.attack.id),
        static_cast<unsigned int>(b.attack.elapsed_ticks),
        static_cast<unsigned int>(a.monster_count),
        static_cast<unsigned int>(b.monster_count),
        static_cast<unsigned int>(a.obstacle_count),
        static_cast<unsigned int>(b.obstacle_count),
        static_cast<unsigned long long>(
            a.player_damage_history.active_tick),
        static_cast<unsigned long long>(
            b.player_damage_history.active_tick));
    for (std::uint16_t index = 0U;
            index < (std::min)(left.equipment_ground_count,
                right.equipment_ground_count); ++index) {
        const auto& x = left.equipment_ground[index];
        const auto& y = right.equipment_ground[index];
        if (x.ordinal == y.ordinal && x.source == y.source
                && x.reward_ordinal == y.reward_ordinal
                && x.position.x == y.position.x
                && x.position.y == y.position.y
                && x.position.z == y.position.z
                && std::memcmp(&x.item, &y.item, sizeof(x.item)) == 0) {
            continue;
        }
        std::fprintf(stderr,
            "[task11-equipment-diff] index=%u ordinal=%u/%u source=%u/%u "
            "reward=%u/%u pos=%.3f,%.3f/%.3f,%.3f item=%llu/%llu "
            "base=%u/%u rarity=%u/%u\n",
            static_cast<unsigned int>(index),
            static_cast<unsigned int>(x.ordinal),
            static_cast<unsigned int>(y.ordinal),
            static_cast<unsigned int>(x.source),
            static_cast<unsigned int>(y.source),
            static_cast<unsigned int>(x.reward_ordinal),
            static_cast<unsigned int>(y.reward_ordinal),
            x.position.x, x.position.y, y.position.x, y.position.y,
            static_cast<unsigned long long>(x.item.id),
            static_cast<unsigned long long>(y.item.id),
            static_cast<unsigned int>(x.item.base_id),
            static_cast<unsigned int>(y.item.base_id),
            static_cast<unsigned int>(x.item.rarity),
            static_cast<unsigned int>(y.item.rarity));
        break;
    }
    for (std::uint16_t index = 0U;
            index < (std::min)(left.secondary_ground_count,
                right.secondary_ground_count); ++index) {
        const auto& x = left.secondary_ground[index];
        const auto& y = right.secondary_ground[index];
        if (x.tag == y.tag && x.ordinal == y.ordinal
                && x.source == y.source && x.material == y.material
                && x.position.x == y.position.x
                && x.position.y == y.position.y
                && x.position.z == y.position.z) {
            continue;
        }
        std::fprintf(stderr,
            "[task11-secondary-diff] index=%u tag=%u/%u ordinal=%u/%u "
            "source=%u/%u material=%u/%u pos=%.3f,%.3f/%.3f,%.3f\n",
            static_cast<unsigned int>(index),
            static_cast<unsigned int>(x.tag),
            static_cast<unsigned int>(y.tag),
            static_cast<unsigned int>(x.ordinal),
            static_cast<unsigned int>(y.ordinal),
            static_cast<unsigned int>(x.source),
            static_cast<unsigned int>(y.source),
            static_cast<unsigned int>(x.material),
            static_cast<unsigned int>(y.material),
            x.position.x, x.position.y, y.position.x, y.position.y);
        break;
    }
}

#if defined(_WIN32) && defined(NDEBUG)

constexpr std::size_t kWorkerCostWarmupCount = 3U;
constexpr std::size_t kWorkerCostSampleCount = 31U;

class WorkerCostDirectory final {
public:
    WorkerCostDirectory() noexcept {
        try {
            std::error_code error;
            const std::filesystem::path root =
                std::filesystem::temp_directory_path(error);
            if (error) return;
            path_ = root / ("arpg_task11_worker_cost_" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
            ready_ = std::filesystem::create_directory(path_, error) && !error;
        } catch (...) {
            ready_ = false;
        }
    }

    ~WorkerCostDirectory() {
        if (!ready_) return;
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_{};
    bool ready_{};
};

bool wait_worker_completion(persistence::SaveCommitWorker& worker,
    persistence::SaveCommitCompletion& completion) noexcept {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{30};
    while (std::chrono::steady_clock::now() < deadline) {
        if (worker.try_take_completion(completion)) return true;
        std::this_thread::yield();
    }
    return false;
}

template <std::size_t Size>
std::uint64_t percentile(
    std::array<std::uint64_t, Size> samples,
    std::size_t percentile_value) noexcept {
    static_assert(Size != 0U);
    std::sort(samples.begin(), samples.end());
    const std::size_t rank = (Size * percentile_value + 99U) / 100U;
    return samples[(std::max)(std::size_t{1U}, rank) - 1U];
}

bool measure_worker_cost(
    const checkpoint::SaveCheckpointSlot& source) noexcept {
    WorkerCostDirectory directory{};
    auto source_bytes = std::unique_ptr<std::uint8_t[]>{new (std::nothrow)
        std::uint8_t[persistence::kMaximumEncodedCheckpointBytes]};
    std::size_t source_size{};
    auto storage = std::unique_ptr<persistence::SaveCommitStorage>{
        new (std::nothrow) persistence::SaveCommitStorage{}};
    if (!directory.ready() || source_bytes == nullptr || storage == nullptr
            || persistence::encode_checkpoint_v10_into(source,
                source_bytes.get(),
                persistence::kMaximumEncodedCheckpointBytes,
                source_size) != persistence::CodecError::none
            || !storage->initialize({directory.path()})) {
        return false;
    }
    storage->release_loaded_checkpoints();
    persistence::SaveCommitWorker worker{*storage};
    if (!worker.start()) return false;

    std::array<std::uint64_t, kWorkerCostSampleCount> encode_samples{};
    std::array<std::uint64_t, kWorkerCostSampleCount> write_samples{};
    std::array<std::uint64_t, kWorkerCostSampleCount> readback_samples{};
    std::size_t encoded_bytes{};
    std::uint16_t minimum_write_operations =
        (std::numeric_limits<std::uint16_t>::max)();
    std::uint16_t minimum_readback_operations =
        (std::numeric_limits<std::uint16_t>::max)();

    constexpr std::size_t kCommitCount =
        kWorkerCostWarmupCount + kWorkerCostSampleCount;
    for (std::size_t commit_index{}; commit_index < kCommitCount;
            ++commit_index) {
        const std::uint64_t revision = kFinalRevision + commit_index + 1U;
        const auto lease = worker.acquire_capture_slot(revision,
            persistence::SaveCommitRequestKind::exact,
            0x11C057U + commit_index);
        if (lease.state != persistence::SaveCommitSubmitState::accepted) {
            return false;
        }
        persistence::SaveCommitJobSlot* const job = worker.capture_job(lease);
        if (job == nullptr) {
            worker.cancel_capture(lease);
            return false;
        }
        try {
            job->checkpoint.state.item_ownership.items.reserve(
                source.state.item_ownership.items.size());
        } catch (...) {
            worker.cancel_capture(lease);
            return false;
        }
        bool migrated = true;
        if (persistence::decode_checkpoint_v10_into(source_bytes.get(),
                source_size, job->checkpoint, migrated)
                != persistence::CodecError::none
                || migrated) {
            worker.cancel_capture(lease);
            return false;
        }
        job->checkpoint.persistence_revision = revision;
        if (worker.submit(lease).state
                != persistence::SaveCommitSubmitState::accepted) {
            worker.cancel_capture(lease);
            return false;
        }
        persistence::SaveCommitCompletion completion{};
        if (!wait_worker_completion(worker, completion)
                || completion.revision != revision
                || completion.intent != 0x11C057U + commit_index
                || completion.result.state
                    != persistence::SaveCommitState::committed
                || completion.result.error != persistence::SaveError::none) {
            return false;
        }
        const persistence::SaveCommitCost& cost = completion.result.cost;
        if (cost.encoded_bytes == 0U || cost.encode_ns == 0U
                || cost.durable_write_ns == 0U
                || cost.file_readback_ns == 0U
                || cost.write_operations == 0U
                || cost.readback_operations == 0U) {
            return false;
        }
        if (commit_index < kWorkerCostWarmupCount) continue;
        const std::size_t sample = commit_index - kWorkerCostWarmupCount;
        encode_samples[sample] = cost.encode_ns;
        write_samples[sample] = cost.durable_write_ns;
        readback_samples[sample] = cost.file_readback_ns;
        if (encoded_bytes == 0U) encoded_bytes = cost.encoded_bytes;
        if (encoded_bytes != cost.encoded_bytes) return false;
        minimum_write_operations = (std::min)(
            minimum_write_operations, cost.write_operations);
        minimum_readback_operations = (std::min)(
            minimum_readback_operations, cost.readback_operations);
    }
    worker.stop_and_join();

    auto reloaded = std::unique_ptr<persistence::SaveCommitStorage>{
        new (std::nothrow) persistence::SaveCommitStorage{}};
    if (reloaded == nullptr || !reloaded->initialize({directory.path()})) {
        return false;
    }
    const checkpoint::SaveCheckpointSlot* const loaded =
        reloaded->loaded_checkpoint();
    const std::uint64_t expected_revision =
        kFinalRevision + kCommitCount;
    if (reloaded->load_state() != persistence::SaveLoadState::ready
            || loaded == nullptr
            || loaded->persistence_revision != expected_revision
            || !dungeon::same_run_state(loaded->state, source.state)
            || !checkpoint::same_room_progress_checkpoint(
                loaded->room_progress, source.room_progress)) {
        return false;
    }

    std::printf(
        "[task11-worker-cost] clock=steady_clock scenario=abyss-max "
        "samples=%zu bytes=%zu writes_min=%u readbacks_min=%u "
        "encode_p50_ns=%llu encode_p95_ns=%llu encode_p99_ns=%llu "
        "encode_max_ns=%llu write_p50_ns=%llu write_p95_ns=%llu "
        "write_p99_ns=%llu write_max_ns=%llu readback_p50_ns=%llu "
        "readback_p95_ns=%llu readback_p99_ns=%llu readback_max_ns=%llu\n",
        kWorkerCostSampleCount, encoded_bytes,
        static_cast<unsigned int>(minimum_write_operations),
        static_cast<unsigned int>(minimum_readback_operations),
        static_cast<unsigned long long>(percentile(encode_samples, 50U)),
        static_cast<unsigned long long>(percentile(encode_samples, 95U)),
        static_cast<unsigned long long>(percentile(encode_samples, 99U)),
        static_cast<unsigned long long>(percentile(encode_samples, 100U)),
        static_cast<unsigned long long>(percentile(write_samples, 50U)),
        static_cast<unsigned long long>(percentile(write_samples, 95U)),
        static_cast<unsigned long long>(percentile(write_samples, 99U)),
        static_cast<unsigned long long>(percentile(write_samples, 100U)),
        static_cast<unsigned long long>(percentile(readback_samples, 50U)),
        static_cast<unsigned long long>(percentile(readback_samples, 95U)),
        static_cast<unsigned long long>(percentile(readback_samples, 99U)),
        static_cast<unsigned long long>(percentile(readback_samples, 100U)));
    return minimum_write_operations >= 2U
        && minimum_readback_operations >= 5U;
}

#endif

arpg::test::Failure large_room_trace_matrix_is_deterministic() noexcept {
#if !defined(_WIN32) || !defined(NDEBUG)
    std::printf(
        "[task11-worker-cost] skipped=requires-windows-release\n");
#endif
    std::size_t trace_count{};
    for (const Scenario& scenario : kScenarios) {
        auto durable_baseline = std::unique_ptr<Trace>{};
        for (const std::size_t reloads : kReloadSchedules) {
            auto first = std::unique_ptr<Trace>{new (std::nothrow) Trace{}};
            auto repeat = std::unique_ptr<Trace>{new (std::nothrow) Trace{}};
            ARPG_REQUIRE(first != nullptr && repeat != nullptr);
            ARPG_REQUIRE(generate_trace(scenario, reloads, *first));
            ARPG_REQUIRE(generate_trace(scenario, reloads, *repeat));
            ARPG_REQUIRE(same_trace(*first, *repeat));
#if defined(_WIN32) && defined(NDEBUG)
            if (scenario.abyss && reloads == 0U) {
                ARPG_REQUIRE(first->final_checkpoint != nullptr);
                ARPG_REQUIRE(measure_worker_cost(*first->final_checkpoint));
            }
#endif
            if (durable_baseline == nullptr) {
                durable_baseline = std::move(first);
            } else {
                if (!same_cross_schedule_result(
                        *durable_baseline, *first)) {
                    std::fprintf(stderr,
                        "[task11-cross-diff] population=%d monster_hash=%d "
                        "environment_hash=%d exit_tick=%d revision=%d "
                        "final_state=%d room=%d target=%d transition=%d "
                        "direction=%d\n",
                        durable_baseline->population == first->population,
                        durable_baseline->monster_blueprint_hash
                            == first->monster_blueprint_hash,
                        durable_baseline->environment_blueprint_hash
                            == first->environment_blueprint_hash,
                        durable_baseline->exit_open_tick
                            == first->exit_open_tick,
                        durable_baseline->final_checkpoint
                                ->persistence_revision
                            == first->final_checkpoint->persistence_revision,
                        dungeon::same_run_state(
                            durable_baseline->final_checkpoint->state,
                            first->final_checkpoint->state),
                        checkpoint::same_room_progress_checkpoint(
                            durable_baseline->final_checkpoint->room_progress,
                            first->final_checkpoint->room_progress),
                        dungeon::same_run_state(
                            durable_baseline->transition_target,
                            first->transition_target),
                        durable_baseline->transition == first->transition,
                        durable_baseline->direction == first->direction);
                    report_run_state_difference(
                        durable_baseline->transition_target,
                        first->transition_target);
                    report_room_checkpoint_difference(
                        durable_baseline->final_checkpoint->room_progress,
                        first->final_checkpoint->room_progress);
                }
                ARPG_REQUIRE(same_cross_schedule_result(
                    *durable_baseline, *first));

                auto& room = first->final_checkpoint->room_progress;
                const std::uint64_t claims = room.secondary_claim_bits[0U];
                room.secondary_claim_bits[0U] ^= std::uint64_t{1U};
                ARPG_REQUIRE(!same_cross_schedule_result(
                    *durable_baseline, *first));
                room.secondary_claim_bits[0U] = claims;

                const auto cooldown =
                    room.combat.player.skill_cooldowns[0U];
                room.combat.player.skill_cooldowns[0U] ^= 1U;
                ARPG_REQUIRE(!same_cross_schedule_result(
                    *durable_baseline, *first));
                room.combat.player.skill_cooldowns[0U] = cooldown;
            }
            trace_count += 2U;
            std::printf("[task11-trace] scenario=%s reloads=%zu repeated=2\n",
                scenario.name, reloads);
        }
    }
    ARPG_REQUIRE(trace_count == 24U);
    return {};
}

arpg::test::Failure warmed_main_thread_preparation_is_allocation_free()
    noexcept {
    constexpr std::array<Scenario, 2U> kMaximumScenarios{{
        {"normal-max", 750U, false},
        {"abyss-max", 1125U, true},
    }};
    for (const Scenario& scenario : kMaximumScenarios) {
        const dungeon::DungeonRules rules{};
        Trace ignored{};
        auto session = make_scenario_session(scenario, rules);
        auto render = std::unique_ptr<dungeon::DungeonRenderSnapshot>{
            new (std::nothrow) dungeon::DungeonRenderSnapshot{}};
        auto checkpoint_slot = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
            new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
        ARPG_REQUIRE(session != nullptr && render != nullptr
            && checkpoint_slot != nullptr);
        ARPG_REQUIRE(enter_combat(*session, ignored));

        bool has_drop_activity{};
        for (std::uint16_t ordinal{};
                ordinal < 32U && !has_drop_activity; ++ordinal) {
            std::array<arpg::combat::CombatEvent, 2U> events{};
            std::size_t event_count{};
            ARPG_REQUIRE(arpg::test::defeat_room_monster_by_ordinal(
                *session, ordinal, events.data(), events.size(), event_count)
                .has_value());
            session->tick({});
            drain_events(*session, ignored);
            const dungeon::DungeonSnapshot snapshot = session->snapshot();
            has_drop_activity = snapshot.ground_item_count != 0U
                || snapshot.ground_material_count != 0U
                || snapshot.ground_health_potion_count != 0U;
        }
        ARPG_REQUIRE(has_drop_activity);

        dungeon::WorldViewQuery query{
            {{-12.0F, -5.5F, -1.0F}, {12.0F, 5.5F, 32.0F}},
            1920, 1080, 1U};
        for (std::size_t warmup{}; warmup < 4U; ++warmup) {
            session->tick({});
            static_cast<void>(session->snapshot());
            query.camera_version = warmup + 1U;
            ARPG_REQUIRE(session->write_render_snapshot(query, *render));
            ARPG_REQUIRE(session->capture_save_checkpoint(
                *checkpoint_slot, 0x3000U + warmup));
        }

        std::uint64_t checksum{};
        const std::uint64_t before = arpg::test::allocation_count();
        for (std::size_t pass{}; pass < 32U; ++pass) {
            const float center_x = static_cast<float>(pass % 8U) * 12.0F
                - 42.0F;
            const float center_y = static_cast<float>(pass / 8U) * 10.0F
                - 15.0F;
            query.world_bounds = {{center_x - 12.0F, center_y - 5.5F, -1.0F},
                {center_x + 12.0F, center_y + 5.5F, 32.0F}};
            query.camera_version = pass + 10U;
            session->tick({});
            const dungeon::DungeonSnapshot snapshot = session->snapshot();
            ARPG_REQUIRE(session->write_render_snapshot(query, *render));
            ARPG_REQUIRE(session->capture_save_checkpoint(
                *checkpoint_slot, 0x3100U + pass));
            ARPG_REQUIRE(snapshot.phase == dungeon::RoomPhase::combat);
            ARPG_REQUIRE(render->combat.monster_count
                <= arpg::combat::kMonsterCapacity);
            ARPG_REQUIRE(render->environment.count
                <= dungeon::kVisibleEnvironmentCapacity);
            checksum += render->combat.monster_count
                + render->environment.count + render->equipment_count
                + render->material_count + render->health_potion_count;
        }
        const std::uint64_t after = arpg::test::allocation_count();
        ARPG_REQUIRE(checksum != 0U);
        ARPG_REQUIRE(after == before);
    }
    return {};
}

arpg::test::Failure active_room_checkpoint_keeps_experience_unsettled()
    noexcept {
    const Scenario scenario{"normal-min", 300U, false};
    const dungeon::DungeonRules rules{};
    Trace ignored{};
    auto session = make_scenario_session(scenario, rules);
    auto baseline = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    auto projected = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    auto overridden = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(session != nullptr && baseline != nullptr
        && projected != nullptr && overridden != nullptr);
    ARPG_REQUIRE(enter_combat(*session, ignored));
    ARPG_REQUIRE(session->capture_save_checkpoint(*baseline, 0x4000U));
    ARPG_REQUIRE(arpg::test::defeat_room_monster_by_ordinal(*session, 0U)
        .has_value());
    const dungeon::DungeonSnapshot live = session->snapshot();
    ARPG_REQUIRE(live.pending_room_experience != 0U);
    ARPG_REQUIRE(session->capture_save_checkpoint(*projected, 0x4001U));
    ARPG_REQUIRE(projected->room_progress.pending_room_experience
        == live.pending_room_experience);
    ARPG_REQUIRE(projected->state.progression.level == live.progression.level);
    ARPG_REQUIRE(projected->state.progression.experience
        == live.progression.experience);
    ARPG_REQUIRE(session->capture_save_checkpoint(
        *overridden, 0x4002U, &baseline->state));
    ARPG_REQUIRE(dungeon::same_run_state(
        overridden->state, baseline->state));
    ARPG_REQUIRE(overridden->room_progress.pending_room_experience == 0U);
    return {};
}

arpg::test::Failure active_room_exact_save_preserves_pending_experience()
    noexcept {
    const Scenario scenario{"normal-min", 300U, false};
    const dungeon::DungeonRules rules{};
    Trace ignored{};
    auto session = make_scenario_session(scenario, rules);
    auto saved = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    auto followup = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(session != nullptr && saved != nullptr
        && followup != nullptr);
    ARPG_REQUIRE(enter_combat(*session, ignored));
    ARPG_REQUIRE(arpg::test::defeat_room_monster_by_ordinal(*session, 0U)
        .has_value());
    const dungeon::DungeonSnapshot earned = session->snapshot();
    ARPG_REQUIRE(earned.pending_room_experience != 0U);
    constexpr std::uint16_t kPotionSpawn = 0U;
    arpg::test::install_ground_health_potion(
        *session, kPotionSpawn, earned.combat->player.position);
    arpg::test::set_player_health(*session, 1, 1'000'000);
    session->tick({});
    const dungeon::PendingSave* const pending = session->pending_save_view();
    ARPG_REQUIRE(pending != nullptr
        && pending->kind == dungeon::PendingSaveKind::health_potion_pickup);
    ARPG_REQUIRE(session->capture_save_checkpoint(
        *saved, 0x4100U, &pending->next_state));
    ARPG_REQUIRE(saved->room_progress.pending_room_experience
        == earned.pending_room_experience);
    ARPG_REQUIRE(saved->state.progression.level == earned.progression.level);
    ARPG_REQUIRE(saved->state.progression.experience
        == earned.progression.experience);

    session->resolve_pending_save({dungeon::SaveDisposition::committed,
        pending->expected_generation, saved->state, pending->kind});
    const dungeon::DungeonSnapshot committed = session->snapshot();
    ARPG_REQUIRE(committed.phase == dungeon::RoomPhase::combat);
    ARPG_REQUIRE(committed.pending_room_experience
        == earned.pending_room_experience);
    ARPG_REQUIRE(committed.progression.level == earned.progression.level);
    ARPG_REQUIRE(committed.progression.experience
        == earned.progression.experience);
    ARPG_REQUIRE(session->capture_save_checkpoint(*followup, 0x4101U));
    ARPG_REQUIRE(followup->room_progress.pending_room_experience
        == earned.pending_room_experience);
    ARPG_REQUIRE(followup->state.progression.level
        == earned.progression.level);
    ARPG_REQUIRE(followup->state.progression.experience
        == earned.progression.experience);

    auto reloaded = std::unique_ptr<dungeon::DungeonSession>{
        new (std::nothrow) dungeon::DungeonSession{rules, saved->state}};
    ARPG_REQUIRE(reloaded != nullptr);
    arpg::test::set_player_health(*reloaded,
        saved->room_progress.combat.player.hp,
        saved->room_progress.combat.player.max_hp);
    ARPG_REQUIRE(reloaded->restore_room_progress_checkpoint(*saved));
    const dungeon::DungeonSnapshot restored = reloaded->snapshot();
    ARPG_REQUIRE(restored.pending_room_experience
        == earned.pending_room_experience);
    ARPG_REQUIRE(restored.progression.level == earned.progression.level);
    ARPG_REQUIRE(restored.progression.experience
        == earned.progression.experience);
    ARPG_REQUIRE(restored.defeated_monster_count
        == earned.defeated_monster_count);

    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(*session));
    session->tick({});
    const dungeon::PendingSave* const live_death =
        session->pending_save_view();
    ARPG_REQUIRE(live_death != nullptr
        && live_death->kind == dungeon::PendingSaveKind::death_retreat);
    auto death_checkpoint = std::unique_ptr<checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(death_checkpoint != nullptr);
    ARPG_REQUIRE(session->capture_save_checkpoint(*death_checkpoint,
        0x4102U, &live_death->next_state));
    ARPG_REQUIRE(death_checkpoint->room_progress.pending_room_experience
        == 0U);
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(*reloaded));
    reloaded->tick({});
    const dungeon::PendingSave* const restored_death =
        reloaded->pending_save_view();
    ARPG_REQUIRE(restored_death != nullptr
        && restored_death->kind == dungeon::PendingSaveKind::death_retreat);
    ARPG_REQUIRE(dungeon::same_run_state(
        live_death->next_state, restored_death->next_state));
    ARPG_REQUIRE(live_death->next_state.progression.level
        == earned.progression.level);
    ARPG_REQUIRE(live_death->next_state.progression.experience
        == earned.progression.experience);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"24 trace matrix is deterministic",
        &large_room_trace_matrix_is_deterministic},
    {"warmed main thread preparation is allocation free",
        &warmed_main_thread_preparation_is_allocation_free},
    {"active room checkpoint keeps experience unsettled",
        &active_room_checkpoint_keeps_experience_unsettled},
    {"active room exact save preserves pending experience",
        &active_room_exact_save_preserves_pending_experience},
};

}  // namespace

arpg::test::TestSuite large_room_end_to_end_suite() noexcept {
    return arpg::test::make_suite("large_room_end_to_end", kCases);
}
