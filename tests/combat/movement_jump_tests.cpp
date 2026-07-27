#include "test_framework.hpp"

#include "combat_test_support.hpp"
#include "combat/combat_world.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_spatial_grid.hpp"
#include "core/gameplay_limits.hpp"

#include "abyss/abyss_rules.hpp"

#include <cmath>
#include <cstdint>

namespace {

using namespace arpg::combat;

constexpr double kFloatTolerance = 1.0e-4;

arpg::test::Failure hundredfold_room_geometry_and_limits_are_exact() noexcept {
    static_assert(arpg::limits::kRoomMonsterCapacity == 1152U);
    static_assert(arpg::limits::kActiveMonsterCapacity == 128U);
    static_assert(arpg::limits::kProjectileCapacity == 512U);
    static_assert(arpg::limits::kHazardCapacity == 128U);
    static_assert(arpg::limits::kCombatEventCapacity == 512U);
    static_assert(arpg::limits::kDefeatLedgerCapacity == 128U);
    static_assert(arpg::limits::kRoomEquipmentClaimWords == 18U);
    static_assert(arpg::limits::kRoomSecondaryClaimWords == 37U);
    ARPG_REQUIRE(arpg::test::near(
        room_bounds::width, 162.48077393, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        room_bounds::depth, 162.48077393, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        room_bounds::width * room_bounds::depth, 26400.0, 0.01));
    ARPG_REQUIRE(arpg::test::near(room_bounds::min_x, room_bounds::min_y));
    ARPG_REQUIRE(arpg::test::near(room_bounds::max_x, room_bounds::max_y));
    return {};
}

arpg::test::Failure spatial_grid_bounds_streaming_regions() noexcept {
    static_assert(room_spatial::columns == 20U);
    static_assert(room_spatial::rows == 20U);
    static_assert(room_spatial::maximum_monsters_per_cell == 3U);
    static_assert(room_spatial::maximum_view_width == 32.0F);
    static_assert(room_spatial::maximum_view_depth == 11.0F);
    static_assert(room_spatial::maximum_streaming_columns == 7U);
    static_assert(room_spatial::maximum_streaming_rows == 5U);
    static_assert(room_spatial::maximum_streaming_monsters == 105U);
    ARPG_REQUIRE(arpg::test::near(
        room_spatial::cell_width, room_bounds::width / 20.0F,
        kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        room_spatial::cell_depth, room_bounds::depth / 20.0F,
        kFloatTolerance));

    const RoomStreamingRegion center = make_room_streaming_region({});
    ARPG_REQUIRE(center.first_column <= 7U);
    ARPG_REQUIRE(center.first_column + center.column_count >= 13U);
    ARPG_REQUIRE(center.first_row <= 8U);
    ARPG_REQUIRE(center.first_row + center.row_count >= 12U);
    ARPG_REQUIRE(center.column_count <= 7U);
    ARPG_REQUIRE(center.row_count <= 5U);
    ARPG_REQUIRE(center.first_column + center.column_count
        <= room_spatial::columns);
    ARPG_REQUIRE(center.first_row + center.row_count
        <= room_spatial::rows);

    const RoomStreamingRegion minimum = make_room_streaming_region(
        {room_bounds::min_x, room_bounds::min_y, 0.0F});
    ARPG_REQUIRE(minimum.first_column == 0U);
    ARPG_REQUIRE(minimum.first_row == 0U);
    ARPG_REQUIRE(minimum.column_count == 5U);
    ARPG_REQUIRE(minimum.row_count == 3U);

    const RoomStreamingRegion maximum = make_room_streaming_region(
        {room_bounds::max_x, room_bounds::max_y, 0.0F});
    ARPG_REQUIRE(maximum.first_column + maximum.column_count
        == room_spatial::columns);
    ARPG_REQUIRE(maximum.first_row + maximum.row_count
        == room_spatial::rows);
    ARPG_REQUIRE(maximum.first_column == 15U);
    ARPG_REQUIRE(maximum.first_row == 17U);
    ARPG_REQUIRE(maximum.column_count == 5U);
    ARPG_REQUIRE(maximum.row_count == 3U);
    return {};
}

arpg::test::Failure ground_motion_is_fixed_and_diagonal_is_normalized() noexcept {
    CombatWorld right_world;
    for (int tick = 0; tick < 60; ++tick) {
        right_world.tick(MovementInput{1, 0});
    }

    const CombatSnapshot right = right_world.snapshot();
    ARPG_REQUIRE(arpg::test::near(right.player.position.x, 5.4, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(right.player.position.y, 0.0, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(right.player.velocity.x, 5.4, kFloatTolerance));
    ARPG_REQUIRE(right.player.state == PlayerState::move);
    ARPG_REQUIRE(right.player.facing == Facing::right);

    CombatWorld diagonal_world;
    for (int tick = 0; tick < 30; ++tick) {
        diagonal_world.tick(MovementInput{1, 1});
    }

    const CombatSnapshot diagonal = diagonal_world.snapshot();
    constexpr double expected_axis = 2.7 * 0.7071067811865475;
    ARPG_REQUIRE(arpg::test::near(
        diagonal.player.position.x, expected_axis, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        diagonal.player.position.y, expected_axis, kFloatTolerance));
    const double distance = std::sqrt(
        static_cast<double>(diagonal.player.position.x) *
            diagonal.player.position.x +
        static_cast<double>(diagonal.player.position.y) *
            diagonal.player.position.y);
    ARPG_REQUIRE(arpg::test::near(distance, 2.7, kFloatTolerance));
    return {};
}

arpg::test::Failure room_clamps_facing_and_reset_are_stable() noexcept {
    CombatLabConfig config;
    config.player_spawn = Vec3{1.0F, -1.0F, 0.0F};
    config.dummy_spawns[0] = Vec3{2.0F, -1.0F, 0.0F};
    CombatWorld world{config};

    for (int tick = 0; tick < 2000; ++tick) {
        world.tick(MovementInput{1, 1});
    }
    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, room_bounds::max_x));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.y, room_bounds::max_y));

    for (int tick = 0; tick < 4000; ++tick) {
        world.tick(MovementInput{-1, -1});
    }
    world.tick(MovementInput{0, 1});
    snapshot = world.snapshot();
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, room_bounds::min_x));
    ARPG_REQUIRE(snapshot.player.position.y > room_bounds::min_y);
    ARPG_REQUIRE(snapshot.player.facing == Facing::left);

    for (int action = 0; action < 32; ++action) {
        ARPG_REQUIRE(world.queue_action(Action::light));
    }
    ARPG_REQUIRE(!world.queue_action(Action::light));
    ARPG_REQUIRE(world.snapshot().diagnostics.input_overflow_count == 1);

    world.reset();
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.tick == 0);
    ARPG_REQUIRE(arpg::test::near(snapshot.player.position.x, 1.0));
    ARPG_REQUIRE(arpg::test::near(snapshot.player.position.y, -1.0));
    ARPG_REQUIRE(arpg::test::near(snapshot.monsters[0].position.x, 2.0));
    ARPG_REQUIRE(snapshot.player.state == PlayerState::idle);
    ARPG_REQUIRE(snapshot.diagnostics.input_size == 0);
    ARPG_REQUIRE(snapshot.diagnostics.input_expired_count == 0);
    ARPG_REQUIRE(snapshot.diagnostics.input_overflow_count == 0);
    return {};
}

