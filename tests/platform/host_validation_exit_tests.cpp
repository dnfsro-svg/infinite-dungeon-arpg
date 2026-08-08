#include "test_framework.hpp"

#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "combat/combat_world.hpp"
#include "combat/room_spatial_grid.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_affix.hpp"
#include "dungeon/room_generation.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon_view_math.hpp"
#include "dungeon_runtime.hpp"
#include "host_input.hpp"
#include "host_validation.hpp"
#include "host_validation_stage10_11.hpp"
#include "host_validation_stage11c.hpp"
#include "host_validation_stage11d.hpp"
#include "pause_menu_state.hpp"
#include "platform/settings/settings_store.hpp"
#include "raylib_host.hpp"
#include "skills/skill_loadout.hpp"
#include "../dungeon/dungeon_test_support.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace {

namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;
namespace validation = arpg::platform::host_validation;

dungeon::DungeonSnapshot unlocked_combat_snapshot() {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::combat;
    snapshot.exits_unlocked = true;
    snapshot.combat.emplace();
    snapshot.combat->player.position = {};
    return snapshot;
}

platform::RaylibHostConfig right_abyss_door_config() {
    platform::RaylibHostConfig config{};
    config.validation_abyss_direction = static_cast<std::uint8_t>(
        dungeon::ExitDirection::right);
    return config;
}

dungeon::DungeonRunState exit_confirmation_abyss_state() noexcept {
    constexpr std::uint64_t depth = 20U;
    dungeon::DungeonRunState state = dungeon::make_initial_run_state(
        0xA9B9555EEDULL, dungeon::DungeonRules{}).state;
    for (std::uint64_t seed = 1U; seed != 0U; ++seed) {
        const auto selection = arpg::abyss::select_abyss_rule(seed, depth);
        if (!arpg::abyss::is_abyss_roll(seed)
                || !selection.has_value()
                || selection->danger != arpg::abyss::AbyssDanger::high) {
            continue;
        }
        state.current_room.seed = seed;
        state.current_room.depth = depth;
        state.current_room.entry = dungeon::EntrySide::left;
        state.current_room.is_abyss = true;
        state.current_room.has_hole = true;
        state.last_transition = dungeon::TransitionKind::door;
        state.last_direction = dungeon::ExitDirection::right;
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::cleared;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
        state.abyss.reward_total = arpg::abyss::reward_profile_for(
            selection->danger, 1U).item_count;
        state.abyss.generated_mask = 7U;
        state.abyss.reward_revision = 3U;
        return state;
    }
    return {};
}

dungeon::DungeonRunState formal_exit_confirmation_abyss_state() noexcept {
    constexpr std::uint64_t root_seed = 244'541U;
    constexpr std::uint64_t room_seed = 0x07EA489A9ABFB2C8ULL;
    constexpr std::array<dungeon::ExitDirection, 4U> directions{{
        dungeon::ExitDirection::up,
        dungeon::ExitDirection::down,
        dungeon::ExitDirection::left,
        dungeon::ExitDirection::right,
    }};
    const auto initial = dungeon::make_initial_run_state(
        root_seed, dungeon::DungeonRules{});
    if (initial.fault != dungeon::DungeonFault::none) return {};
    for (const dungeon::ExitDirection direction : directions) {
        auto next = dungeon::make_door_transition(
            initial.state, direction, dungeon::DungeonRules{});
        if (next.fault != dungeon::DungeonFault::none
                || !next.state.current_room.is_abyss
                || next.state.current_room.seed != room_seed) {
            continue;
        }
        dungeon::DungeonRunState state = std::move(next.state);
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::cleared;
        state.abyss.reward_total = arpg::abyss::reward_profile_for(
            state.abyss.danger, 1U).item_count;
        state.abyss.generated_mask = static_cast<std::uint8_t>(
            (std::uint8_t{1U} << state.abyss.reward_total) - 1U);
        state.abyss.claimed_mask = 0U;
        state.abyss.abandoned_mask = 0U;
        state.abyss.reward_revision = 3U;
        return state;
    }
    return {};
}

dungeon::DungeonSnapshot combat_with_target(bool is_abyss) {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::combat;
    snapshot.is_abyss = is_abyss;
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 100;
    snapshot.combat->player.max_hp = 100;
    auto& monster = snapshot.combat->monsters[0];
    monster.active = true;
    monster.hp = 100;
    monster.monster_ordinal = 7U;
    monster.position = {50.0F, 0.0F, 0.0F};
    snapshot.combat->monster_count = 1U;
    snapshot.remaining_targets = 1U;
    return snapshot;
}

arpg::test::Failure stage10_uses_unlocked_exit_during_combat() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = unlocked_combat_snapshot();
    platform::RaylibHostConfig config = right_abyss_door_config();
    config.stage10_validation = platform::Stage10ValidationScenario::abyss_door;
    validation::Stage10ValidationState state{};

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == 1);
    ARPG_REQUIRE(movement.y == 0);
    ARPG_REQUIRE(state.normal_exit_started);
    return {};
}

arpg::test::Failure stage10_unlocked_exit_joins_grid_before_door() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = unlocked_combat_snapshot();
    snapshot.combat->player.position = {3.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.validation_abyss_direction = static_cast<std::uint8_t>(
        dungeon::ExitDirection::up);
    config.stage10_validation =
        platform::Stage10ValidationScenario::thunderstorm_warning;
    validation::Stage10ValidationState state{};

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == -1);
    ARPG_REQUIRE(movement.y == 0);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_near_x);
    return {};
}

arpg::test::Failure stage10_unlocked_exit_pushes_outward_at_door() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = unlocked_combat_snapshot();
    snapshot.combat->player.position = {
        0.0F, arpg::combat::room_bounds::min_y, 0.0F};
    platform::RaylibHostConfig config{};
    config.validation_abyss_direction = static_cast<std::uint8_t>(
        dungeon::ExitDirection::up);
    config.stage10_validation =
        platform::Stage10ValidationScenario::abyss_door;
    validation::Stage10ValidationState state{};
    state.normal_exit_started = true;
    state.sweep_grid.phase = validation::Stage10GridRoutePhase::route;

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == 0);
    ARPG_REQUIRE(movement.y == -1);
    return {};
}

arpg::test::Failure
stage10_exit_confirmation_joins_right_away_from_center_rewards() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.is_abyss = true;
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 100;
    snapshot.combat->player.position = {3.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::exit_confirmation;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == 1);
    ARPG_REQUIRE(movement.y == 0);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_near_x);
    ARPG_REQUIRE(state.sweep_grid.pending_movement_progress_check);
    return {};
}

arpg::test::Failure
stage10_exit_confirmation_joins_left_away_from_center_rewards() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.is_abyss = true;
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 100;
    snapshot.combat->player.position = {-3.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::exit_confirmation;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == -1);
    ARPG_REQUIRE(movement.y == 0);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_near_x);
    ARPG_REQUIRE(state.sweep_grid.pending_movement_progress_check);
    return {};
}

arpg::test::Failure
stage10_exit_confirmation_routes_left_away_from_center_rewards() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.is_abyss = true;
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 100;
    snapshot.combat->player.position = {
        arpg::combat::room_bounds::min_x, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::exit_confirmation;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.sweep_grid.phase = validation::Stage10GridRoutePhase::route;

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == -1);
    ARPG_REQUIRE(movement.y == 0);
    return {};
}

arpg::test::Failure
stage10_exit_confirmation_routes_right_away_from_center_rewards() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.is_abyss = true;
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 100;
    snapshot.combat->player.position = {
        arpg::combat::room_bounds::max_x, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::exit_confirmation;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.sweep_grid.phase = validation::Stage10GridRoutePhase::route;

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == 1);
    ARPG_REQUIRE(movement.y == 0);
    return {};
}

arpg::test::Failure
stage10_exit_confirmation_blocked_join_recovers_farther_outward() noexcept {
    struct Case final {
        float player_x{};
        std::uint8_t blocked_column{};
        std::uint8_t recovered_column{};
        int expected_x{};
    };
    constexpr std::array<Case, 2U> cases{{
        {3.0F, 11U, 12U, 1},
        {-3.0F, 9U, 8U, -1},
    }};
    for (const Case& current : cases) {
        dungeon::DungeonSession session{};
        dungeon::DungeonSnapshot snapshot{};
        snapshot.phase = dungeon::RoomPhase::awaiting_exit;
        snapshot.is_abyss = true;
        snapshot.combat.emplace();
        snapshot.combat->player.hp = 100;
        snapshot.combat->player.position = {current.player_x, 0.0F, 0.0F};
        platform::RaylibHostConfig config{};
        config.stage10_validation =
            platform::Stage10ValidationScenario::exit_confirmation;
        validation::Stage10ValidationState state{};
        state.entered_abyss = true;
        state.sweep_grid.phase =
            validation::Stage10GridRoutePhase::join_near_x;
        state.sweep_grid.boundary_column = current.blocked_column;
        state.sweep_grid.pending_movement_progress_check = true;
        state.sweep_grid.previous_position = snapshot.combat->player.position;

        const auto movement = validation::stage10_validation_input(
            session, snapshot, config, state);
        ARPG_REQUIRE(movement.x == current.expected_x);
        ARPG_REQUIRE(movement.y == 0);
        ARPG_REQUIRE(
            state.sweep_grid.boundary_column == current.recovered_column);
        ARPG_REQUIRE(state.sweep_grid.phase
            == validation::Stage10GridRoutePhase::join_far_x);

        const auto repeated = validation::stage10_validation_input(
            session, snapshot, config, state);
        ARPG_REQUIRE(repeated.x == current.expected_x);
        ARPG_REQUIRE(repeated.y == 0);
        const auto twice_recovered = static_cast<std::uint8_t>(
            current.recovered_column + current.expected_x);
        ARPG_REQUIRE(
            state.sweep_grid.boundary_column == twice_recovered);
        ARPG_REQUIRE(state.sweep_grid.phase
            == validation::Stage10GridRoutePhase::join_far_x);
    }
    return {};
}

arpg::test::Failure
stage10_exit_confirmation_blocked_join_keeps_safe_same_cell_escape() noexcept {
    struct Case final {
        float player_x{};
        std::uint8_t blocked_column{};
        std::uint8_t escape_column{};
        int expected_x{};
    };
    constexpr std::array<Case, 2U> cases{{
        {-19.306414F, 7U, 8U, 1},
        {19.306414F, 13U, 12U, -1},
    }};
    for (const Case& current : cases) {
        dungeon::DungeonSession session{};
        dungeon::DungeonSnapshot snapshot{};
        snapshot.phase = dungeon::RoomPhase::awaiting_exit;
        snapshot.is_abyss = true;
        snapshot.combat.emplace();
        snapshot.combat->player.hp = 100;
        snapshot.combat->player.position = {current.player_x, -61.262093F, 0.0F};
        platform::RaylibHostConfig config{};
        config.stage10_validation =
            platform::Stage10ValidationScenario::exit_confirmation;
        validation::Stage10ValidationState state{};
        state.entered_abyss = true;
        state.sweep_grid.phase =
            validation::Stage10GridRoutePhase::join_near_x;
        state.sweep_grid.boundary_column = current.blocked_column;
        state.sweep_grid.pending_movement_progress_check = true;
        state.sweep_grid.previous_position = snapshot.combat->player.position;

        const auto movement = validation::stage10_validation_input(
            session, snapshot, config, state);
        ARPG_REQUIRE(movement.x == current.expected_x);
        ARPG_REQUIRE(movement.y == 0);
        ARPG_REQUIRE(state.sweep_grid.boundary_column
            == current.escape_column);
        ARPG_REQUIRE(state.sweep_grid.phase
            == validation::Stage10GridRoutePhase::join_far_x);
        ARPG_REQUIRE(state.sweep_grid.route_rejoins == 0U);
    }

    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.is_abyss = true;
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 100;
    snapshot.combat->player.position = {-19.306414F, -61.262093F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::exit_confirmation;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.sweep_grid.phase = validation::Stage10GridRoutePhase::join_far_x;
    state.sweep_grid.boundary_column = 8U;
    state.sweep_grid.pending_movement_progress_check = true;
    state.sweep_grid.previous_position = snapshot.combat->player.position;

    const auto repeated = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(repeated.x == -1);
    ARPG_REQUIRE(repeated.y == 0);
    ARPG_REQUIRE(state.sweep_grid.boundary_column == 6U);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_far_x);
    ARPG_REQUIRE(state.sweep_grid.route_rejoins == 1U);
    return {};
}

