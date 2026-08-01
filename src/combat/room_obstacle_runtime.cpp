#include "combat/room_obstacle_runtime.hpp"

#include "combat/combat_collision.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_spatial_grid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace arpg::combat {

void RoomObstacleRuntime::clear() noexcept {
    plan_view_ = {};
    ordinal_to_state_.fill(kInvalidRoomObstacleStateIndex);
    for (RoomObstacleState& state : states_) {
        state = RoomObstacleState{};
    }
    obstacle_count_ = 0U;
}

bool RoomObstacleRuntime::initialize(const RoomObstaclePlanView view) noexcept {
    clear();
    if (view.record_count > kRoomEnvironmentRecordCapacity
        || (view.record_count != 0U
            && (view.records == nullptr || view.cell_offsets == nullptr
                || view.cell_counts == nullptr))) {
        return false;
    }
    if (view.record_count == 0U) {
        plan_view_ = {};
        return true;
    }

    if (view.cell_offsets[0] != 0U
            || view.cell_offsets[kRoomEnvironmentCellCount]
                != view.record_count) {
        return false;
    }
    std::array<bool, kRoomEnvironmentCellCount> cell_has_obstacle{};
    for (std::size_t cell = 0U; cell < kRoomEnvironmentCellCount; ++cell) {
        const std::uint16_t begin = view.cell_offsets[cell];
        const std::uint16_t end = view.cell_offsets[cell + 1U];
        if (begin > end || end > view.record_count
            || end - begin != view.cell_counts[cell]) {
            clear();
            return false;
        }
    }

    for (std::size_t index = 0U; index < view.record_count; ++index) {
        const RoomEnvironmentRecord& record = view.records[index];
        if (record.obstacle.kind == RoomObstacleKind::none) continue;
        if (record.ordinal >= ordinal_to_state_.size()
            || record.home_cell >= kRoomEnvironmentCellCount
            || record.obstacle.kind >= RoomObstacleKind::count
            || obstacle_count_ >= states_.size()
            || cell_has_obstacle[record.home_cell]
            || ordinal_to_state_[record.ordinal]
                != kInvalidRoomObstacleStateIndex) {
            clear();
            return false;
        }
        const std::uint16_t state_index = obstacle_count_++;
        cell_has_obstacle[record.home_cell] = true;
        ordinal_to_state_[record.ordinal] = state_index;
        RoomObstacleState& state = states_[state_index];
        state.ordinal = record.ordinal;
        state.home_cell = record.home_cell;
        state.kind = record.obstacle.kind;
        state.bounds = record.obstacle.bounds;
        state.max_hp = record.obstacle.kind == RoomObstacleKind::breakable
            ? record.obstacle.max_hp : 0U;
        state.hp = state.max_hp;
        state.intact = true;
    }
    plan_view_ = view;
    return true;
}

const RoomObstacleState* RoomObstacleRuntime::state(
    const std::uint16_t environment_ordinal) const noexcept {
    if (environment_ordinal >= ordinal_to_state_.size()) return nullptr;
    const std::uint16_t index = ordinal_to_state_[environment_ordinal];
    return index < obstacle_count_ ? &states_[index] : nullptr;
}

RoomObstacleState* RoomObstacleRuntime::state(
    const std::uint16_t environment_ordinal) noexcept {
    return const_cast<RoomObstacleState*>(
        static_cast<const RoomObstacleRuntime&>(*this).state(
            environment_ordinal));
}

modifiers::EffectSet* RoomObstacleRuntime::effects(
    const std::uint16_t environment_ordinal) noexcept {
    RoomObstacleState* obstacle = state(environment_ordinal);
    return obstacle == nullptr ? nullptr : &obstacle->effects;
}

bool RoomObstacleRuntime::apply_damage(
    const std::uint16_t environment_ordinal,
    const std::uint16_t damage,
    const std::uint64_t tick) noexcept {
    RoomObstacleState* obstacle = state(environment_ordinal);
    if (obstacle == nullptr || !obstacle->intact
        || obstacle->kind != RoomObstacleKind::breakable || damage == 0U) {
        return false;
    }
    if (damage < obstacle->hp) {
        obstacle->hp = static_cast<std::uint16_t>(obstacle->hp - damage);
        return true;
    }
    obstacle->hp = 0U;
    obstacle->intact = false;
    obstacle->broken_tick = tick;
    return true;
}

