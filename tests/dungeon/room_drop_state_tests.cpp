#include "test_framework.hpp"

#include "combat/room_bounds.hpp"
#include "combat/room_monster_plan.hpp"
#include "dungeon/room_drop_spatial_index.hpp"
#include "dungeon/room_drop_state.hpp"

#include <array>
#include <limits>
#include <memory>

namespace {

using arpg::combat::Aabb;
using arpg::combat::RoomMonsterPlan;
using arpg::combat::Vec3;
using arpg::dungeon::GroundHealthPotion;
using arpg::dungeon::GroundItem;
using arpg::dungeon::GroundItemSource;
using arpg::dungeon::GroundMaterial;
using arpg::dungeon::GroundMaterialSource;
using arpg::dungeon::RoomDropCandidateSet;
using arpg::dungeon::RoomDropKind;
using arpg::dungeon::RoomDropState;

std::unique_ptr<RoomMonsterPlan> make_plan(
    const std::uint16_t count) noexcept {
    auto plan = std::make_unique<RoomMonsterPlan>();
    plan->monster_count = count;
    for (std::uint16_t ordinal = 0U; ordinal < count; ++ordinal) {
        auto& monster = plan->monsters[ordinal];
        monster.spawn_ordinal = ordinal;
        monster.home_cell = static_cast<std::uint16_t>(ordinal % 400U);
    }
    return plan;
}

Vec3 cell_center(const std::uint16_t cell) noexcept {
    constexpr float kCellWidth = arpg::combat::room_bounds::width / 20.0F;
    constexpr float kCellDepth = arpg::combat::room_bounds::depth / 20.0F;
    const auto column = static_cast<float>(cell % 20U);
    const auto row = static_cast<float>(cell / 20U);
    return {
        arpg::combat::room_bounds::min_x + (column + 0.5F) * kCellWidth,
        arpg::combat::room_bounds::min_y + (row + 0.5F) * kCellDepth,
        0.0F,
    };
}

arpg::test::Failure global_ordinals_cover_the_full_room() noexcept {
    static_assert(arpg::dungeon::kAuthoritativeEquipmentDropCapacity == 1152U);
    static_assert(arpg::dungeon::kAuthoritativeSecondaryDropCapacity == 2320U);
    static_assert(arpg::dungeon::kRoomDropCellCount == 400U);
    static_assert(arpg::dungeon::kRoomDropCellCapacity == 9U);
    static_assert(arpg::dungeon::kRoomDropReserveCapacity == 16U);
    static_assert(arpg::dungeon::kRoomDropQueryCandidateCapacity == 331U);

    constexpr std::array<std::uint16_t, 4U> kOrdinals{
        191U, 192U, 511U, 1124U};
    for (const std::uint16_t spawn : kOrdinals) {
        ARPG_REQUIRE(arpg::dungeon::equipment_drop_ordinal(spawn) == spawn);
        ARPG_REQUIRE(arpg::dungeon::common_material_ordinal(spawn)
            == static_cast<std::uint16_t>(spawn * 2U));
        ARPG_REQUIRE(arpg::dungeon::secondary_drop_ordinal(spawn)
            == static_cast<std::uint16_t>(spawn * 2U + 1U));
    }
    ARPG_REQUIRE(arpg::dungeon::common_material_ordinal(1124U) == 2248U);
    ARPG_REQUIRE(arpg::dungeon::secondary_drop_ordinal(1124U) == 2249U);
    static_assert(arpg::dungeon::kAbyssSecondaryOrdinalBegin == 2304U);
    return {};
}

arpg::test::Failure high_ordinal_drops_remain_authoritative_after_claims() noexcept {
    auto plan = make_plan(1125U);
    auto state = std::make_unique<RoomDropState>();
    ARPG_REQUIRE(state->reset(*plan));

    constexpr std::array<std::uint16_t, 4U> kOrdinals{
        191U, 192U, 511U, 1124U};
    for (const std::uint16_t spawn : kOrdinals) {
        const Vec3 position = cell_center(plan->monsters[spawn].home_cell);
        GroundItem equipment{};
        equipment.active = true;
        equipment.drop_ordinal = spawn;
        equipment.source = GroundItemSource::monster_drop;
        equipment.position = position;
        equipment.item.id = static_cast<std::uint64_t>(spawn) + 1U;
        ARPG_REQUIRE(state->place_equipment(equipment));

        GroundMaterial material{};
        material.active = true;
        material.ordinal = arpg::dungeon::common_material_ordinal(spawn);
        material.source = GroundMaterialSource::monster_common;
        material.position = position;
        material.material = arpg::items::MaterialId::reinforcement_stone;
        ARPG_REQUIRE(state->place_material(material));

        GroundHealthPotion potion{};
        potion.active = true;
        potion.spawn_ordinal = spawn;
        potion.claim_ordinal = arpg::dungeon::secondary_drop_ordinal(spawn);
        potion.position = position;
        ARPG_REQUIRE(state->place_health_potion(potion));
    }

    ARPG_REQUIRE(state->equipment()[1124U].active);
    ARPG_REQUIRE(state->materials()[2248U].active);
    ARPG_REQUIRE(state->health_potions()[1124U].active);
    ARPG_REQUIRE(state->mark_equipment_claimed(1124U));
    ARPG_REQUIRE(state->spatial_index().last_set_present_records_examined()
        <= arpg::dungeon::kRoomDropCellCapacity
            + arpg::dungeon::kRoomDropReserveCapacity);
    ARPG_REQUIRE(!state->mark_equipment_claimed(1124U));
    ARPG_REQUIRE(state->mark_secondary_claimed(2248U));
    ARPG_REQUIRE(state->spatial_index().last_set_present_records_examined()
        <= arpg::dungeon::kRoomDropCellCapacity);
    ARPG_REQUIRE(!state->mark_secondary_claimed(2248U));
    ARPG_REQUIRE(state->mark_secondary_claimed(2249U));
    ARPG_REQUIRE(state->spatial_index().last_set_present_records_examined()
        <= arpg::dungeon::kRoomDropCellCapacity);
    ARPG_REQUIRE(!state->equipment()[1124U].active);
    ARPG_REQUIRE(!state->materials()[2248U].active);
    ARPG_REQUIRE(!state->health_potions()[1124U].active);
    ARPG_REQUIRE(state->equipment()[192U].active);
    return {};
}

arpg::test::Failure visible_query_has_a_population_independent_ceiling() noexcept {
    auto plan = make_plan(1125U);
    // Saturate the 7x5 center-cell span: three source monsters and all three
    // possible drop records per source cell.
    std::uint16_t spawn = 0U;
    for (std::uint16_t row = 7U; row <= 11U; ++row) {
        for (std::uint16_t column = 7U; column <= 13U; ++column) {
            const std::uint16_t cell = static_cast<std::uint16_t>(row * 20U
                + column);
            for (std::uint16_t local = 0U; local < 3U; ++local) {
                plan->monsters[spawn].home_cell = cell;
                ++spawn;
            }
        }
    }
    auto state = std::make_unique<RoomDropState>();
    ARPG_REQUIRE(state->reset(*plan));
    spawn = 0U;
    for (std::uint16_t row = 7U; row <= 11U; ++row) {
        for (std::uint16_t column = 7U; column <= 13U; ++column) {
            const std::uint16_t cell = static_cast<std::uint16_t>(row * 20U
                + column);
            for (std::uint16_t local = 0U; local < 3U; ++local) {
                const Vec3 position = cell_center(cell);
                GroundItem item{};
                item.active = true;
                item.drop_ordinal = spawn;
                item.position = position;
                item.item.id = static_cast<std::uint64_t>(spawn) + 1U;
                ARPG_REQUIRE(state->place_equipment(item));
                GroundMaterial material{};
                material.active = true;
                material.ordinal = arpg::dungeon::common_material_ordinal(spawn);
                material.position = position;
                material.material = arpg::items::MaterialId::reinforcement_stone;
                ARPG_REQUIRE(state->place_material(material));
                GroundHealthPotion potion{};
                potion.active = true;
                potion.spawn_ordinal = spawn;
                potion.claim_ordinal = arpg::dungeon::secondary_drop_ordinal(spawn);
                potion.position = position;
                ARPG_REQUIRE(state->place_health_potion(potion));
                ++spawn;
            }
        }
    }
    for (std::uint16_t index = 0U; index < 16U; ++index) {
        GroundMaterial reserve{};
        reserve.active = true;
        reserve.ordinal = static_cast<std::uint16_t>(
            arpg::dungeon::kAbyssSecondaryOrdinalBegin + index);
        reserve.source = GroundMaterialSource::abyss_reward;
        reserve.position = {0.0F, 0.0F, 0.0F};
        reserve.material = arpg::items::MaterialId::reinforcement_stone;
        ARPG_REQUIRE(state->place_material(reserve));
    }

    RoomDropCandidateSet result{};
    result.count = 91U;
    result.candidates_examined = 999U;
    result.records[0U].ordinal = 0xFFFFU;
    const Aabb query{{-15.0F, -9.0F, -1.0F}, {17.0F, 2.0F, 1.0F}};
    state->spatial_index().write_candidates(query, result);
    ARPG_REQUIRE(result.fault == arpg::dungeon::RoomDropIndexFault::none);
    ARPG_REQUIRE(result.candidates_examined == 331U);
    ARPG_REQUIRE(result.count <= result.records.size());
    for (std::size_t index = 1U; index < result.count; ++index) {
        const auto& previous = result.records[index - 1U];
        const auto& current = result.records[index];
        ARPG_REQUIRE(previous.home_cell < current.home_cell
            || (previous.home_cell == current.home_cell
                && (static_cast<unsigned>(previous.kind)
                        < static_cast<unsigned>(current.kind)
                    || (previous.kind == current.kind
                        && previous.ordinal < current.ordinal))));
    }
    ARPG_REQUIRE(state->active_equipment_count() == 105U);
    ARPG_REQUIRE(state->active_material_count() == 121U);
    ARPG_REQUIRE(state->active_health_potion_count() == 105U);

    constexpr float kNan = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    const std::array<Aabb, 6U> invalid_bounds{{
        {{kNan, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}},
        {{-1.0F, kNan, -1.0F}, {1.0F, 1.0F, 1.0F}},
        {{-1.0F, -1.0F, kNan}, {1.0F, 1.0F, 1.0F}},
        {{-1.0F, -1.0F, -1.0F}, {kInfinity, 1.0F, 1.0F}},
        {{-1.0F, -1.0F, -1.0F}, {1.0F, -kInfinity, 1.0F}},
        {{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, kInfinity}},
    }};
    for (const Aabb& bounds : invalid_bounds) {
        RoomDropCandidateSet invalid_output{};
        invalid_output.count = 1U;
        invalid_output.candidates_examined = 1U;
        invalid_output.records[0U].present = true;
        state->spatial_index().write_candidates(bounds, invalid_output);
        ARPG_REQUIRE(invalid_output.fault
            == arpg::dungeon::RoomDropIndexFault::invalid_bounds);
        ARPG_REQUIRE(invalid_output.count == 0U);
        ARPG_REQUIRE(invalid_output.candidates_examined == 0U);
        ARPG_REQUIRE(!invalid_output.records[0U].present);
    }
    constexpr float kMaximum = (std::numeric_limits<float>::max)();
    for (const Aabb& bounds : std::array<Aabb, 2U>{{
             {{kMaximum, kMaximum, -1.0F},
                 {kMaximum, kMaximum, 1.0F}},
             {{-kMaximum, -kMaximum, -1.0F},
                 {-kMaximum, -kMaximum, 1.0F}},
         }}) {
        RoomDropCandidateSet extreme_output{};
        state->spatial_index().write_candidates(bounds, extreme_output);
        ARPG_REQUIRE(extreme_output.fault
            == arpg::dungeon::RoomDropIndexFault::none);
        ARPG_REQUIRE(extreme_output.count == 0U);
    }

    auto one_monster = make_plan(1U);
    arpg::dungeon::RoomDropSpatialIndex insertion_index{};
    ARPG_REQUIRE(insertion_index.reset(*one_monster));
    ARPG_REQUIRE(!insertion_index.insert_monster_drop(
        RoomDropKind::equipment, 0U, 0U, {kNan, 0.0F, 0.0F}));
    ARPG_REQUIRE(insertion_index.fault()
        == arpg::dungeon::RoomDropIndexFault::none);
    ARPG_REQUIRE(!insertion_index.insert_abyss_reserve(
        RoomDropKind::material,
        arpg::dungeon::kAbyssSecondaryOrdinalBegin,
        {0.0F, kInfinity, 0.0F}));
    ARPG_REQUIRE(insertion_index.fault()
        == arpg::dungeon::RoomDropIndexFault::none);
    ARPG_REQUIRE(insertion_index.insert_monster_drop(
        RoomDropKind::equipment, 0U, 0U, cell_center(0U)));
    ARPG_REQUIRE(insertion_index.insert_abyss_reserve(
        RoomDropKind::material,
        arpg::dungeon::kAbyssSecondaryOrdinalBegin,
        {0.0F, 0.0F, 0.0F}));
    ARPG_REQUIRE(insertion_index.set_present(RoomDropKind::material,
        arpg::dungeon::kAbyssSecondaryOrdinalBegin, false));
    ARPG_REQUIRE(insertion_index.last_set_present_records_examined()
        <= arpg::dungeon::kRoomDropReserveCapacity);
    return {};
}

arpg::test::Failure shared_secondary_slot_is_mutually_exclusive() noexcept {
    auto plan = make_plan(1U);
    auto state = std::make_unique<RoomDropState>();
    ARPG_REQUIRE(state->reset(*plan));

    GroundMaterial coupon{};
    coupon.active = true;
    coupon.ordinal = arpg::dungeon::secondary_drop_ordinal(0U);
    coupon.source = GroundMaterialSource::monster_coupon;
    coupon.position = cell_center(plan->monsters[0U].home_cell);
    coupon.material = arpg::items::MaterialId::reinforcement_stone;
    ARPG_REQUIRE(state->place_material(coupon));

    GroundHealthPotion potion{};
    potion.active = true;
    potion.spawn_ordinal = 0U;
    potion.claim_ordinal = coupon.ordinal;
    potion.position = coupon.position;
    ARPG_REQUIRE(!state->place_health_potion(potion));

    ARPG_REQUIRE(state->reset(*plan));
    ARPG_REQUIRE(state->place_health_potion(potion));
    ARPG_REQUIRE(state->first_health_potion_spawn() == 0U);
    ARPG_REQUIRE(!state->place_material(coupon));
    ARPG_REQUIRE(state->mark_secondary_claimed(potion.claim_ordinal));
    ARPG_REQUIRE(state->first_health_potion_spawn() == 0xFFFFU);
    ARPG_REQUIRE(state->active_health_potion_count() == 0U);

    std::array<std::uint64_t, arpg::limits::kRoomEquipmentClaimWords>
        equipment_claims{};
    std::array<std::uint64_t, arpg::limits::kRoomSecondaryClaimWords>
        secondary_claims{};
    secondary_claims[0U] = std::uint64_t{1U} << coupon.ordinal;
    state->restore_claim_bits(equipment_claims, secondary_claims);
    ARPG_REQUIRE(!state->place_health_potion(potion));
    return {};
}

arpg::test::Failure clear_removes_spatial_candidates() noexcept {
    auto plan = make_plan(1U);
    auto state = std::make_unique<RoomDropState>();
    ARPG_REQUIRE(state->reset(*plan));
    GroundItem item{};
    item.active = true;
    item.drop_ordinal = 0U;
    item.position = cell_center(plan->monsters[0U].home_cell);
    item.item.id = 1U;
    ARPG_REQUIRE(state->place_equipment(item));

    RoomDropCandidateSet before{};
    const Aabb nearby{{item.position.x - 0.1F, item.position.y - 0.1F,
                           -1.0F},
        {item.position.x + 0.1F, item.position.y + 0.1F, 1.0F}};
    state->spatial_index().write_candidates(nearby, before);
    ARPG_REQUIRE(before.count == 1U);

    state->clear();
    RoomDropCandidateSet after{};
    state->spatial_index().write_candidates(nearby, after);
    ARPG_REQUIRE(after.count == 0U);
    ARPG_REQUIRE(after.candidates_examined == 0U);

    ARPG_REQUIRE(state->reset(*plan));
    state->equipment()[0U] = item;
    ARPG_REQUIRE(!state->mark_equipment_claimed(0U));
    ARPG_REQUIRE(!state->equipment_claimed(0U));
    ARPG_REQUIRE(state->equipment()[0U].active);

    GroundMaterial unindexed{};
    unindexed.active = true;
    unindexed.ordinal = 0U;
    unindexed.position = item.position;
    unindexed.material = arpg::items::MaterialId::reinforcement_stone;
    state->materials()[0U] = unindexed;
    ARPG_REQUIRE(!state->mark_secondary_claimed(0U));
    ARPG_REQUIRE(!state->secondary_claimed(0U));
    ARPG_REQUIRE(state->materials()[0U].active);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"global drop ordinals cover 1152 monsters",
        &global_ordinals_cover_the_full_room},
    {"high ordinals remain authoritative after claims",
        &high_ordinal_drops_remain_authoritative_after_claims},
    {"visible query examines at most 331 candidates",
        &visible_query_has_a_population_independent_ceiling},
    {"secondary coupon and potion share one slot",
        &shared_secondary_slot_is_mutually_exclusive},
    {"clear removes stale spatial candidates",
        &clear_removes_spatial_candidates},
};

}  // namespace

arpg::test::TestSuite room_drop_state_suite() noexcept {
    return arpg::test::make_suite("room_drop_state", kCases);
}
