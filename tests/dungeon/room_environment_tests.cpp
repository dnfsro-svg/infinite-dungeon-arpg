#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat/combat_collision.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_environment_plan.hpp"
#include "combat/room_monster_plan.hpp"
#include "combat/room_spatial_grid.hpp"
#include "dungeon/room_combat_template.hpp"
#include "dungeon/room_environment.hpp"
#include "dungeon/room_monster_plan_builder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;

using arpg::combat::Aabb;
using arpg::combat::RoomEnvironmentBlueprint;
using arpg::combat::RoomEnvironmentRecord;
using arpg::combat::RoomMonsterBlueprint;
using arpg::combat::RoomMonsterPlan;
using arpg::combat::RoomObstacleKind;
using arpg::combat::Vec3;
using arpg::dungeon::DungeonFault;
using arpg::dungeon::DungeonRules;

static_assert(!std::is_copy_constructible_v<RoomEnvironmentBlueprint>);
static_assert(!std::is_copy_assignable_v<RoomEnvironmentBlueprint>);

constexpr std::array<Vec3, 4U> kDoorCenters{{
    {0.0F, arpg::combat::room_bounds::min_y, 0.0F},
    {0.0F, arpg::combat::room_bounds::max_y, 0.0F},
    {arpg::combat::room_bounds::min_x, 0.0F, 0.0F},
    {arpg::combat::room_bounds::max_x, 0.0F, 0.0F},
}};
constexpr Vec3 kHoleCenter{0.0F, 3.5F, 0.0F};

checkpoint::RoomDescriptor test_room() noexcept {
    checkpoint::RoomDescriptor room{};
    room.index = 17U;
    room.seed = 0xA11CEULL;
    room.depth = 40U;
    room.floor_room_index = 7U;
    room.entry = checkpoint::EntrySide::left;
    room.ecology = checkpoint::DungeonElement::lightning;
    room.has_hole = true;
    return room;
}

bool same_aabb(const Aabb& left, const Aabb& right) noexcept {
    return left.minimum.x == right.minimum.x
        && left.minimum.y == right.minimum.y
        && left.minimum.z == right.minimum.z
        && left.maximum.x == right.maximum.x
        && left.maximum.y == right.maximum.y
        && left.maximum.z == right.maximum.z;
}

bool same_environment_record(
    const RoomEnvironmentRecord& left,
    const RoomEnvironmentRecord& right) noexcept {
    return left.ordinal == right.ordinal
        && left.home_cell == right.home_cell
        && left.prop == right.prop
        && left.anchor.x == right.anchor.x
        && left.anchor.y == right.anchor.y
        && left.anchor.z == right.anchor.z
        && left.scale_bp == right.scale_bp
        && left.quarter_turns == right.quarter_turns
        && left.mirror_x == right.mirror_x
        && left.obstacle.kind == right.obstacle.kind
        && left.obstacle.max_hp == right.obstacle.max_hp
        && same_aabb(left.obstacle.bounds, right.obstacle.bounds);
}

bool same_environment_blueprint(
    const RoomEnvironmentBlueprint& left,
    const RoomEnvironmentBlueprint& right) noexcept {
    if (left.record_count != right.record_count
            || left.obstacle_count != right.obstacle_count
            || left.generator_version != right.generator_version
            || left.blueprint_hash != right.blueprint_hash
            || left.cell_offsets != right.cell_offsets
            || left.cell_counts != right.cell_counts) {
        return false;
    }
    for (std::size_t index = 0U; index < left.record_count; ++index) {
        if (!same_environment_record(left.records[index], right.records[index])) {
            return false;
        }
    }
    return true;
}

bool circle_overlaps_aabb(
    Vec3 center, float radius, const Aabb& bounds) noexcept {
    const float nearest_x = std::clamp(
        center.x, bounds.minimum.x, bounds.maximum.x);
    const float nearest_y = std::clamp(
        center.y, bounds.minimum.y, bounds.maximum.y);
    const float delta_x = center.x - nearest_x;
    const float delta_y = center.y - nearest_y;
    return delta_x * delta_x + delta_y * delta_y <= radius * radius;
}

