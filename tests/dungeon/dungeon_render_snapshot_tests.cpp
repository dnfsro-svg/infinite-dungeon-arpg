#include "test_framework.hpp"

#include "combat/room_bounds.hpp"
#include "combat/room_environment_plan.hpp"
#include "dungeon/dungeon_render_snapshot.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_environment.hpp"
#include "dungeon_test_support.hpp"

#include <memory>

namespace {

using arpg::combat::Aabb;
using arpg::combat::RoomEnvironmentBlueprint;
using arpg::dungeon::DungeonRenderSnapshot;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::VisibleEnvironmentSet;
using arpg::dungeon::WorldViewQuery;

arpg::test::Failure render_snapshot_is_bounded_and_overwrites_sentinels() noexcept {
    static_assert(arpg::dungeon::kVisibleEquipmentDropCapacity == 192U);
    static_assert(arpg::dungeon::kVisibleMaterialDropCapacity == 400U);
    static_assert(arpg::dungeon::kVisibleHealthPotionCapacity == 192U);
    static_assert(arpg::dungeon::kVisibleEnvironmentCapacity == 128U);

    auto session = std::make_unique<DungeonSession>();
    auto output = std::make_unique<DungeonRenderSnapshot>();
    output->equipment_count = 191U;
    output->material_count = 399U;
    output->health_potion_count = 191U;
    output->environment.count = 127U;
    output->equipment[0U].ordinal = 0xFFFFU;
    const WorldViewQuery query{
        {{-12.0F, -5.5F, -1.0F}, {12.0F, 5.5F, 32.0F}},
        1280,
        720,
        77U,
    };
    ARPG_REQUIRE(session->write_render_snapshot(query, *output));
    ARPG_REQUIRE(output->query.camera_version == 77U);
    ARPG_REQUIRE(output->query.screen_width == 1280);
    ARPG_REQUIRE(output->query.screen_height == 720);
    ARPG_REQUIRE(output->equipment_count <= output->equipment.size());
    ARPG_REQUIRE(output->material_count <= output->materials.size());
    ARPG_REQUIRE(output->health_potion_count <= output->health_potions.size());
    ARPG_REQUIRE(output->environment.count <= output->environment.records.size());
    ARPG_REQUIRE(output->drop_candidates_examined <= 331U);
    ARPG_REQUIRE(output->environment.candidates_examined <= 105U);

    const RoomEnvironmentBlueprint* const blueprint =
        arpg::test::room_environment_blueprint(*session);
    ARPG_REQUIRE(blueprint != nullptr);
    const arpg::combat::RoomEnvironmentRecord* breakable = nullptr;
    for (std::uint16_t index = 0U;
            index < blueprint->record_count; ++index) {
        if (blueprint->records[index].obstacle.kind
                == arpg::combat::RoomObstacleKind::breakable) {
            breakable = &blueprint->records[index];
            break;
        }
    }
    ARPG_REQUIRE(breakable != nullptr);
    const WorldViewQuery obstacle_query{
        {{breakable->anchor.x - 1.0F, breakable->anchor.y - 1.0F, -1.0F},
            {breakable->anchor.x + 1.0F, breakable->anchor.y + 1.0F, 32.0F}},
        1280, 720, 78U};
    ARPG_REQUIRE(session->write_render_snapshot(obstacle_query, *output));
    std::uint16_t visible_index = 0xFFFFU;
    for (std::uint16_t index = 0U;
            index < output->environment.count; ++index) {
        if (output->environment.records[index].ordinal
                == breakable->ordinal) {
            visible_index = index;
            break;
        }
    }
    ARPG_REQUIRE(visible_index != 0xFFFFU);
    ARPG_REQUIRE(output->environment_obstacles[visible_index].present);
    ARPG_REQUIRE(output->environment_obstacles[visible_index].intact);
    ARPG_REQUIRE(output->environment_obstacles[visible_index].ordinal
        == breakable->ordinal);
    ARPG_REQUIRE(output->environment_obstacles[visible_index].kind
        == arpg::combat::RoomObstacleKind::breakable);
    auto* world = arpg::test::mutable_combat_world(*session);
    ARPG_REQUIRE(world != nullptr && world->room_obstacles() != nullptr);
    auto* obstacle_runtime = const_cast<arpg::combat::RoomObstacleRuntime*>(
        world->room_obstacles());
    const auto* obstacle_state = obstacle_runtime->state(breakable->ordinal);
    ARPG_REQUIRE(obstacle_state != nullptr);
    ARPG_REQUIRE(obstacle_runtime->apply_damage(
        breakable->ordinal, obstacle_state->max_hp, 91U));
    ARPG_REQUIRE(session->write_render_snapshot(obstacle_query, *output));
    ARPG_REQUIRE(output->environment.records[visible_index].ordinal
        == breakable->ordinal);
    ARPG_REQUIRE(output->environment_obstacles[visible_index].present);
    ARPG_REQUIRE(!output->environment_obstacles[visible_index].intact);
    ARPG_REQUIRE(output->environment_obstacles[visible_index].hp == 0U);
    ARPG_REQUIRE(output->environment_obstacles[visible_index].broken_tick
        == 91U);
    return {};
}

arpg::test::Failure environment_query_is_stable_and_bounded() noexcept {
    auto blueprint = std::make_unique<RoomEnvironmentBlueprint>();
    std::uint16_t ordinal = 0U;
    for (std::uint16_t row = 7U; row <= 11U; ++row) {
        for (std::uint16_t column = 7U; column <= 13U; ++column) {
            const std::uint16_t cell = static_cast<std::uint16_t>(
                row * 20U + column);
            blueprint->cell_offsets[cell] = ordinal;
            blueprint->cell_counts[cell] = 3U;
            for (std::uint16_t local = 0U; local < 3U; ++local) {
                auto& record = blueprint->records[ordinal];
                record.ordinal = ordinal;
                record.home_cell = cell;
                record.anchor = {
                    arpg::combat::room_bounds::min_x
                        + (static_cast<float>(column) + 0.5F)
                            * arpg::combat::room_bounds::width / 20.0F,
                    arpg::combat::room_bounds::min_y
                        + (static_cast<float>(row) + 0.5F)
                            * arpg::combat::room_bounds::depth / 20.0F,
                    0.0F,
                };
                ++ordinal;
            }
        }
    }
    blueprint->record_count = ordinal;
    for (std::size_t cell = 1U; cell < blueprint->cell_counts.size(); ++cell) {
        if (blueprint->cell_offsets[cell] == 0U
                && blueprint->cell_counts[cell] == 0U) {
            blueprint->cell_offsets[cell] = blueprint->cell_offsets[cell - 1U]
                + blueprint->cell_counts[cell - 1U];
        }
    }
    blueprint->cell_offsets.back() = blueprint->record_count;

    VisibleEnvironmentSet first{};
    VisibleEnvironmentSet second{};
    first.count = 127U;
    first.candidates_examined = 999U;
    const Aabb bounds{{-15.0F, -9.0F, -1.0F}, {17.0F, 2.0F, 1.0F}};
    ARPG_REQUIRE(arpg::dungeon::write_visible_environment(
        *blueprint, bounds, first));
    ARPG_REQUIRE(arpg::dungeon::write_visible_environment(
        *blueprint, bounds, second));
    ARPG_REQUIRE(first.candidates_examined == 105U);
    ARPG_REQUIRE(first.count == second.count);
    for (std::size_t index = 0U; index < first.count; ++index) {
        ARPG_REQUIRE(first.records[index].ordinal
            == second.records[index].ordinal);
        if (index != 0U) {
            ARPG_REQUIRE(first.records[index - 1U].home_cell
                <= first.records[index].home_cell);
        }
    }

    auto edge_blueprint = std::make_unique<RoomEnvironmentBlueprint>();
    constexpr std::uint16_t kCenterCell = 210U;
    edge_blueprint->record_count = 1U;
    edge_blueprint->cell_offsets.fill(1U);
    edge_blueprint->cell_offsets[kCenterCell] = 0U;
    edge_blueprint->cell_counts[kCenterCell] = 1U;
    auto& edge = edge_blueprint->records[0U];
    edge.ordinal = 0U;
    edge.home_cell = kCenterCell;
    edge.anchor = {2.0F, 0.0F, 0.0F};
    edge.obstacle.kind = arpg::combat::RoomObstacleKind::solid;
    edge.obstacle.bounds = {{-0.5F, -0.5F, 0.0F}, {2.5F, 0.5F, 1.0F}};
    VisibleEnvironmentSet edge_result{};
    const Aabb edge_bounds{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    ARPG_REQUIRE(arpg::dungeon::write_visible_environment(
        *edge_blueprint, edge_bounds, edge_result));
    ARPG_REQUIRE(edge_result.count == 1U);
    ARPG_REQUIRE(edge_result.records[0U].ordinal == 0U);
    return {};
}

arpg::test::Failure door_abyss_markers_are_directional() noexcept {
    const WorldViewQuery query{
        {{-12.0F, -5.5F, -1.0F}, {12.0F, 5.5F, 32.0F}},
        1280, 720, 1U};
    auto render = std::make_unique<DungeonRenderSnapshot>();
    auto legacy = std::make_unique<arpg::dungeon::DungeonSnapshot>();
    bool found_mixed_preview = false;
    for (std::uint64_t room = 0U; room < 1024U; ++room) {
        auto session = std::make_unique<DungeonSession>(
            arpg::dungeon::DungeonSessionConfig{
                0xC001D00D12345678ULL, room});
        session->snapshot(*legacy);
        bool any = false;
        bool all = true;
        for (const bool abyss : legacy->abyss_doors) {
            any = any || abyss;
            all = all && abyss;
        }
        if (!any || all) continue;
        ARPG_REQUIRE(session->write_render_snapshot(query, *render));
        for (std::size_t index = 0U; index < render->doors.size(); ++index) {
            ARPG_REQUIRE(render->doors[index].abyss
                == legacy->abyss_doors[index]);
        }
        found_mixed_preview = true;
        break;
    }
    ARPG_REQUIRE(found_mixed_preview);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"render snapshot overwrites bounded output",
        &render_snapshot_is_bounded_and_overwrites_sentinels},
    {"environment query visits no more than 105 records",
        &environment_query_is_stable_and_bounded},
    {"abyss door markers preserve per-direction preview",
        &door_abyss_markers_are_directional},
};

}  // namespace

arpg::test::TestSuite dungeon_render_snapshot_suite() noexcept {
    return arpg::test::make_suite("dungeon_render_snapshot", kCases);
}
