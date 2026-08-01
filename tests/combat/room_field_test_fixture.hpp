#pragma once

#include "combat/monster_pool.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_monster_field.hpp"
#include "combat/room_spatial_grid.hpp"
#include "dungeon/room_monster_plan_builder.hpp"

#include <cstddef>
#include <cstdint>

namespace arpg::test::room_field_fixture {

[[nodiscard]] inline combat::Vec3 cell_center(
    std::size_t column, std::size_t row) noexcept {
    return {
        combat::room_bounds::min_x
            + (static_cast<float>(column) + 0.5F)
                * combat::room_spatial::cell_width,
        combat::room_bounds::min_y
            + (static_cast<float>(row) + 0.5F)
                * combat::room_spatial::cell_depth,
        0.0F,
    };
}

[[nodiscard]] inline combat::RoomStreamingRegion streaming_region_for_cell(
    std::size_t column, std::size_t row) noexcept {
    return combat::make_room_streaming_region(cell_center(column, row));
}

inline void populate_plan(
    combat::RoomMonsterPlan& plan,
    std::uint16_t count) noexcept {
    plan.monster_count = count;
    plan.generator_version = 1U;
    plan.blueprint_hash = 0xA11CE1125ULL;
    plan.threat_total = count;

    const std::uint16_t base = static_cast<std::uint16_t>(
        count / combat::kRoomMonsterCellCount);
    const std::uint16_t extra = static_cast<std::uint16_t>(
        count % combat::kRoomMonsterCellCount);
    std::uint16_t ordinal = 0U;
    for (std::size_t cell = 0U; cell < combat::kRoomMonsterCellCount; ++cell) {
        const std::uint8_t cell_count = static_cast<std::uint8_t>(
            base + (cell < extra ? 1U : 0U));
        plan.cell_offsets[cell] = ordinal;
        plan.cell_counts[cell] = cell_count;
        const std::size_t column = cell % combat::room_spatial::columns;
        const std::size_t row = cell / combat::room_spatial::columns;
        const combat::Vec3 center = cell_center(column, row);
        for (std::uint8_t local = 0U; local < cell_count; ++local) {
            const float offset = (static_cast<float>(local) - 1.0F) * 2.0F;
            plan.monsters[ordinal] = {
                static_cast<combat::MonsterId>(ordinal
                    % static_cast<std::uint16_t>(combat::MonsterId::count)),
                {},
                {center.x + offset, center.y, 0.0F},
                ordinal,
                static_cast<std::uint16_t>(cell),
                1U,
            };
            ++ordinal;
        }
    }
    plan.cell_offsets[combat::kRoomMonsterCellCount] = ordinal;
}

inline void populate_overfull_single_cell_plan(
    combat::RoomMonsterPlan& plan,
    std::uint16_t count,
    std::uint16_t home_cell) noexcept {
    plan.monster_count = count;
    plan.generator_version = 1U;
    plan.blueprint_hash = 0xBAD00129ULL;
    plan.threat_total = count;
    const combat::Vec3 center = cell_center(
        home_cell % combat::room_spatial::columns,
        home_cell / combat::room_spatial::columns);
    for (std::size_t cell = 0U; cell < combat::kRoomMonsterCellCount; ++cell) {
        plan.cell_offsets[cell] = cell <= home_cell ? 0U : count;
        plan.cell_counts[cell] = cell == home_cell
            ? static_cast<std::uint8_t>(count) : 0U;
    }
    plan.cell_offsets[combat::kRoomMonsterCellCount] = count;
    for (std::uint16_t ordinal = 0U; ordinal < count; ++ordinal) {
        plan.monsters[ordinal] = {
            combat::MonsterId::chaos_chaser,
            {},
            center,
            ordinal,
            home_cell,
            1U,
        };
    }
}

[[nodiscard]] inline combat::RoomMonsterFieldFault seal_test_plan(
    combat::RoomMonsterField& field,
    std::uint16_t count) noexcept {
    populate_plan(field.plan_storage_for_construction(), count);
    dungeon::RoomMonsterPlanBuildResult result{};
    result.density.total_count = count;
    return field.seal_plan(result);
}

}  // namespace arpg::test::room_field_fixture
