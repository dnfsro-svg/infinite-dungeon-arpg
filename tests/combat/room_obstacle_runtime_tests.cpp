#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "combat/room_environment_plan.hpp"
#include "combat/monster_pool.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_obstacle_runtime.hpp"
#include "combat/room_spatial_grid.hpp"
#include "modifiers/effect_set.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace arpg::combat;

struct ObstacleFixture final {
    std::array<RoomEnvironmentRecord, 6U> records{};
    std::array<std::uint16_t, kRoomEnvironmentCellCount + 1U> offsets{};
    std::array<std::uint8_t, kRoomEnvironmentCellCount> counts{};

    ObstacleFixture() noexcept {
        records[0] = make_record(0U, 0U, RoomPropKind::crate,
            RoomObstacleKind::breakable, 10U);
        records[1] = make_record(17U, 1U, RoomPropKind::crate,
            RoomObstacleKind::breakable, 20U);
        records[2] = make_record(25U, 2U, RoomPropKind::banner,
            RoomObstacleKind::solid, 0U);
        records[3] = make_record(600U, 3U, RoomPropKind::crate,
            RoomObstacleKind::breakable, 30U);
        records[4] = make_record(1199U, 4U, RoomPropKind::crate,
            RoomObstacleKind::breakable, 40U);
        records[5] = make_record(777U, 5U, RoomPropKind::bone_pile,
            RoomObstacleKind::none, 0U);
        for (std::size_t cell = 0U; cell < counts.size(); ++cell) {
            offsets[cell] = cell < records.size()
                ? static_cast<std::uint16_t>(cell)
                : static_cast<std::uint16_t>(records.size());
            counts[cell] = cell < records.size() ? 1U : 0U;
        }
        offsets.back() = static_cast<std::uint16_t>(records.size());
    }

    [[nodiscard]] static RoomEnvironmentRecord make_record(
        std::uint16_t ordinal,
        std::uint16_t home_cell,
        RoomPropKind prop,
        RoomObstacleKind kind,
        std::uint16_t hp) noexcept {
        RoomEnvironmentRecord record{};
        record.ordinal = ordinal;
        record.home_cell = home_cell;
        record.prop = prop;
        record.anchor = {static_cast<float>(home_cell), 0.0F, 0.0F};
        record.obstacle = {{{-1.0F, -1.0F, 0.0F},
            {1.0F, 1.0F, 2.0F}}, kind, hp};
        return record;
    }

    [[nodiscard]] RoomObstaclePlanView view() const noexcept {
        return {records.data(), offsets.data(), counts.data(),
            static_cast<std::uint16_t>(records.size())};
    }
};

struct FullObstacleFixture final {
    std::array<RoomEnvironmentRecord,
        kRoomEnvironmentRecordCapacity> records{};
    std::array<std::uint16_t,
        kRoomEnvironmentCellCount + 1U> offsets{};
    std::array<std::uint8_t,
        kRoomEnvironmentCellCount> counts{};

    FullObstacleFixture() noexcept {
        for (std::size_t cell = 0U; cell < kRoomEnvironmentCellCount; ++cell) {
            const std::size_t begin = cell * 3U;
            offsets[cell] = static_cast<std::uint16_t>(begin);
            counts[cell] = 3U;
            const std::size_t column = cell % room_spatial::columns;
            const std::size_t row = cell / room_spatial::columns;
            const Vec3 center{
                room_bounds::min_x
                    + (static_cast<float>(column) + 0.5F)
                        * room_spatial::cell_width,
                room_bounds::min_y
                    + (static_cast<float>(row) + 0.5F)
                        * room_spatial::cell_depth,
                0.0F,
            };
            records[begin] = make_record(
                static_cast<std::uint16_t>(begin),
                static_cast<std::uint16_t>(cell), center,
                RoomPropKind::crate, RoomObstacleKind::breakable, 10U);
            records[begin + 1U] = make_record(
                static_cast<std::uint16_t>(begin + 1U),
                static_cast<std::uint16_t>(cell), center,
                RoomPropKind::torch, RoomObstacleKind::none, 0U);
            records[begin + 2U] = make_record(
                static_cast<std::uint16_t>(begin + 2U),
                static_cast<std::uint16_t>(cell), center,
                RoomPropKind::banner, RoomObstacleKind::none, 0U);
        }
        offsets.back() = static_cast<std::uint16_t>(records.size());
    }

    [[nodiscard]] static RoomEnvironmentRecord make_record(
        std::uint16_t ordinal,
        std::uint16_t home_cell,
        Vec3 center,
        RoomPropKind prop,
        RoomObstacleKind kind,
        std::uint16_t hp) noexcept {
        RoomEnvironmentRecord record{};
        record.ordinal = ordinal;
        record.home_cell = home_cell;
        record.prop = prop;
        record.anchor = center;
        if (kind != RoomObstacleKind::none) {
            record.obstacle = {
                {{center.x - 0.5F, center.y - 0.5F, 0.0F},
                    {center.x + 0.5F, center.y + 0.5F, 2.0F}},
                kind,
                hp,
            };
        }
        return record;
    }

    [[nodiscard]] RoomObstaclePlanView view() const noexcept {
        return {records.data(), offsets.data(), counts.data(),
            static_cast<std::uint16_t>(records.size())};
    }
};