arpg::test::Failure
stage10_exit_confirmation_blocked_route_recovers_farther_outward() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.is_abyss = true;
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 100;
    snapshot.combat->player.position = {
        arpg::combat::room_bounds::min_x
            + 11.0F * arpg::combat::room_spatial::cell_width,
        30.0F,
        0.0F};
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::exit_confirmation;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.sweep_grid.phase = validation::Stage10GridRoutePhase::route;
    state.sweep_grid.boundary_column = 11U;
    state.sweep_grid.pending_movement_progress_check = true;
    state.sweep_grid.previous_position = snapshot.combat->player.position;

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == 1);
    ARPG_REQUIRE(movement.y == 0);
    ARPG_REQUIRE(state.sweep_grid.boundary_column == 12U);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_far_x);
    ARPG_REQUIRE(state.sweep_grid.route_rejoins == 1U);
    return {};
}

arpg::test::Failure
stage10_exit_confirmation_reaches_warning_without_claiming_center_rewards()
    noexcept {
    const dungeon::DungeonRunState initial = exit_confirmation_abyss_state();
    ARPG_REQUIRE(arpg::abyss::is_abyss_roll(initial.current_room.seed));
    for (const float start_x : std::array<float, 2U>{{35.0F, -35.0F}}) {
        dungeon::DungeonSession session{dungeon::DungeonRules{}, initial};
        arpg::test::set_phase(session, dungeon::RoomPhase::awaiting_exit);
        arpg::test::set_player_position(session, {start_x, 62.0F, 0.0F});
        platform::RaylibHostConfig config{};
        config.stage10_validation =
            platform::Stage10ValidationScenario::exit_confirmation;
        validation::Stage10ValidationState state{};
        state.entered_abyss = true;

        constexpr std::uint32_t tick_limit = 2048U;
        std::uint32_t tick = 0U;
        while (!session.snapshot().abyss_exit_confirmation_armed
                && tick < tick_limit) {
            const auto snapshot = session.snapshot();
            const auto movement = validation::stage10_validation_input(
                session, snapshot, config, state);
            session.tick(movement);
            ARPG_REQUIRE(
                arpg::test::stable_state(session).abyss.claimed_mask == 0U);
            ARPG_REQUIRE(!session.pending_save().has_value());
            ++tick;
        }

        const auto reached = session.snapshot();
        ARPG_REQUIRE(tick < tick_limit);
        ARPG_REQUIRE(reached.is_abyss);
        ARPG_REQUIRE(reached.abyss_exit_confirmation_armed);
        ARPG_REQUIRE(reached.abyss_unpicked_rewards == 3U);
        ARPG_REQUIRE(
            arpg::test::stable_state(session).abyss.claimed_mask == 0U);
        ARPG_REQUIRE(!session.pending_save().has_value());
    }
    return {};
}

arpg::test::Failure
stage10_formal_exit_failure_position_reaches_warning_without_claiming_rewards()
    noexcept {
    const dungeon::DungeonRunState initial =
        formal_exit_confirmation_abyss_state();
    ARPG_REQUIRE(initial.current_room.seed == 0x07EA489A9ABFB2C8ULL);
    ARPG_REQUIRE(initial.abyss.reward_total == 3U);
    dungeon::DungeonSession session{dungeon::DungeonRules{}, initial};
    arpg::test::set_phase(session, dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_position(
        session, {-19.306414F, -61.262093F, 0.0F});
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::exit_confirmation;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;

    constexpr std::uint32_t tick_limit = 2048U;
    std::uint32_t tick{};
    while (!session.snapshot().abyss_exit_confirmation_armed
            && tick < tick_limit) {
        const auto snapshot = session.snapshot();
        const auto movement = validation::stage10_validation_input(
            session, snapshot, config, state);
        session.tick(movement);
        ARPG_REQUIRE(arpg::test::stable_state(session).abyss.claimed_mask == 0U);
        ARPG_REQUIRE(!session.pending_save().has_value());
        ++tick;
    }

    const auto reached = session.snapshot();
    ARPG_REQUIRE(tick < tick_limit);
    ARPG_REQUIRE(reached.is_abyss);
    ARPG_REQUIRE(reached.abyss_exit_confirmation_armed);
    ARPG_REQUIRE(reached.abyss_unpicked_rewards == 3U);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.claimed_mask == 0U);
    ARPG_REQUIRE(!session.pending_save().has_value());
    return {};
}

arpg::test::Failure stage11_uses_unlocked_exit_during_combat() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = unlocked_combat_snapshot();
    platform::RaylibHostConfig config = right_abyss_door_config();
    config.stage11_validation =
        platform::Stage11ValidationScenario::abyss_death_recap;
    validation::Stage11ValidationState state{};

    const auto movement = validation::stage11_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == 1);
    ARPG_REQUIRE(movement.y == 0);
    return {};
}

arpg::test::Failure stage11_deep_unlocked_combat_routes_to_hole() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.exits_unlocked = true;
    snapshot.has_hole = true;
    snapshot.combat->player.position = {35.0F, 62.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11_validation =
        platform::Stage11ValidationScenario::deep_continue;
    validation::Stage11ValidationState state{};

    const auto movement = validation::stage11_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == -1);
    ARPG_REQUIRE(movement.y == 0);
    return {};
}

arpg::test::Failure stage11_deep_preunlock_reuses_survival_driver() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.combat->player.position.x = 100.0F;
    platform::RaylibHostConfig config{};
    config.stage11_validation =
        platform::Stage11ValidationScenario::deep_continue;
    validation::Stage11ValidationState state{};

    static_cast<void>(validation::stage11_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.combat_driver.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(state.combat_driver.ranged_stance.x
        < snapshot.combat->monsters[0].position.x);
    return {};
}

arpg::test::Failure stage10_captures_unlocked_door_during_combat() noexcept {
    dungeon::DungeonSnapshot snapshot = unlocked_combat_snapshot();
    platform::RaylibHostConfig config = right_abyss_door_config();
    config.stage10_validation = platform::Stage10ValidationScenario::abyss_door;
    snapshot.abyss_doors[static_cast<std::size_t>(
        dungeon::ExitDirection::right)] = true;
    validation::Stage10ValidationState state{};

    ARPG_REQUIRE(validation::stage10_validation_reached(
        snapshot, config, state));
    return {};
}

arpg::test::Failure other_stage10_scenarios_reuse_normal_room_driver()
    noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::thunderstorm_warning;
    validation::Stage10ValidationState state{};
    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.ranged_target_ordinal == 7U);
    return {};
}

arpg::test::Failure warning_abyss_attacks_only_current_target() noexcept {
    dungeon::DungeonSession session{};
    arpg::combat::CombatEncounterConfig encounter{};
    encounter.wave.spawn_count = 2U;
    encounter.wave.spawns[0] = {
        arpg::combat::MonsterId::chaos_chaser,
        {0.10F, 0.60F, 0.0F}, {}, 7U};
    encounter.wave.spawns[1] = {
        arpg::combat::MonsterId::chaos_chaser,
        {1.00F, 0.00F, 0.0F}, {}, 8U};
    arpg::test::install_combat_world(session, encounter);
    arpg::test::set_started_abyss_room(
        session, arpg::abyss::AbyssDanger::low);
    arpg::test::set_phase(session, dungeon::RoomPhase::combat);
    const dungeon::DungeonSnapshot snapshot = session.snapshot();
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::thunderstorm_warning;
    validation::Stage10ValidationState state{};
    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.combat.has_value());
    ARPG_REQUIRE(after.combat->diagnostics.input_size == 0U);
    ARPG_REQUIRE(after.combat->active_skill.id
        == arpg::skills::ActiveSkillId::none);
    return {};
}

arpg::test::Failure player_death_waits_for_abyss_environment() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::player_death;
    validation::Stage10ValidationState state{};

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(movement.x == 0);
    ARPG_REQUIRE(movement.y == 0);
    ARPG_REQUIRE(state.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    return {};
}

arpg::test::Failure stationary_thunderstorm_defeats_player() noexcept {
    arpg::combat::CombatEncounterConfig config{};
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::thunderstorm);
    arpg::combat::CombatWorld world{config};

    for (std::uint16_t tick = 0U; tick < 1305U; ++tick) {
        world.tick({});
    }
    ARPG_REQUIRE(!world.player_defeated());
    world.tick({});
    ARPG_REQUIRE(world.player_defeated());
    ARPG_REQUIRE(world.death_snapshot().has_value());
    ARPG_REQUIRE(world.death_snapshot()->tick == 1305U);
    ARPG_REQUIRE(world.death_snapshot()->source.kind
        == arpg::combat::PlayerDamageSourceKind::abyss_environment);
    ARPG_REQUIRE(world.death_snapshot()->source.detail_id
        == static_cast<std::uint16_t>(
            arpg::abyss::AbyssRuleId::thunderstorm));
    return {};
}

arpg::test::Failure stage10_death_pending_requests_continue() noexcept {
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::player_death;
    const auto runtime = platform::HostValidationRuntime::create(
        config, arpg::settings::SettingsLoadStatus::loaded);
    ARPG_REQUIRE(runtime != nullptr);

    dungeon::DungeonSnapshot snapshot{};
    ARPG_REQUIRE(!runtime->should_continue_death(snapshot));
    snapshot.phase = dungeon::RoomPhase::death_pending;
    snapshot.death.emplace();
    snapshot.death->can_continue = true;
    snapshot.death->saving = true;
    ARPG_REQUIRE(!runtime->should_continue_death(snapshot));
    snapshot.death->saving = false;
    ARPG_REQUIRE(runtime->should_continue_death(snapshot));

    platform::RaylibHostConfig reset_config{};
    reset_config.stage10_validation =
        platform::Stage10ValidationScenario::room_reset;
    const auto reset_runtime = platform::HostValidationRuntime::create(
        reset_config, arpg::settings::SettingsLoadStatus::loaded);
    ARPG_REQUIRE(reset_runtime != nullptr);
    ARPG_REQUIRE(!reset_runtime->should_continue_death(snapshot));
    return {};
}

