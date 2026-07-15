#include "test_framework.hpp"

#include "inventory_view_math.hpp"

#include "items/item_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using namespace arpg;

bool inside(Rectangle inner, Rectangle outer) noexcept {
    return inner.x >= outer.x && inner.y >= outer.y
        && inner.x + inner.width <= outer.x + outer.width
        && inner.y + inner.height <= outer.y + outer.height;
}

bool separated(Rectangle left, Rectangle right) noexcept {
    return left.x + left.width <= right.x
        || right.x + right.width <= left.x
        || left.y + left.height <= right.y
        || right.y + right.height <= left.y;
}

test::Failure layouts_are_bounded_and_non_overlapping() noexcept {
    constexpr std::array<std::array<int, 2>, 3> kSizes{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto& size : kSizes) {
        const Rectangle viewport{0.0F, 0.0F,
            static_cast<float>(size[0]), static_cast<float>(size[1])};
        const platform::InventoryLayout layout =
            platform::inventory_layout(size[0], size[1]);
        ARPG_REQUIRE(inside(layout.equipment, viewport));
        ARPG_REQUIRE(inside(layout.grid, viewport));
        ARPG_REQUIRE(inside(layout.detail, viewport));
        ARPG_REQUIRE(separated(layout.equipment, layout.grid));
        ARPG_REQUIRE(separated(layout.grid, layout.detail));
        ARPG_REQUIRE(layout.equipment.width > 0.0F);
        ARPG_REQUIRE(layout.grid.width > 0.0F);
        ARPG_REQUIRE(layout.detail.width > 0.0F);
    }
    return {};
}

test::Failure visible_range_clamps_first_and_last_rows() noexcept {
    const auto first = platform::visible_grid_range(100U, 4, -9.0F,
        170.0F, 50.0F);
    ARPG_REQUIRE(first.first == 0U);
    ARPG_REQUIRE(first.count == 16U);

    const float maximum = platform::clamp_inventory_scroll_rows(
        100U, 4, 99.0F, 170.0F, 50.0F);
    ARPG_REQUIRE(test::near(maximum, 21.6F, 0.001F));
    const auto last = platform::visible_grid_range(100U, 4, maximum,
        170.0F, 50.0F);
    ARPG_REQUIRE(last.first == 84U);
    ARPG_REQUIRE(last.count == 16U);

    const auto empty = platform::visible_grid_range(0U, 0, 1.0F,
        170.0F, 50.0F);
    ARPG_REQUIRE(empty.first == 0U);
    ARPG_REQUIRE(empty.count == 0U);
    return {};
}

