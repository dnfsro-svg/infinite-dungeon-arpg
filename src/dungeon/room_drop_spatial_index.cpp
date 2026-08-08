#include "dungeon/room_drop_spatial_index.hpp"

#include "combat/room_bounds.hpp"
#include "combat/room_spatial_grid.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace arpg::dungeon {
namespace {

[[nodiscard]] bool precedes(const RoomDropIndexRecord& left,
    const RoomDropIndexRecord& right) noexcept {
    if (left.home_cell != right.home_cell) {
        return left.home_cell < right.home_cell;
    }
    if (left.kind != right.kind) {
        return static_cast<std::uint8_t>(left.kind)
            < static_cast<std::uint8_t>(right.kind);
    }
    return left.ordinal < right.ordinal;
}

template <std::size_t Capacity>
void insertion_sort(std::array<RoomDropIndexRecord, Capacity>& records,
    const std::size_t count) noexcept {
    for (std::size_t index = 1U; index < count; ++index) {
        const RoomDropIndexRecord value = records[index];
        std::size_t insert = index;
        while (insert != 0U && precedes(value, records[insert - 1U])) {
            records[insert] = records[insert - 1U];
            --insert;
        }
        records[insert] = value;
    }
}

[[nodiscard]] bool contains(const combat::Aabb& bounds,
    const combat::Vec3 point) noexcept {
    return point.x >= bounds.minimum.x && point.x <= bounds.maximum.x
        && point.y >= bounds.minimum.y && point.y <= bounds.maximum.y
        && point.z >= bounds.minimum.z && point.z <= bounds.maximum.z;
}

[[nodiscard]] bool finite(const combat::Vec3 point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y)
        && std::isfinite(point.z);
}

[[nodiscard]] std::uint8_t cell_column(const float value) noexcept {
    if (value <= combat::room_bounds::min_x) return 0U;
    if (value >= combat::room_bounds::max_x) {
        return static_cast<std::uint8_t>(
            combat::room_spatial::columns - 1U);
    }
    const float normalized = (value - combat::room_bounds::min_x)
        / combat::room_spatial::cell_width;
    return static_cast<std::uint8_t>(std::floor(normalized));
}

[[nodiscard]] std::uint8_t cell_row(const float value) noexcept {
    if (value <= combat::room_bounds::min_y) return 0U;
    if (value >= combat::room_bounds::max_y) {
        return static_cast<std::uint8_t>(combat::room_spatial::rows - 1U);
    }
    const float normalized = (value - combat::room_bounds::min_y)
        / combat::room_spatial::cell_depth;
    return static_cast<std::uint8_t>(std::floor(normalized));
}

}  // namespace

void RoomDropSpatialIndex::clear() noexcept {
    cells_ = {};
    reserve_ = {};
    home_cells_.fill(0xFFFFU);
    reserve_count_ = 0U;
    last_set_present_records_examined_ = 0U;
    monster_count_ = 0U;
    fault_ = RoomDropIndexFault::none;
}

bool RoomDropSpatialIndex::reset(
    const combat::RoomMonsterPlan& plan) noexcept {
    clear();
    monster_count_ = plan.monster_count;
    if (monster_count_ > home_cells_.size()) {
        fault_ = RoomDropIndexFault::invalid_plan;
        return false;
    }
    for (std::uint16_t ordinal = 0U; ordinal < monster_count_; ++ordinal) {
        const auto& monster = plan.monsters[ordinal];
        if (monster.spawn_ordinal != ordinal
                || monster.home_cell >= kRoomDropCellCount) {
            fault_ = RoomDropIndexFault::invalid_plan;
            return false;
        }
        home_cells_[ordinal] = monster.home_cell;
    }
    return true;
}

bool RoomDropSpatialIndex::insert_monster_drop(const RoomDropKind kind,
    const std::uint16_t ordinal, const std::uint16_t spawn_ordinal,
    const combat::Vec3 position) noexcept {
    if (fault_ != RoomDropIndexFault::none) return false;
    if (!finite(position)) return false;
    if (spawn_ordinal >= monster_count_
            || home_cells_[spawn_ordinal] >= kRoomDropCellCount) {
        fault_ = RoomDropIndexFault::invalid_ordinal;
        return false;
    }
    Bucket& bucket = cells_[home_cells_[spawn_ordinal]];
    for (std::size_t index = 0U; index < bucket.count; ++index) {
        if (bucket.records[index].kind == kind
                && bucket.records[index].ordinal == ordinal) {
            if (bucket.records[index].present) return false;
            bucket.records[index].position = position;
            bucket.records[index].present = true;
            return true;
        }
    }
    if (bucket.count >= bucket.records.size()) {
        fault_ = RoomDropIndexFault::cell_capacity;
        return false;
    }
    bucket.records[bucket.count++] = {
        kind, ordinal, home_cells_[spawn_ordinal], position, true};
    insertion_sort(bucket.records, bucket.count);
    return true;
}