Aabb initial_monster_bounds(const RoomMonsterBlueprint& monster) noexcept {
    return arpg::combat::room_monster_initial_bounds(monster);
}

std::uint16_t cell_for(Vec3 position) noexcept {
    const float column_value = (position.x - arpg::combat::room_bounds::min_x)
        / arpg::combat::room_spatial::cell_width;
    const float row_value = (position.y - arpg::combat::room_bounds::min_y)
        / arpg::combat::room_spatial::cell_depth;
    const std::size_t column = std::min<std::size_t>(
        arpg::combat::room_spatial::columns - 1U,
        column_value <= 0.0F ? 0U
            : static_cast<std::size_t>(column_value));
    const std::size_t row = std::min<std::size_t>(
        arpg::combat::room_spatial::rows - 1U,
        row_value <= 0.0F ? 0U
            : static_cast<std::size_t>(row_value));
    return static_cast<std::uint16_t>(
        row * arpg::combat::room_spatial::columns + column);
}

bool all_door_cells_reachable(
    const RoomEnvironmentBlueprint& environment,
    Vec3 entry) noexcept {
    constexpr std::size_t kCellCount =
        arpg::combat::room_spatial::columns
        * arpg::combat::room_spatial::rows;
    std::array<bool, kCellCount> blocked{};
    for (std::size_t index = 0U; index < environment.record_count; ++index) {
        const RoomEnvironmentRecord& record = environment.records[index];
        if (record.obstacle.kind != RoomObstacleKind::none) {
            if (record.home_cell >= blocked.size()) return false;
            blocked[record.home_cell] = true;
        }
    }

    const std::uint16_t start = cell_for(entry);
    if (blocked[start]) return false;
    std::array<bool, kCellCount> visited{};
    std::array<std::uint16_t, kCellCount> queue{};
    std::size_t head = 0U;
    std::size_t tail = 0U;
    queue[tail++] = start;
    visited[start] = true;
    while (head < tail) {
        const std::uint16_t current = queue[head++];
        const std::size_t row = current
            / arpg::combat::room_spatial::columns;
        const std::size_t column = current
            % arpg::combat::room_spatial::columns;
        const auto visit = [&blocked, &visited, &queue, &tail](
                               std::size_t next) noexcept {
            if (!blocked[next] && !visited[next]) {
                visited[next] = true;
                queue[tail++] = static_cast<std::uint16_t>(next);
            }
        };
        if (column > 0U) visit(current - 1U);
        if (column + 1U < arpg::combat::room_spatial::columns) {
            visit(current + 1U);
        }
        if (row > 0U) {
            visit(current - arpg::combat::room_spatial::columns);
        }
        if (row + 1U < arpg::combat::room_spatial::rows) {
            visit(current + arpg::combat::room_spatial::columns);
        }
    }

    for (const Vec3 door : kDoorCenters) {
        if (!visited[cell_for(door)]) return false;
    }
    return true;
}

bool canonical_empty(const RoomEnvironmentBlueprint& environment) noexcept {
    if (environment.record_count != 0U
            || environment.obstacle_count != 0U
            || environment.generator_version != 0U
            || environment.blueprint_hash != 0U) {
        return false;
    }
    for (const std::uint16_t offset : environment.cell_offsets) {
        if (offset != 0U) return false;
    }
    for (const std::uint8_t count : environment.cell_counts) {
        if (count != 0U) return false;
    }
    const RoomEnvironmentRecord empty_record{};
    for (const RoomEnvironmentRecord& record : environment.records) {
        if (!same_environment_record(record, empty_record)) return false;
    }
    return true;
}