arpg::test::Failure malformed_empty_view_is_canonical_and_safe() noexcept {
    std::array<RoomEnvironmentRecord, 1U> sentinel_records{};
    RoomObstacleRuntime runtime{};
    const RoomObstaclePlanView malformed{
        sentinel_records.data(), nullptr, nullptr, 0U};
    ARPG_REQUIRE(runtime.initialize(malformed));
    const RoomObstaclePlanView normalized = runtime.plan_view();
    ARPG_REQUIRE(normalized.records == nullptr);
    ARPG_REQUIRE(normalized.cell_offsets == nullptr);
    ARPG_REQUIRE(normalized.cell_counts == nullptr);
    ARPG_REQUIRE(normalized.record_count == 0U);
    ARPG_REQUIRE(runtime.obstacle_count() == 0U);
    ARPG_REQUIRE(!runtime.blocks_player({}, {}));
    ARPG_REQUIRE(runtime.damage_overlapping({}, 1U) == 0U);
    return {};
}

arpg::test::Failure full_blueprint_shape_is_fixed_and_allocation_free()
    noexcept {
    const FullObstacleFixture fixture{};
    RoomObstacleRuntime runtime{};
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    ARPG_REQUIRE(runtime.initialize(fixture.view()));
    ARPG_REQUIRE(runtime.obstacle_count() == kRoomEnvironmentCellCount);
    for (std::size_t cell = 0U; cell < kRoomEnvironmentCellCount; ++cell) {
        const std::uint16_t ordinal = static_cast<std::uint16_t>(cell * 3U);
        const RoomObstacleState* obstacle = runtime.state(ordinal);
        ARPG_REQUIRE(obstacle != nullptr);
        ARPG_REQUIRE(obstacle->home_cell == cell);
        ARPG_REQUIRE(obstacle->intact);
        ARPG_REQUIRE(runtime.state(static_cast<std::uint16_t>(ordinal + 1U))
            == nullptr);
        ARPG_REQUIRE(runtime.state(static_cast<std::uint16_t>(ordinal + 2U))
            == nullptr);
        const Vec3 center{
            (obstacle->bounds.minimum.x + obstacle->bounds.maximum.x) * 0.5F,
            (obstacle->bounds.minimum.y + obstacle->bounds.maximum.y) * 0.5F,
            0.0F,
        };
        ARPG_REQUIRE(runtime.blocks_player(
            {center.x - 2.0F, center.y, 0.0F}, center));
        ARPG_REQUIRE(runtime.damage_overlapping(
            obstacle->bounds, 1000U + cell) == 1U);
        ARPG_REQUIRE(!runtime.state(ordinal)->intact);
        ARPG_REQUIRE(runtime.state(ordinal)->broken_tick == 1000U + cell);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    return {};
}

arpg::test::Failure empty_and_full_obstacle_plans_are_bounded() noexcept {
    const arpg::test::Failure empty =
        malformed_empty_view_is_canonical_and_safe();
    if (empty.expression != nullptr) return empty;
    return full_blueprint_shape_is_fixed_and_allocation_free();
}

arpg::test::Failure obstacle_damage_and_effects_ignore_monster_slot_churn()
    noexcept {
    const ObstacleFixture fixture{};
    RoomObstacleRuntime runtime{};
    ARPG_REQUIRE(runtime.initialize(fixture.view()));
    auto* effects = runtime.effects(17U);
    ARPG_REQUIRE(effects != nullptr);
    arpg::modifiers::EffectDefinition effect{};
    effect.id = 0x0B57AC1EU;
    effect.duration_ticks = 240;
    effect.refresh_rule = arpg::modifiers::RefreshRule::refresh_duration;
    ARPG_REQUIRE(effects->apply(effect)
        == arpg::modifiers::ApplyResult::applied);
    ARPG_REQUIRE(runtime.apply_damage(17U, 7U, 77U));
    const auto before = *runtime.state(17U);

    MonsterPool pool{};
    std::array<MonsterHandle, kMonsterCapacity> handles{};
    for (std::size_t slot = 0U; slot < handles.size(); ++slot) {
        const auto handle = pool.spawn(MonsterId::chaos_chaser,
            {static_cast<float>(slot), 0.0F, 0.0F});
        ARPG_REQUIRE(handle.has_value());
        handles[slot] = *handle;
    }
    for (const MonsterHandle handle : handles) {
        ARPG_REQUIRE(pool.destroy(handle));
    }
    for (std::size_t slot = 0U; slot < handles.size(); ++slot) {
        ARPG_REQUIRE(pool.spawn(MonsterId::fire_charger,
            {static_cast<float>(slot), 1.0F, 0.0F}).has_value());
    }

    const auto* after = runtime.state(17U);
    ARPG_REQUIRE(after != nullptr);
    ARPG_REQUIRE(after->intact == before.intact);
    ARPG_REQUIRE(after->hp == before.hp);
    ARPG_REQUIRE(after->broken_tick == before.broken_tick);
    ARPG_REQUIRE(runtime.effects(17U)->remaining_ticks(effect.id) == 240);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"empty and full obstacle plans stay bounded",
        &empty_and_full_obstacle_plans_are_bounded},
    {"obstacle state ignores monster slot churn",
        &obstacle_damage_and_effects_ignore_monster_slot_churn},
};

}  // namespace

arpg::test::TestSuite room_obstacle_runtime_suite() noexcept {
    return arpg::test::make_suite("room_obstacle_runtime", kCases);
}
