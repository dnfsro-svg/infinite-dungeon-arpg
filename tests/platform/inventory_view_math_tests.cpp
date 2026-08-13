#include "test_framework.hpp"

#include "inventory_view_math.hpp"

#include "items/item_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
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
        {{1280, 720}}, {{1600, 900}}, {{1920, 1080}},
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
    state.equipment.equipped_ids[1] = 12U;
    const platform::InventoryFilter filter{
        items::ItemSlot::helmet, items::ItemRarity::magic};
    const std::vector<std::size_t> indices =
        platform::filtered_inventory_indices(state, filter);
    ARPG_REQUIRE(indices.size() == 1U);
    ARPG_REQUIRE(indices[0] == 3U);
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
    current.values.melee_damage = 10300;
    current.values.impulse_scale = 9700;
    for (std::size_t index = 0U; index < 5U; ++index) {
        current.values.flat_damage[index] = 1000 + static_cast<std::int64_t>(index);
        current.values.damage_increased[index] = 10000 + static_cast<std::int32_t>(index);
    }
    current.weapon_physical = 20000;
    current.local_attack_speed_bp = 500;
    combat::PlayerCombatBuild candidate = current;
    candidate.values.max_health = 114;
    candidate.values.armor = 4000;
    candidate.values.evasion = 200;
    candidate.values.damage_reduction[0] = 1500;
    candidate.values.damage_reduction_cap_bonus[0] = 300;
    candidate.values.attack_speed = 11200;
    candidate.values.melee_damage = 11100;
    candidate.values.impulse_scale = 10400;
    for (std::size_t index = 0U; index < 5U; ++index) {
        candidate.values.flat_damage[index] += static_cast<std::int64_t>(10U + index);
        candidate.values.damage_increased[index] += static_cast<std::int32_t>(20U + index);
    }
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
    ARPG_REQUIRE(difference.melee_damage == 800);
    ARPG_REQUIRE(difference.impulse_scale == 700);
    for (std::size_t index = 0U; index < 5U; ++index) {
        ARPG_REQUIRE(difference.flat_damage[index]
            == static_cast<std::int64_t>(10U + index));
        ARPG_REQUIRE(difference.damage_increased[index]
            == static_cast<std::int64_t>(20U + index));
    }
    ARPG_REQUIRE(difference.armor_reduction_bp
        == modifiers::rating_to_basis_points(candidate.values.armor)
            - modifiers::rating_to_basis_points(current.values.armor));
    ARPG_REQUIRE(difference.evasion_rate_bp
        == modifiers::rating_to_basis_points(candidate.values.evasion)
            - modifiers::rating_to_basis_points(current.values.evasion));
    return {};
}

test::Failure recipe_cache_rejects_same_slot_different_bases() noexcept {
    items::ItemOwnershipState state{};
    state.items = {
        make_item(701U, 1U, items::ItemRarity::normal),
        make_item(702U, 7U, items::ItemRarity::normal),
        make_item(703U, 8U, items::ItemRarity::normal),
    };
    platform::RecipeSelection recipe{};
    recipe.ids = {{701U, 702U, 703U}};
    recipe.count = 3U;
    platform::InventoryViewCache cache{};
    platform::refresh_inventory_view_cache(cache, state, 91U, {}, 0U, recipe);
    ARPG_REQUIRE(!platform::cached_recipe_ready(cache));
    return {};
}