arpg::test::Failure stage10_death_continue_commit_reaches_exit() noexcept {
    constexpr std::uint64_t root = 2395U;
    const dungeon::DungeonRules rules{};
    const auto initial = dungeon::make_initial_run_state(root, rules);
    ARPG_REQUIRE(initial.fault == dungeon::DungeonFault::none);

    dungeon::checkpoint::DungeonRunState abyss_state{};
    bool found = false;
    for (const dungeon::ExitDirection direction : {
             dungeon::ExitDirection::up, dungeon::ExitDirection::down,
             dungeon::ExitDirection::left, dungeon::ExitDirection::right}) {
        const auto next = dungeon::make_door_transition(
            initial.state, direction, rules);
        if (next.fault == dungeon::DungeonFault::none
                && next.state.current_room.is_abyss
                && next.state.abyss.rule
                    == arpg::abyss::AbyssRuleId::thunderstorm) {
            abyss_state = next.state;
            found = true;
            break;
        }
    }
    ARPG_REQUIRE(found);

    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::player_death;
    const auto runtime = platform::HostValidationRuntime::create(
        config, arpg::settings::SettingsLoadStatus::loaded);
    ARPG_REQUIRE(runtime != nullptr);

    dungeon::DungeonSession session{rules, abyss_state};
    const auto start = session.pending_save();
    ARPG_REQUIRE(start.has_value());
    ARPG_REQUIRE(start->kind == dungeon::PendingSaveKind::abyss_start);
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        start->expected_generation, start->next_state, start->kind});
    auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.is_abyss);
    static_cast<void>(runtime->fixed_step_movement(session, snapshot, {}));

    session.tick({});
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
    session.tick({});
    const auto retreat = session.pending_save();
    ARPG_REQUIRE(retreat.has_value());
    ARPG_REQUIRE(retreat->kind == dungeon::PendingSaveKind::death_retreat);
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        retreat->expected_generation, retreat->next_state, retreat->kind});
    snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == dungeon::RoomPhase::death_pending);
    ARPG_REQUIRE(snapshot.death.has_value());
    ARPG_REQUIRE(runtime->should_continue_death(snapshot));

    ARPG_REQUIRE(session.request_death_continue()
        == dungeon::RequestResult::accepted);
    const auto continued = session.pending_save();
    ARPG_REQUIRE(continued.has_value());
    ARPG_REQUIRE(continued->kind == dungeon::PendingSaveKind::death_continue);
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        continued->expected_generation, continued->next_state,
        continued->kind});
    snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(!snapshot.is_abyss);
    ARPG_REQUIRE(!snapshot.death.has_value());
    ARPG_REQUIRE(runtime->fixed_step_target_reached(snapshot));
    return {};
}

arpg::test::Failure formal_death_root_is_thunderstorm_fixture() noexcept {
    constexpr std::uint64_t root = 2395U;
    const dungeon::DungeonRules rules{};
    const auto initial = dungeon::make_initial_run_state(root, rules);
    ARPG_REQUIRE(initial.fault == dungeon::DungeonFault::none);
    ARPG_REQUIRE(dungeon::roll_room_density(
        initial.state.current_room.seed, false).total_count == 300U);

    bool found = false;
    for (const dungeon::ExitDirection direction : {
             dungeon::ExitDirection::up, dungeon::ExitDirection::down,
             dungeon::ExitDirection::left, dungeon::ExitDirection::right}) {
        const auto next = dungeon::make_door_transition(
            initial.state, direction, rules);
        if (next.fault == dungeon::DungeonFault::none
                && next.state.current_room.is_abyss
                && next.state.abyss.rule
                    == arpg::abyss::AbyssRuleId::thunderstorm) {
            found = true;
            break;
        }
    }
    ARPG_REQUIRE(found);
    return {};
}

arpg::test::Failure clear_abyss_resets_and_reuses_full_clear_driver() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.ranged_target_ordinal = 42U;
    state.sweep_waypoint = 3U;
    state.sweep_escape = true;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.entered_abyss);
    ARPG_REQUIRE(state.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(state.sweep_waypoint == 0U);
    ARPG_REQUIRE(!state.sweep_escape);
    return {};
}

arpg::test::Failure ranged_driver_uses_room_center_safe_stance() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.combat->player.position.x = 100.0F;
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.ranged_facing == arpg::combat::Facing::right);
    ARPG_REQUIRE(state.ranged_stance.x
        < snapshot.combat->monsters[0].position.x);
    return {};
}

arpg::test::Failure ranged_driver_keeps_progressing_lease() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.ranged_target_ordinal = 7U;
    state.ranged_stance = {45.0F, 0.0F, 0.0F};
    state.pending_stance_progress_check = true;
    state.previous_stance_distance_squared = 2500.0F;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(!state.sweep_escape);
    return {};
}

arpg::test::Failure ranged_driver_stall_excludes_target_and_sweeps() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.ranged_target_ordinal = 7U;
    state.ranged_stance = {45.0F, 0.0F, 0.0F};
    state.pending_stance_progress_check = true;
    state.previous_stance_distance_squared = 2025.0F;

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(state.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(state.stalled_target_ordinal == 7U);
    ARPG_REQUIRE(state.sweep_escape);
    ARPG_REQUIRE(movement.x != 0 || movement.y != 0);
    ARPG_REQUIRE(state.sweep_grid.pending_movement_progress_check);
    return {};
}

arpg::test::Failure ranged_driver_keeps_stalled_lease_until_local_detour()
    noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.combat->player.position = {1.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.ranged_target_ordinal = 7U;
    state.ranged_stance = {45.0F, 0.0F, 0.0F};
    state.pending_stance_progress_check = true;
    state.previous_stance_distance_squared = 1936.0F;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.sweep_escape);
    ARPG_REQUIRE(state.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(state.stalled_target_ordinal == 7U);
    ARPG_REQUIRE(state.sweep_grid.pending_movement_progress_check);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_near_x);
    ARPG_REQUIRE(state.sweep_grid.boundary_column == 10U);

    snapshot.combat->player.position = {0.5F, 0.0F, 0.0F};
    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.sweep_escape);
    ARPG_REQUIRE(state.stalled_target_ordinal == 7U);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_near_x);
    ARPG_REQUIRE(state.sweep_grid.boundary_column == 10U);

    snapshot.combat->player.position = {0.0F, 0.0F, 0.0F};
    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.sweep_escape);
    ARPG_REQUIRE(state.stalled_target_ordinal == 7U);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::route);
    ARPG_REQUIRE(state.sweep_grid.pending_movement_progress_check);

    snapshot.combat->player.position = {45.0F, 0.0F, 0.0F};
    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(!state.sweep_escape);
    ARPG_REQUIRE(state.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(state.stalled_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);

    snapshot = combat_with_target(false);
    snapshot.combat->player.position = {44.9F, 0.0F, 0.0F};
    validation::Stage10ValidationState settled{};
    settled.entered_abyss = true;
    settled.stalled_target_ordinal = 7U;
    settled.recovery_target = {45.0F, 0.0F, 0.0F};
    settled.recovery_target_valid = true;
    settled.sweep_escape = true;
    settled.sweep_grid.phase = validation::Stage10GridRoutePhase::route;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, settled));
    ARPG_REQUIRE(!settled.sweep_escape);
    ARPG_REQUIRE(settled.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(!settled.recovery_target_valid);

    snapshot = combat_with_target(false);
    snapshot.combat->player.position = {-80.0F, 0.0F, 0.0F};
    snapshot.combat->monsters[0].active = false;
    validation::Stage10ValidationState resident_lease{};
    resident_lease.entered_abyss = true;
    resident_lease.stalled_target_ordinal = 7U;
    resident_lease.recovery_target = {45.0F, 0.0F, 0.0F};
    resident_lease.recovery_target_valid = true;
    resident_lease.sweep_escape = true;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, resident_lease));
    snapshot.combat->monsters[0].active = true;
    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, resident_lease));
    ARPG_REQUIRE(resident_lease.sweep_escape);
    ARPG_REQUIRE(resident_lease.recovery_target_valid);
    ARPG_REQUIRE(resident_lease.stalled_target_ordinal == 7U);
    ARPG_REQUIRE(resident_lease.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);

    snapshot = combat_with_target(false);
    snapshot.combat->player.position = {-80.0F, 0.0F, 0.0F};
    auto& alternate = snapshot.combat->monsters[1];
    alternate.active = true;
    alternate.hp = 100;
    alternate.monster_ordinal = 8U;
    alternate.position = {-40.0F, 20.0F, 0.0F};
    snapshot.combat->monster_count = 2U;
    validation::Stage10ValidationState blocked{};
    blocked.entered_abyss = true;
    blocked.stalled_target_ordinal = 7U;
    blocked.recovery_target = {45.0F, 0.0F, 0.0F};
    blocked.recovery_target_valid = true;
    blocked.sweep_escape = true;
    blocked.sweep_grid.phase = validation::Stage10GridRoutePhase::route;
    blocked.sweep_grid.route_rejoins = 4U;
    blocked.sweep_grid.pending_movement_progress_check = true;
    blocked.sweep_grid.previous_position = {-80.0F, 0.0F, 0.0F};

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, blocked));
    ARPG_REQUIRE(!blocked.sweep_escape);
    ARPG_REQUIRE(blocked.ranged_target_ordinal == 8U);
    ARPG_REQUIRE(!blocked.recovery_target_valid);

    snapshot = combat_with_target(false);
    snapshot.combat->player.position = {-80.0F, 0.0F, 0.0F};
    validation::Stage10ValidationState single{};
    single.entered_abyss = true;
    single.stalled_target_ordinal = 7U;
    single.recovery_target = {45.0F, 0.0F, 0.0F};
    single.recovery_target_valid = true;
    single.sweep_escape = true;
    single.sweep_grid.phase = validation::Stage10GridRoutePhase::route;
    single.sweep_grid.route_rejoins = 4U;
    single.sweep_grid.pending_movement_progress_check = true;
    single.sweep_grid.previous_position = {-80.0F, 0.0F, 0.0F};

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, single));
    ARPG_REQUIRE(single.sweep_escape);
    ARPG_REQUIRE(single.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(single.stalled_target_ordinal == 7U);
    ARPG_REQUIRE(!single.recovery_target_valid);
    return {};
}

arpg::test::Failure ranged_driver_releases_stale_geometry() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.combat->player.position = {45.0F, 0.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0].position = {-50.0F, 0.0F, 0.0F};
    auto& other = snapshot.combat->monsters[1];
    other.active = true;
    other.hp = 100;
    other.monster_ordinal = 8U;
    other.position = {-40.0F, 20.0F, 0.0F};
    snapshot.combat->monster_count = 2U;
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.ranged_target_ordinal = 7U;
    state.ranged_stance = {45.0F, 0.0F, 0.0F};
    state.ranged_facing = arpg::combat::Facing::right;
    state.stance_reached = true;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.ranged_target_ordinal == 8U);
    ARPG_REQUIRE(state.ranged_facing == arpg::combat::Facing::left);
    return {};
}

arpg::test::Failure
ranged_driver_continues_past_legacy_sparse_sweep_limit() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.combat->player.position = {-80.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::pending_reward;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    snapshot.remaining_targets = 2U;
    state.sweep_escape = true;
    state.sweep_waypoint = 9U;
    state.sweep_grid.phase = validation::Stage10GridRoutePhase::route;
    state.sweep_grid.route_rejoins = 4U;
    state.sweep_grid.pending_movement_progress_check = true;
    state.sweep_grid.previous_position = snapshot.combat->player.position;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));

    ARPG_REQUIRE(state.sweep_waypoint == 10U);
    return {};
}

arpg::test::Failure ranged_driver_hands_off_to_light_melee() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.combat->player.position = {45.0F, 0.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.ranged_target_ordinal = 7U;
    state.ranged_stance = snapshot.combat->player.position;
    state.ranged_facing = arpg::combat::Facing::right;
    state.stance_reached = true;

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(state.close_for_light);
    ARPG_REQUIRE(state.melee_chain);
    ARPG_REQUIRE(movement.x == 1);
    return {};
}

arpg::test::Failure melee_chain_prioritizes_existing_light_lane() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    auto& in_lane = snapshot.combat->monsters[1];
    in_lane.active = true;
    in_lane.hp = 100;
    in_lane.monster_ordinal = 8U;
    in_lane.position = {1.0F, 0.0F, 0.0F};
    snapshot.combat->monster_count = 2U;
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.melee_chain = true;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.ranged_target_ordinal == 8U);
    ARPG_REQUIRE(state.close_for_light);
    return {};
}

arpg::test::Failure melee_stall_reuses_grid_escape() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.melee_chain = true;
    state.close_for_light = true;
    state.ranged_target_ordinal = 7U;
    state.pending_close_progress_check = true;
    state.previous_close_position = snapshot.combat->player.position;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(state.stalled_target_ordinal == 7U);
    ARPG_REQUIRE(state.sweep_escape);
    ARPG_REQUIRE(state.melee_chain);
    ARPG_REQUIRE(state.recover_until_light_lane);
    return {};
}

