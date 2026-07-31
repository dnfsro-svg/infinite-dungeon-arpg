#include "abyss/abyss_rules.hpp"
#include "combat/active_skill_runtime.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_spatial_grid.hpp"
#include "core/gameplay_limits.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_generation.hpp"
#include "dungeon/room_monster_plan_builder.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "persistence/save_store.hpp"
#include "skills/active_skill_types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>

namespace {

using arpg::combat::Action;
using arpg::combat::CombatSnapshot;
using arpg::combat::MovementInput;
using arpg::combat::MonsterSnapshot;
using arpg::combat::Vec3;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::PendingSaveKind;
using arpg::dungeon::RequestResult;
using arpg::dungeon::RoomPhase;

struct AbyssEntry final {
    DungeonRunState source{};
    DungeonRunState target{};
    ExitDirection direction{ExitDirection::none};
};

struct EnvironmentEvidence final {
    int initial_hp{};
    int minimum_hp{};
    bool warning_seen{};
    bool active_seen{};
    bool active_damage_seen{};
};

struct AbandonEvidence final {
    bool warning_event{};
    bool armed{};
    bool neutral_release{};
    arpg::dungeon::checkpoint::AbyssCheckpoint before{};
    std::uint32_t inventory_before{};
    std::uint16_t ground_before{};
    std::uint16_t abyss_ground_before{};
};

enum class GridRoutePhase : std::uint8_t {
    need_join,
    join_near_x,
    join_far_x,
    route,
};

enum class GridRouteProgress : std::uint8_t {
    none,
    moved,
    recovered,
    unreachable,
};

struct GridRouteState final {
    GridRoutePhase phase{GridRoutePhase::need_join};
    std::uint8_t boundary_column{};
    bool pending_movement_progress_check{};
    Vec3 previous_position{};
    std::uint32_t join_invariant_failures{};
    std::uint32_t route_rejoins{};
};

struct DenseDriverState final {
    std::size_t waypoint{};
    GridRouteState sweep_grid{};
    std::optional<arpg::combat::MonsterOrdinal> locked_monster_ordinal{};
    Vec3 locked_stance{};
    arpg::combat::Facing locked_facing{arpg::combat::Facing::right};
    bool stance_reached{};
    bool close_for_light{};
    bool melee_chain{};
    bool recover_until_light_lane{};
    bool sweep_escape{};
    bool close_facing_turn_attempted{};
    bool pending_stance_progress_check{};
    bool pending_close_progress_check{};
    float previous_stance_distance_squared{};
    Vec3 previous_close_position{};
    std::optional<arpg::combat::MonsterOrdinal> stalled_ordinal{};
    std::uint32_t lock_acquired{};
    std::uint32_t release_absent{};
    std::uint32_t release_stale_geometry{};
    std::uint32_t release_movement_stall{};
    std::uint32_t sweep_ticks{};
    std::uint32_t storm_accepted{};
    std::uint32_t draw_accepted{};
    std::uint32_t light_queued{};
    std::uint32_t melee_chain_entries{};
    std::uint32_t melee_handoffs_after_absent{};
    std::uint32_t melee_in_lane_acquired{};
    std::uint32_t melee_nearest_acquired{};
    std::uint32_t ranged_acquired_after_melee_chain{};
    float melee_initial_distance_squared{};
    std::uint32_t light_lane_recovery_entries{};
    std::uint32_t light_lane_recovery_ticks{};
    std::uint32_t light_lane_reacquired{};
    std::uint32_t close_facing_corrections{};
};

struct DenseResidentTrace final {
    bool seen{};
    std::uint32_t last_tick{};
    Vec3 last_position{};
};

struct DenseCellTrace final {
    std::uint32_t visit_count{};
    std::uint32_t first_tick{(std::numeric_limits<std::uint32_t>::max)()};
    std::uint32_t last_tick{};
};

struct DenseCoverageTrace final {
    std::array<DenseResidentTrace,
        arpg::limits::kRoomMonsterCapacity> residents{};
    std::array<bool, arpg::limits::kRoomMonsterCapacity> defeated{};
    std::array<DenseCellTrace,
        arpg::combat::kRoomMonsterCellCount> cells{};
    std::array<std::uint32_t, 10U> blocked_sweep_waypoints{};
    bool pending_sweep_input{};
    std::size_t pending_sweep_waypoint{};
    Vec3 pending_sweep_position{};
};

struct AttackGeometry final {
    bool storm{};
    bool draw{};
    bool light{};