arpg::test::Failure jump_arc_has_deterministic_apex_and_one_landing_tick() noexcept {
    CombatWorld world;
    ARPG_REQUIRE(world.queue_action(Action::jump));

    int landing_ticks = 0;
    for (int tick = 1; tick <= 21; ++tick) {
        world.tick(MovementInput{});
        if (world.snapshot().player.state == PlayerState::landing) {
            ++landing_ticks;
        }
    }

    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.state == PlayerState::jump_rise);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.velocity.z, 0.1, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.z, 1.575, kFloatTolerance));

    world.tick(MovementInput{});
    snapshot = world.snapshot();
    const float apex = snapshot.player.position.z;
    ARPG_REQUIRE(snapshot.player.state == PlayerState::jump_fall);
    ARPG_REQUIRE(arpg::test::near(apex, 1.5766667, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.velocity.z, -0.3, kFloatTolerance));

    for (int tick = 23; tick <= 44; ++tick) {
        world.tick(MovementInput{});
        snapshot = world.snapshot();
        if (snapshot.player.state == PlayerState::landing) {
            ++landing_ticks;
        }
        if (tick == 23) {
            ARPG_REQUIRE(snapshot.player.position.z < apex);
        }
    }

    ARPG_REQUIRE(snapshot.player.state == PlayerState::landing);
    ARPG_REQUIRE(arpg::test::near(snapshot.player.position.z, 0.0));
    ARPG_REQUIRE(arpg::test::near(snapshot.player.velocity.z, 0.0));
    ARPG_REQUIRE(landing_ticks == 1);
    const std::uint64_t landing_snapshot_tick = snapshot.tick;

    world.tick(MovementInput{});
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.state == PlayerState::idle);
    ARPG_REQUIRE(landing_ticks == 1);

    int landing_events = 0;
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::landing) {
            ++landing_events;
            ARPG_REQUIRE(event->target_index == 0xFF);
            ARPG_REQUIRE(arpg::test::near(event->position.z, 0.0));
            ARPG_REQUIRE(event->tick + 1 == landing_snapshot_tick);
        }
    }
    ARPG_REQUIRE(landing_events == 1);
    return {};
}

