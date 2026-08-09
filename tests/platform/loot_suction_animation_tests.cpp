#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "loot_suction_animation.hpp"

#include <cstddef>
#include <cstdint>

namespace {

namespace combat = arpg::combat;
namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace platform = arpg::platform;

constexpr platform::CombatCameraView kCamera{
    {}, combat::room_bounds::width, combat::room_bounds::depth};
constexpr combat::Vec3 kPlayerPosition{0.0F, 0.0F, 0.0F};
constexpr float kWidth = 1280.0F;
constexpr float kHeight = 720.0F;

[[nodiscard]] bool same_color(platform::Rgba8 left,
    platform::Rgba8 right) noexcept {
    return left.r == right.r && left.g == right.g && left.b == right.b
        && left.a == right.a;
}

platform::DungeonRenderStatus saved_equipment_receipt(
    std::uint64_t generation, std::uint64_t item_id) noexcept {
    platform::DungeonRenderStatus status{};
    status.indicator = platform::SaveIndicator::saved;
    status.loot_pickup = {true, generation, item_id, 3U, 24U,
        items::ItemRarity::rare, dungeon::GroundItemSource::monster_drop};
    return status;
}

dungeon::GroundItemSnapshot equipment(
    std::uint16_t ordinal, std::uint64_t item_id,
    combat::Vec3 position = {9.0F, 0.0F, 0.0F}) noexcept {
    return {ordinal, dungeon::GroundItemSource::monster_drop, 0xFFU,
        position, item_id, 3U, 24U, items::ItemSlot::weapon,
        items::ItemRarity::rare};
}

dungeon::GroundMaterialSnapshot material(
    std::uint16_t ordinal, items::MaterialId id,
    combat::Vec3 position = {9.0F, 0.0F, 0.0F}) noexcept {
    return {ordinal, dungeon::GroundMaterialSource::monster_common,
        position, id};
}

void append(dungeon::DungeonSnapshot& snapshot,
    dungeon::GroundItemSnapshot item) noexcept {
    snapshot.ground_items[snapshot.ground_item_count++] = item;
}

void append(dungeon::DungeonSnapshot& snapshot,
    dungeon::GroundMaterialSnapshot item) noexcept {
    snapshot.ground_materials[snapshot.ground_material_count++] = item;
}

arpg::test::Failure committed_equipment_disappearance_flies_to_player_waist()
    noexcept {
    platform::LootSuctionState state{};
    dungeon::DungeonSnapshot empty{};
    state.observe(empty, empty, {});

    dungeon::DungeonSnapshot previous{};
    append(previous, equipment(4U, 42U));
    dungeon::DungeonSnapshot current{};
    state.observe(previous, current, saved_equipment_receipt(1U, 42U));

    ARPG_REQUIRE(state.active_count() == 1U);
    const platform::LootSuctionPlan start = state.build_plan(
        kCamera, kPlayerPosition, kWidth, kHeight);
    ARPG_REQUIRE(start.count == 1U);
    ARPG_REQUIRE(start.flights[0].center.x > start.destination.x);
    ARPG_REQUIRE(start.flights[0].equipment);
    ARPG_REQUIRE(start.flights[0].sprite
        == platform::ground_loot_item_sprite(items::ItemSlot::weapon));

    state.update(0.35F, false);
    const platform::LootSuctionPlan middle = state.build_plan(
        kCamera, kPlayerPosition, kWidth, kHeight);
    ARPG_REQUIRE(middle.flights[0].center.x < start.flights[0].center.x);
    ARPG_REQUIRE(middle.flights[0].center.x > middle.destination.x);
    ARPG_REQUIRE(middle.flights[0].center.y < start.flights[0].center.y
        + (middle.destination.y - start.flights[0].center.y) * 0.75F);
    return {};
}

arpg::test::Failure failed_or_unremoved_equipment_never_starts_flight()
    noexcept {
    platform::LootSuctionState state{};
    dungeon::DungeonSnapshot empty{};
    state.observe(empty, empty, {});

    dungeon::DungeonSnapshot previous{};
    append(previous, equipment(1U, 42U));
    auto failed = saved_equipment_receipt(1U, 42U);
    failed.indicator = platform::SaveIndicator::error;
    state.observe(previous, empty, failed);
    ARPG_REQUIRE(state.active_count() == 0U);

    state.clear();
    state.observe(empty, empty, {});
    state.observe(previous, previous, saved_equipment_receipt(1U, 42U));
    ARPG_REQUIRE(state.active_count() == 0U);
    return {};
}

arpg::test::Failure same_equipment_receipt_is_not_duplicated() noexcept {
    platform::LootSuctionState state{};
    dungeon::DungeonSnapshot empty{};
    state.observe(empty, empty, {});
    dungeon::DungeonSnapshot previous{};
    append(previous, equipment(1U, 42U));
    const auto receipt = saved_equipment_receipt(1U, 42U);
    state.observe(previous, empty, receipt);
    state.observe(previous, empty, receipt);
    ARPG_REQUIRE(state.active_count() == 1U);
    return {};
}

arpg::test::Failure material_receipt_only_flies_disappeared_matching_ordinals()
    noexcept {
    platform::LootSuctionState state{};
    dungeon::DungeonSnapshot empty{};
    state.observe(empty, empty, {});

    dungeon::DungeonSnapshot previous{};
    previous.room_index = 7U;
    previous.room_instance_generation = 11U;
    append(previous, material(3U, items::MaterialId::chaos));
    append(previous, material(4U, items::MaterialId::exalt));
    dungeon::DungeonSnapshot current{};
    current.room_index = 7U;
    current.room_instance_generation = 11U;
    append(current, material(4U, items::MaterialId::exalt));
    current.material_pickup_receipt.valid = true;
    current.material_pickup_receipt.commit_generation = 1U;
    current.material_pickup_receipt.counts[
        items::material_index(items::MaterialId::chaos)] = 1U;
    platform::DungeonRenderStatus committed{};
    committed.indicator = platform::SaveIndicator::saved;
    state.observe(previous, current, committed);

    const auto plan = state.build_plan(kCamera, kPlayerPosition, kWidth, kHeight);
    ARPG_REQUIRE(plan.count == 1U);
    ARPG_REQUIRE(!plan.flights[0].equipment);
    ARPG_REQUIRE(plan.flights[0].sprite
        == platform::material_loot_sprite(items::MaterialId::chaos));
    ARPG_REQUIRE(same_color(plan.flights[0].color,
        platform::material_color(items::MaterialId::chaos)));
    return {};
}

arpg::test::Failure pause_freezes_and_arrival_reclaims_with_pulse() noexcept {
    platform::LootSuctionState state{};
    dungeon::DungeonSnapshot empty{};
    state.observe(empty, empty, {});
    dungeon::DungeonSnapshot previous{};
    append(previous, equipment(1U, 42U));
    state.observe(previous, empty, saved_equipment_receipt(1U, 42U));
    const auto before = state.build_plan(kCamera, kPlayerPosition, kWidth, kHeight);
    state.update(0.35F, true);
    const auto frozen = state.build_plan(kCamera, kPlayerPosition, kWidth, kHeight);
    ARPG_REQUIRE(frozen.flights[0].center.x == before.flights[0].center.x);
    ARPG_REQUIRE(frozen.flights[0].center.y == before.flights[0].center.y);

    state.update(0.45F, false);
    const auto arrived = state.build_plan(kCamera, kPlayerPosition, kWidth, kHeight);
    ARPG_REQUIRE(state.active_count() == 0U);
    ARPG_REQUIRE(arrived.count == 0U);
    ARPG_REQUIRE(arrived.destination_pulse > 0.0F);
    return {};
}

arpg::test::Failure full_capacity_replaces_most_elapsed_flight_and_allocates_nothing()
    noexcept {
    platform::LootSuctionState state{};
    dungeon::DungeonSnapshot empty{};
    state.observe(empty, empty, {});
    for (std::uint64_t index = 1U; index <= 12U; ++index) {
        dungeon::DungeonSnapshot previous{};
        append(previous, equipment(static_cast<std::uint16_t>(index), index,
            {static_cast<float>(index), 0.0F, 0.0F}));
        state.observe(previous, empty, saved_equipment_receipt(index, index));
        state.update(0.01F, false);
    }
    ARPG_REQUIRE(state.active_count() == 12U);
    dungeon::DungeonSnapshot latest{};
    append(latest, equipment(13U, 13U, {13.0F, 0.0F, 0.0F}));
    state.observe(latest, empty, saved_equipment_receipt(13U, 13U));
    const auto replacement = state.build_plan(
        kCamera, kPlayerPosition, kWidth, kHeight);
    bool has_oldest_origin{};
    bool has_latest_origin{};
    for (std::size_t index{}; index < replacement.count; ++index) {
        has_oldest_origin = has_oldest_origin
            || replacement.flights[index].world_position.x == 1.0F;
        has_latest_origin = has_latest_origin
            || replacement.flights[index].world_position.x == 13.0F;
    }
    ARPG_REQUIRE(replacement.count == 12U);
    ARPG_REQUIRE(!has_oldest_origin);
    ARPG_REQUIRE(has_latest_origin);
    ARPG_REQUIRE(state.active_count() == 12U);
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t index{}; index < 100000U; ++index) {
        state.observe(empty, empty, saved_equipment_receipt(13U, 13U));
        static_cast<void>(state.build_plan(kCamera, kPlayerPosition,
            kWidth, kHeight));
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"equipment trajectory to player waist",
        &committed_equipment_disappearance_flies_to_player_waist},
    {"failed or still present equipment", &failed_or_unremoved_equipment_never_starts_flight},
    {"duplicate equipment receipt", &same_equipment_receipt_is_not_duplicated},
    {"material only disappeared ordinals", &material_receipt_only_flies_disappeared_matching_ordinals},
    {"pause and arrival pulse", &pause_freezes_and_arrival_reclaims_with_pulse},
    {"full capacity replacement and zero allocation",
        &full_capacity_replaces_most_elapsed_flight_and_allocates_nothing},
};

}  // namespace

arpg::test::TestSuite loot_suction_animation_suite() noexcept {
    return arpg::test::make_suite("loot_suction_animation", kCases);
}