arpg::test::Failure environment_is_deterministic_and_hashes_canonical_fields()
    noexcept {
    const checkpoint::RoomDescriptor room = test_room();
    const DungeonRules rules{};
    auto monsters = std::make_unique<RoomMonsterPlan>();
    const auto population = arpg::dungeon::build_room_monster_plan(
        room, rules, 1U, *monsters);
    ARPG_REQUIRE(population.fault == DungeonFault::none);

    auto first = std::make_unique<RoomEnvironmentBlueprint>();
    auto second = std::make_unique<RoomEnvironmentBlueprint>();
    const auto first_result = arpg::dungeon::build_room_environment(
        room, rules, 1U, *monsters, *first);
    const auto second_result = arpg::dungeon::build_room_environment(
        room, rules, 1U, *monsters, *second);
    ARPG_REQUIRE(first_result.fault == DungeonFault::none);
    ARPG_REQUIRE(second_result.fault == DungeonFault::none);
    ARPG_REQUIRE(first->generator_version == 1U);
    ARPG_REQUIRE(same_environment_blueprint(*first, *second));
    ARPG_REQUIRE(arpg::dungeon::room_environment_legal(
        room, *monsters, *first));
    return {};
}

arpg::test::Failure cell_index_and_obstacle_view_contract_is_canonical()
    noexcept {
    const checkpoint::RoomDescriptor room = test_room();
    const DungeonRules rules{};
    auto monsters = std::make_unique<RoomMonsterPlan>();
    auto environment = std::make_unique<RoomEnvironmentBlueprint>();
    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        room, rules, 1U, *monsters).fault == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 1U, *monsters, *environment).fault
        == DungeonFault::none);

    ARPG_REQUIRE(environment->record_count
        <= arpg::combat::kRoomEnvironmentRecordCapacity);
    ARPG_REQUIRE(environment->cell_offsets[0] == 0U);
    std::size_t counted_obstacles = 0U;
    for (std::size_t cell = 0U;
         cell < arpg::combat::kRoomEnvironmentCellCount; ++cell) {
        const std::size_t begin = environment->cell_offsets[cell];
        const std::size_t end = environment->cell_offsets[cell + 1U];
        ARPG_REQUIRE(environment->cell_counts[cell] <= 3U);
        ARPG_REQUIRE(end == begin + environment->cell_counts[cell]);
        ARPG_REQUIRE(end <= environment->record_count);
        std::size_t colliding = 0U;
        for (std::size_t index = begin; index < end; ++index) {
            const RoomEnvironmentRecord& record = environment->records[index];
            ARPG_REQUIRE(record.ordinal == index);
            ARPG_REQUIRE(record.home_cell == cell);
            if (record.obstacle.kind != RoomObstacleKind::none) {
                ++colliding;
                ++counted_obstacles;
            }
        }
        ARPG_REQUIRE(colliding <= 1U);
    }
    ARPG_REQUIRE(environment->cell_offsets[
        arpg::combat::kRoomEnvironmentCellCount]
        == environment->record_count);
    ARPG_REQUIRE(counted_obstacles == environment->obstacle_count);

    const auto view = arpg::combat::room_obstacle_plan_view(*environment);
    ARPG_REQUIRE(view.records == environment->records.data());
    ARPG_REQUIRE(view.cell_offsets == environment->cell_offsets.data());
    ARPG_REQUIRE(view.cell_counts == environment->cell_counts.data());
    ARPG_REQUIRE(view.record_count == environment->record_count);
    return {};
}

arpg::test::Failure obstacles_avoid_reserved_geometry_and_initial_monsters()
    noexcept {
    const checkpoint::RoomDescriptor room = test_room();
    const DungeonRules rules{};
    auto monsters = std::make_unique<RoomMonsterPlan>();
    auto environment = std::make_unique<RoomEnvironmentBlueprint>();
    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        room, rules, 1U, *monsters).fault == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 1U, *monsters, *environment).fault
        == DungeonFault::none);
    const auto entry = arpg::dungeon::make_combat_lab_config(
        room.entry, rules.rules_version);
    ARPG_REQUIRE(entry.has_value());

    for (std::size_t index = 0U; index < environment->record_count; ++index) {
        const auto& obstacle = environment->records[index].obstacle;
        if (obstacle.kind == RoomObstacleKind::none) continue;
        ARPG_REQUIRE(!circle_overlaps_aabb(
            entry->player_spawn, 4.0F, obstacle.bounds));
        for (const Vec3 door : kDoorCenters) {
            ARPG_REQUIRE(!circle_overlaps_aabb(door, 3.0F, obstacle.bounds));
        }
        ARPG_REQUIRE(!circle_overlaps_aabb(
            kHoleCenter, 3.0F, obstacle.bounds));
        for (std::size_t monster_index = 0U;
             monster_index < monsters->monster_count; ++monster_index) {
            ARPG_REQUIRE(!arpg::combat::overlaps_inclusive(
                obstacle.bounds,
                initial_monster_bounds(monsters->monsters[monster_index])));
        }
    }
    return {};
}