    [[nodiscard]] bool any() const noexcept {
        return storm || draw || light;
    }
};

struct DenseActionDecision final {
    bool ready{};
    bool skill_accepted{};
    bool action_requested{};
};

enum class DenseLeaseRelease : std::uint8_t {
    absent,
    stale_geometry,
    movement_stall,
};

DungeonRules validation_rules() noexcept {
    return {};
}

std::optional<std::uint32_t> clear_tick_budget(
    std::uint32_t initial_monster_count) noexcept {
    constexpr std::uint64_t kBaseBudget = 12000U;
    constexpr std::uint64_t kTicksPerMonster = 96U;
    if (initial_monster_count == 0U
            || static_cast<std::uint64_t>(initial_monster_count)
                > static_cast<std::uint64_t>(
                    arpg::limits::kRoomMonsterCapacity)) {
        return std::nullopt;
    }
    const std::uint64_t budget = kBaseBudget
        + static_cast<std::uint64_t>(initial_monster_count)
            * kTicksPerMonster;
    if (budget > (std::numeric_limits<std::uint32_t>::max)()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(budget);
}

bool install_validation_build(DungeonRunState& state) {
    constexpr std::array<std::uint8_t, 6> base_ids{{
        8U, 10U, 12U, 14U, 16U, 18U,
    }};
    constexpr std::array<std::array<arpg::items::AffixRoll, 6>, 6> affixes{{
        {{{1U, 1U, 0xFFU}, {2U, 1U, 0xFFU}, {3U, 1U, 0xFFU},
          {101U, 1U, 0xFFU}, {105U, 1U, 0xFFU}, {106U, 1U, 0xFFU}}},
        {{{7U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}, {111U, 1U, 0xFFU}}},
        {{{7U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}, {111U, 1U, 0xFFU}}},
        {{{3U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {101U, 1U, 0xFFU}, {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}}},
        {{{7U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {102U, 1U, 0xFFU}, {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}}},
        {{{3U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {103U, 1U, 0xFFU}, {110U, 1U, 0xFFU}, {111U, 1U, 0xFFU}}},
    }};
    state.progression = {100U, 0U, 99U, 99U};
    state.item_ownership = {};
    state.item_ownership.items.reserve(6U);
    for (std::uint8_t index = 0U; index < 6U; ++index) {
        const std::uint64_t id = static_cast<std::uint64_t>(index) + 1U;
        auto item = arpg::items::generate_item({
            0xA8100000ULL + index,
            static_cast<arpg::items::ItemSlot>(index),
            100U,
            id,
            arpg::items::ItemRarity::rare,
        });
        if (!item.has_value()) return false;
        item->base_id = base_ids[index];
        item->affixes = {};
        std::copy(affixes[index].begin(), affixes[index].end(),
            item->affixes.begin());
        item->affix_count = 6U;
        item->required_level = 95U;
        item->reinforcement = 15U;
        if (!arpg::items::validate_item(*item)) return false;
        state.item_ownership.items.push_back(*item);
        state.item_ownership.equipment.equipped_ids[index] = id;
    }
    state.item_ownership.next_item_sequence = 7U;
    return true;
}

std::optional<AbyssEntry> find_entry(
    arpg::abyss::AbyssRuleId required_rule) noexcept {
    constexpr std::array<ExitDirection, 4> directions{{
        ExitDirection::up, ExitDirection::down,
        ExitDirection::left, ExitDirection::right,
    }};
    const DungeonRules rules = validation_rules();
    for (std::uint64_t root = 1U; root < 200000U; ++root) {
        const auto initial = arpg::dungeon::make_initial_run_state(root, rules);
        if (initial.fault != arpg::dungeon::DungeonFault::none) continue;
        const auto preview = arpg::dungeon::preview_abyss_doors(
            initial.state.current_room);
        for (std::size_t index = 0U; index < directions.size(); ++index) {
            if (!preview[index]) continue;
            const auto target = arpg::dungeon::make_door_transition(
                initial.state, directions[index], rules);
            if (target.fault == arpg::dungeon::DungeonFault::none
                    && target.state.current_room.is_abyss
                    && target.state.abyss.rule == required_rule) {
                AbyssEntry result{initial.state, target.state, directions[index]};
                if (!install_validation_build(result.target)) return std::nullopt;
                return result;
            }
        }
    }
    return std::nullopt;
}

bool service_pending(DungeonSession& session,
    arpg::persistence::SaveStore& store) noexcept {
    const auto* const pending = session.pending_save_view();
    if (pending == nullptr) return true;
    auto committed = store.commit(pending->next_state);
    arpg::dungeon::SaveDisposition disposition =
        arpg::dungeon::SaveDisposition::indeterminate;
    if (committed.state == arpg::persistence::SaveCommitState::committed) {
        disposition = arpg::dungeon::SaveDisposition::committed;
    } else if (committed.state
            == arpg::persistence::SaveCommitState::not_committed) {
        disposition = arpg::dungeon::SaveDisposition::not_committed;
    }
    session.resolve_pending_save({disposition,
        committed.verified_state.commit_generation,
        std::move(committed.verified_state)});
    return disposition == arpg::dungeon::SaveDisposition::committed
        && session.snapshot().phase != RoomPhase::faulted;
}

float squared_xy_distance(Vec3 lhs, Vec3 rhs) noexcept {
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return dx * dx + dy * dy;
}

const MonsterSnapshot* living_monster_by_ordinal(
    const CombatSnapshot& state,
    arpg::combat::MonsterOrdinal ordinal) noexcept {
    for (const MonsterSnapshot& monster : state.monsters) {
        if (monster.active && monster.hp > 0
                && monster.monster_ordinal == ordinal) {
            return &monster;
        }
    }
    return nullptr;
}

MovementInput movement_toward(Vec3 from, Vec3 to) noexcept {
    MovementInput movement{};
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    if (dx > 0.45F) movement.x = 1;
    else if (dx < -0.45F) movement.x = -1;
    if (dy > 0.25F) movement.y = 1;
    else if (dy < -0.25F) movement.y = -1;
    return movement;
}

bool stance_position_reached(
    Vec3 player,
    Vec3 stance,
    arpg::combat::Facing facing) noexcept {
    const float facing_sign = facing == arpg::combat::Facing::right
        ? 1.0F : -1.0F;
    return facing_sign * (player.x - stance.x) >= 0.0F
        && std::fabs(player.x - stance.x) <= 0.25F
        && std::fabs(player.y - stance.y) <= 0.25F;
}

MovementInput movement_toward_stance(
    Vec3 from,
    Vec3 to,
    arpg::combat::Facing facing) noexcept {
    MovementInput movement{};
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float facing_sign = facing == arpg::combat::Facing::right
        ? 1.0F : -1.0F;
    if (facing_sign * (from.x - to.x) < 0.0F) {
        movement.x = facing == arpg::combat::Facing::right ? 1 : -1;
    } else if (dx > 0.25F) movement.x = 1;
    else if (dx < -0.25F) movement.x = -1;
    if (dy > 0.25F) movement.y = 1;
    else if (dy < -0.25F) movement.y = -1;
    return movement;
}

constexpr float kGridRouteArrivalTolerance = 0.20F;
constexpr std::uint32_t kGridRouteMaximumRejoins = 4U;

float grid_boundary_x(std::uint8_t column) noexcept {
    if (column == 0U) return arpg::combat::room_bounds::min_x;
    if (column >= arpg::combat::room_spatial::columns) {
        return arpg::combat::room_bounds::max_x;
    }
    return arpg::combat::room_bounds::min_x
        + static_cast<float>(column)
            * arpg::combat::room_spatial::cell_width;
}

std::uint8_t nearest_grid_boundary_column(float player_x) noexcept {
    const float grid_x = (std::clamp)(
        (player_x - arpg::combat::room_bounds::min_x)
            / arpg::combat::room_spatial::cell_width,
        0.0F,
        static_cast<float>(arpg::combat::room_spatial::columns));
    return static_cast<std::uint8_t>(std::floor(grid_x + 0.5F));
}

std::uint8_t alternate_grid_boundary_column(
    float player_x,
    std::uint8_t blocked_column) noexcept {
    const float grid_x = (std::clamp)(
        (player_x - arpg::combat::room_bounds::min_x)
            / arpg::combat::room_spatial::cell_width,
        0.0F,
        static_cast<float>(arpg::combat::room_spatial::columns));
    const std::size_t cell = (std::min)(
        static_cast<std::size_t>(grid_x),
        arpg::combat::room_spatial::columns - 1U);
    const auto left = static_cast<std::uint8_t>(cell);
    const auto right = static_cast<std::uint8_t>(cell + 1U);
    return blocked_column == left ? right : left;
}

MovementInput grid_route_movement(
    Vec3 player,
    Vec3 target,
    GridRoutePhase& phase,
    std::uint8_t& boundary_column) noexcept {
    if (phase == GridRoutePhase::need_join) {
        boundary_column = nearest_grid_boundary_column(player.x);
        phase = GridRoutePhase::join_near_x;
    }
    if (phase == GridRoutePhase::join_near_x
            || phase == GridRoutePhase::join_far_x) {
        MovementInput movement{};
        const float dx = grid_boundary_x(boundary_column) - player.x;
        if (dx > kGridRouteArrivalTolerance) movement.x = 1;
        else if (dx < -kGridRouteArrivalTolerance) movement.x = -1;
        if (movement.x != 0) return movement;
        phase = GridRoutePhase::route;
    }

    MovementInput movement{};
    const float dy = target.y - player.y;
    if (dy > kGridRouteArrivalTolerance) movement.y = 1;
    else if (dy < -kGridRouteArrivalTolerance) movement.y = -1;
    else {
        const float dx = target.x - player.x;
        if (dx > kGridRouteArrivalTolerance) movement.x = 1;
        else if (dx < -kGridRouteArrivalTolerance) movement.x = -1;
    }
    return movement;
}

void observe_grid_route_movement(
    GridRouteState& state,
    Vec3 player,
    MovementInput movement,
    bool controllable) noexcept {
    if (!controllable || (movement.x == 0 && movement.y == 0)) return;
    state.pending_movement_progress_check = true;
    state.previous_position = player;
}

GridRouteProgress settle_grid_route_movement(
    GridRouteState& state,
    Vec3 player) noexcept {
    if (!state.pending_movement_progress_check) {
        return GridRouteProgress::none;
    }
    state.pending_movement_progress_check = false;
    if (squared_xy_distance(player, state.previous_position) > 0.0F) {
        return GridRouteProgress::moved;
    }
    switch (state.phase) {
    case GridRoutePhase::need_join:
        ++state.join_invariant_failures;
        return GridRouteProgress::unreachable;
    case GridRoutePhase::join_near_x:
        state.boundary_column = alternate_grid_boundary_column(
            player.x, state.boundary_column);
        state.phase = GridRoutePhase::join_far_x;
        return GridRouteProgress::recovered;
    case GridRoutePhase::join_far_x:
        ++state.join_invariant_failures;
        return GridRouteProgress::unreachable;
    case GridRoutePhase::route:
        ++state.route_rejoins;
        if (state.route_rejoins > kGridRouteMaximumRejoins) {
            return GridRouteProgress::unreachable;
        }
        state.phase = GridRoutePhase::need_join;
        return GridRouteProgress::recovered;
    }
    return GridRouteProgress::unreachable;
}

MovementInput sweep_movement(Vec3 player, DenseDriverState& state) noexcept {
    constexpr std::array<std::uint8_t, 5> rows{{2U, 6U, 10U, 14U, 18U}};
    constexpr std::size_t waypoint_count = rows.size() * 2U;
    const auto waypoint = [&](std::size_t index) noexcept {
        const std::size_t row_index = index / 2U;
        const bool left = (index % 4U) == 0U || (index % 4U) == 3U;
        const float x = left ? arpg::combat::room_bounds::min_x
                             : arpg::combat::room_bounds::max_x;
        const float y = arpg::combat::room_bounds::min_y
            + static_cast<float>(rows[row_index])
                * arpg::combat::room_spatial::cell_depth;
        return Vec3{x, y, 0.0F};
    };

    Vec3 target = waypoint(state.waypoint);
    MovementInput movement = grid_route_movement(
        player, target, state.sweep_grid.phase,
        state.sweep_grid.boundary_column);
    if (movement.x != 0 || movement.y != 0) return movement;
    state.sweep_grid.route_rejoins = 0U;
    state.waypoint = (state.waypoint + 1U) % waypoint_count;
    target = waypoint(state.waypoint);
    return grid_route_movement(player, target, state.sweep_grid.phase,
        state.sweep_grid.boundary_column);
}

bool in_attack_lane(const CombatSnapshot& state,
    const MonsterSnapshot& target) noexcept {
    const float dx = target.position.x - state.player.position.x;
    const float dy = target.position.y - state.player.position.y;
    const bool facing = std::fabs(dx) <= 0.2F
        || (dx > 0.0F && state.player.facing == arpg::combat::Facing::right)
        || (dx < 0.0F && state.player.facing == arpg::combat::Facing::left);
    return facing && std::fabs(dx) <= 1.70F && std::fabs(dy) <= 0.55F;
}

const MonsterSnapshot* select_navigation_target(
    const CombatSnapshot& state,
    std::optional<arpg::combat::MonsterOrdinal> excluded_ordinal,
    bool melee_mode,
    bool require_light_lane) noexcept {
    const MonsterSnapshot* best = nullptr;
    bool best_in_lane = false;
    float best_distance = 0.0F;
    for (const MonsterSnapshot& monster : state.monsters) {
        if (!monster.active || monster.hp <= 0
                || (excluded_ordinal.has_value()
                    && monster.monster_ordinal == *excluded_ordinal)) {
            continue;
        }
        const bool in_lane = melee_mode && in_attack_lane(state, monster);
        if (require_light_lane && !in_lane) continue;
        const float distance = squared_xy_distance(
            monster.position, state.player.position);
        const bool better_tier = melee_mode && in_lane && !best_in_lane;
        const bool same_tier = !melee_mode || in_lane == best_in_lane;
        if (best == nullptr || better_tier
                || (same_tier && (distance < best_distance
                    || (distance == best_distance
                        && monster.monster_ordinal
                            < best->monster_ordinal)))) {
            best = &monster;
            best_in_lane = in_lane;
            best_distance = distance;
        }
    }
    return best;
}

AttackGeometry attack_geometry(const CombatSnapshot& state) noexcept {
    AttackGeometry geometry{};
    const auto& player = state.player;
    const float facing = player.facing == arpg::combat::Facing::right
        ? 1.0F : -1.0F;
    const float storm_center_x = player.position.x
        + facing * arpg::combat::kStormCenterForward;
    for (const MonsterSnapshot& monster : state.monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float storm_dx = monster.position.x - storm_center_x;
        const float storm_dy = monster.position.y - player.position.y;
        geometry.storm = geometry.storm
            || storm_dx * storm_dx + storm_dy * storm_dy
                <= arpg::combat::kStormStrikeRadius
                    * arpg::combat::kStormStrikeRadius;

        const float forward = (monster.position.x - player.position.x)
            * facing;
        const float half_width = forward >= 0.0F
                && forward <= arpg::combat::kDrawSlashRange
            ? arpg::combat::kDrawSlashHalfWidthAtEnd
                * (forward / arpg::combat::kDrawSlashRange)
            : -1.0F;
        geometry.draw = geometry.draw
            || (half_width >= 0.0F
                && std::fabs(monster.position.y - player.position.y)
                    <= half_width);
        geometry.light = geometry.light || in_attack_lane(state, monster);
    }
    return geometry;
}

bool controllable_movement_snapshot(const CombatSnapshot& state) noexcept {
    return state.player.hp > 0
        && state.player.hurt_ticks == 0U
        && state.player.hit_stop_ticks == 0U
        && state.player.active_attack == arpg::combat::AttackId::none
        && state.active_skill.id == arpg::skills::ActiveSkillId::none;
}

void acquire_navigation_target(
    const CombatSnapshot& state,
    const MonsterSnapshot& target,
    bool melee_mode,
    DenseDriverState& driver) noexcept {
    driver.locked_monster_ordinal = target.monster_ordinal;
    if (melee_mode) {
        driver.locked_stance = {};
        driver.locked_facing = state.player.facing;
        driver.close_for_light = true;
        if (in_attack_lane(state, target)) {
            ++driver.melee_in_lane_acquired;
        } else {
            ++driver.melee_nearest_acquired;
        }
        driver.melee_initial_distance_squared += squared_xy_distance(
            state.player.position, target.position);
    } else {
        const float room_center_x =
            (arpg::combat::room_bounds::min_x
                + arpg::combat::room_bounds::max_x) * 0.5F;
        driver.locked_facing = target.position.x >= room_center_x
            ? arpg::combat::Facing::right
            : arpg::combat::Facing::left;
        const float facing =
            driver.locked_facing == arpg::combat::Facing::right
            ? 1.0F : -1.0F;
        driver.locked_stance = {
            target.position.x - facing * arpg::combat::kDrawSlashRange,
            (std::clamp)(target.position.y,
                arpg::combat::room_bounds::min_y
                    + arpg::combat::kDrawSlashHalfWidthAtEnd,
                arpg::combat::room_bounds::max_y
                    - arpg::combat::kDrawSlashHalfWidthAtEnd),
            0.0F,
        };
        driver.close_for_light = false;
        if (driver.melee_chain) {
            ++driver.ranged_acquired_after_melee_chain;
        }
    }
    driver.stance_reached = false;
    driver.sweep_escape = false;
    driver.sweep_grid = {};
    driver.close_facing_turn_attempted = false;
    driver.pending_stance_progress_check = false;
    driver.pending_close_progress_check = false;
    driver.previous_stance_distance_squared = 0.0F;
    driver.previous_close_position = {};
    ++driver.lock_acquired;
}

void release_navigation_target(
    DenseDriverState& driver,
    DenseLeaseRelease reason) noexcept {
    driver.locked_monster_ordinal.reset();
    driver.locked_stance = {};
    driver.locked_facing = arpg::combat::Facing::right;
    driver.stance_reached = false;
    driver.close_for_light = false;
    driver.sweep_grid = {};
    driver.close_facing_turn_attempted = false;
    driver.pending_stance_progress_check = false;
    driver.pending_close_progress_check = false;
    driver.previous_stance_distance_squared = 0.0F;
    driver.previous_close_position = {};
    switch (reason) {
    case DenseLeaseRelease::absent:
        ++driver.release_absent;
        if (driver.melee_chain) {
            ++driver.melee_handoffs_after_absent;
        }
        break;
    case DenseLeaseRelease::stale_geometry:
        ++driver.release_stale_geometry;
        break;
    case DenseLeaseRelease::movement_stall:
        ++driver.release_movement_stall;
        break;
    }
}

const MonsterSnapshot* try_acquire_navigation_target(
    const CombatSnapshot& state,
    DenseDriverState& driver) noexcept {
    const auto excluded = driver.stalled_ordinal;
    const bool melee_mode = driver.melee_chain;
    const bool light_lane_recovery = driver.recover_until_light_lane;
    const MonsterSnapshot* target = select_navigation_target(
        state, excluded, melee_mode, light_lane_recovery);
    driver.stalled_ordinal.reset();
    if (target != nullptr) {
        acquire_navigation_target(state, *target, melee_mode, driver);
        if (light_lane_recovery) {
            driver.recover_until_light_lane = false;
            ++driver.light_lane_reacquired;
        }
    }
    return target;
}

DenseActionDecision request_dense_action(
    DungeonSession& session,
    const CombatSnapshot& state,
    const AttackGeometry& geometry,
    DenseDriverState& driver) noexcept {
    DenseActionDecision decision{};
    const auto& player = state.player;
    decision.ready = player.hp > 0
        && player.hurt_ticks == 0U && player.hit_stop_ticks == 0U
        && player.active_attack == arpg::combat::AttackId::none
        && state.active_skill.id == arpg::skills::ActiveSkillId::none
        && state.diagnostics.input_size == 0U;
    if (!decision.ready) return decision;

    if (geometry.storm) {
        decision.skill_accepted = session.request_active_skill_slot(1U)
            == arpg::combat::SkillCastResult::accepted;
        if (decision.skill_accepted) ++driver.storm_accepted;
    }
    if (!decision.skill_accepted && geometry.draw) {
        decision.skill_accepted = session.request_active_skill_slot(0U)
            == arpg::combat::SkillCastResult::accepted;
        if (decision.skill_accepted) ++driver.draw_accepted;
    }
    if (!decision.skill_accepted && geometry.light
            && session.queue_action(Action::light)) {
        ++driver.light_queued;
        decision.action_requested = true;
    }
    decision.action_requested = decision.action_requested
        || decision.skill_accepted;
    return decision;
}

bool build_dense_diagnostic_plan(
    const arpg::dungeon::DungeonSnapshot& state,
    const DungeonRules& rules,
    arpg::combat::RoomMonsterPlan& plan) noexcept {
    arpg::dungeon::checkpoint::RoomDescriptor room{};
    room.index = state.room_index;
    room.seed = state.room_seed;
    room.depth = state.depth;
    room.floor_room_index = state.floor_room_index;
    room.entry = state.entry_side;
    room.ecology = state.ecology;
    room.has_hole = state.has_hole;
    room.is_abyss = state.is_abyss;
    const auto result = arpg::dungeon::build_room_monster_plan(
        room, rules, state.monster_generator_version, plan);
    const bool valid = result.fault == arpg::dungeon::DungeonFault::none
        && arpg::dungeon::room_monster_plan_legal(room, plan)
        && plan.monster_count == state.initial_monster_count
        && plan.generator_version == state.monster_generator_version
        && plan.blueprint_hash == state.monster_blueprint_hash;
    if (!valid) {
        std::cerr << "dense diagnostic plan mismatch fault="
                  << static_cast<unsigned>(result.fault)
                  << " expected_count=" << state.initial_monster_count
                  << " actual_count=" << plan.monster_count
                  << " expected_version=" << state.monster_generator_version
                  << " actual_version=" << plan.generator_version
                  << " expected_hash=" << state.monster_blueprint_hash
                  << " actual_hash=" << plan.blueprint_hash << '\n';
    }
    return valid;
}

void record_dense_snapshot(const CombatSnapshot& state,
    std::uint32_t tick, DenseCoverageTrace& trace) noexcept {
    for (const MonsterSnapshot& monster : state.monsters) {
        const std::size_t ordinal = monster.spawn_ordinal;
        if (!monster.active || monster.monster_ordinal != monster.spawn_ordinal
                || ordinal >= trace.residents.size()) {
            continue;
        }
        trace.residents[ordinal].seen = true;
        trace.residents[ordinal].last_tick = tick;
        trace.residents[ordinal].last_position = monster.position;
    }
    const auto region = arpg::combat::make_room_streaming_region(
        state.player.position);
    for (std::size_t row = region.first_row;
            row < static_cast<std::size_t>(region.first_row)
                + region.row_count; ++row) {
        for (std::size_t column = region.first_column;
                column < static_cast<std::size_t>(region.first_column)
                    + region.column_count; ++column) {
            auto& cell = trace.cells[
                row * arpg::combat::room_spatial::columns + column];
            if (cell.visit_count == 0U) cell.first_tick = tick;
            ++cell.visit_count;
            cell.last_tick = tick;
        }
    }
}

void record_dense_defeat(const arpg::combat::CombatEvent& event,
    DenseCoverageTrace& trace) noexcept {
    const std::size_t ordinal = event.spawn_ordinal;
    if (event.kind == arpg::combat::CombatEventKind::defeated
            && ordinal < trace.defeated.size()) {
        trace.defeated[ordinal] = true;
    }
}

void print_dense_coverage_failure(
    const arpg::combat::RoomMonsterPlan& plan,
    const DenseCoverageTrace& trace) {
    std::size_t covered_cells = 0U;
    std::uint64_t blocked_total = 0U;
    for (const auto& cell : trace.cells) covered_cells += cell.visit_count != 0U;
    for (const auto count : trace.blocked_sweep_waypoints) {
        blocked_total += count;
    }
    std::cerr << "dense coverage covered_cells=" << covered_cells
              << " blocked_sweep_total=" << blocked_total
              << " blocked_sweep_waypoints=";
    for (std::size_t index = 0U;
            index < trace.blocked_sweep_waypoints.size(); ++index) {
        if (index != 0U) std::cerr << ',';
        std::cerr << index << ':' << trace.blocked_sweep_waypoints[index];
    }
    std::cerr << '\n';
    for (std::size_t ordinal = 0U; ordinal < plan.monster_count; ++ordinal) {
        if (trace.defeated[ordinal]) continue;
        const auto& monster = plan.monsters[ordinal];
        const std::size_t home_cell = monster.home_cell;
        const auto& resident = trace.residents[ordinal];
        const auto& cell = trace.cells[home_cell];
        std::cerr << "dense missing ordinal=" << ordinal
                  << " id=" << static_cast<unsigned>(monster.id)
                  << " home_cell=" << home_cell
                  << " row="
                  << home_cell / arpg::combat::room_spatial::columns
                  << " column="
                  << home_cell % arpg::combat::room_spatial::columns
                  << " initial_x=" << monster.initial_position.x
                  << " initial_y=" << monster.initial_position.y
                  << " seen=" << resident.seen
                  << " last_seen_tick=" << resident.last_tick
                  << " last_x=" << resident.last_position.x
                  << " last_y=" << resident.last_position.y
                  << " visit_count=" << cell.visit_count
                  << " first_tick=";
        if (cell.visit_count == 0U) std::cerr << "none";
        else std::cerr << cell.first_tick;
        std::cerr << " last_tick=";
        if (cell.visit_count == 0U) std::cerr << "none";
        else std::cerr << cell.last_tick;
        std::cerr << '\n';
    }
}

void print_dense_driver_failure(
    const char* reason,
    std::uint32_t driver_tick,
    const arpg::dungeon::DungeonSnapshot& state,
    const DenseDriverState& driver,
    std::uint32_t driver_budget,
    std::uint32_t initial_population) {
    AttackGeometry geometry{};
    const MonsterSnapshot* locked_monster = nullptr;
    if (state.combat.has_value()) {
        geometry = attack_geometry(*state.combat);
        if (driver.locked_monster_ordinal.has_value()) {
            locked_monster = living_monster_by_ordinal(
                *state.combat, *driver.locked_monster_ordinal);
        }
    }
    std::cerr << "dense driver failed reason=" << reason
              << " driver_tick=" << driver_tick
              << " driver_budget=" << driver_budget
              << " initial_population=" << initial_population
              << " session_tick=" << state.session_tick
              << " phase=" << static_cast<unsigned>(state.phase)
              << " initial=" << state.initial_monster_count
              << " defeated=" << state.defeated_monster_count
              << " remaining=" << state.remaining_targets
              << " hp=" << (state.combat.has_value()
                    ? state.combat->player.hp : -1)
              << " max_hp=" << (state.combat.has_value()
                    ? state.combat->player.max_hp : -1)
              << " barrier=" << (state.combat.has_value()
                    ? state.combat->player.barrier : -1)
              << " x=" << (state.combat.has_value()
                    ? state.combat->player.position.x : 0.0F)
              << " y=" << (state.combat.has_value()
                    ? state.combat->player.position.y : 0.0F)
              << " potion_restored="
              << state.health_potion_pickup_receipt.restored_hp
              << " lock=";
    if (driver.locked_monster_ordinal.has_value()) {
        std::cerr << *driver.locked_monster_ordinal;
    } else {
        std::cerr << "none";
    }
    std::cerr << " stance_x=" << driver.locked_stance.x
              << " stance_y=" << driver.locked_stance.y
              << " stance_facing="
              << static_cast<int>(driver.locked_facing)
              << " stance_reached=" << driver.stance_reached
              << " close_for_light=" << driver.close_for_light
              << " melee_chain=" << driver.melee_chain
              << " recover_until_light_lane="
              << driver.recover_until_light_lane
              << " close_facing_turn_attempted="
              << driver.close_facing_turn_attempted
              << " close_facing_corrections="
              << driver.close_facing_corrections
              << " stance_distance_squared="
              << (state.combat.has_value()
                    && driver.locked_monster_ordinal.has_value()
                  ? squared_xy_distance(state.combat->player.position,
                        driver.locked_stance)
                  : 0.0F)
              << " lock_acquired=" << driver.lock_acquired
              << " release_absent=" << driver.release_absent
              << " release_stale_geometry="
              << driver.release_stale_geometry
              << " release_movement_stall="
              << driver.release_movement_stall
              << " sweep_escape=" << driver.sweep_escape
              << " sweep_ticks=" << driver.sweep_ticks
              << " sweep_waypoint=" << driver.waypoint
              << " sweep_grid_phase="
              << static_cast<unsigned>(driver.sweep_grid.phase)
              << " sweep_boundary_column="
              << static_cast<unsigned>(driver.sweep_grid.boundary_column)
              << " sweep_join_invariant_failures="
              << driver.sweep_grid.join_invariant_failures
              << " sweep_route_rejoins="
              << driver.sweep_grid.route_rejoins
              << " geometry_storm=" << geometry.storm
              << " geometry_draw=" << geometry.draw
              << " geometry_light=" << geometry.light
              << " locked_x="
              << (locked_monster != nullptr
                    ? locked_monster->position.x : 0.0F)
              << " locked_y="
              << (locked_monster != nullptr
                    ? locked_monster->position.y : 0.0F)
              << " locked_hp="
              << (locked_monster != nullptr ? locked_monster->hp : -1)
              << " player_facing="
              << (state.combat.has_value()
                    ? static_cast<int>(state.combat->player.facing) : 0)
              << " cooldown_draw="
              << (state.combat.has_value()
                    ? state.combat->skill_cooldowns[0] : 0U)
              << " cooldown_storm="
              << (state.combat.has_value()
                    ? state.combat->skill_cooldowns[1] : 0U)
              << " active_skill="
              << (state.combat.has_value()
                    ? static_cast<unsigned>(state.combat->active_skill.id) : 0U)
              << " active_attack="
              << (state.combat.has_value()
                    ? static_cast<unsigned>(
                        state.combat->player.active_attack) : 0U)
              << " hurt="
              << (state.combat.has_value()
                    ? state.combat->player.hurt_ticks : 0U)
              << " hit_stop="
              << (state.combat.has_value()
                    ? state.combat->player.hit_stop_ticks : 0U)
              << " input_size="
              << (state.combat.has_value()
                    ? state.combat->diagnostics.input_size : 0U)
              << " storm_accepted=" << driver.storm_accepted
              << " draw_accepted=" << driver.draw_accepted
              << " light_queued=" << driver.light_queued
              << " melee_chain_entries=" << driver.melee_chain_entries
              << " melee_handoffs_after_absent="
              << driver.melee_handoffs_after_absent
              << " melee_in_lane_acquired="
              << driver.melee_in_lane_acquired
              << " melee_nearest_acquired="
              << driver.melee_nearest_acquired
              << " ranged_acquired_after_melee_chain="
              << driver.ranged_acquired_after_melee_chain
              << " melee_initial_distance_squared="
              << driver.melee_initial_distance_squared
              << " light_lane_recovery_entries="
              << driver.light_lane_recovery_entries
              << " light_lane_recovery_ticks="
              << driver.light_lane_recovery_ticks
              << " light_lane_reacquired="
              << driver.light_lane_reacquired
              << '\n';
}

bool drive_clear(DungeonSession& session,
    arpg::persistence::SaveStore& store,
    int& initial_hp, int& minimum_hp,
    std::uint64_t& clear_generation,
    arpg::abyss::AbyssRuleId expected_rule) noexcept {
    initial_hp = 0;
    minimum_hp = 0;
    DenseDriverState dense_driver{};
    if (!service_pending(session, store)) {
        const auto failed = session.snapshot();
        print_dense_driver_failure(
            "service_pending", 0U, failed, dense_driver,
            0U, failed.initial_monster_count);
        return false;
    }
    const auto initial_state = session.snapshot();
    const std::uint32_t initial_population =
        initial_state.initial_monster_count;
    arpg::combat::RoomMonsterPlan diagnostic_plan{};
    if (!build_dense_diagnostic_plan(
            initial_state, validation_rules(), diagnostic_plan)) {
        return false;
    }
    DenseCoverageTrace coverage_trace{};
    const auto budget = clear_tick_budget(initial_population);
    if (!budget.has_value()) {
        std::cerr << "invalid clear budget initial_population="
                  << initial_population << " capacity="
                  << arpg::limits::kRoomMonsterCapacity << '\n';
        print_dense_driver_failure(
            "invalid_budget", 0U, initial_state, dense_driver,
            0U, initial_population);
        return false;
    }
    for (std::uint32_t tick = 0U; tick < *budget; ++tick) {
        if (!service_pending(session, store)) {
            print_dense_driver_failure(
                "service_pending", tick, session.snapshot(), dense_driver,
                *budget, initial_population);
            return false;
        }
        const auto state = session.snapshot();
        if (state.combat.has_value()) {
            if (coverage_trace.pending_sweep_input) {
                coverage_trace.pending_sweep_input = false;
                if (squared_xy_distance(state.combat->player.position,
                        coverage_trace.pending_sweep_position) == 0.0F
                        && coverage_trace.pending_sweep_waypoint
                            < coverage_trace.blocked_sweep_waypoints.size()) {
                    ++coverage_trace.blocked_sweep_waypoints[
                        coverage_trace.pending_sweep_waypoint];
                }
            }
            record_dense_snapshot(*state.combat, tick, coverage_trace);
            if (initial_hp == 0) {
                initial_hp = state.combat->player.hp;
                minimum_hp = initial_hp;
            }
            minimum_hp = (std::min)(minimum_hp, state.combat->player.hp);
        }
        if (state.phase == RoomPhase::cleared
                || state.phase == RoomPhase::awaiting_exit) {
            clear_generation = state.commit_generation;
            const bool expected = state.is_abyss
                && state.abyss_rule == expected_rule;
            if (!expected) {
                std::cerr << "cleared unexpected room=" << state.room_index
                          << " abyss=" << state.is_abyss
                          << " rule=" << static_cast<unsigned>(state.abyss_rule)
                          << " generation=" << state.commit_generation << '\n';
                print_dense_driver_failure(
                    "unexpected_clear", tick, state, dense_driver,
                    *budget, initial_population);
            }
            return expected;
        }
        if (state.phase == RoomPhase::faulted || !state.combat.has_value()) {
            std::cerr << "combat stopped phase="
                      << static_cast<unsigned>(state.phase)
                      << " room=" << state.room_index
                      << " abyss=" << state.is_abyss
                      << " generation=" << state.commit_generation
                      << " fault=" << static_cast<unsigned>(
                            state.diagnostics.fault) << '\n';
            print_dense_driver_failure(
                "combat_stopped", tick, state, dense_driver,
                *budget, initial_population);
            return false;
        }
        MovementInput movement{};
        if (state.phase == RoomPhase::combat) {
            const auto& combat = *state.combat;
            const auto& player = combat.player;
            if (dense_driver.pending_stance_progress_check) {
                dense_driver.pending_stance_progress_check = false;
                const float distance_squared = squared_xy_distance(
                    player.position, dense_driver.locked_stance);
                if (dense_driver.previous_stance_distance_squared
                        - distance_squared < 1e-4F) {
                    const auto stalled = dense_driver.locked_monster_ordinal;
                    release_navigation_target(dense_driver,
                        DenseLeaseRelease::movement_stall);
                    dense_driver.stalled_ordinal = stalled;
                    dense_driver.sweep_escape = true;
                }
            }
            if (dense_driver.pending_close_progress_check) {
                dense_driver.pending_close_progress_check = false;
                const bool position_changed = squared_xy_distance(
                    player.position,
                    dense_driver.previous_close_position) > 0.0F;
                if (dense_driver.close_for_light && !position_changed) {
                    const auto stalled =
                        dense_driver.locked_monster_ordinal;
                    release_navigation_target(dense_driver,
                        DenseLeaseRelease::movement_stall);
                    dense_driver.stalled_ordinal = stalled;
                    dense_driver.recover_until_light_lane = true;
                    ++dense_driver.light_lane_recovery_entries;
                    dense_driver.sweep_escape = true;
                }
            }
            const GridRouteProgress sweep_progress =
                settle_grid_route_movement(
                    dense_driver.sweep_grid, player.position);
            if (sweep_progress == GridRouteProgress::unreachable) {
                print_dense_driver_failure(
                    "sweep_grid_unreachable", tick, state, dense_driver,
                    *budget, initial_population);
                return false;
            }
            if (sweep_progress == GridRouteProgress::moved
                    && !dense_driver.recover_until_light_lane) {
                dense_driver.sweep_escape = false;
            }

            const MonsterSnapshot* target = nullptr;
            if (dense_driver.locked_monster_ordinal.has_value()) {
                target = living_monster_by_ordinal(combat,
                    *dense_driver.locked_monster_ordinal);
                if (target == nullptr) {
                    release_navigation_target(
                        dense_driver, DenseLeaseRelease::absent);
                }
            }
            if (target == nullptr
                    && (dense_driver.recover_until_light_lane
                        || !dense_driver.sweep_escape)) {
                target = try_acquire_navigation_target(combat, dense_driver);
                if (target == nullptr && state.remaining_targets != 0U) {
                    dense_driver.sweep_escape = true;
                }
            }

            const AttackGeometry geometry = attack_geometry(combat);
            bool stance_movement_requested = false;
            bool close_movement_requested = false;
            bool close_facing_candidate = false;
            std::int8_t close_facing_direction = 0;
            if (target != nullptr) {
                if (dense_driver.close_for_light) {
                    movement = movement_toward(
                        player.position, target->position);
                    close_movement_requested =
                        movement.x != 0 || movement.y != 0;
                    const float dx = target->position.x - player.position.x;
                    const float dy = target->position.y - player.position.y;
                    const auto target_facing = dx > 0.0F
                        ? arpg::combat::Facing::right
                        : arpg::combat::Facing::left;
                    close_facing_candidate = !close_movement_requested
                        && controllable_movement_snapshot(combat)
                        && std::fabs(dx) <= 1.70F
                        && std::fabs(dy) <= 0.55F
                        && std::fabs(dx) > 0.20F
                        && player.facing != target_facing
                        && !in_attack_lane(combat, *target);
                    if (close_facing_candidate) {
                        close_facing_direction = dx > 0.0F ? 1 : -1;
                    }
                } else {
                    movement = movement_toward_stance(
                        player.position, dense_driver.locked_stance,
                        dense_driver.locked_facing);
                    stance_movement_requested =
                        movement.x != 0 || movement.y != 0;
                    const bool at_stance = stance_position_reached(
                        player.position, dense_driver.locked_stance,
                        dense_driver.locked_facing);
                    if (at_stance
                            && player.facing != dense_driver.locked_facing) {
                        movement = {};
                        movement.x = dense_driver.locked_facing
                                == arpg::combat::Facing::right
                            ? 1 : -1;
                        stance_movement_requested = false;
                    } else if (at_stance) {
                        dense_driver.stance_reached = true;
                    }

                    if (dense_driver.stance_reached && !geometry.any()) {
                        release_navigation_target(dense_driver,
                            DenseLeaseRelease::stale_geometry);
                        target = try_acquire_navigation_target(
                            combat, dense_driver);
                        movement = {};
                        stance_movement_requested = false;
                        if (target != nullptr) {
                            movement = movement_toward_stance(
                                player.position, dense_driver.locked_stance,
                                dense_driver.locked_facing);
                            stance_movement_requested =
                                movement.x != 0 || movement.y != 0;
                        } else if (state.remaining_targets != 0U) {
                            dense_driver.sweep_escape = true;
                        }
                    }
                }
            }

            if (dense_driver.sweep_escape && state.remaining_targets != 0U) {
                movement = sweep_movement(player.position, dense_driver);
                stance_movement_requested = false;
                ++dense_driver.sweep_ticks;
                if (dense_driver.recover_until_light_lane) {
                    ++dense_driver.light_lane_recovery_ticks;
                }
            }

            const DenseActionDecision action = request_dense_action(
                session, combat, geometry, dense_driver);
            if (target != nullptr && dense_driver.stance_reached
                    && !dense_driver.close_for_light && action.ready
                    && !action.skill_accepted && !geometry.light) {
                dense_driver.close_for_light = true;
                dense_driver.melee_chain = true;
                ++dense_driver.melee_chain_entries;
                movement = movement_toward(
                    player.position, target->position);
                stance_movement_requested = false;
                close_movement_requested =
                    movement.x != 0 || movement.y != 0;
            }
            if (close_facing_candidate && action.ready
                    && !action.action_requested) {
                if (!dense_driver.close_facing_turn_attempted) {
                    movement = {};
                    movement.x = close_facing_direction;
                    dense_driver.close_facing_turn_attempted = true;
                    ++dense_driver.close_facing_corrections;
                    close_movement_requested = false;
                } else {
                    const auto stalled = dense_driver.locked_monster_ordinal;
                    release_navigation_target(dense_driver,
                        DenseLeaseRelease::movement_stall);
                    dense_driver.stalled_ordinal = stalled;
                    dense_driver.recover_until_light_lane = true;
                    ++dense_driver.light_lane_recovery_entries;
                    dense_driver.sweep_escape = true;
                    movement = sweep_movement(
                        player.position, dense_driver);
                    ++dense_driver.sweep_ticks;
                    ++dense_driver.light_lane_recovery_ticks;
                    close_movement_requested = false;
                }
            }
            const bool controllable = controllable_movement_snapshot(combat)
                && !action.action_requested;
            if (controllable && stance_movement_requested) {
                dense_driver.pending_stance_progress_check = true;
                dense_driver.previous_stance_distance_squared =
                    squared_xy_distance(
                        player.position, dense_driver.locked_stance);
            } else if (controllable && close_movement_requested) {
                dense_driver.pending_close_progress_check = true;
                dense_driver.previous_close_position = player.position;
            }
            if (dense_driver.sweep_escape) {
                observe_grid_route_movement(dense_driver.sweep_grid,
                    player.position, movement, controllable);
                if (controllable
                        && (movement.x != 0 || movement.y != 0)) {
                    coverage_trace.pending_sweep_input = true;
                    coverage_trace.pending_sweep_waypoint =
                        dense_driver.waypoint;
                    coverage_trace.pending_sweep_position = player.position;
                }
            }
        }
        session.tick(movement);
        while (session.try_pop_event().has_value()) {}
        while (const auto event = session.try_pop_combat_event()) {
            record_dense_defeat(*event, coverage_trace);
        }
    }
    const auto failed = session.snapshot();
    if (failed.combat.has_value()) {
        if (coverage_trace.pending_sweep_input
                && squared_xy_distance(failed.combat->player.position,
                    coverage_trace.pending_sweep_position) == 0.0F
                && coverage_trace.pending_sweep_waypoint
                    < coverage_trace.blocked_sweep_waypoints.size()) {
            ++coverage_trace.blocked_sweep_waypoints[
                coverage_trace.pending_sweep_waypoint];
        }
        record_dense_snapshot(*failed.combat, *budget, coverage_trace);
    }
    std::cerr << "clear failed phase=" << static_cast<unsigned>(failed.phase)
              << " abyss=" << failed.is_abyss
              << " rule=" << static_cast<unsigned>(failed.abyss_rule)
              << " lifecycle_pending=" << failed.abyss_pending_rewards
              << " room=" << failed.room_index
              << " hp=" << (failed.combat.has_value()
                    ? failed.combat->player.hp : -1)
              << " targets=" << static_cast<unsigned>(failed.remaining_targets)
              << " budget=" << *budget
              << " initial_population=" << initial_population
              << '\n';
    print_dense_driver_failure(
        "tick_budget", *budget, failed, dense_driver,
        *budget, initial_population);
    print_dense_coverage_failure(diagnostic_plan, coverage_trace);
    return false;
}

bool wait_for_rewards(DungeonSession& session,
    arpg::persistence::SaveStore& store,
    std::array<std::uint64_t, 3>& ids) noexcept {
    for (int tick = 0; tick < 64; ++tick) {
        if (!service_pending(session, store)) return false;
        const auto state = session.snapshot();
        std::size_t found = 0U;
        for (std::size_t index = 0U; index < state.ground_item_count; ++index) {
            const auto& ground = state.ground_items[index];
            if (ground.source != arpg::dungeon::GroundItemSource::abyss_chest
                    || ground.abyss_reward_ordinal >= ids.size()) {
                continue;
            }
            ids[ground.abyss_reward_ordinal] = ground.item_id;
        }
        for (const std::uint64_t id : ids) found += id != 0U;
        if (found == ids.size()) return true;
        session.tick({});
    }
    const auto failed = session.snapshot();
    std::cerr << "reward wait failed phase="
              << static_cast<unsigned>(failed.phase)
              << " pending=" << failed.abyss_pending_rewards
              << " unpicked=" << failed.abyss_unpicked_rewards
              << " ground=" << failed.ground_item_count
              << " fault=" << static_cast<unsigned>(failed.diagnostics.fault)
              << '\n';
    return false;
}

void print_claim_reward_failure(
    const char* reason,
    std::uint32_t tick,
    const arpg::dungeon::DungeonSnapshot& state,
    std::uint8_t reward_ordinal,
    const GridRouteState& grid,
    bool target_seen,
    Vec3 target_position) {
    const bool has_combat = state.combat.has_value();
    std::cerr << "claim failed reason=" << reason
              << " tick=" << tick
              << " reward=" << static_cast<unsigned>(reward_ordinal)
              << " inventory=" << state.inventory_count
              << " ground=" << state.ground_item_count
              << " pending=" << state.abyss_pending_rewards
              << " unpicked=" << state.abyss_unpicked_rewards
              << " phase=" << static_cast<unsigned>(state.phase)
              << " fault=" << static_cast<unsigned>(state.diagnostics.fault)
              << " player_x="
              << (has_combat ? state.combat->player.position.x : 0.0F)
              << " player_y="
              << (has_combat ? state.combat->player.position.y : 0.0F)
              << " target_seen=" << target_seen
              << " target_x=" << (target_seen ? target_position.x : 0.0F)
              << " target_y=" << (target_seen ? target_position.y : 0.0F)
              << " grid_phase=" << static_cast<unsigned>(grid.phase)
              << " grid_boundary_column="
              << static_cast<unsigned>(grid.boundary_column)
              << " grid_boundary_x=" << grid_boundary_x(grid.boundary_column)
              << " grid_pending_progress="
              << grid.pending_movement_progress_check
              << " grid_join_invariant_failures="
              << grid.join_invariant_failures
              << " grid_route_rejoins=" << grid.route_rejoins
              << '\n';
}

bool claim_reward(DungeonSession& session,
    arpg::persistence::SaveStore& store,
    std::uint8_t reward_ordinal) noexcept {
    constexpr std::uint32_t kClaimTickBudget = 2400U;
    GridRouteState grid{};
    Vec3 last_target_position{};
    bool target_seen = false;
    for (std::uint32_t tick = 0U; tick < kClaimTickBudget; ++tick) {
        if (!service_pending(session, store)) {
            print_claim_reward_failure("service_pending", tick,
                session.snapshot(), reward_ordinal, grid,
                target_seen, last_target_position);
            return false;
        }
        const auto state = session.snapshot();
        if (state.abyss_unpicked_rewards < 3U
                && state.abyss_pending_rewards == 0U) {
            return true;
        }
        if (!state.combat.has_value()) {
            print_claim_reward_failure("no_combat", tick, state,
                reward_ordinal, grid, target_seen, last_target_position);
            return false;
        }
        const auto& combat = *state.combat;
        const GridRouteProgress claim_progress =
            settle_grid_route_movement(grid, combat.player.position);
        if (claim_progress == GridRouteProgress::unreachable) {
            print_claim_reward_failure("grid_unreachable", tick, state,
                reward_ordinal, grid, target_seen, last_target_position);
            return false;
        }
        const arpg::dungeon::GroundItemSnapshot* target = nullptr;
        for (std::size_t index = 0U; index < state.ground_item_count; ++index) {
            if (state.ground_items[index].source
                    == arpg::dungeon::GroundItemSource::abyss_chest
                    && state.ground_items[index].abyss_reward_ordinal
                        == reward_ordinal) {
                target = &state.ground_items[index];
                break;
            }
        }
        if (target == nullptr) {
            print_claim_reward_failure("target_missing", tick, state,
                reward_ordinal, grid, target_seen, last_target_position);
            return false;
        }
        last_target_position = target->position;
        target_seen = true;
        const float dx = target->position.x - combat.player.position.x;
        const float dy = target->position.y - combat.player.position.y;
        if (dx * dx + dy * dy
                <= arpg::dungeon::kPickupRadius * arpg::dungeon::kPickupRadius) {
            const RequestResult requested = session.request_pickup(target->ordinal);
            if (requested == RequestResult::faulted) {
                print_claim_reward_failure("request_faulted", tick, state,
                    reward_ordinal, grid, target_seen, last_target_position);
                return false;
            }
            session.tick({});
            continue;
        }
        const MovementInput movement = grid_route_movement(
            combat.player.position, target->position,
            grid.phase, grid.boundary_column);
        observe_grid_route_movement(grid, combat.player.position, movement,
            controllable_movement_snapshot(combat));
        session.tick(movement);
    }
    const auto failed = session.snapshot();
    print_claim_reward_failure("tick_budget", kClaimTickBudget, failed,
        reward_ordinal, grid, target_seen, last_target_position);
    return false;
}

Vec3 door_position(ExitDirection direction) noexcept {
    switch (direction) {
    case ExitDirection::up:
        return {0.0F, arpg::combat::room_bounds::min_y, 0.0F};
    case ExitDirection::down:
        return {0.0F, arpg::combat::room_bounds::max_y, 0.0F};
    case ExitDirection::left:
        return {arpg::combat::room_bounds::min_x, 0.0F, 0.0F};
    case ExitDirection::right:
        return {arpg::combat::room_bounds::max_x, 0.0F, 0.0F};
    case ExitDirection::none: return {};
    }
    return {};
}

MovementInput exit_movement(Vec3 player, ExitDirection direction) noexcept {
    MovementInput movement = movement_toward(player, door_position(direction));
    switch (direction) {
    case ExitDirection::up: movement.y = -1; break;
    case ExitDirection::down: movement.y = 1; break;
    case ExitDirection::left: movement.x = -1; break;
    case ExitDirection::right: movement.x = 1; break;
    case ExitDirection::none: break;
    }
    return movement;
}

bool abandon_through_door(DungeonSession& session,
    arpg::persistence::SaveStore& store,
    ExitDirection direction,
    AbandonEvidence& evidence) noexcept {
    bool released = false;
    for (int tick = 0; tick < 2000; ++tick) {
        if (!service_pending(session, store)) return false;
        const auto state = session.snapshot();
        if (state.room_index > 1U) {
            return evidence.warning_event && evidence.armed
                && evidence.neutral_release && released;
        }
        if (state.phase == RoomPhase::faulted || !state.combat.has_value()) {
            std::cerr << "abandon stopped phase="
                      << static_cast<unsigned>(state.phase)
                      << " fault=" << static_cast<unsigned>(
                            state.diagnostics.fault)
                      << " pending=" << state.abyss_pending_rewards
                      << " unpicked=" << state.abyss_unpicked_rewards << '\n';
            return false;
        }
        if (state.abyss_exit_confirmation_armed) {
            evidence.armed = true;
            while (const auto event = session.try_pop_event()) {
                evidence.warning_event = evidence.warning_event
                    || (event->kind
                        == arpg::dungeon::DungeonEventKind::abyss_exit_warning
                        && event->transition
                            == arpg::dungeon::TransitionKind::door
                        && event->direction == direction);
            }
            if (!released) {
                const auto loaded = store.load();
                if (loaded.state != arpg::persistence::SaveLoadState::ready) {
                    return false;
                }
                evidence.before = loaded.checkpoint.abyss;
                evidence.inventory_before = state.inventory_count;
                evidence.ground_before = state.ground_item_count;
                for (const auto& ground : state.ground_items) {
                    evidence.abyss_ground_before +=
                        ground.source
                        == arpg::dungeon::GroundItemSource::abyss_chest;
                }
                session.tick({});
                released = true;
                evidence.neutral_release =
                    session.snapshot().abyss_exit_confirmation_armed;
                continue;
            }
        }
        session.tick(exit_movement(
            state.combat->player.position, direction));
    }
    const auto failed = session.snapshot();
    std::cerr << "abandon timed out phase="
              << static_cast<unsigned>(failed.phase)
              << " room=" << failed.room_index
              << " armed=" << failed.abyss_exit_confirmation_armed
              << " pending=" << failed.abyss_pending_rewards
              << " unpicked=" << failed.abyss_unpicked_rewards << '\n';
    return false;
}

std::uint8_t popcount8(std::uint8_t value) noexcept {
    std::uint8_t result = 0U;
    while (value != 0U) {
        result = static_cast<std::uint8_t>(result + (value & 1U));
        value = static_cast<std::uint8_t>(value >> 1U);
    }
    return result;
}

std::optional<EnvironmentEvidence> run_environment_probe(
    const std::filesystem::path& save_directory) noexcept {
    const auto entry = find_entry(arpg::abyss::AbyssRuleId::thunderstorm);
    if (!entry.has_value()) return std::nullopt;
    arpg::persistence::SaveStore store({save_directory});
    const auto seeded = store.commit(entry->target);
    if (seeded.state != arpg::persistence::SaveCommitState::committed) {
        return std::nullopt;
    }
    DungeonSession session{validation_rules(), seeded.verified_state};
    if (!service_pending(session, store)) return std::nullopt;
    EnvironmentEvidence evidence{};
    int previous_hp = 0;
    bool previous_warning = false;
    for (int tick = 0; tick < 1200; ++tick) {
        if (!service_pending(session, store)) return std::nullopt;
        const auto state = session.snapshot();
        if (!state.combat.has_value() || !state.is_abyss) break;
        if (evidence.initial_hp == 0) {
            evidence.initial_hp = state.combat->player.hp;
            evidence.minimum_hp = evidence.initial_hp;
        }
        evidence.minimum_hp = (std::min)(
            evidence.minimum_hp, state.combat->player.hp);
        bool active_this_tick = false;
        bool warning_this_tick = false;
        for (std::size_t index = 0U;
             index < state.combat->hazard_count; ++index) {
            const auto& hazard = state.combat->hazards[index];
            if (!hazard.active
                    || hazard.source
                        != arpg::combat::HazardSource::abyss_environment) {
                continue;
            }
            evidence.warning_seen = evidence.warning_seen
                || hazard.telegraph_ticks != 0U;
            warning_this_tick = warning_this_tick
                || hazard.telegraph_ticks != 0U;
            evidence.active_seen = evidence.active_seen
                || hazard.telegraph_ticks == 0U;
            active_this_tick = active_this_tick
                || hazard.telegraph_ticks == 0U;
        }
        evidence.active_damage_seen = evidence.active_damage_seen
            || (active_this_tick && previous_hp != 0
                && state.combat->player.hp < previous_hp)
            || (previous_warning && !warning_this_tick
                && !active_this_tick && previous_hp != 0
                && state.combat->player.hp < previous_hp);
        evidence.active_seen = evidence.active_seen
            || (previous_warning && !warning_this_tick);
        if (evidence.active_damage_seen) {
            return evidence;
        }
        previous_hp = state.combat->player.hp;
        previous_warning = warning_this_tick;
        MovementInput movement{};
        if (!evidence.warning_seen) {
            const int leg = tick % 240;
            movement = leg < 60 ? MovementInput{1, 0}
                : leg < 120 ? MovementInput{0, 1}
                : leg < 180 ? MovementInput{-1, 0}
                : MovementInput{0, -1};
            if (state.combat->player.active_attack
                    == arpg::combat::AttackId::none
                    && state.combat->diagnostics.input_size == 0U) {
                static_cast<void>(session.queue_action(Action::jump));
            }
        }
        session.tick(movement);
        while (session.try_pop_event().has_value()) {}
        while (session.try_pop_combat_event().has_value()) {}
    }
    const auto stopped = session.snapshot();
    std::cerr << "environment probe stopped phase="
              << static_cast<unsigned>(stopped.phase)
              << " room=" << stopped.room_index
              << " abyss=" << stopped.is_abyss
              << " warning=" << evidence.warning_seen
              << " active=" << evidence.active_seen
              << " active_damage=" << evidence.active_damage_seen
              << " initial_hp=" << evidence.initial_hp
              << " minimum_hp=" << evidence.minimum_hp << '\n';
    return evidence.warning_seen && evidence.active_seen
        && evidence.active_damage_seen
        ? std::optional<EnvironmentEvidence>{evidence} : std::nullopt;
}

std::filesystem::path clean_fixture_directory(
    const std::filesystem::path& executable) {
    const auto directory = std::filesystem::absolute(executable).parent_path()
        / "stage10-validation-fixture-save";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    if (error) return {};
    std::filesystem::create_directories(directory, error);
    return error ? std::filesystem::path{} : directory;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    const auto entry = find_entry(arpg::abyss::AbyssRuleId::life_sacrifice);
    if (!entry.has_value()) return 3;
    const auto save_directory = clean_fixture_directory(argv[0]);
    if (save_directory.empty()) return 4;
    const DungeonRules rules = validation_rules();

    arpg::persistence::SaveStore store({save_directory});
    const auto seeded = store.commit(entry->target);
    if (seeded.state != arpg::persistence::SaveCommitState::committed) return 5;
    DungeonSession session{rules, seeded.verified_state};
    if (!service_pending(session, store)) return 6;
    const auto started = session.snapshot();
    if (started.pending_save_kind.has_value()
            || started.abyss_rule != arpg::abyss::AbyssRuleId::life_sacrifice) {
        return 7;
    }

    int initial_hp = 0;
    int minimum_hp = 0;
    std::uint64_t clear_generation = 0U;
    if (!drive_clear(session, store, initial_hp, minimum_hp, clear_generation,
            arpg::abyss::AbyssRuleId::life_sacrifice)) {
        return 8;
    }
    const auto cleared = store.load();
    if (cleared.state != arpg::persistence::SaveLoadState::ready
            || cleared.checkpoint.abyss.lifecycle
                != arpg::abyss::AbyssLifecycle::cleared
            || cleared.checkpoint.current_room.seed
                != entry->target.current_room.seed
            || cleared.checkpoint.abyss.rule
                != arpg::abyss::AbyssRuleId::life_sacrifice
            || cleared.checkpoint.commit_generation != clear_generation
            || cleared.checkpoint.abyss.reward_total == 0U
            || cleared.checkpoint.abyss.reward_total > 3U
            || cleared.checkpoint.abyss.generated_mask != 0U
            || cleared.checkpoint.abyss.claimed_mask != 0U
            || cleared.checkpoint.abyss.abandoned_mask != 0U) {
        return 20;
    }
    std::array<std::uint64_t, 3> reward_ids{};
    if (!wait_for_rewards(session, store, reward_ids)) return 9;
    if (!claim_reward(session, store, 1U)) return 10;
    const auto claimed = session.snapshot();
    const auto loaded = store.load();
    if (loaded.state != arpg::persistence::SaveLoadState::ready) return 11;

    arpg::persistence::SaveStore abandon_store({save_directory / "abandon"});
    const auto abandon_seeded = abandon_store.commit(cleared.checkpoint);
    if (abandon_seeded.state
            != arpg::persistence::SaveCommitState::committed) return 12;
    DungeonSession abandon_session{rules, abandon_seeded.verified_state};
    if (!service_pending(abandon_session, abandon_store)) return 13;
    AbandonEvidence abandon_evidence{};
    if (!abandon_through_door(abandon_session, abandon_store,
            ExitDirection::right, abandon_evidence)) return 15;
    const auto abandoned = abandon_store.load();
    if (abandoned.state != arpg::persistence::SaveLoadState::ready
            || !abandoned.checkpoint.last_abyss_resolution.valid) return 16;
    const std::uint8_t valid_mask = static_cast<std::uint8_t>(
        (1U << abandon_evidence.before.reward_total) - 1U);
    const std::uint8_t expected_generated = popcount8(static_cast<std::uint8_t>(
        abandon_evidence.before.generated_mask & valid_mask));
    const std::uint8_t expected_claimed = popcount8(static_cast<std::uint8_t>(
        abandon_evidence.before.claimed_mask & valid_mask));
    const std::uint8_t expected_abandoned = popcount8(static_cast<std::uint8_t>(
        valid_mask & static_cast<std::uint8_t>(
            ~abandon_evidence.before.generated_mask)));
    const auto& resolution = abandoned.checkpoint.last_abyss_resolution;
    if (!abandon_evidence.warning_event || !abandon_evidence.armed
            || !abandon_evidence.neutral_release
            || resolution.total != abandon_evidence.before.reward_total
            || resolution.room_seed != entry->target.current_room.seed
            || resolution.rule != arpg::abyss::AbyssRuleId::life_sacrifice
            || resolution.generated != expected_generated
            || resolution.claimed != expected_claimed
            || resolution.abandoned != expected_abandoned
            || abandon_evidence.abyss_ground_before
                != static_cast<std::uint16_t>(
                    expected_generated - expected_claimed)
            || abandon_evidence.ground_before
                < abandon_evidence.abyss_ground_before
            || abandoned.checkpoint.item_ownership.items.size()
                != abandon_evidence.inventory_before + 6U) return 17;
    const auto environment = run_environment_probe(
        save_directory / "environment");
    if (!environment.has_value()) return 18;

    const auto preview = arpg::dungeon::preview_abyss_doors(
        entry->source.current_room);
    const std::size_t direction_index = static_cast<std::size_t>(entry->direction);
    std::cout << "door_preview source_seed=" << entry->source.current_room.seed
              << " direction=" << direction_index
              << " target_seed=" << entry->target.current_room.seed
              << " marked=" << preview[direction_index] << '\n'
              << "started generation=" << started.commit_generation
              << " rule=" << static_cast<unsigned>(started.abyss_rule)
              << " danger=" << static_cast<unsigned>(started.abyss_danger)
              << '\n'
              << "environment rule=" << static_cast<unsigned>(
                    arpg::abyss::AbyssRuleId::thunderstorm)
              << " initial_hp=" << environment->initial_hp
              << " minimum_hp=" << environment->minimum_hp
              << " warning=" << environment->warning_seen
              << " active=" << environment->active_seen
              << " active_damage=" << environment->active_damage_seen
              << " damaged="
              << (environment->minimum_hp < environment->initial_hp) << '\n'
              << "combat initial_hp=" << initial_hp
              << " minimum_hp=" << minimum_hp << '\n'
              << "clear generation=" << clear_generation << '\n'
              << "rewards ids=" << reward_ids[0] << ',' << reward_ids[1]
              << ',' << reward_ids[2] << '\n'
              << "reload generated="
              << static_cast<unsigned>(loaded.checkpoint.abyss.generated_mask)
              << " claimed="
              << static_cast<unsigned>(loaded.checkpoint.abyss.claimed_mask)
              << " abandoned="
              << static_cast<unsigned>(loaded.checkpoint.abyss.abandoned_mask)
              << " inventory=" << claimed.inventory_count << '\n'
              << "abandon total="
              << static_cast<unsigned>(
                    abandoned.checkpoint.last_abyss_resolution.total)
              << " generated="
              << static_cast<unsigned>(
                    abandoned.checkpoint.last_abyss_resolution.generated)
              << " claimed="
              << static_cast<unsigned>(
                    abandoned.checkpoint.last_abyss_resolution.claimed)
              << " abandoned="
              << static_cast<unsigned>(
                    abandoned.checkpoint.last_abyss_resolution.abandoned)
              << '\n';
    return preview[direction_index]
            && started.commit_generation > entry->target.commit_generation
            && reward_ids[0] != 0U && reward_ids[1] != 0U
            && reward_ids[2] != 0U
            && loaded.checkpoint.abyss.claimed_mask != 0U
        ? 0 : 19;
}