arpg::test::Failure melee_recovery_filters_multiple_but_reacquires_last()
    noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    auto& alternate = snapshot.combat->monsters[1];
    alternate.active = true;
    alternate.hp = 100;
    alternate.monster_ordinal = 8U;
    alternate.position = {-50.0F, 20.0F, 0.0F};
    snapshot.combat->monster_count = 2U;
    snapshot.remaining_targets = 2U;
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.melee_chain = true;
    state.recover_until_light_lane = true;
    state.sweep_escape = true;
    state.stalled_target_ordinal = 7U;
    state.sweep_grid.pending_movement_progress_check = true;
    state.sweep_grid.previous_position = {-1.0F, 0.0F, 0.0F};

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.recover_until_light_lane);
    ARPG_REQUIRE(state.sweep_escape);
    ARPG_REQUIRE(state.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);

    snapshot = combat_with_target(false);
    validation::Stage10ValidationState last{};
    last.entered_abyss = true;
    last.melee_chain = true;
    last.recover_until_light_lane = true;
    last.sweep_escape = true;
    last.stalled_target_ordinal = 7U;
    last.sweep_grid.phase = validation::Stage10GridRoutePhase::route;
    last.sweep_grid.route_rejoins = 4U;
    last.sweep_grid.pending_movement_progress_check = true;
    last.sweep_grid.previous_position =
        snapshot.combat->player.position;
    snapshot.combat->monsters[0].active = false;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, last));
    ARPG_REQUIRE(!last.recover_until_light_lane);
    ARPG_REQUIRE(last.sweep_escape);
    ARPG_REQUIRE(last.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(last.stalled_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(last.sweep_waypoint == 1U);

    snapshot.combat->monsters[0].active = true;
    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, last));
    ARPG_REQUIRE(!last.sweep_escape);
    ARPG_REQUIRE(last.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(last.close_for_light);
    return {};
}

arpg::test::Failure melee_recovery_exits_when_lane_appears() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.combat->monsters[0].position = {1.0F, 0.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.melee_chain = true;
    state.recover_until_light_lane = true;
    state.sweep_escape = true;
    state.stalled_target_ordinal = 7U;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(!state.recover_until_light_lane);
    ARPG_REQUIRE(!state.sweep_escape);
    ARPG_REQUIRE(state.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(state.close_for_light);
    return {};
}

arpg::test::Failure ranged_driver_grid_uses_far_boundary_after_block() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.sweep_escape = true;
    state.sweep_grid.phase =
        validation::Stage10GridRoutePhase::join_near_x;
    state.sweep_grid.boundary_column = 10U;
    state.sweep_grid.pending_movement_progress_check = true;
    state.sweep_grid.previous_position = snapshot.combat->player.position;

    const auto movement = validation::stage10_validation_input(
        session, snapshot, config, state);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_far_x);
    ARPG_REQUIRE(state.sweep_grid.boundary_column == 11U);
    ARPG_REQUIRE(movement.x == 1);
    return {};
}

arpg::test::Failure ranged_driver_grid_rotates_after_rejoin_limit() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.sweep_escape = true;
    state.sweep_waypoint = 2U;
    state.sweep_grid.phase = validation::Stage10GridRoutePhase::route;
    state.sweep_grid.route_rejoins = 4U;
    state.sweep_grid.pending_movement_progress_check = true;
    state.sweep_grid.previous_position = snapshot.combat->player.position;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.sweep_waypoint == 3U);
    ARPG_REQUIRE(state.sweep_grid.route_rejoins == 0U);
    return {};
}

arpg::test::Failure warning_abyss_does_not_mutate_clear_navigation() noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::thunderstorm_warning;
    validation::Stage10ValidationState state{};
    state.entered_abyss = true;
    state.sweep_escape = true;
    state.sweep_waypoint = 6U;
    state.ranged_target_ordinal = 42U;
    state.melee_chain = true;
    state.close_for_light = true;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));
    ARPG_REQUIRE(state.sweep_escape);
    ARPG_REQUIRE(state.sweep_waypoint == 6U);
    ARPG_REQUIRE(state.ranged_target_ordinal == 42U);
    ARPG_REQUIRE(state.melee_chain);
    ARPG_REQUIRE(state.close_for_light);
    return {};
}

arpg::test::Failure reward_targets_require_full_abyss_clear() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.is_abyss = true;
    snapshot.phase = dungeon::RoomPhase::combat;
    snapshot.ground_item_count = 1U;
    snapshot.abyss_pending_rewards = 1U;
    validation::Stage10ValidationState state{};
    platform::RaylibHostConfig config{};

    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    ARPG_REQUIRE(!validation::stage10_validation_reached(
        snapshot, config, state));
    config.stage10_validation =
        platform::Stage10ValidationScenario::pending_reward;
    ARPG_REQUIRE(!validation::stage10_validation_reached(
        snapshot, config, state));
    snapshot.phase = dungeon::RoomPhase::committing;
    ARPG_REQUIRE(validation::stage10_validation_reached(
        snapshot, config, state));
    snapshot.is_abyss = false;
    ARPG_REQUIRE(!validation::stage10_validation_reached(
        snapshot, config, state));
    snapshot.is_abyss = true;
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    ARPG_REQUIRE(!validation::stage10_validation_reached(
        snapshot, config, state));
    config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    ARPG_REQUIRE(validation::stage10_validation_reached(
        snapshot, config, state));
    return {};
}

arpg::test::Failure restarted_failed_name_reaches_resumed_v9_started_combat()
    noexcept {
    platform::RaylibHostConfig config{};
    config.stage10_validation =
        platform::Stage10ValidationScenario::restarted_failed;
    validation::Stage10ValidationState state{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.is_abyss = true;
    snapshot.phase = dungeon::RoomPhase::combat;

    ARPG_REQUIRE(!validation::stage10_validation_reached(
        snapshot, config, state));
    state.entered_abyss = true;
    ARPG_REQUIRE(validation::stage10_validation_reached(
        snapshot, config, state));

    snapshot.phase = dungeon::RoomPhase::locked;
    ARPG_REQUIRE(!validation::stage10_validation_reached(
        snapshot, config, state));
    snapshot.phase = dungeon::RoomPhase::combat;
    snapshot.is_abyss = false;
    ARPG_REQUIRE(!validation::stage10_validation_reached(
        snapshot, config, state));
    return {};
}

arpg::test::Failure hole_driver_defeats_last_target_before_routing_to_hole()
    noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    dungeon::DungeonSession reward_session{};
    platform::RaylibHostConfig reward_config{};
    reward_config.stage10_validation =
        platform::Stage10ValidationScenario::reward_chest;
    validation::Stage10ValidationState reward_state{};
    const auto reward_movement = validation::stage10_validation_input(
        reward_session, snapshot, reward_config, reward_state);

    dungeon::DungeonSession hole_session{};
    platform::RaylibHostConfig hole_config{};
    hole_config.stage10_validation =
        platform::Stage10ValidationScenario::abyss_hole_descent;
    validation::Stage10ValidationState hole_state{};
    const auto hole_movement = validation::stage10_validation_input(
        hole_session, snapshot, hole_config, hole_state);

    ARPG_REQUIRE(hole_movement.x == reward_movement.x);
    ARPG_REQUIRE(hole_movement.y == reward_movement.y);
    ARPG_REQUIRE(hole_state.ranged_target_ordinal
        == reward_state.ranged_target_ordinal);
    ARPG_REQUIRE(hole_state.ranged_stance.x == reward_state.ranged_stance.x);
    ARPG_REQUIRE(hole_state.ranged_stance.y == reward_state.ranged_stance.y);
    ARPG_REQUIRE(hole_state.ranged_facing == reward_state.ranged_facing);
    ARPG_REQUIRE(hole_state.pending_stance_progress_check
        == reward_state.pending_stance_progress_check);
    ARPG_REQUIRE(hole_state.stance_reached == reward_state.stance_reached);
    ARPG_REQUIRE(hole_state.close_for_light == reward_state.close_for_light);

    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.remaining_targets = 0U;
    snapshot.combat->player.position = {35.0F, 62.0F, 0.0F};
    validation::Stage10ValidationState descent{};
    descent.entered_abyss = true;
    auto movement = validation::stage10_validation_input(
        hole_session, snapshot, hole_config, descent);
    ARPG_REQUIRE(movement.x == -1);
    ARPG_REQUIRE(movement.y == 0);
    ARPG_REQUIRE(descent.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_near_x);
    ARPG_REQUIRE(descent.sweep_grid.pending_movement_progress_check);

    movement = validation::stage10_validation_input(
        hole_session, snapshot, hole_config, descent);
    ARPG_REQUIRE(movement.x == 1);
    ARPG_REQUIRE(movement.y == 0);
    ARPG_REQUIRE(descent.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_far_x);
    ARPG_REQUIRE(descent.sweep_grid.pending_movement_progress_check);

    snapshot.combat->player.position = arpg::platform::kHoleCenter;
    snapshot.abyss_exit_confirmation_armed = true;
    descent.sweep_grid = {};
    movement = validation::stage10_validation_input(
        hole_session, snapshot, hole_config, descent);
    ARPG_REQUIRE(movement.x == 0);
    ARPG_REQUIRE(movement.y == 0);
    ARPG_REQUIRE(descent.descent_warning_seen);
    return {};
}

arpg::test::Failure abyss_skill_whitelist_only_contains_clear_scenarios()
    noexcept {
    using Scenario = platform::Stage10ValidationScenario;
    ARPG_REQUIRE(validation::stage10_validation_abyss_skills_enabled(
        Scenario::reward_chest));
    ARPG_REQUIRE(validation::stage10_validation_abyss_skills_enabled(
        Scenario::pending_reward));
    ARPG_REQUIRE(validation::stage10_validation_abyss_skills_enabled(
        Scenario::exit_confirmation));
    ARPG_REQUIRE(validation::stage10_validation_abyss_skills_enabled(
        Scenario::abyss_hole_descent));
    ARPG_REQUIRE(!validation::stage10_validation_abyss_skills_enabled(
        Scenario::thunderstorm_warning));
    ARPG_REQUIRE(!validation::stage10_validation_abyss_skills_enabled(
        Scenario::hunting_flames_warning));
    ARPG_REQUIRE(!validation::stage10_validation_abyss_skills_enabled(
        Scenario::chaos_expansion));
    return {};
}

arpg::test::Failure stage11c_full_clear_skips_frame_combat_injection()
    noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.combat->player.position = {0.0F, 0.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::left;
    snapshot.combat->monsters[0].position = {-1.0F, 0.62F, 0.0F};
    snapshot.combat->skill_cooldowns[0] = 1U;
    snapshot.combat->skill_cooldowns[1] = 1U;
    platform::RaylibHostConfig config{};
    config.stage11c_hud_validation =
        platform::Stage11CHudValidationScenario::cleared_exit;
    validation::Stage11CHudValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11c_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    ARPG_REQUIRE(!input.combat_actions[0]);
    ARPG_REQUIRE(!input.combat_actions[1]);
    ARPG_REQUIRE(!input.active_skill_slots[0]);
    ARPG_REQUIRE(!input.active_skill_slots[1]);

    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    config.stage11c_hud_validation =
        platform::Stage11CHudValidationScenario::abyss_abandon;
    const auto exit_keys = validation::inject_stage11c_physical_edges(
        {}, config, settings, snapshot, state);
    const auto exit_input = platform::map_host_frame_input(
        settings, exit_keys);
    ARPG_REQUIRE(exit_input.movement.x == 0);
    ARPG_REQUIRE(exit_input.movement.y == 0);
    return {};
}

arpg::test::Failure stage11c_full_clear_fixed_steps_ignore_early_exit()
    noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.exits_unlocked = true;
    snapshot.combat->player.position = {};
    snapshot.combat->monsters[0].position = {-50.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config = right_abyss_door_config();
    config.stage11c_hud_validation =
        platform::Stage11CHudValidationScenario::cleared_exit;
    const auto runtime = platform::HostValidationRuntime::create(
        config, arpg::settings::SettingsLoadStatus::loaded);
    dungeon::DungeonSession session{};

    ARPG_REQUIRE(runtime != nullptr);
    const auto movement = runtime->fixed_step_movement(session, snapshot, {});
    ARPG_REQUIRE(movement.x == -1);
    ARPG_REQUIRE(movement.y == 0);
    return {};
}