arpg::test::Failure entry_reaches_every_door_without_breaking_obstacles()
    noexcept {
    const checkpoint::RoomDescriptor room = test_room();
    const DungeonRules rules{};
    auto monsters = std::make_unique<RoomMonsterPlan>();
    auto environment = std::make_unique<RoomEnvironmentBlueprint>();
    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        room, rules, 1U, *monsters).fault == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 1U, *monsters, *environment).fault
        == DungeonFault::none);
    const auto entry = arpg::dungeon::make_combat_lab_config(
        room.entry, rules.rules_version);
    ARPG_REQUIRE(entry.has_value());
    ARPG_REQUIRE(all_door_cells_reachable(
        *environment, entry->player_spawn));
    return {};
}

arpg::test::Failure environment_version_never_mutates_sealed_monster_plan()
    noexcept {
    const checkpoint::RoomDescriptor room = test_room();
    const DungeonRules rules{};
    auto monsters = std::make_unique<RoomMonsterPlan>();
    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        room, rules, 1U, *monsters).fault == DungeonFault::none);
    const std::uint64_t monster_hash = monsters->blueprint_hash;
    std::array<unsigned char, sizeof(RoomMonsterPlan)> original_bytes{};
    std::memcpy(original_bytes.data(), monsters.get(), original_bytes.size());

    auto first = std::make_unique<RoomEnvironmentBlueprint>();
    auto second = std::make_unique<RoomEnvironmentBlueprint>();
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 1U, *monsters, *first).fault == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 2U, *monsters, *second).fault == DungeonFault::none);
    ARPG_REQUIRE(first->blueprint_hash != second->blueprint_hash);
    ARPG_REQUIRE(monsters->blueprint_hash == monster_hash);
    ARPG_REQUIRE(std::memcmp(original_bytes.data(), monsters.get(),
        original_bytes.size()) == 0);
    return {};
}

arpg::test::Failure failures_publish_only_a_canonical_empty_blueprint()
    noexcept {
    const checkpoint::RoomDescriptor room = test_room();
    const DungeonRules rules{};
    auto monsters = std::make_unique<RoomMonsterPlan>();
    auto environment = std::make_unique<RoomEnvironmentBlueprint>();
    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        room, rules, 1U, *monsters).fault == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 1U, *monsters, *environment).fault
        == DungeonFault::none);

    const auto invalid_version = arpg::dungeon::build_room_environment(
        room, rules, 0U, *monsters, *environment);
    ARPG_REQUIRE(invalid_version.fault == DungeonFault::invalid_rules);
    ARPG_REQUIRE(canonical_empty(*environment));

    const auto at_capacity =
        arpg::dungeon::test_support::build_room_environment_with_record_count(
            room, rules, 1U, *monsters,
            static_cast<std::uint16_t>(
                arpg::combat::kRoomEnvironmentRecordCapacity),
            *environment);
    ARPG_REQUIRE(at_capacity.fault == DungeonFault::none);
    ARPG_REQUIRE(environment->record_count
        == arpg::combat::kRoomEnvironmentRecordCapacity);
    for (const std::uint8_t count : environment->cell_counts) {
        ARPG_REQUIRE(count == 3U);
    }
    ARPG_REQUIRE(arpg::dungeon::room_environment_legal(
        room, *monsters, *environment));

    const auto over_capacity =
        arpg::dungeon::test_support::build_room_environment_with_record_count(
            room, rules, 1U, *monsters,
            static_cast<std::uint16_t>(
                arpg::combat::kRoomEnvironmentRecordCapacity + 1U),
            *environment);
    ARPG_REQUIRE(over_capacity.fault == DungeonFault::environment_capacity);
    ARPG_REQUIRE(canonical_empty(*environment));

    const auto forced_placement_failure = arpg::dungeon::test_support::
        build_room_environment_with_forced_placement_failure(
            room, rules, 1U, *monsters, *environment);
    ARPG_REQUIRE(forced_placement_failure.fault
        == DungeonFault::environment_placement);
    ARPG_REQUIRE(canonical_empty(*environment));
    return {};
}

