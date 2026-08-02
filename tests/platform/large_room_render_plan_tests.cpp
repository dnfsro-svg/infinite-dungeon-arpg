#include "test_framework.hpp"

#include "allocation_probe.hpp"

#include "combat/room_bounds.hpp"
#include "combat/room_monster_field.hpp"
#include "combat/room_spatial_grid.hpp"
#include "combat_renderer.hpp"
#include "combat_view_math.hpp"
#include "core/gameplay_limits.hpp"
#include "dungeon/dungeon_render_snapshot.hpp"
#include "dungeon/room_drop_spatial_index.hpp"
#include "environment_prop_layout.hpp"
#include "material_manifest.hpp"
#include "material_residency.hpp"
#include "room_background_render_plan.hpp"

#include "../combat/room_field_test_fixture.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace {

std::unique_ptr<arpg::dungeon::DungeonRenderSnapshot>
make_saturated_render_snapshot() {
    auto world = std::unique_ptr<arpg::dungeon::DungeonRenderSnapshot>{
        new (std::nothrow) arpg::dungeon::DungeonRenderSnapshot{}};
    if (world == nullptr) return {};
    world->ecology = arpg::dungeon::DungeonElement::fire;
    world->has_active_room = true;
    world->has_combat = true;
    world->environment.count = static_cast<std::uint16_t>(
        world->environment.records.size());
    for (std::size_t index = 0U;
            index < world->environment.records.size(); ++index) {
        auto& record = world->environment.records[index];
        record.ordinal = static_cast<std::uint16_t>(index);
        record.home_cell = static_cast<std::uint16_t>(index);
        record.prop = arpg::combat::RoomPropKind::torch;
        record.anchor = {
            arpg::combat::room_bounds::min_x
                + static_cast<float>(index % 16U) * 8.0F,
            arpg::combat::room_bounds::min_y
                + static_cast<float>(index / 16U) * 6.0F,
            0.0F};
        record.scale_bp = 10000U;
    }

    world->equipment_count = 1U;
    auto& equipment = world->equipment[0U];
    equipment.ordinal = 7U;
    equipment.position = {-48.0F, -24.0F, 0.0F};
    equipment.item_id = 1007U;
    equipment.base_id = 1U;
    equipment.item_level = 24U;
    equipment.slot = arpg::items::ItemSlot::weapon;
    equipment.rarity = arpg::items::ItemRarity::rare;

    world->material_count = 1U;
    world->materials[0U] = {
        12U,
        arpg::dungeon::GroundMaterialSource::monster_common,
        {0.0F, 48.0F, 0.0F},
        arpg::items::MaterialId::chaos,
    };

    world->health_potion_count = 1U;
    world->health_potions[0U] = {
        9U, 19U, {48.0F, -24.0F, 0.0F},
    };
    return world;
}

std::unique_ptr<arpg::dungeon::DungeonSnapshot>
make_saturated_dungeon_snapshot() {
    auto snapshot = std::unique_ptr<arpg::dungeon::DungeonSnapshot>{
        new (std::nothrow) arpg::dungeon::DungeonSnapshot{}};
    if (snapshot == nullptr) return {};
    snapshot->has_active_room = true;
    snapshot->ecology = arpg::dungeon::DungeonElement::fire;
    snapshot->combat.emplace();
    snapshot->combat->monster_count = static_cast<std::uint16_t>(
        snapshot->combat->monsters.size());
    for (std::size_t index = 0U;
            index < snapshot->combat->monsters.size(); ++index) {
        snapshot->combat->monsters[index].active = true;
        snapshot->combat->monsters[index].id =
            index % 2U == 0U ? arpg::combat::MonsterId::fire_bomber
                : arpg::combat::MonsterId::fire_charger;
    }
    snapshot->combat->projectile_count =
        snapshot->combat->projectiles.size();
    snapshot->combat->hazard_count = snapshot->combat->hazards.size();
    return snapshot;
}