std::size_t RoomObstacleRuntime::obstacle_count() const noexcept {
    return obstacle_count_;
}

RoomObstaclePlanView RoomObstacleRuntime::plan_view() const noexcept {
    return plan_view_;
}

namespace {

[[nodiscard]] std::size_t cell_axis(
    const float coordinate,
    const float minimum,
    const float cell_size,
    const std::size_t count) noexcept {
    if (!std::isfinite(coordinate) || coordinate <= minimum) return 0U;
    const float value = (coordinate - minimum) / cell_size;
    if (value >= static_cast<float>(count)) return count - 1U;
    return static_cast<std::size_t>(value);
}

}  // namespace

bool RoomObstacleRuntime::blocks_player(
    const Vec3 current, const Vec3 candidate) const noexcept {
    if (plan_view_.records == nullptr) return false;
    const std::size_t center_column = cell_axis(candidate.x,
        room_bounds::min_x, room_spatial::cell_width, room_spatial::columns);
    const std::size_t center_row = cell_axis(candidate.y,
        room_bounds::min_y, room_spatial::cell_depth, room_spatial::rows);
    const std::size_t first_column = center_column == 0U
        ? 0U : center_column - 1U;
    const std::size_t last_column = (std::min)(
        center_column + 1U, room_spatial::columns - 1U);
    const std::size_t first_row = center_row == 0U ? 0U : center_row - 1U;
    const std::size_t last_row = (std::min)(
        center_row + 1U, room_spatial::rows - 1U);
    for (std::size_t row = first_row; row <= last_row; ++row) {
        for (std::size_t column = first_column; column <= last_column;
             ++column) {
            const std::size_t cell = row * room_spatial::columns + column;
            const std::uint16_t begin = plan_view_.cell_offsets[cell];
            const std::uint16_t end = plan_view_.cell_offsets[cell + 1U];
            for (std::uint16_t index = begin; index < end; ++index) {
                const RoomEnvironmentRecord& record = plan_view_.records[index];
                const RoomObstacleState* obstacle = state(record.ordinal);
                if (obstacle != nullptr && obstacle->intact
                    && fire_room_obstacle::blocks_player(
                        obstacle->bounds, current, candidate)) {
                    return true;
                }
            }
        }
    }
    return false;
}

Vec3 RoomObstacleRuntime::route_monster(
    const Vec3 previous, const Vec3 candidate) const noexcept {
    return blocks_player(previous, candidate) ? previous : candidate;
}

std::size_t RoomObstacleRuntime::damage_overlapping(
    const Aabb bounds, const std::uint64_t tick) noexcept {
    if (plan_view_.records == nullptr) return 0U;
    const std::size_t first_column = cell_axis(bounds.minimum.x,
        room_bounds::min_x, room_spatial::cell_width, room_spatial::columns);
    const std::size_t last_column = cell_axis(bounds.maximum.x,
        room_bounds::min_x, room_spatial::cell_width, room_spatial::columns);
    const std::size_t first_row = cell_axis(bounds.minimum.y,
        room_bounds::min_y, room_spatial::cell_depth, room_spatial::rows);
    const std::size_t last_row = cell_axis(bounds.maximum.y,
        room_bounds::min_y, room_spatial::cell_depth, room_spatial::rows);
    std::size_t damaged = 0U;
    for (std::size_t row = first_row; row <= last_row; ++row) {
        for (std::size_t column = first_column; column <= last_column;
             ++column) {
            const std::size_t cell = row * room_spatial::columns + column;
            const std::uint16_t begin = plan_view_.cell_offsets[cell];
            const std::uint16_t end = plan_view_.cell_offsets[cell + 1U];
            for (std::uint16_t index = begin; index < end; ++index) {
                const RoomEnvironmentRecord& record = plan_view_.records[index];
                RoomObstacleState* obstacle = state(record.ordinal);
                if (obstacle == nullptr || !obstacle->intact
                    || obstacle->kind != RoomObstacleKind::breakable
                    || !overlaps_inclusive(bounds, obstacle->bounds)) {
                    continue;
                }
                if (apply_damage(obstacle->ordinal, obstacle->hp, tick)) {
                    ++damaged;
                }
            }
        }
    }
    return damaged;
}

}  // namespace arpg::combat