test::Failure view_cache_never_rescans_stable_maximum_inventory() noexcept {
    items::ItemOwnershipState state{};
    state.items.reserve(65535U);
    for (std::uint64_t id = 1U; id <= 65535U; ++id) {
        state.items.push_back(make_item(id,
            static_cast<std::uint8_t>((id - 1U) % 6U + 1U),
            id % 3U == 0U ? items::ItemRarity::magic
                          : items::ItemRarity::normal));
    }
    platform::RecipeSelection recipe{};
    recipe.ids = {65521U, 65527U, 65533U};
    recipe.count = 3U;
    platform::InventoryViewCache cache{};
    platform::refresh_inventory_view_cache(cache, state, 77U, {},
        65535U, recipe);
    ARPG_REQUIRE(cache.item_inspection_count == 65535U);
    ARPG_REQUIRE(cache.refresh_count == 1U);
    const std::size_t after_refresh = cache.item_inspection_count;
    for (int frame = 0; frame < 1000; ++frame) {
        ARPG_REQUIRE(platform::cached_selected_item(cache, state) != nullptr);
        ARPG_REQUIRE(platform::cached_recipe_ready(cache));
        const auto range = platform::visible_grid_range(
            cache.filtered_indices.size(), 4, 0.0F, 170.0F, 50.0F);
        ARPG_REQUIRE(range.count == 16U);
        platform::refresh_inventory_view_cache(cache, state, 77U, {},
            65535U, recipe);
    }
    ARPG_REQUIRE(cache.item_inspection_count == after_refresh);
    ARPG_REQUIRE(cache.refresh_count == 1U);

    platform::refresh_inventory_view_cache(cache, state, 77U, {},
        65534U, recipe);
    ARPG_REQUIRE(cache.item_inspection_count == after_refresh + 65535U);
    ARPG_REQUIRE(cache.refresh_count == 2U);
    return {};
}

test::Failure attribute_labels_are_human_readable_and_scoped() noexcept {
    const items::AffixDefinition* const physical =
        items::affix_definition(2U);
    ARPG_REQUIRE(physical != nullptr);
    const auto local = platform::item_attribute_label(
        physical->effect, physical->stat, physical->operation,
        items::ItemSlot::weapon, 0xFFU);
    ARPG_REQUIRE(std::string_view{local.name} == "Weapon physical");
    ARPG_REQUIRE(std::string_view{local.scope} == "LOCAL");
    ARPG_REQUIRE(std::string_view{local.suffix} == "%");
    ARPG_REQUIRE(std::string_view{local.qualifier} == " inc");
    ARPG_REQUIRE(test::near(platform::item_attribute_display_value(
        physical->values[7], local), 42.0));

    const items::AffixDefinition* const attack_speed =
        items::affix_definition(101U);
    ARPG_REQUIRE(attack_speed != nullptr);
    const auto weapon_speed = platform::item_attribute_label(
        attack_speed->effect, attack_speed->stat, attack_speed->operation,
        items::ItemSlot::weapon, 0xFFU);
    const auto glove_speed = platform::item_attribute_label(
        attack_speed->effect, attack_speed->stat, attack_speed->operation,
        items::ItemSlot::gloves, 0xFFU);
    ARPG_REQUIRE(std::string_view{weapon_speed.scope} == "LOCAL");
    ARPG_REQUIRE(std::string_view{glove_speed.scope} == "GLOBAL");
    ARPG_REQUIRE(std::string_view{weapon_speed.suffix} == "%");
    ARPG_REQUIRE(std::string_view{glove_speed.suffix} == "%");
    ARPG_REQUIRE(test::near(platform::item_attribute_display_value(
        attack_speed->values[7], weapon_speed), 12.0));

    const auto global = platform::item_attribute_label(
        items::ItemEffectKind::global_modifier,
        modifiers::StatId::max_health,
        modifiers::ModifierOperation::flat,
        items::ItemSlot::helmet, 0xFFU);
    ARPG_REQUIRE(std::string_view{global.name} == "Maximum health");
    ARPG_REQUIRE(std::string_view{global.scope} == "GLOBAL");
    ARPG_REQUIRE(std::string_view{global.suffix}.empty());
    ARPG_REQUIRE(std::string_view{global.qualifier}.empty());

    const auto variant = platform::item_attribute_label(
        items::ItemEffectKind::variant_element_damage_reduction_cap,
        modifiers::StatId::fire_damage_reduction_cap,
        modifiers::ModifierOperation::flat,
        items::ItemSlot::accessory, 2U);
    ARPG_REQUIRE(std::string_view{variant.name} == "Lightning DR cap");
    ARPG_REQUIRE(std::string_view{variant.scope} == "GLOBAL");
    ARPG_REQUIRE(std::string_view{variant.suffix} == "%");
    ARPG_REQUIRE(std::string_view{variant.qualifier}.empty());
    ARPG_REQUIRE(test::near(
        platform::item_attribute_display_value(2000, variant), 20.0));
    ARPG_REQUIRE(test::near(
        platform::item_attribute_display_value(46, global), 46.0));
    return {};
}