items::ItemInstance make_item(std::uint64_t id, std::uint8_t base,
    items::ItemRarity rarity) noexcept {
    items::ItemInstance item{};
    item.id = id;
    item.base_id = base;
    item.rarity = rarity;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

test::Failure filtering_preserves_acquisition_order_and_excludes_equipped() noexcept {
    items::ItemOwnershipState state{};
    state.items = {
        make_item(11U, 1U, items::ItemRarity::normal),
        make_item(12U, 2U, items::ItemRarity::magic),
        make_item(13U, 1U, items::ItemRarity::rare),
        make_item(14U, 2U, items::ItemRarity::magic),
    };
    state.equipment.equipped_ids[0] = 11U;
    const platform::InventoryFilter filter{
        items::ItemSlot::helmet, items::ItemRarity::magic};
    const std::vector<std::size_t> indices =
        platform::filtered_inventory_indices(state, filter);
    ARPG_REQUIRE(indices.size() == 2U);
    ARPG_REQUIRE(indices[0] == 1U);
    ARPG_REQUIRE(indices[1] == 3U);
    return {};
}

test::Failure click_tracker_distinguishes_single_and_double_click() noexcept {
    platform::InventoryClickTracker tracker{};
    ARPG_REQUIRE(platform::register_inventory_click(tracker, 7U, 10.0)
        == platform::InventoryClickKind::single);
    ARPG_REQUIRE(platform::register_inventory_click(tracker, 7U, 10.29)
        == platform::InventoryClickKind::double_click);
    ARPG_REQUIRE(platform::register_inventory_click(tracker, 7U, 10.60)
        == platform::InventoryClickKind::single);
    ARPG_REQUIRE(platform::register_inventory_click(tracker, 8U, 10.70)
        == platform::InventoryClickKind::single);
    return {};
}

test::Failure ctrl_selection_toggles_and_never_exceeds_three() noexcept {
    platform::RecipeSelection selection{};
    ARPG_REQUIRE(platform::toggle_recipe_selection(selection, 1U));
    ARPG_REQUIRE(platform::toggle_recipe_selection(selection, 2U));
    ARPG_REQUIRE(platform::toggle_recipe_selection(selection, 3U));
    ARPG_REQUIRE(selection.count == 3U);
    ARPG_REQUIRE(!platform::toggle_recipe_selection(selection, 4U));
    ARPG_REQUIRE(selection.count == 3U);
    ARPG_REQUIRE(platform::toggle_recipe_selection(selection, 2U));
    ARPG_REQUIRE(selection.count == 2U);
    ARPG_REQUIRE(platform::toggle_recipe_selection(selection, 4U));
    ARPG_REQUIRE(selection.ids[0] == 1U);
    ARPG_REQUIRE(selection.ids[1] == 3U);
    ARPG_REQUIRE(selection.ids[2] == 4U);
    return {};
}

test::Failure equipped_slot_hit_requires_an_occupied_slot() noexcept {
    const platform::InventoryLayout layout = platform::inventory_layout(1280, 720);
    items::EquipmentState equipment{};
    equipment.equipped_ids[2] = 42U;
    const Rectangle occupied = platform::equipment_slot_rectangle(
        layout, items::ItemSlot::chest);
    const Vector2 center{occupied.x + occupied.width * 0.5F,
        occupied.y + occupied.height * 0.5F};
    const auto hit = platform::hit_test_equipped_slot(center, layout, equipment);
    ARPG_REQUIRE(hit.has_value());
    ARPG_REQUIRE(*hit == items::ItemSlot::chest);

    const Rectangle empty = platform::equipment_slot_rectangle(
        layout, items::ItemSlot::weapon);
    ARPG_REQUIRE(!platform::hit_test_equipped_slot(
        {empty.x + 2.0F, empty.y + 2.0F}, layout, equipment).has_value());
    return {};
}

test::Failure inventory_and_passive_overlays_are_mutually_exclusive() noexcept {
    ARPG_REQUIRE(platform::inventory_can_open(true, false));
    ARPG_REQUIRE(!platform::inventory_can_open(true, true));
    ARPG_REQUIRE(!platform::inventory_can_open(false, false));
    ARPG_REQUIRE(platform::passive_overlay_can_toggle(false));
    ARPG_REQUIRE(!platform::passive_overlay_can_toggle(true));
    const auto open = platform::inventory_input_gate(true);
    ARPG_REQUIRE(!open.forward_actions);
    ARPG_REQUIRE(!open.forward_movement);
    ARPG_REQUIRE(!open.forward_descent);
    ARPG_REQUIRE(!open.forward_room_reset);
    ARPG_REQUIRE(!open.forward_passive_toggle);
    const auto closed = platform::inventory_input_gate(false);
    ARPG_REQUIRE(closed.forward_actions);
    ARPG_REQUIRE(closed.forward_movement);
    ARPG_REQUIRE(closed.forward_descent);
    ARPG_REQUIRE(closed.forward_room_reset);
    ARPG_REQUIRE(closed.forward_passive_toggle);
    return {};
}

test::Failure comparison_uses_full_build_fields() noexcept {
    combat::PlayerCombatBuild current{};
    current.values.max_health = 100;
    current.values.armor = 1000;
    current.values.evasion = 250;
    current.values.damage_reduction[0] = 1200;
    current.values.damage_reduction_cap_bonus[0] = 100;
    current.values.attack_speed = 10000;
    current.weapon_physical = 20000;
    current.local_attack_speed_bp = 500;
    combat::PlayerCombatBuild candidate = current;
    candidate.values.max_health = 114;
    candidate.values.armor = 4000;
    candidate.values.evasion = 200;
    candidate.values.damage_reduction[0] = 1500;
    candidate.values.damage_reduction_cap_bonus[0] = 300;
    candidate.values.attack_speed = 11200;
    candidate.weapon_physical = 35000;
    candidate.local_attack_speed_bp = 900;

    const platform::BuildDifference difference =
        platform::compare_player_builds(current, candidate);
    ARPG_REQUIRE(difference.max_health == 14);
    ARPG_REQUIRE(difference.armor == 3000);
    ARPG_REQUIRE(difference.evasion == -50);
    ARPG_REQUIRE(difference.damage_reduction[0] == 300);
    ARPG_REQUIRE(difference.damage_reduction_cap_bonus[0] == 200);
    ARPG_REQUIRE(difference.attack_speed == 1200);
    ARPG_REQUIRE(difference.weapon_physical == 15000);
    ARPG_REQUIRE(difference.local_attack_speed_bp == 400);
    return {};
}

constexpr test::TestCase kCases[] = {
    {"inventory layouts", &layouts_are_bounded_and_non_overlapping},
    {"inventory visible range", &visible_range_clamps_first_and_last_rows},
    {"inventory filtering", &filtering_preserves_acquisition_order_and_excludes_equipped},
    {"inventory click tracker", &click_tracker_distinguishes_single_and_double_click},
    {"inventory recipe selection", &ctrl_selection_toggles_and_never_exceeds_three},
    {"inventory equipment hit", &equipped_slot_hit_requires_an_occupied_slot},
    {"inventory overlay gates", &inventory_and_passive_overlays_are_mutually_exclusive},
    {"inventory build comparison", &comparison_uses_full_build_fields},
};

}  // namespace

arpg::test::TestSuite inventory_view_math_suite() noexcept {
    return arpg::test::make_suite("inventory_view_math", kCases);
}