arpg::test::Failure large_room_render_preparation_is_bounded() noexcept {
    static_assert(arpg::limits::kRoomMonsterCapacity == 1152U);
    static_assert(arpg::combat::kMonsterCapacity == 128U);
    static_assert(arpg::combat::kProjectileCapacity == 512U);
    static_assert(arpg::combat::kHazardCapacity == 128U);
    static_assert(arpg::dungeon::kVisibleEnvironmentCapacity == 128U);
    static_assert(arpg::platform::kRoomBackgroundWorldTileCapacity == 25U);
    static_assert(arpg::dungeon::kVisibleEquipmentDropCapacity == 192U);
    static_assert(arpg::dungeon::kVisibleMaterialDropCapacity == 400U);
    static_assert(arpg::dungeon::kVisibleHealthPotionCapacity == 192U);

    auto world = make_saturated_render_snapshot();
    auto snapshot = make_saturated_dungeon_snapshot();
    ARPG_REQUIRE(world != nullptr && snapshot != nullptr);
    world->combat = *snapshot->combat;
    const auto camera = arpg::platform::make_combat_camera_view(
        {0.0F, 0.0F, 0.0F}, 1920.0F, 1080.0F);

    auto background = arpg::platform::room_background_world_tile_plan(
        world->ecology, camera);
    auto props = arpg::platform::environment_prop_layout(*world);
    auto residency = arpg::platform::world_material_residency_request(
        *world, snapshot->skill_loadout);
    auto render_plan = arpg::platform::make_combat_render_plan(
        *snapshot, *world, false, 0.0F, camera, {},
        arpg::settings::LootFilterMode::show_all, 1920.0F, 1080.0F);
    ARPG_REQUIRE(background.valid && background.count != 0U);
    ARPG_REQUIRE(props.status
        == arpg::platform::EnvironmentPropLayoutStatus::ok);
    ARPG_REQUIRE(props.count == arpg::dungeon::kVisibleEnvironmentCapacity);
    ARPG_REQUIRE(world->equipment_count == 1U);
    ARPG_REQUIRE(world->material_count == 1U);
    ARPG_REQUIRE(world->health_potion_count == 1U);
    ARPG_REQUIRE(render_plan.ground_loot.count == 1U);
    ARPG_REQUIRE(render_plan.ground_loot.labels[0U].ordinal == 7U);
    ARPG_REQUIRE(render_plan.material_loot.count == 2U);
    ARPG_REQUIRE(render_plan.material_loot.labels[0U].kind
        == arpg::platform::SecondaryLootKind::material);
    ARPG_REQUIRE(render_plan.material_loot.labels[1U].kind
        == arpg::platform::SecondaryLootKind::health_potion);
    ARPG_REQUIRE(render_plan.stage_count == render_plan.stages.size());
    ARPG_REQUIRE(arpg::platform::material_residency_bytes(
        arpg::platform::default_material_manifest(), residency) > 0U);

    const std::uint64_t before = arpg::test::allocation_count();
    std::size_t projected_count{};
    for (std::size_t pass = 0U; pass < 32U; ++pass) {
        const auto pass_camera = arpg::platform::make_combat_camera_view(
            {static_cast<float>(pass) - 16.0F, 0.0F, 0.0F},
            1920.0F, 1080.0F);
        const auto query = arpg::platform::make_world_view_query(
            pass_camera, 1920.0F, 1080.0F, pass + 1U);
        background = arpg::platform::room_background_world_tile_plan(
            world->ecology, pass_camera);
        props = arpg::platform::environment_prop_layout(*world);
        residency = arpg::platform::world_material_residency_request(
            *world, snapshot->skill_loadout);
        render_plan = arpg::platform::make_combat_render_plan(
            *snapshot, *world, false, 0.0F, pass_camera, {},
            arpg::settings::LootFilterMode::show_all, 1920.0F, 1080.0F);
        ARPG_REQUIRE(query.world_bounds.minimum.x
            >= arpg::combat::room_bounds::min_x);
        ARPG_REQUIRE(query.world_bounds.maximum.x
            <= arpg::combat::room_bounds::max_x);
        ARPG_REQUIRE(background.valid
            && background.count <= background.tiles.size());
        ARPG_REQUIRE(props.count <= props.props.size());
        ARPG_REQUIRE(render_plan.ground_loot.count == 1U);
        ARPG_REQUIRE(render_plan.ground_loot.labels[0U].ordinal == 7U);
        ARPG_REQUIRE(render_plan.material_loot.count == 2U);
        ARPG_REQUIRE(render_plan.material_loot.labels[0U].kind
            == arpg::platform::SecondaryLootKind::material);
        ARPG_REQUIRE(render_plan.material_loot.labels[0U].ordinal == 12U);
        ARPG_REQUIRE(render_plan.material_loot.labels[1U].kind
            == arpg::platform::SecondaryLootKind::health_potion);
        ARPG_REQUIRE(render_plan.material_loot.labels[1U].ordinal == 19U);
        ARPG_REQUIRE(render_plan.stage_count == render_plan.stages.size());
        for (std::size_t index = 0U; index < background.count; ++index) {
            projected_count += arpg::platform::project_room_background_world_tile(
                background.tiles[index], pass_camera, 1920.0F, 1080.0F).valid;
        }
        for (std::size_t index = 0U; index < props.count; ++index) {
            const auto projected = arpg::platform::project_environment_prop(
                props.props[index], pass_camera, 1920.0F, 1080.0F);
            projected_count += projected.scale > 0.0F;
        }
        projected_count += residency.atlases != 0U;
        projected_count += render_plan.stage_count;
    }
    const std::uint64_t after = arpg::test::allocation_count();
    ARPG_REQUIRE(projected_count != 0U);
    ARPG_REQUIRE(after == before);
    return {};
}

