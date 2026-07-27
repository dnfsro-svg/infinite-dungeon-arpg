#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_monster_plan.hpp"
#include "combat/room_spatial_grid.hpp"
#include "core/gameplay_limits.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/dungeon_rules.hpp"
#include "dungeon/room_affix.hpp"
#include "dungeon/room_monster_plan_builder.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;

constexpr std::uint32_t kGeneratorVersion = 1U;
constexpr std::size_t kCellCount = 400U;
constexpr arpg::combat::Vec3 kHoleCenter{0.0F, 3.5F, 0.0F};
constexpr std::array<arpg::combat::Vec3, 4U> kDoorCenters{{
    {arpg::combat::room_bounds::min_x, 0.0F, 0.0F},
    {arpg::combat::room_bounds::max_x, 0.0F, 0.0F},
    {0.0F, arpg::combat::room_bounds::min_y, 0.0F},
    {0.0F, arpg::combat::room_bounds::max_y, 0.0F},
}};
constexpr std::array<checkpoint::EntrySide, 4U> kEntrySides{{
    checkpoint::EntrySide::top,
    checkpoint::EntrySide::bottom,
    checkpoint::EntrySide::left,
    checkpoint::EntrySide::right,
}};

static_assert(!std::is_copy_constructible_v<arpg::combat::RoomMonsterPlan>);
static_assert(!std::is_copy_assignable_v<arpg::combat::RoomMonsterPlan>);
static_assert(arpg::combat::room_spatial::columns
        * arpg::combat::room_spatial::rows
    == kCellCount);

[[nodiscard]] checkpoint::RoomDescriptor make_room(
    std::uint64_t seed,
    checkpoint::DungeonElement ecology = checkpoint::DungeonElement::fire,
    checkpoint::EntrySide entry = checkpoint::EntrySide::left,
    bool abyss = false,
    bool has_hole = true) noexcept {
    checkpoint::RoomDescriptor room{};
    room.index = seed + 1U;
    room.seed = seed;
    room.depth = 40U;
    room.floor_room_index = 1U;
    room.entry = entry;
    room.ecology = ecology;
    room.has_hole = has_hole;
    room.is_abyss = abyss;
    return room;
}

[[nodiscard]] arpg::combat::Vec3 entry_spawn(
    checkpoint::EntrySide entry) noexcept {
    using namespace arpg::combat;
    switch (entry) {
    case checkpoint::EntrySide::initial:
        return {};
    case checkpoint::EntrySide::top:
        return {0.0F, room_bounds::min_y + 0.75F, 0.0F};
    case checkpoint::EntrySide::bottom:
        return {0.0F, room_bounds::max_y - 0.75F, 0.0F};
    case checkpoint::EntrySide::left:
        return {room_bounds::min_x + 1.50F, 0.0F, 0.0F};
    case checkpoint::EntrySide::right:
        return {room_bounds::max_x - 1.50F, 0.0F, 0.0F};
    }
    return {};
}

[[nodiscard]] float distance_squared(
    arpg::combat::Vec3 left,
    arpg::combat::Vec3 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    return x * x + y * y;
}

[[nodiscard]] bool same_blueprint(
    const arpg::combat::RoomMonsterBlueprint& left,
    const arpg::combat::RoomMonsterBlueprint& right) noexcept {
    return left.id == right.id
        && left.affixes == right.affixes
        && left.initial_position.x == right.initial_position.x
        && left.initial_position.y == right.initial_position.y
        && left.initial_position.z == right.initial_position.z
        && left.spawn_ordinal == right.spawn_ordinal
        && left.home_cell == right.home_cell
        && left.roaming_leash_cells == right.roaming_leash_cells;
}