arpg::test::Failure stage11c_full_clear_awaiting_exit_joins_grid_before_door()
    noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = unlocked_combat_snapshot();
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.remaining_targets = 0U;
    snapshot.combat->player.hp = 100;
    snapshot.combat->player.position = {3.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.validation_abyss_direction = static_cast<std::uint8_t>(
        dungeon::ExitDirection::up);
    config.stage11c_hud_validation =
        platform::Stage11CHudValidationScenario::abyss_abandon;
    const auto runtime = platform::HostValidationRuntime::create(
        config, arpg::settings::SettingsLoadStatus::loaded);

    ARPG_REQUIRE(runtime != nullptr);
    const auto movement = runtime->fixed_step_movement(
        session, snapshot, {});

    ARPG_REQUIRE(movement.x == -1);
    ARPG_REQUIRE(movement.y == 0);

    snapshot.is_abyss = true;
    snapshot.abyss_exit_confirmation_armed = true;
    const auto armed = runtime->fixed_step_movement(
        session, snapshot, {1, 1});
    ARPG_REQUIRE(armed.x == 0);
    ARPG_REQUIRE(armed.y == 0);

    platform::RaylibHostConfig cleared_config{};
    cleared_config.stage11c_hud_validation =
        platform::Stage11CHudValidationScenario::cleared_exit;
    const auto cleared_runtime = platform::HostValidationRuntime::create(
        cleared_config, arpg::settings::SettingsLoadStatus::loaded);
    snapshot.is_abyss = false;
    snapshot.abyss_exit_confirmation_armed = false;
    ARPG_REQUIRE(cleared_runtime != nullptr);
    const auto preserved = cleared_runtime->fixed_step_movement(
        session, snapshot, {1, 1});
    ARPG_REQUIRE(preserved.x == 1);
    ARPG_REQUIRE(preserved.y == 1);
    return {};
}

arpg::test::Failure stage11c_full_clear_reacquires_after_failed_local_detour()
    noexcept {
    dungeon::DungeonSession session{};
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.remaining_targets = 2U;
    auto& alternate = snapshot.combat->monsters[1U];
    alternate.active = true;
    alternate.hp = 100;
    alternate.max_hp = 100;
    alternate.monster_ordinal = 8U;
    alternate.position = {-40.0F, 0.0F, 0.0F};
    snapshot.combat->monster_count = 2U;
    platform::RaylibHostConfig config{};
    config.stage11c_hud_validation =
        platform::Stage11CHudValidationScenario::abyss_abandon;
    validation::Stage10ValidationState state{};
    state.sweep_escape = true;
    state.stalled_target_ordinal = 7U;

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, state));

    ARPG_REQUIRE(!state.sweep_escape);
    ARPG_REQUIRE(state.ranged_target_ordinal == 8U);

    validation::Stage10ValidationState lane_recovery{};
    lane_recovery.sweep_escape = true;
    lane_recovery.recover_until_light_lane = true;
    lane_recovery.stalled_target_ordinal = 7U;
    alternate.position = {-40.0F, 40.0F, 0.0F};

    const auto recovery_movement = validation::stage10_validation_input(
        session, snapshot, config, lane_recovery);

    ARPG_REQUIRE(lane_recovery.sweep_escape);
    ARPG_REQUIRE(lane_recovery.recover_until_light_lane);
    ARPG_REQUIRE(lane_recovery.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(recovery_movement.x != 0 || recovery_movement.y != 0);

    validation::Stage10ValidationState reached_waypoint{};
    reached_waypoint.melee_chain = true;
    reached_waypoint.sweep_escape = true;
    reached_waypoint.recover_until_light_lane = true;
    reached_waypoint.stalled_target_ordinal =
        arpg::combat::kInvalidMonsterOrdinal;
    snapshot.combat->player.position = {
        arpg::combat::room_bounds::min_x,
        arpg::combat::room_bounds::min_y
            + 0.5F * arpg::combat::room_spatial::cell_depth,
        0.0F,
    };
    snapshot.combat->monsters[0U].position = {-40.0F, 40.0F, 0.0F};
    alternate.position = {-30.0F, 40.0F, 0.0F};

    static_cast<void>(validation::stage10_validation_input(
        session, snapshot, config, reached_waypoint));

    ARPG_REQUIRE(!reached_waypoint.sweep_escape);
    ARPG_REQUIRE(!reached_waypoint.recover_until_light_lane);
    ARPG_REQUIRE(reached_waypoint.ranged_target_ordinal
        != arpg::combat::kInvalidMonsterOrdinal);
    return {};
}