arpg::test::Failure camera_motion_keeps_background_and_props_world_anchored()
    noexcept {
    auto world = make_saturated_render_snapshot();
    ARPG_REQUIRE(world != nullptr);
    constexpr std::array<arpg::combat::Vec3, 4U> kRequestedCenters{{
        {arpg::combat::room_bounds::min_x,
            arpg::combat::room_bounds::min_y, 0.0F},
        {arpg::combat::room_bounds::max_x,
            arpg::combat::room_bounds::min_y, 0.0F},
        {arpg::combat::room_bounds::min_x,
            arpg::combat::room_bounds::max_y, 0.0F},
        {arpg::combat::room_bounds::max_x,
            arpg::combat::room_bounds::max_y, 0.0F},
    }};
    std::array<arpg::platform::CombatCameraView, 4U> cameras{};
    std::array<arpg::platform::RoomBackgroundWorldTilePlan, 4U> backgrounds{};
    for (std::size_t index = 0U; index < cameras.size(); ++index) {
        cameras[index] = arpg::platform::make_combat_camera_view(
            kRequestedCenters[index], 1920.0F, 1080.0F);
        backgrounds[index] = arpg::platform::room_background_world_tile_plan(
            world->ecology, cameras[index]);
        ARPG_REQUIRE(backgrounds[index].valid);
        ARPG_REQUIRE(backgrounds[index].count > 0U);
        ARPG_REQUIRE(backgrounds[index].count
            <= arpg::platform::kRoomBackgroundWorldTileCapacity);
        ARPG_REQUIRE(cameras[index].center.x
            - cameras[index].visible_width * 0.5F
                >= arpg::combat::room_bounds::min_x);
        ARPG_REQUIRE(cameras[index].center.x
            + cameras[index].visible_width * 0.5F
                <= arpg::combat::room_bounds::max_x);
        ARPG_REQUIRE(cameras[index].center.y
            - cameras[index].visible_depth * 0.5F
                >= arpg::combat::room_bounds::min_y);
        ARPG_REQUIRE(cameras[index].center.y
            + cameras[index].visible_depth * 0.5F
                <= arpg::combat::room_bounds::max_y);
    }
    ARPG_REQUIRE(backgrounds[0U].tiles[0U].column
        != backgrounds[1U].tiles[0U].column);
    ARPG_REQUIRE(backgrounds[0U].tiles[0U].row
        != backgrounds[2U].tiles[0U].row);

    world->environment.count = 1U;
    const arpg::combat::Vec3 anchor = world->environment.records[0U].anchor;
    const auto props = arpg::platform::environment_prop_layout(*world);
    ARPG_REQUIRE(props.count == 1U);
    const auto left = arpg::platform::project_environment_prop(
        props.props[0U], cameras[0U], 1920.0F, 1080.0F);
    const auto right = arpg::platform::project_environment_prop(
        props.props[0U], cameras[1U], 1920.0F, 1080.0F);
    ARPG_REQUIRE(left.foot_position.x != right.foot_position.x);
    ARPG_REQUIRE(props.props[0U].world_foot_position.x == anchor.x);
    ARPG_REQUIRE(props.props[0U].world_foot_position.y == anchor.y);
    return {};
}