bool RoomDropSpatialIndex::can_insert_monster_drop(const RoomDropKind kind,
    const std::uint16_t ordinal, const std::uint16_t spawn_ordinal,
    const combat::Vec3 position) const noexcept {
    if (fault_ != RoomDropIndexFault::none || !finite(position)
            || spawn_ordinal >= monster_count_
            || home_cells_[spawn_ordinal] >= kRoomDropCellCount) {
        return false;
    }
    const Bucket& bucket = cells_[home_cells_[spawn_ordinal]];
    for (std::size_t index = 0U; index < bucket.count; ++index) {
        const RoomDropIndexRecord& record = bucket.records[index];
        if (record.kind == kind && record.ordinal == ordinal) {
            return !record.present;
        }
    }
    return bucket.count < bucket.records.size();
}

bool RoomDropSpatialIndex::insert_abyss_reserve(const RoomDropKind kind,
    const std::uint16_t ordinal, const combat::Vec3 position) noexcept {
    if (fault_ != RoomDropIndexFault::none) return false;
    if (!finite(position)) return false;
    for (std::size_t index = 0U; index < reserve_count_; ++index) {
        if (reserve_[index].kind == kind
                && reserve_[index].ordinal == ordinal) {
            if (reserve_[index].present) return false;
            reserve_[index].position = position;
            reserve_[index].present = true;
            return true;
        }
    }
    if (reserve_count_ >= reserve_.size()) {
        fault_ = RoomDropIndexFault::reserve_capacity;
        return false;
    }
    reserve_[reserve_count_++] = {
        kind, ordinal, static_cast<std::uint16_t>(kRoomDropCellCount),
        position, true};
    insertion_sort(reserve_, reserve_count_);
    return true;
}

bool RoomDropSpatialIndex::can_insert_abyss_reserve(
    const RoomDropKind kind, const std::uint16_t ordinal,
    const combat::Vec3 position) const noexcept {
    if (fault_ != RoomDropIndexFault::none || !finite(position)) return false;
    for (std::size_t index = 0U; index < reserve_count_; ++index) {
        const RoomDropIndexRecord& record = reserve_[index];
        if (record.kind == kind && record.ordinal == ordinal) {
            return !record.present;
        }
    }
    return reserve_count_ < reserve_.size();
}

bool RoomDropSpatialIndex::set_present(const RoomDropKind kind,
    const std::uint16_t ordinal, const bool present) noexcept {
    last_set_present_records_examined_ = 0U;
    if (fault_ != RoomDropIndexFault::none) return false;
    const auto visit_bucket = [&](Bucket& bucket) noexcept {
        for (std::size_t index = 0U; index < bucket.count; ++index) {
            ++last_set_present_records_examined_;
            auto& record = bucket.records[index];
            if (record.kind == kind && record.ordinal == ordinal) {
                record.present = present;
                return true;
            }
        }
        return false;
    };
    std::uint16_t spawn_ordinal = 0xFFFFU;
    if (kind == RoomDropKind::equipment) {
        spawn_ordinal = ordinal;
    } else if (ordinal < limits::kRoomMonsterCapacity * 2U) {
        spawn_ordinal = static_cast<std::uint16_t>(ordinal / 2U);
    }
    if (spawn_ordinal < monster_count_) {
        const std::uint16_t home_cell = home_cells_[spawn_ordinal];
        if (home_cell < cells_.size()
                && visit_bucket(cells_[home_cell])) return true;
    }
    const bool may_be_reserve = kind == RoomDropKind::equipment
        || (kind == RoomDropKind::material
            && ordinal >= limits::kRoomMonsterCapacity * 2U);
    if (may_be_reserve) {
        for (std::size_t index = 0U; index < reserve_count_; ++index) {
            ++last_set_present_records_examined_;
            auto& record = reserve_[index];
            if (record.kind == kind && record.ordinal == ordinal) {
                record.present = present;
                return true;
            }
        }
    }
    return false;
}