test::Failure all_catalog_affixes_use_semantic_value_units() noexcept {
    constexpr std::array<std::uint16_t, 24> kIds{{
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U,
        101U, 102U, 103U, 104U, 105U, 106U, 107U, 108U, 109U,
        110U, 111U, 112U,
    }};
    constexpr std::array<bool, 24> kPercent{{
        false, true, false, false, false, false,
        true, true, true, true, false, false,
        true, true, false, false, true, true,
        true, true, true, true, true, true,
    }};
    for (std::size_t index = 0U; index < kIds.size(); ++index) {
        const items::AffixDefinition* const affix =
            items::affix_definition(kIds[index]);
        ARPG_REQUIRE(affix != nullptr);
        items::ItemSlot slot = items::ItemSlot::weapon;
        while ((affix->slot_mask & items::slot_bit(slot)) == 0U) {
            slot = static_cast<items::ItemSlot>(
                static_cast<std::uint8_t>(slot) + 1U);
        }
        const auto label = platform::item_attribute_label(
            affix->effect, affix->stat, affix->operation, slot,
            affix->id == 112U ? 3U : 0xFFU);
        ARPG_REQUIRE(label.name != nullptr);
        ARPG_REQUIRE(std::string_view{label.name} != "Attribute");
        ARPG_REQUIRE((std::string_view{label.suffix} == "%")
            == kPercent[index]);
        const double displayed = platform::item_attribute_display_value(
            affix->values[7], label);
        const double expected = kPercent[index]
            ? static_cast<double>(affix->values[7]) / 100.0
            : static_cast<double>(affix->values[7]);
        ARPG_REQUIRE(test::near(displayed, expected));
    }
    return {};
}

test::Failure detail_content_stays_inside_all_required_viewports() noexcept {
    constexpr std::array<std::array<int, 2>, 3> kSizes{{
        {{1280, 720}}, {{1600, 900}}, {{1920, 1080}},
    }};
    for (const auto& size : kSizes) {
        const platform::InventoryLayout layout =
            platform::inventory_layout(size[0], size[1]);
        ARPG_REQUIRE(platform::detail_content_fits(layout, 21U));
        for (std::size_t line = 0U; line < 21U; ++line) {
            const Rectangle rectangle =
                platform::detail_line_rectangle(layout, line);
            ARPG_REQUIRE(inside(rectangle, layout.detail));
            ARPG_REQUIRE(rectangle.width >= 211.0F);
        }
        ARPG_REQUIRE(platform::detail_line_character_capacity(layout) >= 35U);
        constexpr std::array<std::string_view, 5> kLongestLines{{
            "T1 Suf [GLOBAL] Lightning DR cap +6%",
            "T1 Pre [GLOBAL] Fire damage +42% inc",
            "Base [GLOBAL] All element DR +10%",
            "LocalAtk +999.9%  Weapon +1e+18",
            "Evasion -1e+18  Chance -99.99%",
        }};
        for (const std::string_view line : kLongestLines) {
            ARPG_REQUIRE(platform::detail_text_fits(layout, line));
        }
    }
    return {};
}