arpg::test::Failure early_unlock_and_full_clear_have_distinct_geometry()
    noexcept {
    const auto early = arpg::platform::door_render_decision(
        arpg::platform::DoorVisualMode::open,
        arpg::dungeon::ExitDirection::right, false);
    const auto cleared = arpg::platform::door_render_decision(
        arpg::platform::DoorVisualMode::open,
        arpg::dungeon::ExitDirection::right, true);
    ARPG_REQUIRE(!early.draw_lock_marker && !cleared.draw_lock_marker);
    ARPG_REQUIRE(!early.draw_full_clear_decoration);
    ARPG_REQUIRE(cleared.draw_full_clear_decoration);
    ARPG_REQUIRE(early.sprite == cleared.sprite);

    const auto left_camera = arpg::platform::make_combat_camera_view(
        {-40.0F, 0.0F, 0.0F}, 1920.0F, 1080.0F);
    const auto right_camera = arpg::platform::make_combat_camera_view(
        {40.0F, 0.0F, 0.0F}, 1920.0F, 1080.0F);
    const auto left_hole = arpg::platform::project_hole_geometry(
        arpg::platform::kHoleCenter, left_camera, 1920.0F, 1080.0F);
    const auto right_hole = arpg::platform::project_hole_geometry(
        arpg::platform::kHoleCenter, right_camera, 1920.0F, 1080.0F);
    ARPG_REQUIRE(left_hole.radius_x > 0.0F && left_hole.radius_y > 0.0F);
    ARPG_REQUIRE(right_hole.radius_x > 0.0F && right_hole.radius_y > 0.0F);
    ARPG_REQUIRE(left_hole.center.x != right_hole.center.x);
    return {};
}