bool RoomDropSpatialIndex::has_present_record(const RoomDropKind kind,
    const std::uint16_t ordinal) const noexcept {
    if (fault_ != RoomDropIndexFault::none) return false;
    const auto bucket_contains = [&](const Bucket& bucket) noexcept {
        for (std::size_t index = 0U; index < bucket.count; ++index) {
            const RoomDropIndexRecord& record = bucket.records[index];
            if (record.kind == kind && record.ordinal == ordinal) {
                return record.present;
            }
        }
        return false;
    };
    std::uint16_t spawn_ordinal = 0xFFFFU;
    if (kind == RoomDropKind::equipment) {
        spawn_ordinal = ordinal;
    } else if (ordinal < limits::kRoomMonsterCapacity * 2U) {
        spawn_ordinal = static_cast<std::uint16_t>(ordinal / 2U);
    }
    if (spawn_ordinal < monster_count_) {
        const std::uint16_t home_cell = home_cells_[spawn_ordinal];
        if (home_cell < cells_.size()
                && bucket_contains(cells_[home_cell])) return true;
    }
    const bool may_be_reserve = kind == RoomDropKind::equipment
        || (kind == RoomDropKind::material
            && ordinal >= limits::kRoomMonsterCapacity * 2U);
    if (!may_be_reserve) return false;
    for (std::size_t index = 0U; index < reserve_count_; ++index) {
        const RoomDropIndexRecord& record = reserve_[index];
        if (record.kind == kind && record.ordinal == ordinal) {
            return record.present;
        }
    }
    return false;
}

void RoomDropSpatialIndex::write_candidates(const combat::Aabb& world_bounds,
    RoomDropCandidateSet& output) const noexcept {
    output = {};
    if (fault_ != RoomDropIndexFault::none) {
        output.fault = fault_;
        return;
    }
    if (!std::isfinite(world_bounds.minimum.x)
            || !std::isfinite(world_bounds.minimum.y)
            || !std::isfinite(world_bounds.minimum.z)
            || !std::isfinite(world_bounds.maximum.x)
            || !std::isfinite(world_bounds.maximum.y)
            || !std::isfinite(world_bounds.maximum.z)
            || world_bounds.minimum.x > world_bounds.maximum.x
            || world_bounds.minimum.y > world_bounds.maximum.y
            || world_bounds.minimum.z > world_bounds.maximum.z) {
        output.fault = RoomDropIndexFault::invalid_bounds;
        return;
    }
    const std::uint8_t query_first_column = cell_column(world_bounds.minimum.x);
    const std::uint8_t query_last_column = cell_column(world_bounds.maximum.x);
    const std::uint8_t query_first_row = cell_row(world_bounds.minimum.y);
    const std::uint8_t query_last_row = cell_row(world_bounds.maximum.y);
    const std::uint8_t first_column = query_first_column == 0U
        ? 0U : static_cast<std::uint8_t>(query_first_column - 1U);
    const std::uint8_t last_column = static_cast<std::uint8_t>((std::min)(
        static_cast<std::size_t>(query_last_column) + 1U,
        combat::room_spatial::columns - 1U));
    const std::uint8_t first_row = query_first_row == 0U
        ? 0U : static_cast<std::uint8_t>(query_first_row - 1U);
    const std::uint8_t last_row = static_cast<std::uint8_t>((std::min)(
        static_cast<std::size_t>(query_last_row) + 1U,
        combat::room_spatial::rows - 1U));
    if (last_column - first_column + 1U > 7U
            || last_row - first_row + 1U > 5U) {
        output.fault = RoomDropIndexFault::query_capacity;
        return;
    }
    const auto visit = [&](const RoomDropIndexRecord& record) noexcept {
        ++output.candidates_examined;
        if (!record.present || !contains(world_bounds, record.position)) return;
        if (output.count >= output.records.size()) {
            output.fault = RoomDropIndexFault::query_capacity;
            return;
        }
        output.records[output.count++] = record;
    };
    for (std::uint8_t row = first_row; row <= last_row; ++row) {
        for (std::uint8_t column = first_column;
                column <= last_column; ++column) {
            const Bucket& bucket = cells_[static_cast<std::size_t>(row) * 20U
                + column];
            for (std::size_t index = 0U; index < bucket.count; ++index) {
                visit(bucket.records[index]);
            }
        }
    }
    for (std::size_t index = 0U; index < reserve_count_; ++index) {
        visit(reserve_[index]);
    }
}

}  // namespace arpg::dungeon