arpg::test::Failure environment_build_is_allocation_free() noexcept {
    const checkpoint::RoomDescriptor room = test_room();
    const DungeonRules rules{};
    auto monsters = std::make_unique<RoomMonsterPlan>();
    auto environment = std::make_unique<RoomEnvironmentBlueprint>();
    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        room, rules, 1U, *monsters).fault == DungeonFault::none);

    const std::uint64_t before = arpg::test::allocation_count();
    const auto result = arpg::dungeon::build_room_environment(
        room, rules, 1U, *monsters, *environment);
    const std::uint64_t after = arpg::test::allocation_count();
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(after == before);
    return {};
}

arpg::test::Failure all_room_input_bits_build_legally_without_allocation()
    noexcept {
    constexpr std::array<checkpoint::EntrySide, 4U> kEntries{{
        checkpoint::EntrySide::top,
        checkpoint::EntrySide::bottom,
        checkpoint::EntrySide::left,
        checkpoint::EntrySide::right,
    }};
    const DungeonRules rules{};
    auto monsters = std::make_unique<RoomMonsterPlan>();
    auto environment = std::make_unique<RoomEnvironmentBlueprint>();
    bool observed_hundredfold_population = false;

    const std::uint64_t before = arpg::test::allocation_count();
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        checkpoint::RoomDescriptor room = test_room();
        room.seed = seed;
        room.ecology = static_cast<checkpoint::DungeonElement>(seed & 0x3U);
        room.entry = kEntries[(seed >> 2U) & 0x3U];
        room.has_hole = (seed & 0x10U) != 0U;
        room.is_abyss = (seed & 0x20U) != 0U;

        const auto population = arpg::dungeon::build_room_monster_plan(
            room, rules, 1U, *monsters);
        ARPG_REQUIRE(population.fault == DungeonFault::none);
        ARPG_REQUIRE(arpg::dungeon::room_monster_plan_legal(
            room, *monsters));
        observed_hundredfold_population = observed_hundredfold_population
            || monsters->monster_count == 1125U;

        const auto placement = arpg::dungeon::build_room_environment(
            room, rules, 1U, *monsters, *environment);
        ARPG_REQUIRE(placement.fault == DungeonFault::none);
        ARPG_REQUIRE(arpg::dungeon::room_environment_legal(
            room, *monsters, *environment));
    }
    const std::uint64_t after = arpg::test::allocation_count();
    ARPG_REQUIRE(after == before);
    ARPG_REQUIRE(observed_hundredfold_population);
    return {};
}

arpg::test::Failure depth_changes_the_environment_hash_for_the_same_seed()
    noexcept {
    checkpoint::RoomDescriptor shallow_room = test_room();
    checkpoint::RoomDescriptor deep_room = shallow_room;
    ++deep_room.depth;
    const DungeonRules rules{};
    auto shallow_monsters = std::make_unique<RoomMonsterPlan>();
    auto deep_monsters = std::make_unique<RoomMonsterPlan>();
    auto shallow_environment = std::make_unique<RoomEnvironmentBlueprint>();
    auto deep_environment = std::make_unique<RoomEnvironmentBlueprint>();

    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        shallow_room, rules, 1U, *shallow_monsters).fault
        == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        deep_room, rules, 1U, *deep_monsters).fault == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        shallow_room, rules, 1U, *shallow_monsters, *shallow_environment).fault
        == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        deep_room, rules, 1U, *deep_monsters, *deep_environment).fault
        == DungeonFault::none);
    ARPG_REQUIRE(shallow_environment->blueprint_hash
        != deep_environment->blueprint_hash);
    return {};
}