test::Failure inventory_and_skill_text_boxes_use_disjoint_safe_areas() noexcept {
    constexpr std::array<std::array<int, 2>, 2> kSizes{{
        {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto size : kSizes) {
        const Rectangle viewport{0.0F, 0.0F,
            static_cast<float>(size[0]), static_cast<float>(size[1])};
        const platform::InventoryLayout panels =
            platform::inventory_layout(size[0], size[1]);
        const platform::ActiveSkillLoadoutLayout skill =
            platform::active_skill_loadout_layout(size[0], size[1]);
        const platform::InventoryTextSafeLayout text =
            platform::inventory_text_safe_layout(size[0], size[1]);
        ARPG_REQUIRE(inside(text.page_title, viewport));
        ARPG_REQUIRE(separated(text.page_title,
            skill.equipment_page_button));
        ARPG_REQUIRE(separated(text.page_title,
            skill.skill_stones_page_button));
        ARPG_REQUIRE(inside(text.equipment_panel_title, panels.equipment));
        ARPG_REQUIRE(inside(text.grid_panel_title, panels.grid));
        ARPG_REQUIRE(inside(text.detail_panel_title, panels.detail));
        ARPG_REQUIRE(inside(text.skill_panel_title, skill.panel));
        ARPG_REQUIRE(separated(text.skill_panel_title, skill.main_slots[0]));
        ARPG_REQUIRE(separated(text.support_section_title,
            skill.support_slots[0]));
        ARPG_REQUIRE(separated(text.inventory_section_title,
            skill.inventory_slots[0]));
    }
    return {};
}

test::Failure full_hd_inventory_and_skill_geometry_scales_by_one_and_a_half()
    noexcept {
    const platform::InventoryLayout inventory_720 =
        platform::inventory_layout(1280, 720);
    const platform::InventoryLayout inventory_1080 =
        platform::inventory_layout(1920, 1080);
    const platform::ActiveSkillLoadoutLayout skill_720 =
        platform::active_skill_loadout_layout(1280, 720);
    const platform::ActiveSkillLoadoutLayout skill_1080 =
        platform::active_skill_loadout_layout(1920, 1080);
    ARPG_REQUIRE(test::near(inventory_720.scale, 1.0F));
    ARPG_REQUIRE(test::near(inventory_1080.scale, 1.5F));
    ARPG_REQUIRE(test::near(skill_720.scale, 1.0F));
    ARPG_REQUIRE(test::near(skill_1080.scale, 1.5F));
    ARPG_REQUIRE(skill_720.main_slots[0].width >= 200.0F);
    ARPG_REQUIRE(test::near(skill_1080.main_slots[0].width,
        skill_720.main_slots[0].width * 1.5F, 0.01));
    ARPG_REQUIRE(test::near(skill_1080.remove_button.height,
        skill_720.remove_button.height * 1.5F, 0.01));
    return {};
}

test::Failure inventory_page_titles_follow_the_current_binding() noexcept {
    const auto equipment = platform::inventory_page_title(
        platform::InventoryPage::equipment_materials, "Right");
    const auto skills = platform::inventory_page_title(
        platform::InventoryPage::skill_stones, "Right");
    ARPG_REQUIRE(std::string_view(equipment.data())
        == "Right / ESC  ·  EQUIPMENT INVENTORY");
    ARPG_REQUIRE(std::string_view(skills.data())
        == u8"Right / ESC  ·  技能石背包");
    const auto longest = platform::inventory_page_title(
        platform::InventoryPage::equipment_materials, "Right Shift");
    ARPG_REQUIRE(std::string_view(longest.data()).rfind(
        "Right Shift / ESC", 0U) == 0U);
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
    {"inventory recipe base contract",
        &recipe_cache_rejects_same_slot_different_bases},
    {"inventory maximum cache complexity",
        &view_cache_never_rescans_stable_maximum_inventory},
    {"inventory readable attribute labels",
        &attribute_labels_are_human_readable_and_scoped},
    {"inventory catalog attribute units",
        &all_catalog_affixes_use_semantic_value_units},
    {"inventory detail content boundaries",
        &detail_content_stays_inside_all_required_viewports},
    {"inventory and skill text safe areas",
        &inventory_and_skill_text_boxes_use_disjoint_safe_areas},
    {"full-HD inventory and skill scaling",
        &full_hd_inventory_and_skill_geometry_scales_by_one_and_a_half},
    {"inventory page titles follow binding",
        &inventory_page_titles_follow_the_current_binding},
};

}  // namespace

arpg::test::TestSuite inventory_view_math_suite() noexcept {
    return arpg::test::make_suite("inventory_view_math", kCases);
}