arpg::test::Failure airborne_motion_uses_seventy_percent_control() noexcept {
    CombatWorld world;
    ARPG_REQUIRE(world.queue_action(Action::jump));
    world.tick(MovementInput{});

    for (int tick = 0; tick < 10; ++tick) {
        world.tick(MovementInput{1, 0});
    }

    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.state == PlayerState::jump_rise);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.x, 0.63, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.velocity.x, 3.78, kFloatTolerance));
    ARPG_REQUIRE(snapshot.player.facing == Facing::right);
    return {};
}

arpg::test::Failure heavy_steps_scales_only_ground_horizontal_motion() noexcept {
    CombatEncounterConfig normal_config{};
    normal_config.wave = {};
    CombatEncounterConfig heavy_config = normal_config;
    heavy_config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::heavy_steps);
    CombatWorld normal{normal_config};
    CombatWorld heavy{heavy_config};

    normal.tick(MovementInput{1, 0});
    heavy.tick(MovementInput{1, 0});
    ARPG_REQUIRE(arpg::test::near(
        normal.snapshot().player.velocity.x, 5.4, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        heavy.snapshot().player.velocity.x, 4.59, kFloatTolerance));

    ARPG_REQUIRE(normal.queue_action(Action::jump));
    ARPG_REQUIRE(heavy.queue_action(Action::jump));
    normal.tick({});
    heavy.tick({});
    ARPG_REQUIRE(arpg::test::near(
        heavy.snapshot().player.velocity.z,
        normal.snapshot().player.velocity.z));
    ARPG_REQUIRE(arpg::test::near(
        heavy.snapshot().player.position.z,
        normal.snapshot().player.position.z));
    normal.tick(MovementInput{1, 0});
    heavy.tick(MovementInput{1, 0});
    ARPG_REQUIRE(arpg::test::near(
        normal.snapshot().player.velocity.x, 3.78, kFloatTolerance));
    ARPG_REQUIRE(arpg::test::near(
        heavy.snapshot().player.velocity.x, 3.78, kFloatTolerance));

    CombatWorld normal_attack{normal_config};
    CombatWorld heavy_attack{heavy_config};
    ARPG_REQUIRE(normal_attack.queue_action(Action::light));
    ARPG_REQUIRE(heavy_attack.queue_action(Action::light));
    for (int tick = 0; tick < 40; ++tick) {
        normal_attack.tick({});
        heavy_attack.tick({});
        ARPG_REQUIRE(normal_attack.snapshot().player.active_attack
                     == heavy_attack.snapshot().player.active_attack);
        ARPG_REQUIRE(normal_attack.snapshot().player.attack_phase
                     == heavy_attack.snapshot().player.attack_phase);
    }
    return {};
}