arpg::test::Failure monster_versions_rebuild_environment_deterministically()
    noexcept {
    const checkpoint::RoomDescriptor room = test_room();
    const DungeonRules rules{};
    auto version_one_monsters = std::make_unique<RoomMonsterPlan>();
    auto version_two_monsters = std::make_unique<RoomMonsterPlan>();
    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        room, rules, 1U, *version_one_monsters).fault == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_monster_plan(
        room, rules, 2U, *version_two_monsters).fault == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::room_monster_plan_legal(
        room, *version_one_monsters));
    ARPG_REQUIRE(arpg::dungeon::room_monster_plan_legal(
        room, *version_two_monsters));

    std::array<unsigned char, sizeof(RoomMonsterPlan)> version_one_bytes{};
    std::array<unsigned char, sizeof(RoomMonsterPlan)> version_two_bytes{};
    std::memcpy(version_one_bytes.data(), version_one_monsters.get(),
        version_one_bytes.size());
    std::memcpy(version_two_bytes.data(), version_two_monsters.get(),
        version_two_bytes.size());

    auto version_one_first = std::make_unique<RoomEnvironmentBlueprint>();
    auto version_one_repeat = std::make_unique<RoomEnvironmentBlueprint>();
    auto version_two_first = std::make_unique<RoomEnvironmentBlueprint>();
    auto version_two_repeat = std::make_unique<RoomEnvironmentBlueprint>();
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 1U, *version_one_monsters, *version_one_first).fault
        == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 1U, *version_one_monsters, *version_one_repeat).fault
        == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 1U, *version_two_monsters, *version_two_first).fault
        == DungeonFault::none);
    ARPG_REQUIRE(arpg::dungeon::build_room_environment(
        room, rules, 1U, *version_two_monsters, *version_two_repeat).fault
        == DungeonFault::none);

    ARPG_REQUIRE(same_environment_blueprint(
        *version_one_first, *version_one_repeat));
    ARPG_REQUIRE(same_environment_blueprint(
        *version_two_first, *version_two_repeat));
    ARPG_REQUIRE(arpg::dungeon::room_environment_legal(
        room, *version_one_monsters, *version_one_first));
    ARPG_REQUIRE(arpg::dungeon::room_environment_legal(
        room, *version_two_monsters, *version_two_first));
    ARPG_REQUIRE(std::memcmp(version_one_bytes.data(),
        version_one_monsters.get(), version_one_bytes.size()) == 0);
    ARPG_REQUIRE(std::memcmp(version_two_bytes.data(),
        version_two_monsters.get(), version_two_bytes.size()) == 0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"environment is deterministic and hashes canonical fields",
        &environment_is_deterministic_and_hashes_canonical_fields},
    {"cell index and obstacle view contract is canonical",
        &cell_index_and_obstacle_view_contract_is_canonical},
    {"obstacles avoid reserved geometry and initial monsters",
        &obstacles_avoid_reserved_geometry_and_initial_monsters},
    {"entry reaches every door without breaking obstacles",
        &entry_reaches_every_door_without_breaking_obstacles},
    {"environment version never mutates sealed monster plan",
        &environment_version_never_mutates_sealed_monster_plan},
    {"failures publish only a canonical empty blueprint",
        &failures_publish_only_a_canonical_empty_blueprint},
    {"environment build is allocation free",
        &environment_build_is_allocation_free},
    {"all room input bits build legally without allocation",
        &all_room_input_bits_build_legally_without_allocation},
    {"depth changes the environment hash for the same seed",
        &depth_changes_the_environment_hash_for_the_same_seed},
    {"monster versions rebuild environment deterministically",
        &monster_versions_rebuild_environment_deterministically},
};

}  // namespace

arpg::test::TestSuite room_environment_suite() noexcept {
    return arpg::test::make_suite("room_environment", kCases);
}