arpg::test::Failure
stage11d_priority_target_waits_for_confirmed_death_before_advancing()
    noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(false);
    snapshot.initial_monster_count = 300U;
    snapshot.defeated_monster_count = 0U;
    snapshot.remaining_targets = 300U;
    auto& first = snapshot.combat->monsters[0U];
    first.spawn_ordinal = 124U;
    auto& second = snapshot.combat->monsters[1U];
    second.active = true;
    second.hp = 100;
    second.max_hp = 100;
    second.spawn_ordinal = 125U;
    auto& streamed_lower = snapshot.combat->monsters[2U];
    streamed_lower.active = true;
    streamed_lower.hp = 100;
    streamed_lower.max_hp = 100;
    streamed_lower.spawn_ordinal = 6U;
    snapshot.combat->monster_count = 3U;
    validation::Stage11DLootValidationState state{};

    ARPG_REQUIRE(validation::stage11d_priority_monster_ordinal(
        snapshot, state) == 6U);
    streamed_lower.active = false;
    snapshot.defeated_monster_count = 1U;
    ARPG_REQUIRE(validation::stage11d_priority_monster_ordinal(
        snapshot, state) == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(state.target_ordinal == 6U);

    snapshot.ground_item_count = 1U;
    snapshot.ground_items[0U].ordinal = 6U;
    snapshot.ground_items[0U].source = dungeon::GroundItemSource::monster_drop;
    ARPG_REQUIRE(validation::stage11d_priority_monster_ordinal(
        snapshot, state) == 124U);
    snapshot.ground_item_count = 0U;

    streamed_lower.active = true;
    ARPG_REQUIRE(validation::stage11d_priority_monster_ordinal(
        snapshot, state) == 124U);

    first.active = false;
    snapshot.defeated_monster_count = 2U;
    ARPG_REQUIRE(validation::stage11d_priority_monster_ordinal(
        snapshot, state) == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(state.target_ordinal == 124U);
    first.active = true;
    first.hp = 0;
    ARPG_REQUIRE(validation::stage11d_priority_monster_ordinal(
        snapshot, state) == 125U);

    second.active = false;
    ARPG_REQUIRE(validation::stage11d_priority_monster_ordinal(
        snapshot, state) == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(state.target_ordinal == 125U);
    second.active = true;
    second.hp = 0;

    snapshot.defeated_monster_count = 3U;
    auto& later = snapshot.combat->monsters[3U];
    later.active = true;
    later.hp = 100;
    later.max_hp = 100;
    later.spawn_ordinal = 137U;
    snapshot.combat->monster_count = 4U;
    ARPG_REQUIRE(validation::stage11d_priority_monster_ordinal(
        snapshot, state) == 137U);

    later.active = false;
    snapshot.defeated_monster_count = 4U;
    ARPG_REQUIRE(validation::stage11d_priority_monster_ordinal(
        snapshot, state) == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(state.target_ordinal == 137U);
    return {};
}

arpg::test::Failure stage11d_rearm_suspends_physical_injection() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {1.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    const auto runtime = platform::HostValidationRuntime::create(
        config, arpg::settings::SettingsLoadStatus::loaded);
    const auto settings = arpg::settings::default_settings();

    ARPG_REQUIRE(runtime != nullptr);
    const auto active_keys = runtime->inject_physical_edges(
        {}, settings, snapshot, false);
    const auto active_input = platform::map_host_frame_input(
        settings, active_keys);
    ARPG_REQUIRE(active_input.movement.x != 0
        || active_input.movement.y != 0);

    const auto keys = runtime->inject_physical_edges(
        {}, settings, snapshot, true);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    ARPG_REQUIRE(!input.combat_actions[0]);
    ARPG_REQUIRE(!input.combat_actions[1]);
    ARPG_REQUIRE(!input.combat_actions[2]);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_ignores_ordinary_rarity_capture_gate() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {10.0F, 0.0F, 0.0F};
    snapshot.ground_item_count = 3U;
    constexpr std::array<arpg::items::ItemRarity, 3U> kRarities{{
        arpg::items::ItemRarity::normal,
        arpg::items::ItemRarity::magic,
        arpg::items::ItemRarity::rare,
    }};
    for (std::size_t index = 0U; index < kRarities.size(); ++index) {
        snapshot.ground_items[index].source =
            dungeon::GroundItemSource::monster_drop;
        snapshot.ground_items[index].rarity = kRarities[index];
    }
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x != 0 || input.movement.y != 0);
    return {};
}

arpg::test::Failure
stage11d_player_damage_requires_consecutive_live_hp_drop() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.combat->player.hp = 80;
    snapshot.combat->player.max_hp = 100;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    platform::PauseMenuState pause_menu{};
    platform::DungeonRenderStatus status{};
    platform::GroundLootView view{};
    platform::HudNoticeView notices{};
    validation::Stage11DLootValidationState state{};

    static_cast<void>(validation::stage11d_target_visible(
        config, snapshot, pause_menu, status,
        arpg::settings::LootFilterMode::rare_only, view, notices,
        state, 1280, 720));
    ARPG_REQUIRE(!state.player_damage_observed);

    snapshot.combat->player.max_hp = 120;
    static_cast<void>(validation::stage11d_target_visible(
        config, snapshot, pause_menu, status,
        arpg::settings::LootFilterMode::rare_only, view, notices,
        state, 1280, 720));
    ARPG_REQUIRE(!state.player_damage_observed);

    snapshot.combat->player.hp = 79;
    static_cast<void>(validation::stage11d_target_visible(
        config, snapshot, pause_menu, status,
        arpg::settings::LootFilterMode::rare_only, view, notices,
        state, 1280, 720));
    ARPG_REQUIRE(state.player_damage_observed);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_capture_requires_matching_visible_label() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ground_item_count = 1U;
    auto& item = snapshot.ground_items[0U];
    item.ordinal = 42U;
    item.item_id = 9001U;
    item.source = dungeon::GroundItemSource::abyss_chest;
    item.rarity = arpg::items::ItemRarity::magic;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    platform::PauseMenuState pause_menu{};
    platform::DungeonRenderStatus status{};
    platform::GroundLootView view{};
    platform::HudNoticeView notices{};
    validation::Stage11DLootValidationState state{};

    ARPG_REQUIRE(!validation::stage11d_target_visible(
        config, snapshot, pause_menu, status,
        arpg::settings::LootFilterMode::rare_only, view, notices,
        state, 1280, 720));

    view.count = 1U;
    view.labels[0U].ordinal = item.ordinal;
    ARPG_REQUIRE(!validation::stage11d_target_visible(
        config, snapshot, pause_menu, status,
        arpg::settings::LootFilterMode::rare_only, view, notices,
        state, 1280, 720));

    view.labels[0U].abyss = true;
    ARPG_REQUIRE(validation::stage11d_target_visible(
        config, snapshot, pause_menu, status,
        arpg::settings::LootFilterMode::rare_only, view, notices,
        state, 1280, 720));
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_moves_to_uncaptured_ground_reward() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.remaining_targets = 0U;
    snapshot.combat->monster_count = 0U;
    snapshot.combat->player.position = {};
    snapshot.ground_item_count = 1U;
    auto& item = snapshot.ground_items[0U];
    item.ordinal = 42U;
    item.item_id = 9001U;
    item.source = dungeon::GroundItemSource::abyss_chest;
    item.rarity = arpg::items::ItemRarity::magic;
    item.position = {4.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto approach_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto approach_input = platform::map_host_frame_input(
        settings, approach_keys);
    ARPG_REQUIRE(approach_input.movement.x == 1);
    ARPG_REQUIRE(approach_input.movement.y == 0);
    ARPG_REQUIRE(!state.abyss_claim_requested);

    state.abyss_item_id = item.item_id;
    state.captured = true;
    const auto pickup_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto pickup_input = platform::map_host_frame_input(
        settings, pickup_keys);
    ARPG_REQUIRE(pickup_input.movement.x == 1);
    ARPG_REQUIRE(pickup_input.movement.y == 0);
    ARPG_REQUIRE(state.abyss_claim_requested);
    return {};
}

arpg::test::Failure
stage11d_close_boundary_target_retreats_to_ranged_stance() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.combat->player.position = {
        arpg::combat::room_bounds::min_x + 0.10F, 0.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {
        arpg::combat::room_bounds::min_x + 0.12F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x == 1);
    ARPG_REQUIRE(input.movement.y == 0);
    ARPG_REQUIRE(!input.combat_actions[0]);
    ARPG_REQUIRE(!input.combat_actions[1]);
    ARPG_REQUIRE(!input.combat_actions[2]);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_unavailable_player_is_neutral() noexcept {
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    const auto settings = arpg::settings::default_settings();
    const auto require_neutral = [&](dungeon::DungeonSnapshot snapshot) {
        validation::Stage11DLootValidationState state{};
        const auto keys = validation::inject_stage11d_physical_edges(
            {}, config, settings, snapshot, state);
        const auto input = platform::map_host_frame_input(settings, keys);
        ARPG_REQUIRE(input.movement.x == 0);
        ARPG_REQUIRE(input.movement.y == 0);
        ARPG_REQUIRE(!input.combat_actions[0]);
        ARPG_REQUIRE(!input.combat_actions[1]);
        ARPG_REQUIRE(!input.combat_actions[2]);
        for (const bool skill : input.active_skill_slots) {
            ARPG_REQUIRE(!skill);
        }
        return arpg::test::Failure{};
    };
    const auto base = [&] {
        auto snapshot = combat_with_target(true);
        snapshot.skill_loadout = arpg::skills::default_skill_loadout();
        snapshot.combat->player.position = {};
        snapshot.combat->player.facing = arpg::combat::Facing::right;
        snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
        return snapshot;
    };

    auto hurt = base();
    hurt.combat->player.hurt_ticks = 1U;
    if (const auto failure = require_neutral(hurt); failure.expression != nullptr)
        return failure;
    auto hit_stop = base();
    hit_stop.combat->player.hit_stop_ticks = 1U;
    if (const auto failure = require_neutral(hit_stop);
            failure.expression != nullptr) return failure;
    auto attacking = base();
    attacking.combat->player.active_attack = arpg::combat::AttackId::j1;
    if (const auto failure = require_neutral(attacking);
            failure.expression != nullptr) return failure;
    auto skill_active = base();
    skill_active.combat->active_skill.id =
        arpg::skills::ActiveSkillId::draw_slash;
    if (const auto failure = require_neutral(skill_active);
            failure.expression != nullptr) return failure;
    auto buffered = base();
    buffered.combat->diagnostics.input_size = 1U;
    if (const auto failure = require_neutral(buffered);
            failure.expression != nullptr) return failure;
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_turns_at_ranged_stance_before_cast() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.combat->player.position = {-5.0F, 0.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {-10.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x == -1);
    ARPG_REQUIRE(input.movement.y == 0);
    ARPG_REQUIRE(!input.combat_actions[0]);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }

    snapshot.combat->player.facing = arpg::combat::Facing::left;
    const auto cast_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto cast_input = platform::map_host_frame_input(
        settings, cast_keys);
    ARPG_REQUIRE(cast_input.movement.x == 0);
    ARPG_REQUIRE(cast_input.movement.y == 0);
    ARPG_REQUIRE(cast_input.active_skill_slots[1U]);
    ARPG_REQUIRE(state.abyss_ranged.stance_reached);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_blocked_facing_turn_enters_local_recovery() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {-5.0F, 1.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {-10.0F, 1.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto turn_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto turn_input = platform::map_host_frame_input(
        settings, turn_keys);
    ARPG_REQUIRE(turn_input.movement.x == -1);
    ARPG_REQUIRE(turn_input.movement.y == 0);

    const auto recovery_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto recovery_input = platform::map_host_frame_input(
        settings, recovery_keys);

    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.recovery_target_valid);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(recovery_input.movement.x != 0
        || recovery_input.movement.y != 0);
    ARPG_REQUIRE(!recovery_input.combat_actions[0U]);
    for (const bool skill : recovery_input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_edge_stance_turn_does_not_oscillate() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {-5.21F, 0.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {-10.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto recenter_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto recenter_input = platform::map_host_frame_input(
        settings, recenter_keys);
    ARPG_REQUIRE(recenter_input.movement.x == 1);
    ARPG_REQUIRE(recenter_input.movement.y == 0);

    snapshot.combat->player.position.x = -5.11F;
    const auto turn_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto turn_input = platform::map_host_frame_input(
        settings, turn_keys);
    ARPG_REQUIRE(turn_input.movement.x == -1);
    ARPG_REQUIRE(turn_input.movement.y == 0);

    snapshot.combat->player.position.x = -5.21F;
    snapshot.combat->player.facing = arpg::combat::Facing::left;
    const auto cast_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto cast_input = platform::map_host_frame_input(
        settings, cast_keys);
    ARPG_REQUIRE(cast_input.movement.x == 0);
    ARPG_REQUIRE(cast_input.movement.y == 0);
    ARPG_REQUIRE(cast_input.active_skill_slots[1U]);
    return {};
}

arpg::test::Failure
stage11d_streaming_gap_uses_physical_grid_sweep() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.combat->player.position = {};
    snapshot.combat->monsters[0U].active = false;
    snapshot.combat->monster_count = 0U;
    snapshot.remaining_targets = 1U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x != 0 || input.movement.y != 0);
    ARPG_REQUIRE(!input.combat_actions[0]);
    ARPG_REQUIRE(!input.combat_actions[1]);
    ARPG_REQUIRE(!input.combat_actions[2]);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_ranged_stance_preserves_fire_routing() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::fire;
    snapshot.combat->player.position = {-1.41F, 0.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 1);
    ARPG_REQUIRE(!input.combat_actions[0]);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_blocked_stance_enters_grid_recovery() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {10.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    static_cast<void>(validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state));
    ARPG_REQUIRE(state.abyss_ranged.pending_stance_progress_check);

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.recovery_target_valid);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(input.movement.x != 0 || input.movement.y != 0);
    ARPG_REQUIRE(!input.combat_actions[0]);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_moved_target_releases_stale_geometry() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto cast_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto cast_input = platform::map_host_frame_input(
        settings, cast_keys);
    ARPG_REQUIRE(cast_input.active_skill_slots[1U]);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal == 7U);

    snapshot.combat->monsters[0U].position = {12.0F, 0.0F, 0.0F};
    const auto release_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto release_input = platform::map_host_frame_input(
        settings, release_keys);
    ARPG_REQUIRE(release_input.movement.x == 0);
    ARPG_REQUIRE(release_input.movement.y == 0);
    for (const bool skill : release_input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);

    const auto reacquire_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto reacquire_input = platform::map_host_frame_input(
        settings, reacquire_keys);
    ARPG_REQUIRE(reacquire_input.movement.x == 1);
    ARPG_REQUIRE(reacquire_input.movement.y == 0);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_area_skill_uses_equipped_physical_slot() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.skill_loadout.slots[4U] = snapshot.skill_loadout.slots[1U];
    snapshot.skill_loadout.slots[1U].active =
        arpg::skills::ActiveSkillId::none;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    snapshot.combat->skill_cooldowns[0U] = 1U;
    snapshot.combat->skill_cooldowns[1U] = 0U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.active_skill_slots[4U]);
    ARPG_REQUIRE(!input.active_skill_slots[1U]);
    ARPG_REQUIRE(!input.active_skill_slots[0U]);
    ARPG_REQUIRE(!input.combat_actions[0]);
    ARPG_REQUIRE(!input.combat_actions[1]);
    ARPG_REQUIRE(!input.combat_actions[2]);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_high_hp_attacks_through_nearby_danger() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    auto& danger = snapshot.combat->monsters[1U];
    danger.active = true;
    danger.hp = 100;
    danger.max_hp = 100;
    danger.monster_ordinal = 8U;
    danger.spawn_ordinal = 8U;
    danger.position = {2.0F, 0.0F, 0.0F};
    danger.reaction = arpg::combat::ReactionState::idle;
    danger.ai_phase = arpg::combat::MonsterAiPhase::telegraph;
    snapshot.combat->monster_count = 2U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.ranged_target_ordinal = 7U;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(!state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(input.active_skill_slots[1U]);
    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_low_hp_escapes_approaching_danger() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {0.0F, 0.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::left;
    snapshot.combat->monsters[0U].position = {-10.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    static_cast<void>(validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state));
    snapshot.combat->player.position = {-0.10F, 0.0F, 0.0F};
    snapshot.combat->player.hp = snapshot.combat->player.max_hp / 5;
    auto& danger = snapshot.combat->monsters[1U];
    danger.active = true;
    danger.hp = 100;
    danger.max_hp = 100;
    danger.monster_ordinal = 8U;
    danger.spawn_ordinal = 8U;
    danger.position = {-3.5F, 0.0F, 0.0F};
    danger.reaction = arpg::combat::ReactionState::idle;
    danger.ai_phase = arpg::combat::MonsterAiPhase::telegraph;
    snapshot.combat->monster_count = 2U;

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x != 0 || input.movement.y != 0);
    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(!input.combat_actions[0]);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_low_hp_attacks_reachable_danger() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->player.hp = snapshot.combat->player.max_hp / 5;
    auto& target = snapshot.combat->monsters[0U];
    target.position = {1.0F, 0.0F, 0.0F};
    target.reaction = arpg::combat::ReactionState::idle;
    target.ai_phase = arpg::combat::MonsterAiPhase::telegraph;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.melee_chain = true;
    state.abyss_ranged.close_for_light = true;
    state.abyss_ranged.ranged_target_ordinal = target.monster_ordinal;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(!state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal
        == target.monster_ordinal);
    ARPG_REQUIRE(input.combat_actions[0U]);
    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_low_hp_preserves_grid_recovery() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.hp = snapshot.combat->player.max_hp / 5;
    snapshot.combat->monsters[0U].position = {2.0F, 0.0F, 0.0F};
    snapshot.combat->monsters[0U].reaction =
        arpg::combat::ReactionState::idle;
    snapshot.combat->monsters[0U].ai_phase =
        arpg::combat::MonsterAiPhase::telegraph;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.stalled_target_ordinal = 7U;
    state.abyss_ranged.recovery_target = {10.0F, 0.0F, 0.0F};
    state.abyss_ranged.recovery_target_valid = true;
    state.abyss_ranged.sweep_escape = true;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.recovery_target_valid);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(input.movement.x != 0 || input.movement.y != 0);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_low_hp_global_sweep_ignores_opportunistic_attack()
    noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->player.hp = snapshot.combat->player.max_hp / 5;
    snapshot.combat->monsters[0U].position = {1.5F, 0.0F, 0.0F};
    snapshot.combat->skill_cooldowns[0U] = 1U;
    snapshot.combat->skill_cooldowns[1U] = 0U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.sweep_cursor_initialized = true;
    state.abyss_ranged.sweep_escape = true;
    state.abyss_ranged.sweep_waypoint = 20U;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 1);
    ARPG_REQUIRE(!input.combat_actions[0U]);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_global_sweep_routes_fire_to_waypoint() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ecology = dungeon::DungeonElement::fire;
    snapshot.combat->player.position = {-1.41F, 0.0F, 0.0F};
    snapshot.combat->monsters[0U].active = false;
    snapshot.combat->monster_count = 0U;
    snapshot.remaining_targets = 1U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.recovery_target = {10.0F, 0.0F, 0.0F};
    state.abyss_ranged.recovery_target_valid = true;
    state.abyss_ranged.sweep_escape = true;
    state.abyss_ranged.sweep_grid.phase =
        validation::Stage10GridRoutePhase::join_far_x;
    state.abyss_ranged.sweep_grid.pending_movement_progress_check = true;
    state.abyss_ranged.sweep_grid.previous_position =
        snapshot.combat->player.position;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(!state.abyss_ranged.recovery_target_valid);
    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == -1);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_global_sweep_casts_ready_area_skill() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {1.5F, 0.0F, 0.0F};
    snapshot.combat->skill_cooldowns[0U] = 1U;
    snapshot.combat->skill_cooldowns[1U] = 0U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.sweep_escape = true;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    ARPG_REQUIRE(input.active_skill_slots[1U]);
    ARPG_REQUIRE(!input.combat_actions[0U]);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_global_sweep_attacks_light_lane_without_skill()
    noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {1.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.sweep_escape = true;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    ARPG_REQUIRE(input.combat_actions[0U]);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_close_stall_hands_off_outer_sweep_cursor() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {10.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.sweep_waypoint = 20U;
    state.sweep_grid.phase = validation::Stage10GridRoutePhase::route;
    state.sweep_grid.pending_movement_progress_check = true;
    state.sweep_grid.previous_position = {-1.0F, 0.0F, 0.0F};
    state.abyss_ranged.melee_chain = true;
    state.abyss_ranged.close_for_light = true;
    state.abyss_ranged.ranged_target_ordinal = 7U;
    state.abyss_ranged.pending_close_progress_check = true;
    state.abyss_ranged.previous_close_position = {};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.recover_until_light_lane);
    ARPG_REQUIRE(state.abyss_ranged.sweep_waypoint == 20U);
    ARPG_REQUIRE(state.sweep_waypoint == 20U);
    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::need_join);
    ARPG_REQUIRE(!state.sweep_grid.pending_movement_progress_check);
    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 1);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_first_close_stall_starts_at_nearest_high_sweep_row()
    noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {0.0F, 70.0F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {10.0F, 70.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.melee_chain = true;
    state.abyss_ranged.close_for_light = true;
    state.abyss_ranged.ranged_target_ordinal = 7U;
    state.abyss_ranged.pending_close_progress_check = true;
    state.abyss_ranged.previous_close_position =
        snapshot.combat->player.position;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.recover_until_light_lane);
    ARPG_REQUIRE(state.sweep_waypoint == 36U);
    ARPG_REQUIRE(state.abyss_ranged.sweep_waypoint == 36U);
    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == -1);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_streaming_gap_routes_fire_grid_sweep() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ecology = dungeon::DungeonElement::fire;
    snapshot.combat->player.position = {-1.41F, 0.0F, 0.0F};
    snapshot.combat->monsters[0U].active = false;
    snapshot.combat->monster_count = 0U;
    snapshot.remaining_targets = 1U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.sweep_grid.phase
        == validation::Stage10GridRoutePhase::join_near_x);
    ARPG_REQUIRE(state.sweep_grid.boundary_column == 10U);
    ARPG_REQUIRE(state.sweep_grid.pending_movement_progress_check);
    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == -1);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_reacquires_resident_after_lane_recovery_waypoint()
    noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position =
        validation::stage10_validation_sweep_waypoint(0U);
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {};
    snapshot.remaining_targets = 52U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.sweep_cursor_initialized = true;
    state.sweep_waypoint = 9U;
    state.abyss_ranged.melee_chain = true;
    state.abyss_ranged.recover_until_light_lane = true;
    state.abyss_ranged.sweep_escape = true;
    state.abyss_ranged.stalled_target_ordinal =
        arpg::combat::kInvalidMonsterOrdinal;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(!state.abyss_ranged.recover_until_light_lane);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(input.movement.x == 1);
    ARPG_REQUIRE(input.movement.y == 1);
    ARPG_REQUIRE(state.sweep_waypoint == 0U);
    ARPG_REQUIRE(!state.sweep_grid.pending_movement_progress_check);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_unavailable_skills_closes_for_light() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.melee_chain);
    ARPG_REQUIRE(state.abyss_ranged.close_for_light);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(input.movement.x == 1);
    ARPG_REQUIRE(input.movement.y == 0);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_waiting_skills_closes_for_light() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    snapshot.combat->skill_cooldowns[0U] = 1U;
    snapshot.combat->skill_cooldowns[1U] = 1U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.melee_chain);
    ARPG_REQUIRE(state.abyss_ranged.close_for_light);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(input.movement.x == 1);
    ARPG_REQUIRE(input.movement.y == 0);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_waits_for_live_player_damage() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.combat->player.position = {};
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    config.validation_exit_after_presented_frames = 1U;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    const auto waiting_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto waiting_input = platform::map_host_frame_input(
        settings, waiting_keys);
    ARPG_REQUIRE(waiting_input.movement.x == 0);
    ARPG_REQUIRE(waiting_input.movement.y == 0);
    for (const bool action : waiting_input.combat_actions) {
        ARPG_REQUIRE(!action);
    }
    for (const bool skill : waiting_input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }

    state.player_damage_observed = true;
    const auto combat_keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto combat_input = platform::map_host_frame_input(
        settings, combat_keys);
    ARPG_REQUIRE(combat_input.movement.x != 0
        || combat_input.movement.y != 0);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_melee_lane_prefers_ready_area_skill() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {1.5F, 0.0F, 0.0F};
    snapshot.combat->skill_cooldowns[0U] = 1U;
    snapshot.combat->skill_cooldowns[1U] = 0U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.melee_chain = true;
    state.abyss_ranged.close_for_light = true;
    state.abyss_ranged.ranged_target_ordinal = 7U;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    ARPG_REQUIRE(input.active_skill_slots[1U]);
    ARPG_REQUIRE(!input.combat_actions[0U]);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_close_movement_casts_ready_area_skill() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {0.0F, 1.59F, 0.0F};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {
        arpg::combat::kStormCenterForward, 0.0F, 0.0F};
    snapshot.combat->skill_cooldowns[0U] = 1U;
    snapshot.combat->skill_cooldowns[1U] = 0U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.melee_chain = true;
    state.abyss_ranged.close_for_light = true;
    state.abyss_ranged.ranged_target_ordinal = 7U;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x != 0 || input.movement.y != 0);
    ARPG_REQUIRE(input.active_skill_slots[1U]);
    ARPG_REQUIRE(!input.combat_actions[0U]);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_no_skill_geometry_in_light_lane_attacks_without_release()
    noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    static_cast<void>(validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state));
    ARPG_REQUIRE(state.abyss_ranged.melee_chain);
    ARPG_REQUIRE(state.abyss_ranged.close_for_light);
    ARPG_REQUIRE(state.abyss_ranged.pending_close_progress_check);

    snapshot.combat->player.position = {5.0F, -0.20F, 0.0F};
    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    ARPG_REQUIRE(input.combat_actions[0U]);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal == 7U);
    ARPG_REQUIRE(state.abyss_ranged.melee_chain);
    ARPG_REQUIRE(state.abyss_ranged.close_for_light);
    for (const bool skill : input.active_skill_slots) {
        ARPG_REQUIRE(!skill);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_blocked_light_close_enters_grid_recovery() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    static_cast<void>(validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state));
    ARPG_REQUIRE(state.abyss_ranged.pending_close_progress_check);
    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.recover_until_light_lane);
    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal
        == arpg::combat::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(input.movement.x != 0 || input.movement.y != 0);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_close_stall_attacks_light_lane_blocker() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
    auto& blocker = snapshot.combat->monsters[1U];
    blocker.active = true;
    blocker.hp = 100;
    blocker.max_hp = 100;
    blocker.monster_ordinal = 8U;
    blocker.spawn_ordinal = 8U;
    blocker.position = {1.0F, 0.0F, 0.0F};
    snapshot.combat->monster_count = 2U;
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    state.abyss_ranged.melee_chain = true;
    state.abyss_ranged.close_for_light = true;
    state.abyss_ranged.ranged_target_ordinal = 7U;
    state.abyss_ranged.pending_close_progress_check = true;
    state.abyss_ranged.previous_close_position =
        snapshot.combat->player.position;
    const auto settings = arpg::settings::default_settings();

    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal == 8U);
    ARPG_REQUIRE(!state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(input.combat_actions[0U]);
    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_close_for_light_nonprogress_enters_grid_recovery()
    noexcept {
    constexpr std::array<arpg::combat::Vec3, 2U> kNonProgressPositions{{
        {0.0F, 0.10F, 0.0F},
        {-0.10F, 0.0F, 0.0F},
    }};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    const auto settings = arpg::settings::default_settings();

    for (const arpg::combat::Vec3 position : kNonProgressPositions) {
        dungeon::DungeonSnapshot snapshot = combat_with_target(true);
        snapshot.ecology = dungeon::DungeonElement::water;
        snapshot.combat->player.position = {};
        snapshot.combat->player.facing = arpg::combat::Facing::right;
        snapshot.combat->monsters[0U].position = {5.0F, 0.0F, 0.0F};
        validation::Stage11DLootValidationState state{};

        static_cast<void>(validation::inject_stage11d_physical_edges(
            {}, config, settings, snapshot, state));
        ARPG_REQUIRE(state.abyss_ranged.melee_chain);
        ARPG_REQUIRE(state.abyss_ranged.close_for_light);
        ARPG_REQUIRE(state.abyss_ranged.pending_close_progress_check);

        snapshot.combat->player.position = position;
        const auto keys = validation::inject_stage11d_physical_edges(
            {}, config, settings, snapshot, state);
        const auto input = platform::map_host_frame_input(settings, keys);

        ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
        ARPG_REQUIRE(state.abyss_ranged.ranged_target_ordinal
            == arpg::combat::kInvalidMonsterOrdinal);
        ARPG_REQUIRE(input.movement.x != 0 || input.movement.y != 0);
        ARPG_REQUIRE(!input.combat_actions[0U]);
    }
    return {};
}

arpg::test::Failure
stage11d_rare_abyss_lateral_stall_enters_grid_recovery() noexcept {
    dungeon::DungeonSnapshot snapshot = combat_with_target(true);
    snapshot.skill_loadout = arpg::skills::default_skill_loadout();
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.combat->player.position = {};
    snapshot.combat->player.facing = arpg::combat::Facing::right;
    snapshot.combat->monsters[0U].position = {10.0F, 0.0F, 0.0F};
    platform::RaylibHostConfig config{};
    config.stage11d_loot_validation =
        platform::Stage11DLootValidationScenario::rare_only_abyss;
    validation::Stage11DLootValidationState state{};
    const auto settings = arpg::settings::default_settings();

    static_cast<void>(validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state));
    snapshot.combat->player.position.y = 0.10F;
    const auto keys = validation::inject_stage11d_physical_edges(
        {}, config, settings, snapshot, state);
    const auto input = platform::map_host_frame_input(settings, keys);

    ARPG_REQUIRE(state.abyss_ranged.sweep_escape);
    ARPG_REQUIRE(state.abyss_ranged.recovery_target_valid);
    ARPG_REQUIRE(input.movement.x != 0 || input.movement.y != 0);
    return {};
}

}  // namespace