arpg::test::Failure player_inside_fire_brazier_can_move_out() noexcept {
    CombatEncounterConfig config{};
    config.player_spawn = {-1.06F, 0.38F, 0.0F};
    config.fire_room_obstacles = true;
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::set_player_position(
        world, {-1.06F, 0.38F, 0.0F});

    for (int tick = 0; tick < 5; ++tick) {
        world.tick(MovementInput{-1, 0});
    }

    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.player.position.x < -1.40F);
    ARPG_REQUIRE(arpg::test::near(
        snapshot.player.position.y, 0.38, kFloatTolerance));

    CombatEncounterConfig outside_config{};
    outside_config.player_spawn = {-1.45F, 0.0F, 0.0F};
    outside_config.fire_room_obstacles = true;
    CombatWorld outside{outside_config};
    outside.tick(MovementInput{1, 0});
    ARPG_REQUIRE(!fire_room_obstacle::contains(
        outside.snapshot().player.position));
    return {};
}

arpg::test::Failure attack_lunge_does_not_enter_fire_brazier() noexcept {
    CombatEncounterConfig config{};
    config.player_spawn = {-1.45F, 0.0F, 0.0F};
    config.fire_room_obstacles = true;
    CombatWorld world{config};

    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});

    ARPG_REQUIRE(!fire_room_obstacle::contains(
        world.snapshot().player.position));
    return {};
}

arpg::test::Failure active_skill_movement_does_not_enter_fire_brazier() noexcept {
    CombatEncounterConfig config{};
    config.player_spawn = {-1.45F, 0.0F, 0.0F};
    config.fire_room_obstacles = true;
    CombatWorld world{config};

    ARPG_REQUIRE(world.request_active_skill(
        arpg::skills::ActiveSkillId::draw_slash)
        == SkillCastResult::accepted);
    world.tick(MovementInput{1, 0});

    ARPG_REQUIRE(!fire_room_obstacle::contains(
        world.snapshot().player.position));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"hundredfold room geometry and limits",
     &hundredfold_room_geometry_and_limits_are_exact},
    {"spatial grid bounds streaming regions",
     &spatial_grid_bounds_streaming_regions},
    {"fixed ground motion and normalized diagonal",
     &ground_motion_is_fixed_and_diagonal_is_normalized},
    {"room clamps, facing, and reset", &room_clamps_facing_and_reset_are_stable},
    {"deterministic jump apex and landing",
     &jump_arc_has_deterministic_apex_and_one_landing_tick},
    {"seventy percent air control", &airborne_motion_uses_seventy_percent_control},
    {"heavy steps only scales ground movement",
     &heavy_steps_scales_only_ground_horizontal_motion},
    {"player inside fire brazier can move out",
     &player_inside_fire_brazier_can_move_out},
    {"attack lunge does not enter fire brazier",
     &attack_lunge_does_not_enter_fire_brazier},
    {"active skill movement does not enter fire brazier",
     &active_skill_movement_does_not_enter_fire_brazier},
};

}  // namespace

arpg::test::TestSuite movement_jump_suite() noexcept {
    return arpg::test::make_suite("movement_jump", kCases);
}