arpg::test::Failure maximum_population_queries_only_streaming_cells()
    noexcept {
    namespace fixture = arpg::test::room_field_fixture;
    static_assert(arpg::dungeon::kEnvironmentQueryCandidateCapacity == 105U);
    static_assert(arpg::dungeon::kRoomDropQueryCandidateCapacity == 331U);
    static_assert(arpg::combat::room_spatial::maximum_streaming_monsters
        == 105U);
    auto field = std::unique_ptr<arpg::combat::RoomMonsterField>{
        new (std::nothrow) arpg::combat::RoomMonsterField{}};
    ARPG_REQUIRE(field != nullptr);
    ARPG_REQUIRE(fixture::seal_test_plan(*field, 1125U)
        == arpg::combat::RoomMonsterFieldFault::none);
    ARPG_REQUIRE(field->total_count() == 1125U);

    auto environment = std::unique_ptr<
        arpg::combat::RoomEnvironmentBlueprint>{new (std::nothrow)
            arpg::combat::RoomEnvironmentBlueprint{}};
    auto drops = std::unique_ptr<arpg::dungeon::RoomDropSpatialIndex>{
        new (std::nothrow) arpg::dungeon::RoomDropSpatialIndex{}};
    ARPG_REQUIRE(environment != nullptr && drops != nullptr);
    environment->record_count = static_cast<std::uint16_t>(
        arpg::combat::kRoomEnvironmentRecordCapacity);
    environment->generator_version = 1U;
    environment->blueprint_hash = 0xA11CE400ULL;
    for (std::size_t cell{};
            cell < arpg::combat::kRoomEnvironmentCellCount; ++cell) {
        const std::size_t begin = cell * 3U;
        environment->cell_offsets[cell] =
            static_cast<std::uint16_t>(begin);
        environment->cell_counts[cell] = 3U;
        const auto center = fixture::cell_center(
            cell % arpg::combat::room_spatial::columns,
            cell / arpg::combat::room_spatial::columns);
        for (std::size_t local{}; local < 3U; ++local) {
            const std::size_t ordinal = begin + local;
            auto& record = environment->records[ordinal];
            record.ordinal = static_cast<std::uint16_t>(ordinal);
            record.home_cell = static_cast<std::uint16_t>(cell);
            record.prop = arpg::combat::RoomPropKind::torch;
            record.anchor = {center.x + static_cast<float>(local) - 1.0F,
                center.y, center.z};
            record.scale_bp = 10000U;
        }
    }
    environment->cell_offsets[arpg::combat::kRoomEnvironmentCellCount] =
        environment->record_count;
    ARPG_REQUIRE(drops->reset(field->plan()));
    for (std::size_t cell{};
            cell < arpg::combat::kRoomMonsterCellCount; ++cell) {
        const std::uint16_t spawn = field->plan().cell_offsets[cell];
        ARPG_REQUIRE(drops->insert_monster_drop(
            arpg::dungeon::RoomDropKind::material,
            static_cast<std::uint16_t>(cell), spawn,
            fixture::cell_center(
                cell % arpg::combat::room_spatial::columns,
                cell / arpg::combat::room_spatial::columns)));
    }

    constexpr std::size_t kFrameCount = 10'000U;
    std::size_t maximum_cells_examined{};
    std::size_t maximum_monsters_examined{};
    std::size_t maximum_environment_candidates{};
    std::size_t maximum_drop_candidates{};
    std::uint64_t checksum{};
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t frame{}; frame < kFrameCount; ++frame) {
        const std::size_t column = (frame * 37U)
            % arpg::combat::room_spatial::columns;
        const std::size_t row = (frame * 53U)
            % arpg::combat::room_spatial::rows;
        const auto region = fixture::streaming_region_for_cell(column, row);
        const auto camera = arpg::platform::make_combat_camera_view(
            fixture::cell_center(column, row), 1920.0F, 1080.0F);
        const auto query = arpg::platform::make_world_view_query(
            camera, 1920.0F, 1080.0F, frame + 1U);
        const std::size_t cells_examined =
            static_cast<std::size_t>(region.column_count)
                * region.row_count;
        std::size_t monsters_examined{};
        for (std::size_t local_row = region.first_row;
                local_row < static_cast<std::size_t>(region.first_row)
                    + region.row_count; ++local_row) {
            for (std::size_t local_column = region.first_column;
                    local_column
                        < static_cast<std::size_t>(region.first_column)
                            + region.column_count; ++local_column) {
                const std::size_t cell = local_row
                    * arpg::combat::room_spatial::columns + local_column;
                monsters_examined += field->plan().cell_counts[cell];
            }
        }
        const auto residents = field->required_residents(region);
        ARPG_REQUIRE(residents.fault
            == arpg::combat::RoomMonsterFieldFault::none);
        ARPG_REQUIRE(residents.count == monsters_examined);
        ARPG_REQUIRE(cells_examined <= 35U);
        ARPG_REQUIRE(monsters_examined
            <= arpg::combat::room_spatial::maximum_streaming_monsters);
        arpg::dungeon::VisibleEnvironmentSet visible_environment{};
        const auto environment_result =
            arpg::dungeon::query_visible_environment(
                *environment, query.world_bounds, visible_environment);
        ARPG_REQUIRE(environment_result.status
            == arpg::dungeon::VisibleEnvironmentQueryStatus::ok);
        ARPG_REQUIRE(visible_environment.candidates_examined
            <= arpg::dungeon::kEnvironmentQueryCandidateCapacity);
        arpg::dungeon::RoomDropCandidateSet visible_drops{};
        drops->write_candidates(query.world_bounds, visible_drops);
        ARPG_REQUIRE(visible_drops.fault
            == arpg::dungeon::RoomDropIndexFault::none);
        ARPG_REQUIRE(visible_drops.candidates_examined
            <= arpg::dungeon::kRoomDropQueryCandidateCapacity);
        maximum_cells_examined = (std::max)(
            maximum_cells_examined, cells_examined);
        maximum_monsters_examined = (std::max)(
            maximum_monsters_examined, monsters_examined);
        maximum_environment_candidates = (std::max)(
            maximum_environment_candidates,
            static_cast<std::size_t>(
                visible_environment.candidates_examined));
        maximum_drop_candidates = (std::max)(
            maximum_drop_candidates,
            static_cast<std::size_t>(visible_drops.candidates_examined));
        checksum += static_cast<std::uint64_t>(residents.count)
            * (frame + 1U)
            + visible_environment.candidates_examined
            + visible_drops.candidates_examined;
    }
    const std::uint64_t after = arpg::test::allocation_count();
    ARPG_REQUIRE(after == before);
    ARPG_REQUIRE(maximum_cells_examined == 35U);
    ARPG_REQUIRE(maximum_monsters_examined == 105U);
    ARPG_REQUIRE(maximum_environment_candidates > 0U);
    ARPG_REQUIRE(maximum_environment_candidates
        <= arpg::dungeon::kEnvironmentQueryCandidateCapacity);
    ARPG_REQUIRE(maximum_drop_candidates > 0U);
    ARPG_REQUIRE(maximum_drop_candidates <= 35U);
    ARPG_REQUIRE(checksum != 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"large room render preparation is bounded",
        &large_room_render_preparation_is_bounded},
    {"camera motion keeps background and props world anchored",
        &camera_motion_keeps_background_and_props_world_anchored},
    {"early unlock and full clear keep distinct geometry",
        &early_unlock_and_full_clear_have_distinct_geometry},
    {"1125 population only queries streaming cells",
        &maximum_population_queries_only_streaming_cells},
};

}  // namespace

arpg::test::TestSuite large_room_render_plan_suite() noexcept {
    return arpg::test::make_suite("large_room_render_plan", kCases);
}