arpg::test::TestSuite host_validation_exit_suite() noexcept {
    static const arpg::test::TestCase tests[]{
        {"stage10_uses_unlocked_exit_during_combat",
            &stage10_uses_unlocked_exit_during_combat},
        {"stage10_unlocked_exit_joins_grid_before_door",
            &stage10_unlocked_exit_joins_grid_before_door},
        {"stage10_unlocked_exit_pushes_outward_at_door",
            &stage10_unlocked_exit_pushes_outward_at_door},
        {"stage10_exit_confirmation_joins_right_away_from_center_rewards",
            &stage10_exit_confirmation_joins_right_away_from_center_rewards},
        {"stage10_exit_confirmation_joins_left_away_from_center_rewards",
            &stage10_exit_confirmation_joins_left_away_from_center_rewards},
        {"stage10_exit_confirmation_routes_left_away_from_center_rewards",
            &stage10_exit_confirmation_routes_left_away_from_center_rewards},
        {"stage10_exit_confirmation_routes_right_away_from_center_rewards",
            &stage10_exit_confirmation_routes_right_away_from_center_rewards},
        {"stage10_exit_confirmation_blocked_join_recovers_farther_outward",
            &stage10_exit_confirmation_blocked_join_recovers_farther_outward},
        {"stage10_exit_confirmation_blocked_join_keeps_safe_same_cell_escape",
            &stage10_exit_confirmation_blocked_join_keeps_safe_same_cell_escape},
        {"stage10_exit_confirmation_blocked_route_recovers_farther_outward",
            &stage10_exit_confirmation_blocked_route_recovers_farther_outward},
        {"stage10_exit_confirmation_reaches_warning_without_claiming_center_rewards",
            &stage10_exit_confirmation_reaches_warning_without_claiming_center_rewards},
        {"stage10_formal_exit_failure_position_reaches_warning_without_claiming_rewards",
            &stage10_formal_exit_failure_position_reaches_warning_without_claiming_rewards},
        {"stage11_uses_unlocked_exit_during_combat",
            &stage11_uses_unlocked_exit_during_combat},
        {"stage11_deep_unlocked_combat_routes_to_hole",
            &stage11_deep_unlocked_combat_routes_to_hole},
        {"stage11_deep_preunlock_reuses_survival_driver",
            &stage11_deep_preunlock_reuses_survival_driver},
        {"stage10_captures_unlocked_door_during_combat",
            &stage10_captures_unlocked_door_during_combat},
        {"other_stage10_scenarios_reuse_normal_room_driver",
            &other_stage10_scenarios_reuse_normal_room_driver},
        {"warning_abyss_attacks_only_current_target",
            &warning_abyss_attacks_only_current_target},
        {"player_death_waits_for_abyss_environment",
            &player_death_waits_for_abyss_environment},
        {"stationary_thunderstorm_defeats_player",
            &stationary_thunderstorm_defeats_player},
        {"stage10_death_pending_requests_continue",
            &stage10_death_pending_requests_continue},
        {"stage10_death_continue_commit_reaches_exit",
            &stage10_death_continue_commit_reaches_exit},
        {"formal_death_root_is_thunderstorm_fixture",
            &formal_death_root_is_thunderstorm_fixture},
        {"clear_abyss_resets_and_reuses_full_clear_driver",
            &clear_abyss_resets_and_reuses_full_clear_driver},
        {"ranged_driver_uses_room_center_safe_stance",
            &ranged_driver_uses_room_center_safe_stance},
        {"ranged_driver_keeps_progressing_lease",
            &ranged_driver_keeps_progressing_lease},
        {"ranged_driver_stall_excludes_target_and_sweeps",
            &ranged_driver_stall_excludes_target_and_sweeps},
        {"ranged_driver_keeps_stalled_lease_until_local_detour",
            &ranged_driver_keeps_stalled_lease_until_local_detour},
        {"ranged_driver_releases_stale_geometry",
            &ranged_driver_releases_stale_geometry},
        {"ranged_driver_continues_past_legacy_sparse_sweep_limit",
            &ranged_driver_continues_past_legacy_sparse_sweep_limit},
        {"ranged_driver_hands_off_to_light_melee",
            &ranged_driver_hands_off_to_light_melee},
        {"melee_chain_prioritizes_existing_light_lane",
            &melee_chain_prioritizes_existing_light_lane},
        {"melee_stall_reuses_grid_escape",
            &melee_stall_reuses_grid_escape},
        {"melee_recovery_filters_multiple_but_reacquires_last",
            &melee_recovery_filters_multiple_but_reacquires_last},
        {"melee_recovery_exits_when_lane_appears",
            &melee_recovery_exits_when_lane_appears},
        {"ranged_driver_grid_uses_far_boundary_after_block",
            &ranged_driver_grid_uses_far_boundary_after_block},
        {"ranged_driver_grid_rotates_after_rejoin_limit",
            &ranged_driver_grid_rotates_after_rejoin_limit},
        {"warning_abyss_does_not_mutate_clear_navigation",
            &warning_abyss_does_not_mutate_clear_navigation},
        {"reward_targets_require_full_abyss_clear",
            &reward_targets_require_full_abyss_clear},
        {"restarted_failed_name_reaches_resumed_v9_started_combat",
            &restarted_failed_name_reaches_resumed_v9_started_combat},
        {"hole_driver_defeats_last_target_before_routing_to_hole",
            &hole_driver_defeats_last_target_before_routing_to_hole},
        {"abyss_skill_whitelist_only_contains_clear_scenarios",
            &abyss_skill_whitelist_only_contains_clear_scenarios},
        {"stage11c_full_clear_skips_frame_combat_injection",
            &stage11c_full_clear_skips_frame_combat_injection},
        {"stage11c_full_clear_fixed_steps_ignore_early_exit",
            &stage11c_full_clear_fixed_steps_ignore_early_exit},
        {"stage11c_full_clear_awaiting_exit_joins_grid_before_door",
            &stage11c_full_clear_awaiting_exit_joins_grid_before_door},
        {"stage11c_full_clear_reacquires_after_failed_local_detour",
            &stage11c_full_clear_reacquires_after_failed_local_detour},
        {"stage11d_priority_target_waits_for_confirmed_death_before_advancing",
            &stage11d_priority_target_waits_for_confirmed_death_before_advancing},
        {"stage11d_rearm_suspends_physical_injection",
            &stage11d_rearm_suspends_physical_injection},
        {"stage11d_rare_abyss_ignores_ordinary_rarity_capture_gate",
            &stage11d_rare_abyss_ignores_ordinary_rarity_capture_gate},
        {"stage11d_player_damage_requires_consecutive_live_hp_drop",
            &stage11d_player_damage_requires_consecutive_live_hp_drop},
        {"stage11d_rare_abyss_capture_requires_matching_visible_label",
            &stage11d_rare_abyss_capture_requires_matching_visible_label},
        {"stage11d_rare_abyss_moves_to_uncaptured_ground_reward",
            &stage11d_rare_abyss_moves_to_uncaptured_ground_reward},
        {"stage11d_close_boundary_target_retreats_to_ranged_stance",
            &stage11d_close_boundary_target_retreats_to_ranged_stance},
        {"stage11d_rare_abyss_unavailable_player_is_neutral",
            &stage11d_rare_abyss_unavailable_player_is_neutral},
        {"stage11d_rare_abyss_turns_at_ranged_stance_before_cast",
            &stage11d_rare_abyss_turns_at_ranged_stance_before_cast},
        {"stage11d_rare_abyss_blocked_facing_turn_enters_local_recovery",
            &stage11d_rare_abyss_blocked_facing_turn_enters_local_recovery},
        {"stage11d_rare_abyss_edge_stance_turn_does_not_oscillate",
            &stage11d_rare_abyss_edge_stance_turn_does_not_oscillate},
        {"stage11d_streaming_gap_uses_physical_grid_sweep",
            &stage11d_streaming_gap_uses_physical_grid_sweep},
        {"stage11d_rare_abyss_ranged_stance_preserves_fire_routing",
            &stage11d_rare_abyss_ranged_stance_preserves_fire_routing},
        {"stage11d_rare_abyss_blocked_stance_enters_grid_recovery",
            &stage11d_rare_abyss_blocked_stance_enters_grid_recovery},
        {"stage11d_rare_abyss_moved_target_releases_stale_geometry",
            &stage11d_rare_abyss_moved_target_releases_stale_geometry},
        {"stage11d_rare_abyss_area_skill_uses_equipped_physical_slot",
            &stage11d_rare_abyss_area_skill_uses_equipped_physical_slot},
        {"stage11d_rare_abyss_high_hp_attacks_through_nearby_danger",
            &stage11d_rare_abyss_high_hp_attacks_through_nearby_danger},
        {"stage11d_rare_abyss_low_hp_escapes_approaching_danger",
            &stage11d_rare_abyss_low_hp_escapes_approaching_danger},
        {"stage11d_rare_abyss_low_hp_attacks_reachable_danger",
            &stage11d_rare_abyss_low_hp_attacks_reachable_danger},
        {"stage11d_rare_abyss_low_hp_preserves_grid_recovery",
            &stage11d_rare_abyss_low_hp_preserves_grid_recovery},
        {"stage11d_rare_abyss_low_hp_global_sweep_ignores_opportunistic_attack",
            &stage11d_rare_abyss_low_hp_global_sweep_ignores_opportunistic_attack},
        {"stage11d_rare_abyss_global_sweep_routes_fire_to_waypoint",
            &stage11d_rare_abyss_global_sweep_routes_fire_to_waypoint},
        {"stage11d_rare_abyss_global_sweep_casts_ready_area_skill",
            &stage11d_rare_abyss_global_sweep_casts_ready_area_skill},
        {"stage11d_rare_abyss_global_sweep_attacks_light_lane_without_skill",
            &stage11d_rare_abyss_global_sweep_attacks_light_lane_without_skill},
        {"stage11d_rare_abyss_close_stall_hands_off_outer_sweep_cursor",
            &stage11d_rare_abyss_close_stall_hands_off_outer_sweep_cursor},
        {"stage11d_rare_abyss_first_close_stall_starts_at_nearest_high_sweep_row",
            &stage11d_rare_abyss_first_close_stall_starts_at_nearest_high_sweep_row},
        {"stage11d_rare_abyss_streaming_gap_routes_fire_grid_sweep",
            &stage11d_rare_abyss_streaming_gap_routes_fire_grid_sweep},
        {"stage11d_rare_abyss_reacquires_resident_after_lane_recovery_waypoint",
            &stage11d_rare_abyss_reacquires_resident_after_lane_recovery_waypoint},
        {"stage11d_rare_abyss_unavailable_skills_closes_for_light",
            &stage11d_rare_abyss_unavailable_skills_closes_for_light},
        {"stage11d_rare_abyss_waiting_skills_closes_for_light",
            &stage11d_rare_abyss_waiting_skills_closes_for_light},
        {"stage11d_rare_abyss_waits_for_live_player_damage",
            &stage11d_rare_abyss_waits_for_live_player_damage},
        {"stage11d_rare_abyss_melee_lane_prefers_ready_area_skill",
            &stage11d_rare_abyss_melee_lane_prefers_ready_area_skill},
        {"stage11d_rare_abyss_close_movement_casts_ready_area_skill",
            &stage11d_rare_abyss_close_movement_casts_ready_area_skill},
        {"stage11d_rare_abyss_no_skill_geometry_in_light_lane_attacks_without_release",
            &stage11d_rare_abyss_no_skill_geometry_in_light_lane_attacks_without_release},
        {"stage11d_rare_abyss_blocked_light_close_enters_grid_recovery",
            &stage11d_rare_abyss_blocked_light_close_enters_grid_recovery},
        {"stage11d_rare_abyss_close_stall_attacks_light_lane_blocker",
            &stage11d_rare_abyss_close_stall_attacks_light_lane_blocker},
        {"stage11d_rare_abyss_close_for_light_nonprogress_enters_grid_recovery",
            &stage11d_rare_abyss_close_for_light_nonprogress_enters_grid_recovery},
        {"stage11d_rare_abyss_lateral_stall_enters_grid_recovery",
            &stage11d_rare_abyss_lateral_stall_enters_grid_recovery},
    };
    return arpg::test::make_suite("host_validation_exit", tests);
}