[[nodiscard]] bool same_plan_fields(
    const arpg::combat::RoomMonsterPlan& left,
    const arpg::combat::RoomMonsterPlan& right,
    bool compare_unused = false) noexcept {
    if (left.cell_offsets != right.cell_offsets
            || left.cell_counts != right.cell_counts
            || left.monster_count != right.monster_count
            || left.threat_total != right.threat_total
            || left.generator_version != right.generator_version
            || left.blueprint_hash != right.blueprint_hash) {
        return false;
    }
    const std::size_t count = compare_unused
        ? left.monsters.size() : left.monster_count;
    if (!compare_unused && left.monster_count != right.monster_count) {
        return false;
    }
    for (std::size_t index = 0U; index < count; ++index) {
        if (!same_blueprint(left.monsters[index], right.monsters[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] arpg::test::Failure verify_plan_contract(
    const arpg::combat::RoomMonsterPlan& plan,
    const checkpoint::RoomDescriptor& room,
    const arpg::dungeon::DungeonRules& rules,
    std::uint32_t generator_version) noexcept {
    using namespace arpg::combat;
    static_cast<void>(rules);
    ARPG_REQUIRE(plan.monster_count > 0U);
    ARPG_REQUIRE(plan.monster_count <= arpg::limits::kRoomMonsterCapacity);
    ARPG_REQUIRE(plan.generator_version == generator_version);
    ARPG_REQUIRE(plan.cell_offsets[0] == 0U);
    ARPG_REQUIRE(plan.cell_offsets[kCellCount] == plan.monster_count);
    ARPG_REQUIRE(arpg::dungeon::room_monster_plan_legal(room, plan));

    std::uint32_t expected_threat = 0U;
    const std::uint16_t base_cell_count = static_cast<std::uint16_t>(
        plan.monster_count / kCellCount);
    const std::uint16_t expected_extra_cells = static_cast<std::uint16_t>(
        plan.monster_count % kCellCount);
    std::uint16_t actual_extra_cells = 0U;
    for (std::size_t cell = 0U; cell < kCellCount; ++cell) {
        const std::uint16_t begin = plan.cell_offsets[cell];
        const std::uint16_t end = plan.cell_offsets[cell + 1U];
        ARPG_REQUIRE(begin <= end);
        ARPG_REQUIRE(end <= plan.monster_count);
        ARPG_REQUIRE(plan.cell_counts[cell] <= 3U);
        ARPG_REQUIRE(end - begin == plan.cell_counts[cell]);
        ARPG_REQUIRE(plan.cell_counts[cell] == base_cell_count
            || plan.cell_counts[cell] == base_cell_count + 1U);
        actual_extra_cells += plan.cell_counts[cell] == base_cell_count + 1U;

        std::uint8_t direct_targets = 0U;
        std::uint8_t supports = 0U;
        std::uint8_t ranged = 0U;
        std::uint8_t high_priority = 0U;
        std::uint8_t ground_hazards = 0U;
        const std::size_t column = cell % room_spatial::columns;
        const std::size_t row = cell / room_spatial::columns;
        const float cell_min_x = room_bounds::min_x
            + static_cast<float>(column) * room_spatial::cell_width;
        const float cell_max_x = cell_min_x + room_spatial::cell_width;
        const float cell_min_y = room_bounds::min_y
            + static_cast<float>(row) * room_spatial::cell_depth;
        const float cell_max_y = cell_min_y + room_spatial::cell_depth;

        for (std::uint16_t ordinal = begin; ordinal < end; ++ordinal) {
            const RoomMonsterBlueprint& monster = plan.monsters[ordinal];
            const MonsterDefinition* definition = monster_definition(monster.id);
            ARPG_REQUIRE(definition != nullptr);
            ARPG_REQUIRE(monster.spawn_ordinal == ordinal);
            ARPG_REQUIRE(monster.home_cell == cell);
            ARPG_REQUIRE(monster.roaming_leash_cells == 1U);
            ARPG_REQUIRE(std::isfinite(monster.initial_position.x));
            ARPG_REQUIRE(std::isfinite(monster.initial_position.y));
            ARPG_REQUIRE(monster.initial_position.z == 0.0F);
            ARPG_REQUIRE(monster.initial_position.x >= room_bounds::min_x);
            ARPG_REQUIRE(monster.initial_position.x <= room_bounds::max_x);
            ARPG_REQUIRE(monster.initial_position.y >= room_bounds::min_y);
            ARPG_REQUIRE(monster.initial_position.y <= room_bounds::max_y);
            ARPG_REQUIRE(monster.initial_position.x >= cell_min_x);
            ARPG_REQUIRE(monster.initial_position.x <= cell_max_x);
            ARPG_REQUIRE(monster.initial_position.y >= cell_min_y);
            ARPG_REQUIRE(monster.initial_position.y <= cell_max_y);
            ARPG_REQUIRE(distance_squared(
                monster.initial_position, entry_spawn(room.entry)) >= 16.0F);
            for (const Vec3 door : kDoorCenters) {
                ARPG_REQUIRE(distance_squared(monster.initial_position, door)
                    >= 9.0F);
            }
            if (room.has_hole) {
                ARPG_REQUIRE(distance_squared(
                    monster.initial_position, kHoleCenter) >= 9.0F);
            }

            direct_targets += has_tag(*definition, MonsterTag::direct_target);
            supports += has_tag(*definition, MonsterTag::support);
            ranged += has_tag(*definition, MonsterTag::ranged);
            high_priority += has_tag(*definition, MonsterTag::high_priority);
            ground_hazards += has_tag(*definition, MonsterTag::ground_hazard);
            expected_threat += definition->threat_cost;
        }
        if (begin != end) {
            ARPG_REQUIRE(direct_targets >= 1U);
        }
        ARPG_REQUIRE(supports <= 1U);
        ARPG_REQUIRE(ranged <= 1U);
        ARPG_REQUIRE(high_priority <= 1U);
        ARPG_REQUIRE(ground_hazards <= 1U);
    }
    ARPG_REQUIRE(actual_extra_cells == expected_extra_cells);
    ARPG_REQUIRE(expected_threat == plan.threat_total);
    return {};
}

arpg::test::Failure plans_are_deterministic_and_noncopyable() noexcept {
    const arpg::dungeon::DungeonRules rules{};
    const checkpoint::RoomDescriptor room = make_room(
        0xA11CEULL, checkpoint::DungeonElement::lightning,
        checkpoint::EntrySide::left, false, true);
    auto first = std::make_unique<arpg::combat::RoomMonsterPlan>();
    auto second = std::make_unique<arpg::combat::RoomMonsterPlan>();
    const auto first_result = arpg::dungeon::build_room_monster_plan(
        room, rules, kGeneratorVersion, *first);
    const auto second_result = arpg::dungeon::build_room_monster_plan(
        room, rules, kGeneratorVersion, *second);
    ARPG_REQUIRE(first_result.fault == arpg::dungeon::DungeonFault::none);
    ARPG_REQUIRE(second_result.fault == arpg::dungeon::DungeonFault::none);
    ARPG_REQUIRE(first_result.density.affix == second_result.density.affix);
    ARPG_REQUIRE(first_result.density.base_count
        == second_result.density.base_count);
    ARPG_REQUIRE(first_result.density.total_count
        == second_result.density.total_count);
    ARPG_REQUIRE(first->monster_count == first_result.density.total_count);
    ARPG_REQUIRE(first->blueprint_hash == second->blueprint_hash);
    ARPG_REQUIRE(arpg::dungeon::room_monster_plan_equal_fields(
        *first, *second));
    ARPG_REQUIRE(same_plan_fields(*first, *second));
    return verify_plan_contract(*first, room, rules, kGeneratorVersion);
}

arpg::test::Failure cell_spans_ordinals_roles_and_direct_targets_are_legal()
    noexcept {
    const arpg::dungeon::DungeonRules rules{};
    const checkpoint::RoomDescriptor room = make_room(
        0xD19E5EEDULL, checkpoint::DungeonElement::chaos,
        checkpoint::EntrySide::right, true, true);
    auto plan = std::make_unique<arpg::combat::RoomMonsterPlan>();
    const auto result = arpg::dungeon::build_room_monster_plan(
        room, rules, kGeneratorVersion, *plan);
    ARPG_REQUIRE(result.fault == arpg::dungeon::DungeonFault::none);
    ARPG_REQUIRE(plan->monster_count == result.density.total_count);
    return verify_plan_contract(*plan, room, rules, kGeneratorVersion);
}

arpg::test::Failure singleton_cells_keep_weighted_direct_target_variety()
    noexcept {
    using namespace arpg::combat;
    const arpg::dungeon::DungeonRules rules{};
    auto plan = std::make_unique<RoomMonsterPlan>();
    constexpr std::array<MonsterId, 4U> kFallbacks{{
        MonsterId::fire_bomber,
        MonsterId::water_bulwark,
        MonsterId::lightning_shooter,
        MonsterId::chaos_chaser,
    }};
    for (std::uint8_t ecology_index = 0U; ecology_index < 4U;
         ++ecology_index) {
        const auto ecology = static_cast<checkpoint::DungeonElement>(
            ecology_index);
        const checkpoint::RoomDescriptor room = make_room(
            0x51A9E000ULL + ecology_index, ecology,
            checkpoint::EntrySide::left, false, true);
        const auto result =
            arpg::dungeon::test_support::build_room_monster_plan_with_count(
                room, rules, kGeneratorVersion, 400U, *plan);
        ARPG_REQUIRE(result.fault == arpg::dungeon::DungeonFault::none);
        ARPG_REQUIRE(plan->monster_count == 400U);

        std::array<bool, static_cast<std::size_t>(MonsterId::count)> seen{};
        std::size_t distinct_direct_identities = 0U;
        bool saw_non_fallback = false;
        for (std::size_t cell = 0U; cell < kCellCount; ++cell) {
            ARPG_REQUIRE(plan->cell_counts[cell] == 1U);
            const RoomMonsterBlueprint& monster =
                plan->monsters[plan->cell_offsets[cell]];
            const MonsterDefinition* definition = monster_definition(monster.id);
            ARPG_REQUIRE(definition != nullptr);
            ARPG_REQUIRE(has_tag(*definition, MonsterTag::direct_target));
            const std::size_t id = static_cast<std::size_t>(monster.id);
            ARPG_REQUIRE(id < seen.size());
            if (!seen[id]) {
                seen[id] = true;
                ++distinct_direct_identities;
            }
            saw_non_fallback = saw_non_fallback
                || monster.id != kFallbacks[ecology_index];
        }
        ARPG_REQUIRE(distinct_direct_identities >= 2U);
        ARPG_REQUIRE(saw_non_fallback);
    }
    return {};
}

arpg::test::Failure unused_affix_slots_do_not_change_canonical_plan_hash()
    noexcept {
    const arpg::dungeon::DungeonRules rules{};
    checkpoint::RoomDescriptor room = make_room(
        0xAFF10001ULL, checkpoint::DungeonElement::fire,
        checkpoint::EntrySide::left, false, false);
    room.depth = 1U;
    auto plan = std::make_unique<arpg::combat::RoomMonsterPlan>();
    const auto result = arpg::dungeon::build_room_monster_plan(
        room, rules, kGeneratorVersion, *plan);
    ARPG_REQUIRE(result.fault == arpg::dungeon::DungeonFault::none);
    ARPG_REQUIRE(plan->monster_count > 0U);
    ARPG_REQUIRE(plan->monsters[0].affixes.count == 0U);
    const std::uint64_t canonical_hash = plan->blueprint_hash;

    plan->monsters[0].affixes.values[0] = {
        arpg::combat::MonsterAffixId::death_blast,
        arpg::combat::MonsterAffixTier::m3,
    };
    ARPG_REQUIRE(plan->blueprint_hash == canonical_hash);
    ARPG_REQUIRE(arpg::dungeon::room_monster_plan_legal(room, *plan));
    return {};
}

arpg::test::Failure every_ecology_entry_and_seed_is_legal_and_allocation_free()
    noexcept {
    const arpg::dungeon::DungeonRules rules{};
    auto plan = std::make_unique<arpg::combat::RoomMonsterPlan>();
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const auto ecology = static_cast<checkpoint::DungeonElement>(
            seed & 0x3U);
        const checkpoint::EntrySide entry = kEntrySides[(seed >> 2U) & 0x3U];
        checkpoint::RoomDescriptor room = make_room(
            seed, ecology, entry,
            (seed & (std::uint64_t{1U} << 5U)) != 0U,
            (seed & (std::uint64_t{1U} << 4U)) != 0U);
        room.depth = 1U + seed % 80U;
        const auto result = arpg::dungeon::build_room_monster_plan(
            room, rules, kGeneratorVersion, *plan);
        ARPG_REQUIRE(result.fault == arpg::dungeon::DungeonFault::none);
        ARPG_REQUIRE(plan->monster_count == result.density.total_count);
        const arpg::test::Failure failure = verify_plan_contract(
            *plan, room, rules, kGeneratorVersion);
        if (failure.expression != nullptr) return failure;
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

arpg::test::Failure all_normal_maxima_and_abyss_maximum_build_exact_counts()
    noexcept {
    const arpg::dungeon::DungeonRules rules{};
    auto plan = std::make_unique<arpg::combat::RoomMonsterPlan>();
    std::array<bool, 3U> found_normal{};
    bool found_abyss = false;
    constexpr std::array<std::uint16_t, 3U> kNormalMaxima{{
        400U, 550U, 750U}};

    for (std::uint64_t seed = 0U; seed < 100000U; ++seed) {
        const arpg::dungeon::RoomDensityRoll normal =
            arpg::dungeon::roll_room_density(seed, false);
        for (std::size_t index = 0U; index < kNormalMaxima.size(); ++index) {
            if (found_normal[index]
                    || normal.total_count != kNormalMaxima[index]) {
                continue;
            }
            const checkpoint::RoomDescriptor room = make_room(seed);
            const auto result = arpg::dungeon::build_room_monster_plan(
                room, rules, kGeneratorVersion, *plan);
            ARPG_REQUIRE(result.fault == arpg::dungeon::DungeonFault::none);
            ARPG_REQUIRE(plan->monster_count == kNormalMaxima[index]);
            const arpg::test::Failure failure = verify_plan_contract(
                *plan, room, rules, kGeneratorVersion);
            if (failure.expression != nullptr) return failure;
            found_normal[index] = true;
        }

        const arpg::dungeon::RoomDensityRoll abyss =
            arpg::dungeon::roll_room_density(seed, true);
        if (!found_abyss && abyss.total_count == 1125U) {
            const checkpoint::RoomDescriptor room = make_room(
                seed, checkpoint::DungeonElement::chaos,
                checkpoint::EntrySide::bottom, true, true);
            const auto result = arpg::dungeon::build_room_monster_plan(
                room, rules, kGeneratorVersion, *plan);
            ARPG_REQUIRE(result.fault == arpg::dungeon::DungeonFault::none);
            ARPG_REQUIRE(plan->monster_count == 1125U);
            ARPG_REQUIRE(plan->cell_offsets[kCellCount] == 1125U);
            const arpg::test::Failure failure = verify_plan_contract(
                *plan, room, rules, kGeneratorVersion);
            if (failure.expression != nullptr) return failure;
            found_abyss = true;
        }
        if (found_normal[0] && found_normal[1] && found_normal[2]
                && found_abyss) {
            break;
        }
    }
    ARPG_REQUIRE(found_normal[0]);
    ARPG_REQUIRE(found_normal[1]);
    ARPG_REQUIRE(found_normal[2]);
    ARPG_REQUIRE(found_abyss);
    return {};
}

arpg::test::Failure capacity_failure_clears_every_plan_field() noexcept {
    const arpg::dungeon::DungeonRules rules{};
    const checkpoint::RoomDescriptor room = make_room(0xBAD5EEDULL);
    auto plan = std::make_unique<arpg::combat::RoomMonsterPlan>();
    auto canonical_empty = std::make_unique<arpg::combat::RoomMonsterPlan>();
    const auto valid = arpg::dungeon::build_room_monster_plan(
        room, rules, kGeneratorVersion, *plan);
    ARPG_REQUIRE(valid.fault == arpg::dungeon::DungeonFault::none);
    ARPG_REQUIRE(plan->monster_count > 0U);

    const auto rejected =
        arpg::dungeon::test_support::build_room_monster_plan_with_count(
            room, rules, kGeneratorVersion, 1153U, *plan);
    ARPG_REQUIRE(rejected.fault
        == arpg::dungeon::DungeonFault::population_capacity);
    ARPG_REQUIRE(same_plan_fields(*plan, *canonical_empty, true));
    ARPG_REQUIRE(!arpg::dungeon::room_monster_plan_legal(room, *plan));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"plans are deterministic and noncopyable",
        &plans_are_deterministic_and_noncopyable},
    {"cell spans ordinals roles and direct targets are legal",
        &cell_spans_ordinals_roles_and_direct_targets_are_legal},
    {"singleton cells keep weighted direct target variety",
        &singleton_cells_keep_weighted_direct_target_variety},
    {"unused affix slots do not change canonical plan hash",
        &unused_affix_slots_do_not_change_canonical_plan_hash},
    {"4096 seeds cover every ecology and entry without allocation",
        &every_ecology_entry_and_seed_is_legal_and_allocation_free},
    {"normal and abyss maxima build exact populations",
        &all_normal_maxima_and_abyss_maximum_build_exact_counts},
    {"capacity failure clears every plan field",
        &capacity_failure_clears_every_plan_field},
};

}  // namespace

arpg::test::TestSuite room_population_suite() noexcept {
    return arpg::test::make_suite("room_population", kCases);
}
