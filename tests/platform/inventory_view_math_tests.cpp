#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "inventory_view_math.hpp"
#include "ui_text_catalog.hpp"
#include "ui_text_layout.hpp"
#include "ui_utf8.hpp"

#include "items/item_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

std::size_t codepoint_count(const char* text) noexcept {
    if (text == nullptr) return 0U;
    std::size_t count{};
    for (const auto* cursor = reinterpret_cast<const unsigned char*>(text);
         *cursor != 0U; ++cursor) {
        if ((*cursor & 0xC0U) != 0x80U) ++count;
    }
    return count;
}

platform::UiTextExtent linear_measure(const char* text, float font_size,
    float spacing, void*) noexcept {
    const std::size_t count = codepoint_count(text);
    return {
        static_cast<float>(count) * font_size * 0.5F
            + static_cast<float>(count == 0U ? 0U : count - 1U) * spacing,
        font_size,
    };
}

platform::UiTextExtent mixed_width_measure(const char* text, float font_size,
    float spacing, void*) noexcept {
    if (text == nullptr) return {};
    float width_units{};
    std::size_t glyph_count{};
    const auto* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor != 0U) {
        if (*cursor < 0x80U) {
            width_units += 0.5F;
            ++cursor;
        } else {
            width_units += 1.0F;
            if ((*cursor & 0xE0U) == 0xC0U) cursor += 2U;
            else if ((*cursor & 0xF0U) == 0xE0U) cursor += 3U;
            else cursor += 4U;
        }
        ++glyph_count;
    }
    return {
        width_units * font_size
            + static_cast<float>(glyph_count == 0U ? 0U : glyph_count - 1U)
                * spacing,
        font_size,
    };
}

test::Failure layouts_are_bounded_and_non_overlapping() noexcept {
    constexpr std::array<std::array<int, 2>, 3> kSizes{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto& size : kSizes) {
        // This catches regressions where the shared POE2 shell falls back to
        // the legacy three-panel layout or lets compact controls overlap.
        const auto shell = platform::inventory_shell_layout(size[0], size[1]);
        ARPG_REQUIRE(shell.valid);
        const float scale = shell.geometry_scale;
        const float expected_header_height = std::max(32.0F, 44.0F * scale);
        const float expected_status_height = std::max(32.0F, 52.0F * scale);
        ARPG_REQUIRE(test::near(shell.header.x, 16.0F * scale));
        ARPG_REQUIRE(test::near(shell.header.y, 16.0F * scale));
        ARPG_REQUIRE(test::near(shell.header.height, expected_header_height));
        ARPG_REQUIRE(test::near(shell.status_bar.height,
            expected_status_height));
        ARPG_REQUIRE(test::near(shell.content.y
            - (shell.header.y + shell.header.height), 16.0F * scale));
        ARPG_REQUIRE(test::near(shell.status_bar.y
            - (shell.content.y + shell.content.height), 16.0F * scale));
        ARPG_REQUIRE(test::near(shell.viewport.y + shell.viewport.height
            - (shell.status_bar.y + shell.status_bar.height), 16.0F * scale));
        ARPG_REQUIRE(test::near(shell.right_panel.x
            - (shell.left_panel.x + shell.left_panel.width), 16.0F * scale));
        ARPG_REQUIRE(test::near(shell.left_panel.width, 440.0F * scale));
        ARPG_REQUIRE(inside(shell.header, shell.viewport));
        ARPG_REQUIRE(inside(shell.left_panel, shell.viewport));
        ARPG_REQUIRE(inside(shell.right_panel, shell.viewport));
        ARPG_REQUIRE(inside(shell.left_panel, shell.content));
        ARPG_REQUIRE(inside(shell.right_panel, shell.content));
        ARPG_REQUIRE(inside(shell.status_bar, shell.viewport));
        ARPG_REQUIRE(separated(shell.left_panel, shell.right_panel));
        ARPG_REQUIRE(shell.right_panel.width > shell.left_panel.width);
        ARPG_REQUIRE(platform::inventory_grid_columns(shell) == 3);
        ARPG_REQUIRE(shell.body_font_size >= 12.0F);
        ARPG_REQUIRE(shell.hint_font_size >= 12.0F);
        ARPG_REQUIRE(inside(shell.slot_filter_bounds, shell.filter_bar));
        ARPG_REQUIRE(inside(shell.rarity_filter_bounds, shell.filter_bar));
        ARPG_REQUIRE(shell.slot_filter_bounds.width >= 40.0F
            && shell.slot_filter_bounds.height >= 32.0F);
        ARPG_REQUIRE(shell.rarity_filter_bounds.width >= 40.0F
            && shell.rarity_filter_bounds.height >= 32.0F);
        ARPG_REQUIRE(separated(shell.slot_filter_bounds,
            shell.rarity_filter_bounds));
        ARPG_REQUIRE(inside(shell.empty_state_bounds, shell.right_panel));
        ARPG_REQUIRE(inside(shell.status_bounds, shell.status_bar));
        ARPG_REQUIRE(inside(shell.action_bounds, shell.status_bar));
        ARPG_REQUIRE(shell.action_bounds.width >= 40.0F
            && shell.action_bounds.height >= 32.0F);
        ARPG_REQUIRE(separated(shell.status_bounds, shell.action_bounds));
        const Vector2 action_center{
            shell.action_bounds.x + shell.action_bounds.width * 0.5F,
            shell.action_bounds.y + shell.action_bounds.height * 0.5F};
        platform::InventoryStatusInput disabled_action_status_input{};
        disabled_action_status_input.page = platform::InventoryPage::equipment;
        const auto disabled_action_status =
            platform::inventory_status_presentation(
                disabled_action_status_input);
        ARPG_REQUIRE(disabled_action_status.action_visible
            && !disabled_action_status.action_enabled);
        ARPG_REQUIRE(platform::inventory_page_action_contains(shell,
            platform::InventoryPage::equipment, action_center));
        ARPG_REQUIRE(platform::inventory_page_action_contains(shell,
            platform::InventoryPage::skill_stones, action_center));
        ARPG_REQUIRE(!platform::inventory_page_action_contains(shell,
            platform::InventoryPage::materials, action_center));
        platform::InventoryPointerPriorityInput disabled_action_click{};
        disabled_action_click.page_action =
            platform::inventory_page_action_contains(shell,
                platform::InventoryPage::equipment, action_center);
        disabled_action_click.background_clear = true;
        ARPG_REQUIRE(platform::inventory_pointer_layer(disabled_action_click)
            == platform::InventoryPointerLayer::page_action);
        disabled_action_click.material_selected = true;
        ARPG_REQUIRE(platform::inventory_pointer_layer(disabled_action_click)
            == platform::InventoryPointerLayer::background_clear);
        disabled_action_click.page = platform::InventoryPage::skill_stones;
        ARPG_REQUIRE(platform::inventory_pointer_layer(disabled_action_click)
            == platform::InventoryPointerLayer::page_action);
        if (shell.hint_visible) {
            ARPG_REQUIRE(inside(shell.hint_bounds, shell.status_bar));
            ARPG_REQUIRE(separated(shell.action_bounds, shell.hint_bounds));
        } else {
            ARPG_REQUIRE(size[0] == 800 && size[1] == 450);
        }
        for (const Rectangle tab : shell.tabs) {
            ARPG_REQUIRE(tab.width >= 40.0F);
            ARPG_REQUIRE(tab.height >= 32.0F);
            ARPG_REQUIRE(test::near(tab.height, expected_header_height));
            ARPG_REQUIRE(inside(tab, shell.viewport));
        }
        for (std::size_t left{}; left < shell.tabs.size(); ++left) {
            for (std::size_t right = left + 1U;
                 right < shell.tabs.size(); ++right) {
                ARPG_REQUIRE(separated(shell.tabs[left], shell.tabs[right]));
            }
        }
        for (const Rectangle slot : shell.equipment_slots) {
            ARPG_REQUIRE(inside(slot, shell.left_panel));
            ARPG_REQUIRE(test::near(slot.width, 78.0F * scale));
            ARPG_REQUIRE(test::near(slot.height, 78.0F * scale));
        }
        for (std::size_t left{}; left < shell.equipment_slots.size(); ++left) {
            for (std::size_t right = left + 1U;
                 right < shell.equipment_slots.size(); ++right) {
                ARPG_REQUIRE(separated(shell.equipment_slots[left],
                    shell.equipment_slots[right]));
            }
        }
        const Rectangle helmet = shell.equipment_slots[
            static_cast<std::size_t>(items::ItemSlot::helmet)];
        const Rectangle chest = shell.equipment_slots[
            static_cast<std::size_t>(items::ItemSlot::chest)];
        const Rectangle boots = shell.equipment_slots[
            static_cast<std::size_t>(items::ItemSlot::boots)];
        ARPG_REQUIRE(helmet.y < chest.y && chest.y < boots.y);
        ARPG_REQUIRE(inside(shell.silhouette, shell.left_panel));
    }
    platform::InventoryPointerPriorityInput layered{};
    layered.tooltip = true;
    layered.directed_material = true;
    layered.material_cancel_requested = true;
    layered.material_select = true;
    layered.material_target = true;
    layered.page_action = true;
    layered.filter = true;
    layered.skill_content = true;
    layered.recipe = true;
    layered.equipment = true;
    layered.background_clear = true;
    ARPG_REQUIRE(platform::inventory_pointer_layer(layered)
        == platform::InventoryPointerLayer::tooltip);

    // The shell builders run once per frame; this catches an accidental
    // dynamic string/container path after their fixed-capacity contracts pass.
    items::ItemInstance item{};
    item.id = 901U;
    item.base_id = 1U;
    item.rarity = items::ItemRarity::rare;
    item.item_level = 1U;
    item.required_level = 1U;
    items::ItemOwnershipState ownership{};
    ownership.items.push_back(item);
    ownership.equipment.equipped_ids[0] = item.id;
    platform::InventoryViewCache view_cache{};
    platform::refresh_inventory_view_cache(view_cache, ownership, 1U, {},
        item.id, {});
    const items::ItemOwnershipState empty_state{};
    platform::InventoryStatusInput status_input{};
    status_input.recipe_count = 3U;
    status_input.recipe_ready = true;
    status_input.authority_enabled = true;
    platform::InventoryItemVisualInput visual_input{};
    visual_input.item_id = item.id;
    visual_input.selected_item_id = item.id;
    platform::InventoryTransientStatusState transient{};
    platform::InventoryTransientStatusInput transient_input{};
    transient_input.commit_generation = 1U;
    transient_input.save_state = platform::InventorySavePresentationState::saved;
    const auto warm_shell = platform::inventory_shell_layout(800, 450);
    const auto warm_view = platform::inventory_tooltip_view(item,
        platform::BuildDifference{});
    static_cast<void>(platform::inventory_tooltip_layout(warm_shell,
        warm_shell.equipment_slots[0], false, warm_view, &linear_measure,
        nullptr));
    static_cast<void>(platform::inventory_status_presentation(status_input));
    const std::uint64_t before = test::allocation_count();
    for (std::size_t iteration{}; iteration < 10000U; ++iteration) {
        const auto shell = platform::inventory_shell_layout(800, 450);
        const auto view = platform::inventory_tooltip_view(item,
            platform::BuildDifference{});
        const auto tip = platform::inventory_tooltip_layout(shell,
            shell.equipment_slots[0], false, view, &linear_measure, nullptr);
        const auto transition = platform::inventory_tooltip_transition({}, {});
        const auto empty = platform::inventory_empty_presentation(empty_state, 0U);
        const auto material_empty = platform::inventory_empty_presentation(
            ownership, 0U, items::MaterialId::chaos);
        const auto transient_frame = platform::inventory_transient_status_for_frame(
            transient, transient_input);
        transient = transient_frame.next;
        const auto status = platform::inventory_status_presentation(status_input);
        const auto visual = platform::inventory_item_visual_state(visual_input);
        const auto source = platform::inventory_visible_item_source(shell,
            platform::InventoryPage::equipment, ownership, view_cache, item.id,
            0.0F);
        const auto pointer_layer = platform::inventory_pointer_layer(layered);
        const float scroll = platform::inventory_grid_scroll_after_wheel(
            platform::InventoryPage::equipment, shell,
            {shell.grid_viewport.x + 1.0F, shell.grid_viewport.y + 1.0F},
            false, -1.0F, 40U, 0.0F);
        ARPG_REQUIRE(shell.valid && tip.valid && view.valid
            && transient_frame.next.initialized);
        ARPG_REQUIRE(!transition.state.visible && empty.title != nullptr
            && material_empty.hint[0] != '\0'
            && status.status[0] != '\0'
            && visual.outline != platform::InventoryItemOutline::none
            && source.valid
            && pointer_layer != platform::InventoryPointerLayer::none
            && scroll >= 0.0F);
    }
    ARPG_REQUIRE(test::allocation_count() == before);
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

    items::ItemOwnershipState empty{};
    const auto empty_presentation = platform::inventory_empty_presentation(
        empty, 0U);
    ARPG_REQUIRE(std::string_view(empty_presentation.title) == u8"背包无装备");
    ARPG_REQUIRE(std::string_view(empty_presentation.hint.data())
        == u8"击败怪物，拾取装备");

    items::ItemOwnershipState all_equipped{};
    all_equipped.items.push_back(make_item(80U, 1U, items::ItemRarity::normal));
    all_equipped.equipment.equipped_ids[0] = 80U;
    const auto equipped_presentation = platform::inventory_empty_presentation(
        all_equipped, 0U);
    ARPG_REQUIRE(std::string_view(equipped_presentation.title) == u8"无剩余装备");
    ARPG_REQUIRE(std::string_view(equipped_presentation.hint.data())
        == u8"左击左方装备取出");

    const auto material_equipped_presentation =
        platform::inventory_empty_presentation(
            all_equipped, 0U, items::MaterialId::chaos);
    const std::string_view material_equipped_hint{
        material_equipped_presentation.hint.data()};
    ARPG_REQUIRE(material_equipped_hint.find(u8"取出") == std::string_view::npos);
    ARPG_REQUIRE(material_equipped_hint
        == u8"已选择：混沌石 · 可用装备已标出 · 右键取消");

    const auto filtered_presentation = platform::inventory_empty_presentation(
        state, 0U);
    ARPG_REQUIRE(std::string_view(filtered_presentation.title) == u8"当前过滤无结果");
    ARPG_REQUIRE(std::string_view(filtered_presentation.hint.data())
        == u8"切换装备槽及稀有度");

    items::ItemInstance reinforced = make_item(
        81U, 1U, items::ItemRarity::rare);
    reinforced.reinforcement = 12U;
    const auto reinforced_slot_label =
        platform::inventory_equipment_slot_label(
            items::ItemSlot::weapon, &reinforced);
    ARPG_REQUIRE(std::string_view{reinforced_slot_label.data()}
        == u8"+12 · 精炼之刃");
    const auto empty_slot_label = platform::inventory_equipment_slot_label(
        items::ItemSlot::weapon, nullptr);
    ARPG_REQUIRE(std::string_view{empty_slot_label.data()} == u8"武器");
    ARPG_REQUIRE(platform::ui_utf8_is_valid(reinforced_slot_label.data()));

    const auto reinforced_grid_metadata =
        platform::inventory_grid_item_metadata(reinforced);
    ARPG_REQUIRE(std::string_view{reinforced_grid_metadata.data()}
        == u8"稀有 · 1级 · +12");
    ARPG_REQUIRE(platform::ui_utf8_is_valid(
        reinforced_grid_metadata.data()));
    items::ItemInstance unreinforced = reinforced;
    unreinforced.reinforcement = 0U;
    const auto unreinforced_grid_metadata =
        platform::inventory_grid_item_metadata(unreinforced);
    ARPG_REQUIRE(std::string_view{unreinforced_grid_metadata.data()}
        == u8"稀有 · 1级 · +0");
    ARPG_REQUIRE(unreinforced_grid_metadata.back() == '\0');

    platform::RecipeSelection recipe{};
    recipe.ids = {{11U, 13U, 14U}};
    recipe.count = 3U;
    const auto visual = platform::inventory_item_visual_state({
        11U, 11U, 11U, recipe, false, false});
    ARPG_REQUIRE(visual.outline == platform::InventoryItemOutline::recipe);
    ARPG_REQUIRE(visual.recipe_ordinal == 1U);
    ARPG_REQUIRE(visual.selected_indicator);

    // These independently exercise each precedence boundary: a later visual
    // layer must not replace an earlier interaction mode.
    ARPG_REQUIRE(platform::inventory_item_visual_state({11U, 11U, 0U,
        {}, false, false}).outline == platform::InventoryItemOutline::hover);
    ARPG_REQUIRE(platform::inventory_item_visual_state({11U, 11U, 11U,
        {}, false, false}).outline == platform::InventoryItemOutline::selected);
    ARPG_REQUIRE(platform::inventory_item_visual_state({11U, 11U, 11U,
        recipe, true, true}).outline
        == platform::InventoryItemOutline::material_target);
    ARPG_REQUIRE(platform::inventory_item_visual_state({11U, 11U, 11U,
        recipe, true, false}).outline
        == platform::InventoryItemOutline::disabled_target);
    ARPG_REQUIRE(platform::inventory_item_visual_state({11U, 11U, 11U,
        recipe, true, false, true}).outline
        == platform::InventoryItemOutline::rejected_target);

    state.materials[items::material_index(items::MaterialId::transmute)] = 1U;
    const items::ItemInstance& normal_target = state.items[0U];
    ARPG_REQUIRE(platform::inventory_material_target_enabled(state,
        normal_target, items::MaterialId::transmute, std::nullopt,
        0xA11CEU, 12U, true));
    ARPG_REQUIRE(!platform::inventory_material_target_enabled(state,
        normal_target, items::MaterialId::transmute, std::nullopt,
        0xA11CEU, 12U, false));
    state.materials[items::material_index(items::MaterialId::transmute)] = 0U;
    ARPG_REQUIRE(!platform::inventory_material_target_enabled(state,
        normal_target, items::MaterialId::transmute, std::nullopt,
        0xA11CEU, 12U, true));
    state.materials[items::material_index(items::MaterialId::transmute)] = 1U;
    ARPG_REQUIRE(!platform::inventory_material_target_enabled(state,
        state.items[2U], items::MaterialId::transmute, std::nullopt,
        0xA11CEU, 12U, true));

    const auto shell_1280 = platform::inventory_shell_layout(1280, 720);
    platform::InventoryViewCache source_cache{};
    platform::refresh_inventory_view_cache(source_cache, state, 8U, {}, 11U, {});
    const auto grid_source = platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::equipment, state, source_cache, 11U, 0.0F);
    ARPG_REQUIRE(grid_source.valid && grid_source.source_is_in_right_panel);
    ARPG_REQUIRE(grid_source.bounds.x == platform::inventory_grid_item_rectangle(
        shell_1280, 0U, 0.0F).x);
    const auto hidden_by_page = platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::materials, state, source_cache, 11U, 0.0F);
    ARPG_REQUIRE(!hidden_by_page.valid);
    items::ItemOwnershipState stale{};
    stale.equipment.equipped_ids[0] = 999U;
    platform::InventoryViewCache stale_cache{};
    platform::refresh_inventory_view_cache(stale_cache, stale, 9U, {}, 999U, {});
    ARPG_REQUIRE(!platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::equipment, stale, stale_cache, 999U, 0.0F).valid);
    const auto unknown_source = platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::equipment, state, source_cache, 999U, 0.0F);
    ARPG_REQUIRE(!unknown_source.valid);

    // Source resolution and tooltip ownership are deliberately separate: an
    // unavailable source closes hover/frozen presentation but never mutates
    // the persistent selection owned by the caller.
    items::ItemOwnershipState source_state{};
    source_state.items = {make_item(101U, 1U, items::ItemRarity::rare),
        make_item(102U, 2U, items::ItemRarity::magic)};
    source_state.equipment.equipped_ids[0] = 101U;
    platform::InventoryViewCache equipped_cache{};
    platform::refresh_inventory_view_cache(equipped_cache, source_state, 12U,
        {}, 101U, {});
    const auto equipped_source = platform::inventory_visible_item_source(
        shell_1280, platform::InventoryPage::equipment, source_state,
        equipped_cache, 101U, 0.0F);
    ARPG_REQUIRE(equipped_source.valid && !equipped_source.source_is_in_right_panel);
    ARPG_REQUIRE(equipped_source.bounds.x == shell_1280.equipment_slots[0].x);
    source_state.equipment.equipped_ids[0] = 0U;
    platform::InventoryViewCache grid_cache{};
    platform::refresh_inventory_view_cache(grid_cache, source_state, 13U, {},
        101U, {});
    const auto current_grid = platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::equipment, source_state, grid_cache, 101U, 0.0F);
    ARPG_REQUIRE(current_grid.valid && current_grid.source_is_in_right_panel);
    ARPG_REQUIRE(current_grid.bounds.x == platform::inventory_grid_item_rectangle(
        shell_1280, 0U, 0.0F).x);
    const std::uint64_t selected_item_id = 101U;
    platform::InventoryTooltipInteractionState tooltip_state{};
    tooltip_state.hovered_item_id = selected_item_id;
    tooltip_state.frozen_item_id = selected_item_id;
    tooltip_state.source = current_grid.bounds;
    tooltip_state.retention_corridor = current_grid.bounds;
    tooltip_state.visible = true;
    const auto close_for_invalid = [&tooltip_state, selected_item_id]() noexcept {
        const auto closed = platform::inventory_tooltip_transition(tooltip_state,
            {{}, {}, {}, false, {-10.0F, -10.0F}, false, true, false, false});
        tooltip_state = closed.state;
        const bool cleared = !tooltip_state.visible
            && tooltip_state.hovered_item_id == 0U
            && tooltip_state.frozen_item_id == 0U
            && selected_item_id == 101U;
        tooltip_state.suppressed_item_id = 0U;
        return cleared;
    };
    platform::InventoryFilter excluding{items::ItemSlot::helmet, {}};
    platform::refresh_inventory_view_cache(grid_cache, source_state, 14U, excluding,
        selected_item_id, {});
    ARPG_REQUIRE(!platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::equipment, source_state, grid_cache,
        selected_item_id, 0.0F).valid);
    ARPG_REQUIRE(close_for_invalid());
    ARPG_REQUIRE(!platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::skill_stones, source_state, grid_cache,
        selected_item_id, 0.0F).valid);
    ARPG_REQUIRE(close_for_invalid());
    ARPG_REQUIRE(!platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::materials, source_state, grid_cache,
        selected_item_id, 0.0F).valid);
    ARPG_REQUIRE(close_for_invalid());
    platform::refresh_inventory_view_cache(grid_cache, source_state, 15U, {},
        selected_item_id, {});
    ARPG_REQUIRE(!platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::equipment, source_state, grid_cache,
        selected_item_id, 99.0F).valid);
    ARPG_REQUIRE(close_for_invalid());
    ARPG_REQUIRE(!platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::equipment, source_state, grid_cache, 404U,
        0.0F).valid);
    ARPG_REQUIRE(close_for_invalid());
    const auto restored_source = platform::inventory_visible_item_source(shell_1280,
        platform::InventoryPage::equipment, source_state, grid_cache,
        selected_item_id, 0.0F);
    ARPG_REQUIRE(restored_source.valid);
    const auto shell_800 = platform::inventory_shell_layout(800, 450);
    tooltip_state = {selected_item_id, selected_item_id, 0U, restored_source.bounds,
        restored_source.bounds, true, true};
    ARPG_REQUIRE(close_for_invalid());
    const auto resized_source = platform::inventory_visible_item_source(shell_800,
        platform::InventoryPage::equipment, source_state, grid_cache,
        selected_item_id, 0.0F);
    ARPG_REQUIRE(resized_source.valid);
    ARPG_REQUIRE(resized_source.bounds.x != restored_source.bounds.x
        || resized_source.bounds.y != restored_source.bounds.y);
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
    const platform::InventoryShellLayout layout =
        platform::inventory_shell_layout(1280, 720);
    items::EquipmentState equipment{};
    equipment.equipped_ids[2] = 42U;
    const Rectangle occupied = layout.equipment_slots[
        static_cast<std::size_t>(items::ItemSlot::chest)];
    const Vector2 center{occupied.x + occupied.width * 0.5F,
        occupied.y + occupied.height * 0.5F};
    const auto hit = platform::hit_test_equipped_slot(center, layout, equipment);
    ARPG_REQUIRE(hit.has_value());
    ARPG_REQUIRE(*hit == items::ItemSlot::chest);

    const Rectangle empty = layout.equipment_slots[
        static_cast<std::size_t>(items::ItemSlot::weapon)];
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
    current.values.max_barrier = 200;
    current.values.movement_speed = 10000;
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
    candidate.values.max_barrier = 225;
    candidate.values.movement_speed = 10750;
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
    ARPG_REQUIRE(difference.max_barrier == 25);
    ARPG_REQUIRE(difference.move_speed == 750);
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

    platform::InventoryStatusInput status_input{};
    status_input.page = platform::InventoryPage::equipment;
    status_input.recipe_count = 3U;
    status_input.recipe_ready = true;
    status_input.authority_enabled = true;
    const auto status = platform::inventory_status_presentation(status_input);
    ARPG_REQUIRE(status.action_kind == platform::InventoryActionKind::combine);
    ARPG_REQUIRE(status.action_enabled);
    ARPG_REQUIRE(status.tone == platform::InventoryStatusTone::positive);
    ARPG_REQUIRE(std::string_view{status.action.data()} == u8"三合一（3/3）");
    status_input.selected_material = items::MaterialId::chaos;
    const auto material_target_status =
        platform::inventory_status_presentation(status_input);
    ARPG_REQUIRE(!material_target_status.action_visible);
    ARPG_REQUIRE(!material_target_status.action_enabled);

    // The materials page must not inherit the equipment double-click/recipe
    // instruction when no material is selected.
    platform::InventoryStatusInput materials_status_input{};
    materials_status_input.page = platform::InventoryPage::materials;
    const auto materials_status =
        platform::inventory_status_presentation(materials_status_input);
    ARPG_REQUIRE(std::string_view{materials_status.status.data()} == u8"材料");
    ARPG_REQUIRE(std::string_view{materials_status.hint.data()}.empty());
    ARPG_REQUIRE(!materials_status.action_visible);
    materials_status_input.selected_material = items::MaterialId::chaos;
    const auto selected_material_status =
        platform::inventory_status_presentation(materials_status_input);
    ARPG_REQUIRE(std::string_view{selected_material_status.status.data()}
        == u8"已选择：混沌石 · 切换至装备 · 右键取消");
    ARPG_REQUIRE(std::string_view{selected_material_status.hint.data()}
        == platform::ui_material_description(items::MaterialId::chaos));

    materials_status_input.selected_material = items::MaterialId::directed;
    const auto directed_material_status =
        platform::inventory_status_presentation(materials_status_input);
    ARPG_REQUIRE(std::string_view{directed_material_status.status.data()}
        == u8"已选择：定向核心 · 右键核心切换 · 右键装备取消");
    ARPG_REQUIRE(std::string_view{directed_material_status.hint.data()}
        == u8"右键切换");

    platform::InventoryStatusInput equipment_material_status_input{};
    equipment_material_status_input.page = platform::InventoryPage::equipment;
    equipment_material_status_input.selected_material = items::MaterialId::chaos;
    const auto equipment_material_status =
        platform::inventory_status_presentation(
            equipment_material_status_input);
    ARPG_REQUIRE(std::string_view{equipment_material_status.status.data()}
        == u8"已选择：混沌石 · 可用装备已标出 · 右键取消");

    platform::InventoryStatusInput rejected_material_status_input{};
    rejected_material_status_input.page = platform::InventoryPage::equipment;
    rejected_material_status_input.material_target_rejected = true;
    const auto rejected_material_status =
        platform::inventory_status_presentation(
            rejected_material_status_input);
    ARPG_REQUIRE(std::string_view{rejected_material_status.status.data()}
        == u8"应用失败 · 切换装备 · 右键取消");
    ARPG_REQUIRE(rejected_material_status.tone
        == platform::InventoryStatusTone::negative);
    rejected_material_status_input.transient.kind =
        platform::InventoryTransientStatusKind::reinforcement_receipt;
    rejected_material_status_input.transient.receipt_success = true;
    rejected_material_status_input.transient.receipt_before = 1U;
    rejected_material_status_input.transient.receipt_after = 2U;
    ARPG_REQUIRE(std::string_view{platform::inventory_status_presentation(
        rejected_material_status_input).status.data()}
        == u8"应用失败 · 切换装备 · 右键取消");
    rejected_material_status_input.save_state =
        platform::InventorySavePresentationState::error;
    ARPG_REQUIRE(std::string_view{platform::inventory_status_presentation(
        rejected_material_status_input).status.data()}
        == u8"保存失败，当前进度未保存，请重试");

    platform::InventoryTransientStatusState transient{};
    platform::InventoryTransientStatusInput baseline_input{};
    baseline_input.commit_generation = 10U;
    baseline_input.save_state = platform::InventorySavePresentationState::saved;
    baseline_input.receipt_valid = true;
    baseline_input.receipt_commit_generation = 10U;
    const auto baseline = platform::inventory_transient_status_for_frame(
        transient, baseline_input);
    ARPG_REQUIRE(baseline.presentation.kind
        == platform::InventoryTransientStatusKind::none);
    platform::InventoryTransientStatusInput receipt_input{};
    receipt_input.commit_generation = 11U;
    receipt_input.save_state = platform::InventorySavePresentationState::saved;
    receipt_input.receipt_valid = true;
    receipt_input.receipt_success = true;
    receipt_input.receipt_commit_generation = 11U;
    receipt_input.receipt_before = 2U;
    receipt_input.receipt_after = 3U;
    const auto new_receipt = platform::inventory_transient_status_for_frame(
        baseline.next, receipt_input);
    ARPG_REQUIRE(new_receipt.presentation.kind
        == platform::InventoryTransientStatusKind::reinforcement_receipt);

    items::ItemInstance tooltip_item = make_item(
        9001U, 1U, items::ItemRarity::rare);
    tooltip_item.affixes[0] = {2U, 1U, 0xFFU,
        items::kAffixValueRollCanonicalBp};
    tooltip_item.affixes[1] = {1U, 1U, 0xFFU,
        items::kAffixValueRollCanonicalBp};
    tooltip_item.affixes[2] = {101U, 1U, 0xFFU,
        items::kAffixValueRollCanonicalBp};
    tooltip_item.affix_count = 3U;
    platform::BuildDifference display_difference{};
    display_difference.max_health = 14 * modifiers::kFixedOne;
    display_difference.max_barrier = 25 * modifiers::kFixedOne;
    display_difference.flat_damage[0] = 25000;
    display_difference.damage_increased[0] = 4200;
    display_difference.move_speed = -1250;
    display_difference.attack_speed = 1200;
    display_difference.melee_damage = 800;
    display_difference.impulse_scale = 700;
    display_difference.weapon_physical = 3;
    display_difference.local_attack_speed_bp = 250;
    display_difference.armor = 7;
    display_difference.evasion = -5;
    display_difference.armor_reduction_bp = 125;
    display_difference.evasion_rate_bp = -250;
    display_difference.damage_reduction[0] = 300;
    display_difference.damage_reduction_cap_bonus[0] = 200;
    const auto tooltip = platform::inventory_tooltip_view(tooltip_item,
        display_difference);
    ARPG_REQUIRE(tooltip.valid);
    ARPG_REQUIRE(tooltip.line_count <= tooltip.lines.size());
    ARPG_REQUIRE(tooltip.line_count > 0U);
    const auto has_segment = [&tooltip](std::string_view expected) noexcept {
        for (std::size_t line{}; line < tooltip.line_count; ++line) {
            for (std::size_t segment{};
                 segment < tooltip.lines[line].segment_count; ++segment) {
                if (std::string_view{
                        tooltip.lines[line].segments[segment].text.data()}
                        == expected) return true;
            }
        }
        return false;
    };
    const auto group_has_segment = [&tooltip](std::string_view heading,
        std::string_view expected) noexcept {
        for (std::size_t line{}; line < tooltip.line_count; ++line) {
            const auto& candidate = tooltip.lines[line];
            if (candidate.segment_count < 2U
                || std::string_view{candidate.segments[0].text.data()}
                    != heading) continue;
            for (std::size_t segment = 1U;
                 segment < candidate.segment_count; ++segment) {
                if (std::string_view{candidate.segments[segment].text.data()}
                    == expected) return true;
            }
        }
        return false;
    };
    // These literals protect the player-facing unit contract. Affix rolls use
    // their semantic display unit, while comparison fields mix fixed scalars,
    // integer ratings and basis-point percentages.
    ARPG_REQUIRE(has_segment(u8"1级 前缀 凶猛 +42%"));
    ARPG_REQUIRE(has_segment(u8"1级 前缀 锋利 +12"));
    ARPG_REQUIRE(has_segment(u8"1级 后缀 火焰 +12%"));
    ARPG_REQUIRE(group_has_segment(u8"生存", u8"生命 +14"));
    ARPG_REQUIRE(group_has_segment(u8"生存", u8"护盾 +25"));
    ARPG_REQUIRE(group_has_segment(u8"基础伤害", u8"物理 +2.5"));
    ARPG_REQUIRE(group_has_segment(u8"伤害提高", u8"物理 +42%"));
    ARPG_REQUIRE(group_has_segment(u8"速度", u8"移动 -12.5%"));
    ARPG_REQUIRE(group_has_segment(u8"速度", u8"攻击 +12%"));
    ARPG_REQUIRE(group_has_segment(u8"近战", u8"伤害 +8%"));
    ARPG_REQUIRE(group_has_segment(u8"近战", u8"击退 +7%"));
    ARPG_REQUIRE(group_has_segment(u8"武器", u8"物理 +3"));
    ARPG_REQUIRE(group_has_segment(u8"武器", u8"局部攻速 +2.5%"));
    ARPG_REQUIRE(group_has_segment(u8"防御", u8"护甲 +7"));
    ARPG_REQUIRE(group_has_segment(u8"防御", u8"闪避 -5"));
    ARPG_REQUIRE(group_has_segment(u8"防御", u8"减伤 +1.25%"));
    ARPG_REQUIRE(group_has_segment(u8"防御", u8"闪避率 -2.5%"));
    ARPG_REQUIRE(group_has_segment(u8"元素减伤", u8"火 +3%"));
    ARPG_REQUIRE(group_has_segment(u8"减伤上限", u8"火 +2%"));

    // A narrow 224px content region must reserve visual lines before placing
    // wrapped segments. The old implementation reserved only logical lines.
    platform::InventoryTooltipView wrapped{};
    static_cast<void>(platform::ui_utf8_copy(wrapped.title.data(),
        wrapped.title.size(), u8"装备后变化"));
    for (std::size_t line{}; line < 6U; ++line) {
        wrapped.lines[line].segment_count = 2U;
        for (std::size_t segment{}; segment < 2U; ++segment) {
            static_cast<void>(platform::ui_utf8_copy(
                wrapped.lines[line].segments[segment].text.data(),
                wrapped.lines[line].segments[segment].text.size(),
                u8"强化结果：成功强化结果：成功强化结果：成功强化"));
        }
    }
    wrapped.line_count = 6U;
    wrapped.valid = true;
    const auto shell = platform::inventory_shell_layout(800, 450);
    const auto wrapped_layout = platform::inventory_tooltip_layout(shell,
        shell.equipment_slots[0], false, wrapped, &linear_measure, nullptr);
    ARPG_REQUIRE(wrapped_layout.valid);
    ARPG_REQUIRE(wrapped_layout.placement_count == 12U);
    ARPG_REQUIRE(wrapped_layout.visual_line_count > wrapped.line_count);
    for (std::size_t line{}; line < wrapped.line_count; ++line) {
        for (std::size_t segment{}; segment < wrapped.lines[line].segment_count;
             ++segment) {
            std::size_t placements{};
            for (std::size_t index{}; index < wrapped_layout.placement_count;
                 ++index) {
                placements += wrapped_layout.placements[index].logical_line == line
                    && wrapped_layout.placements[index].segment == segment ? 1U : 0U;
            }
            ARPG_REQUIRE(placements == 1U);
        }
    }

    // First observation establishes a baseline. A sticky save or an old
    // receipt must not replay when the view opens or on later equal frames.
    platform::InventoryTransientStatusState trace{};
    platform::InventoryTransientStatusInput trace_input{};
    trace_input.commit_generation = 20U;
    trace_input.save_state = platform::InventorySavePresentationState::saved;
    trace_input.receipt_valid = true;
    trace_input.receipt_commit_generation = 20U;
    trace = platform::inventory_transient_status_for_frame(trace, trace_input).next;
    for (std::size_t frame{}; frame < 300U; ++frame) {
        const auto repeated = platform::inventory_transient_status_for_frame(
            trace, trace_input);
        ARPG_REQUIRE(repeated.presentation.kind
            == platform::InventoryTransientStatusKind::none);
        trace = repeated.next;
    }
    trace_input.commit_generation = 21U;
    for (std::size_t frame{}; frame < 180U; ++frame) {
        const auto saved = platform::inventory_transient_status_for_frame(trace,
            trace_input);
        ARPG_REQUIRE(saved.presentation.kind
            == platform::InventoryTransientStatusKind::saved);
        trace = saved.next;
    }
    ARPG_REQUIRE(platform::inventory_transient_status_for_frame(trace,
        trace_input).presentation.kind == platform::InventoryTransientStatusKind::none);
    trace_input.commit_generation = 22U;
    trace_input.receipt_commit_generation = 22U;
    trace_input.receipt_success = true;
    trace_input.receipt_before = 12U;
    trace_input.receipt_after = 13U;
    for (std::size_t frame{}; frame < 180U; ++frame) {
        const auto receipt = platform::inventory_transient_status_for_frame(trace,
            trace_input);
        ARPG_REQUIRE(receipt.presentation.kind
            == platform::InventoryTransientStatusKind::reinforcement_receipt);
        ARPG_REQUIRE(receipt.presentation.receipt_before == 12U
            && receipt.presentation.receipt_after == 13U);
        trace = receipt.next;
    }
    trace_input.commit_generation = 23U;
    trace_input.save_state = platform::InventorySavePresentationState::none;
    const auto no_replay = platform::inventory_transient_status_for_frame(trace,
        trace_input);
    ARPG_REQUIRE(no_replay.presentation.kind == platform::InventoryTransientStatusKind::none);
    trace = no_replay.next;
    trace_input.save_state = platform::InventorySavePresentationState::recovered;
    trace_input.commit_generation = 24U;
    const auto recovered = platform::inventory_transient_status_for_frame(trace,
        trace_input);
    ARPG_REQUIRE(recovered.presentation.kind
        == platform::InventoryTransientStatusKind::recovered);
    trace = recovered.next;
    trace_input.commit_generation = 1U;
    trace_input.receipt_commit_generation = 1U;
    const auto rollback = platform::inventory_transient_status_for_frame(trace,
        trace_input);
    ARPG_REQUIRE(rollback.presentation.kind == platform::InventoryTransientStatusKind::none
        && rollback.next.remaining_frames == 0U);

    platform::InventoryTransientStatusState receipt_then_save{};
    platform::InventoryTransientStatusInput old_receipt{};
    old_receipt.commit_generation = 10U;
    old_receipt.receipt_valid = true;
    old_receipt.receipt_commit_generation = 11U;
    receipt_then_save = platform::inventory_transient_status_for_frame(
        receipt_then_save, old_receipt).next;
    platform::InventoryTransientStatusInput fresh_save{};
    fresh_save.commit_generation = 12U;
    fresh_save.save_state = platform::InventorySavePresentationState::saved;
    const auto saved_after_old_receipt = platform::inventory_transient_status_for_frame(
        receipt_then_save, fresh_save);
    ARPG_REQUIRE(saved_after_old_receipt.presentation.kind
        == platform::InventoryTransientStatusKind::saved);

    platform::InventoryTransientStatusState old_session{};
    old_session.initialized = true;
    old_session.last_commit_generation = 50U;
    old_session.last_receipt_generation = 50U;
    old_session.kind = platform::InventoryTransientStatusKind::saved;
    old_session.remaining_frames = 3U;
    platform::InventoryTransientStatusInput new_session{};
    new_session.commit_generation = 1U;
    const auto new_session_baseline = platform::inventory_transient_status_for_frame(
        old_session, new_session);
    ARPG_REQUIRE(new_session_baseline.presentation.kind
        == platform::InventoryTransientStatusKind::none);
    ARPG_REQUIRE(new_session_baseline.next.last_receipt_generation == 0U);
    new_session.commit_generation = 2U;
    new_session.receipt_valid = true;
    new_session.receipt_commit_generation = 2U;
    const auto new_session_receipt = platform::inventory_transient_status_for_frame(
        new_session_baseline.next, new_session);
    ARPG_REQUIRE(new_session_receipt.presentation.kind
        == platform::InventoryTransientStatusKind::reinforcement_receipt);

    platform::InventoryTransientStatusState initial_invalid{};
    platform::InventoryTransientStatusInput invalid_first{};
    invalid_first.commit_generation = 1U;
    invalid_first.receipt_commit_generation = 20U;
    initial_invalid = platform::inventory_transient_status_for_frame(
        initial_invalid, invalid_first).next;
    ARPG_REQUIRE(initial_invalid.last_receipt_generation == 0U);
    invalid_first.receipt_valid = true;
    const auto valid_after_invalid = platform::inventory_transient_status_for_frame(
        initial_invalid, invalid_first);
    ARPG_REQUIRE(valid_after_invalid.presentation.kind
        == platform::InventoryTransientStatusKind::reinforcement_receipt);

    const platform::InventoryTransientStatusPresentation receipts[] = {
        {platform::InventoryTransientStatusKind::reinforcement_receipt,
            true, false, false, 7U, 8U},
        {platform::InventoryTransientStatusKind::reinforcement_receipt,
            false, false, false, 7U, 8U},
        {platform::InventoryTransientStatusKind::reinforcement_receipt,
            false, true, false, 7U, 8U},
        {platform::InventoryTransientStatusKind::reinforcement_receipt,
            true, false, true, 7U, 8U},
    };
    const platform::UiTextId receipt_formats[] = {
        platform::UiTextId::inventory_reinforcement_success_format,
        platform::UiTextId::inventory_reinforcement_failure_format,
        platform::UiTextId::inventory_reinforcement_destroyed_format,
        platform::UiTextId::inventory_coupon_success_format,
    };
    const platform::InventoryStatusTone receipt_tones[] = {
        platform::InventoryStatusTone::positive,
        platform::InventoryStatusTone::negative,
        platform::InventoryStatusTone::negative,
        platform::InventoryStatusTone::positive,
    };
    for (std::size_t index{}; index < 4U; ++index) {
        platform::InventoryStatusInput receipt_status{};
        receipt_status.transient = receipts[index];
        const auto presented = platform::inventory_status_presentation(receipt_status);
        std::array<char, 128> expected{};
        if (index == 2U) static_cast<void>(platform::ui_utf8_format(expected.data(),
            expected.size(), platform::ui_text(receipt_formats[index]), 7U));
        else static_cast<void>(platform::ui_utf8_format(expected.data(),
            expected.size(), platform::ui_text(receipt_formats[index]), 7U, 8U));
        ARPG_REQUIRE(std::string_view(presented.status.data())
            == std::string_view(expected.data()));
        ARPG_REQUIRE(platform::ui_utf8_is_valid(presented.status.data()));
        ARPG_REQUIRE(presented.tone == receipt_tones[index]);
    }
    platform::InventoryStatusInput tone_input{};
    tone_input.save_state = platform::InventorySavePresentationState::saving;
    ARPG_REQUIRE(platform::inventory_status_presentation(tone_input).tone
        == platform::InventoryStatusTone::warning);
    tone_input.save_state = platform::InventorySavePresentationState::error;
    const auto save_error_status =
        platform::inventory_status_presentation(tone_input);
    ARPG_REQUIRE(save_error_status.tone
        == platform::InventoryStatusTone::negative);
    ARPG_REQUIRE(std::string_view(save_error_status.status.data())
        == u8"保存失败，当前进度未保存，请重试");
    tone_input = {};
    tone_input.selected_material = items::MaterialId::transmute;
    ARPG_REQUIRE(platform::inventory_status_presentation(tone_input).tone
        == platform::InventoryStatusTone::warning);
    tone_input = {};
    tone_input.recipe_count = 3U; tone_input.recipe_ready = true;
    ARPG_REQUIRE(platform::inventory_status_presentation(tone_input).tone
        == platform::InventoryStatusTone::positive);
    tone_input.reinforced_recipe_input = true;
    ARPG_REQUIRE(platform::inventory_status_presentation(tone_input).tone
        == platform::InventoryStatusTone::negative);
    ARPG_REQUIRE(platform::inventory_status_presentation({}).tone
        == platform::InventoryStatusTone::muted);

    platform::InventoryStatusInput skill_status_input{};
    skill_status_input.page = platform::InventoryPage::skill_stones;
    skill_status_input.authority_enabled = true;
    const auto skill_equip_status =
        platform::inventory_status_presentation(skill_status_input);
    ARPG_REQUIRE(std::string_view(skill_equip_status.status.data())
        == platform::ui_text(platform::UiTextId::inventory_skill_equip_hint));
    ARPG_REQUIRE(skill_equip_status.action_visible
        && !skill_equip_status.action_enabled);
    skill_status_input.active_skill_removable = true;
    skill_status_input.selected_material = items::MaterialId::chaos;
    const auto skill_selected_status =
        platform::inventory_status_presentation(skill_status_input);
    ARPG_REQUIRE(std::string_view(skill_selected_status.status.data())
        == platform::ui_text(platform::UiTextId::inventory_skill_swap_hint));
    ARPG_REQUIRE(std::string_view(skill_selected_status.hint.data())
        == platform::ui_text(platform::UiTextId::inventory_skill_remove_hint));
    ARPG_REQUIRE(skill_selected_status.action_visible
        && skill_selected_status.action_enabled);
    skill_status_input.save_state =
        platform::InventorySavePresentationState::saving;
    const auto skill_saving_status =
        platform::inventory_status_presentation(skill_status_input);
    ARPG_REQUIRE(skill_saving_status.action_visible
        && !skill_saving_status.action_enabled);

    platform::InventoryTooltipInteractionState interaction{};
    const Rectangle corridor{10.0F, 10.0F, 80.0F, 40.0F};
    auto hover = platform::inventory_tooltip_transition(interaction,
        {55U, corridor, corridor, false, {20.0F, 20.0F}, false, false, false, false});
    ARPG_REQUIRE(hover.state.visible && !hover.state.source_is_in_right_panel);
    interaction = hover.state;
    const auto frozen = platform::inventory_tooltip_transition(interaction,
        {{}, {}, {}, false, {20.0F, 20.0F}, false, false, false, false});
    ARPG_REQUIRE(frozen.state.frozen_item_id == 55U
        && !frozen.blocks_pointer);
    const platform::InventoryVisibleItemSource rebuilt_source{
        55U, {140.0F, 120.0F, 60.0F, 60.0F}, true, true};
    const Rectangle rebuilt_corridor{120.0F, 100.0F, 220.0F, 180.0F};
    const auto rebuilt_state = platform::inventory_tooltip_state_with_retained_source(
        frozen.state, rebuilt_source, rebuilt_corridor);
    ARPG_REQUIRE(rebuilt_state.source.x == rebuilt_source.bounds.x
        && rebuilt_state.source.y == rebuilt_source.bounds.y
        && rebuilt_state.source.width == rebuilt_source.bounds.width
        && rebuilt_state.source.height == rebuilt_source.bounds.height
        && rebuilt_state.retention_corridor.x == rebuilt_corridor.x
        && rebuilt_state.retention_corridor.y == rebuilt_corridor.y
        && rebuilt_state.retention_corridor.width == rebuilt_corridor.width
        && rebuilt_state.retention_corridor.height == rebuilt_corridor.height
        && rebuilt_state.source_is_in_right_panel);
    const auto retained_after_rebuild = platform::inventory_tooltip_transition(
        rebuilt_state,
        {{}, {}, {}, false, {300.0F, 250.0F}, false, false, false, false});
    ARPG_REQUIRE(retained_after_rebuild.state.visible
        && retained_after_rebuild.state.frozen_item_id == 55U);
    auto invalidated_input = platform::InventoryTooltipInteractionInput{};
    invalidated_input.pointer = {20.0F, 20.0F};
    invalidated_input.source_invalidated = true;
    const auto invalidated = platform::inventory_tooltip_transition(frozen.state,
        invalidated_input);
    ARPG_REQUIRE(!invalidated.state.visible && invalidated.state.hovered_item_id == 0U
        && invalidated.state.frozen_item_id == 0U
        && invalidated.state.suppressed_item_id == 0U
        && invalidated.state.source.width == 0.0F
        && invalidated.state.retention_corridor.width == 0.0F
        && !invalidated.state.source_is_in_right_panel
        && !invalidated.blocks_pointer);
    const auto closed = platform::inventory_tooltip_transition(frozen.state,
        {{}, {}, {}, false, {20.0F, 20.0F}, true, true, true, false});
    ARPG_REQUIRE(!closed.state.visible && closed.state.suppressed_item_id == 55U
        && closed.clear_persistent_selection && closed.blocks_pointer);
    const auto suppressed = platform::inventory_tooltip_transition(closed.state,
        {55U, corridor, corridor, false, {20.0F, 20.0F}, false, false, false, false});
    ARPG_REQUIRE(!suppressed.state.visible);
    const auto alternate_suppressed = platform::inventory_tooltip_transition(
        suppressed.state,
        {77U, corridor, corridor, false, {20.0F, 20.0F}, false, false, false,
            false});
    ARPG_REQUIRE(!alternate_suppressed.state.visible
        && alternate_suppressed.state.suppressed_item_id == 55U);
    const auto restored = platform::inventory_tooltip_transition(suppressed.state,
        {55U, corridor, corridor, true, {200.0F, 200.0F}, false, false, false, false});
    ARPG_REQUIRE(restored.state.visible && restored.state.source_is_in_right_panel);
    const auto right_click = platform::inventory_tooltip_transition(restored.state,
        {{}, {}, {}, false, {20.0F, 20.0F}, true, true, false, false});
    ARPG_REQUIRE(!right_click.clear_persistent_selection && right_click.blocks_pointer);
    const auto background = platform::inventory_tooltip_transition(restored.state,
        {{}, {}, {}, false, {20.0F, 20.0F}, false, false, false, true});
    ARPG_REQUIRE(background.clear_persistent_selection
        && !background.blocks_pointer);
    const Vector2 grid_pointer{shell.grid_viewport.x + 1.0F,
        shell.grid_viewport.y + 1.0F};
    const float scrolled = platform::inventory_grid_scroll_after_wheel(
        platform::InventoryPage::equipment, shell, grid_pointer, false, -1.0F,
        40U, 0.0F);
    ARPG_REQUIRE(scrolled > 0.0F);
    for (const platform::InventoryPage page : {platform::InventoryPage::skill_stones,
            platform::InventoryPage::materials}) {
        ARPG_REQUIRE(platform::inventory_grid_scroll_after_wheel(page, shell,
            grid_pointer, false, -1.0F, 40U, 2.0F) == 2.0F);
    }
    ARPG_REQUIRE(platform::inventory_grid_scroll_after_wheel(
        platform::InventoryPage::equipment, shell, {0.0F, 0.0F}, false,
        -1.0F, 40U, 2.0F) == 2.0F);
    ARPG_REQUIRE(platform::inventory_grid_scroll_after_wheel(
        platform::InventoryPage::equipment, shell, grid_pointer, true,
        -1.0F, 40U, 2.0F) == 2.0F);
    ARPG_REQUIRE(platform::inventory_grid_scroll_after_wheel(
        platform::InventoryPage::equipment, shell, grid_pointer, false,
        0.0F, 40U, 2.0F) == 2.0F);
    return {};
}

test::Failure recipe_cache_rejects_incompatible_or_filtered_items() noexcept {
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

    state.items = {
        make_item(701U, 1U, items::ItemRarity::normal),
        make_item(702U, 1U, items::ItemRarity::normal),
        make_item(703U, 1U, items::ItemRarity::normal),
    };
    platform::refresh_inventory_view_cache(cache, state, 92U, {}, 0U, recipe);
    ARPG_REQUIRE(platform::cached_recipe_ready(cache));
    platform::InventoryFilter hidden_recipe_filter{};
    hidden_recipe_filter.rarity = items::ItemRarity::magic;
    platform::refresh_inventory_view_cache(cache, state, 92U,
        hidden_recipe_filter, 0U, recipe);
    ARPG_REQUIRE(cache.filtered_indices.empty());
    ARPG_REQUIRE(cache.resolved_recipe.count == 0U);
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
    ARPG_REQUIRE(std::string_view{local.name} == u8"武器物理伤害");
    ARPG_REQUIRE(std::string_view{local.scope} == u8"局部");
    ARPG_REQUIRE(std::string_view{local.suffix} == "%");
    ARPG_REQUIRE(std::string_view{local.qualifier} == u8"提高");
    ARPG_REQUIRE(!local.multiplier);
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
    ARPG_REQUIRE(std::string_view{weapon_speed.scope} == u8"局部");
    ARPG_REQUIRE(std::string_view{glove_speed.scope} == u8"全局");
    ARPG_REQUIRE(std::string_view{weapon_speed.suffix} == "%");
    ARPG_REQUIRE(std::string_view{glove_speed.suffix} == "%");
    ARPG_REQUIRE(test::near(platform::item_attribute_display_value(
        attack_speed->values[7], weapon_speed), 12.0));

    const auto global = platform::item_attribute_label(
        items::ItemEffectKind::global_modifier,
        modifiers::StatId::max_health,
        modifiers::ModifierOperation::flat,
        items::ItemSlot::helmet, 0xFFU);
    ARPG_REQUIRE(std::string_view{global.name} == u8"最大生命");
    ARPG_REQUIRE(std::string_view{global.scope} == u8"全局");
    ARPG_REQUIRE(std::string_view{global.suffix}.empty());
    ARPG_REQUIRE(std::string_view{global.qualifier}.empty());

    const auto variant = platform::item_attribute_label(
        items::ItemEffectKind::variant_element_damage_reduction_cap,
        modifiers::StatId::fire_damage_reduction_cap,
        modifiers::ModifierOperation::flat,
        items::ItemSlot::accessory, 2U);
    ARPG_REQUIRE(std::string_view{variant.name} == u8"闪电减伤上限");
    ARPG_REQUIRE(std::string_view{variant.scope} == u8"全局");
    ARPG_REQUIRE(std::string_view{variant.suffix} == "%");
    ARPG_REQUIRE(std::string_view{variant.qualifier}.empty());
    ARPG_REQUIRE(test::near(
        platform::item_attribute_display_value(2000, variant), 20.0));
    ARPG_REQUIRE(test::near(
        platform::item_attribute_display_value(46, global), 46.0));

    const auto more = platform::item_attribute_label(
        items::ItemEffectKind::global_modifier,
        modifiers::StatId::melee_damage,
        modifiers::ModifierOperation::more,
        items::ItemSlot::weapon, 0xFFU);
    ARPG_REQUIRE(std::string_view{more.name} == u8"最终近战伤害");
    ARPG_REQUIRE(std::string_view{more.scope} == u8"全局");
    ARPG_REQUIRE(std::string_view{more.suffix}.empty());
    ARPG_REQUIRE(std::string_view{more.qualifier}.empty());
    ARPG_REQUIRE(more.multiplier);
    ARPG_REQUIRE(test::near(
        platform::item_attribute_display_value(11200, more), 1.12));

    const auto conversion = platform::item_attribute_label(
        items::ItemEffectKind::global_modifier,
        modifiers::StatId::fire_damage,
        modifiers::ModifierOperation::conversion,
        items::ItemSlot::weapon, 0xFFU);
    ARPG_REQUIRE(std::string_view{conversion.name} == u8"火焰伤害");
    ARPG_REQUIRE(std::string_view{conversion.qualifier} == u8"转化");
    ARPG_REQUIRE(!conversion.multiplier);
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
        ARPG_REQUIRE(std::string_view{label.name} != u8"装备值");
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

test::Failure tooltip_content_stays_inside_all_required_viewports() noexcept {
    constexpr std::array<std::array<int, 2>, 3> kSizes{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    items::ItemInstance item = make_item(9100U, 1U, items::ItemRarity::rare);
    item.item_level = 72U;
    item.required_level = 64U;
    item.reinforcement = 12U;
    item.affix_count = 6U;
    for (std::size_t index{}; index < item.affix_count; ++index) {
        item.affixes[index].affix_id = static_cast<std::uint16_t>(index + 1U);
        item.affixes[index].tier = 1U;
    }
    platform::BuildDifference difference{};
    difference.max_health = 100;
    difference.max_barrier = -50;
    difference.melee_damage = 200;
    difference.impulse_scale = -100;
    difference.move_speed = 300;
    difference.attack_speed = -200;
    difference.armor = 400;
    difference.evasion = -300;
    difference.weapon_physical = 500;
    difference.local_attack_speed_bp = -100;
    difference.flat_damage = {{1, -2, 3, -4, 5}};
    difference.damage_increased = {{10, -20, 30, -40, 50}};
    difference.damage_reduction = {{100, -200, 300, -400}};
    difference.damage_reduction_cap_bonus = {{100, -100, 200, -200}};
    const auto view = platform::inventory_tooltip_view(item, difference);
    ARPG_REQUIRE(view.valid);
    ARPG_REQUIRE(view.line_count >= 12U);
    std::size_t expected_placements{};
    for (std::size_t line{}; line < view.line_count; ++line)
        expected_placements += view.lines[line].segment_count;
    for (const auto& size : kSizes) {
        const auto shell = platform::inventory_shell_layout(size[0], size[1]);
        const Rectangle right_source = platform::inventory_grid_item_rectangle(
            shell, 0U, 0.0F);
        const auto right_layout = platform::inventory_tooltip_layout(shell,
            right_source, true, view, &mixed_width_measure, nullptr);
        ARPG_REQUIRE(right_layout.valid);
        ARPG_REQUIRE(inside(right_layout.backing, shell.left_panel));
        ARPG_REQUIRE(inside(right_layout.backing, shell.content));
        ARPG_REQUIRE(right_layout.backing.x + right_layout.backing.width
            + 8.0F * shell.geometry_scale <= right_source.x);
        ARPG_REQUIRE(right_layout.backing.width >= 224.0F);
        ARPG_REQUIRE(right_layout.backing.width <= 630.0F);
        const Rectangle left_source = shell.equipment_slots[
            static_cast<std::size_t>(items::ItemSlot::weapon)];
        const auto left_layout = platform::inventory_tooltip_layout(shell,
            left_source, false, view, &mixed_width_measure, nullptr);
        ARPG_REQUIRE(left_layout.valid);
        ARPG_REQUIRE(inside(left_layout.backing, shell.right_panel));
        ARPG_REQUIRE(inside(left_layout.backing, shell.content));
        ARPG_REQUIRE(left_layout.backing.x >= left_source.x
            + left_source.width + 8.0F * shell.geometry_scale);
        ARPG_REQUIRE(left_layout.placement_count == right_layout.placement_count);
        ARPG_REQUIRE(left_layout.placement_count == expected_placements);
    }

    auto compact_item = item;
    compact_item.affix_count = 0U;
    const auto compact_view = platform::inventory_tooltip_view(
        compact_item, std::nullopt);
    const auto compact_shell = platform::inventory_shell_layout(800, 450);
    const Rectangle compact_source = compact_shell.equipment_slots[
        static_cast<std::size_t>(items::ItemSlot::weapon)];
    const auto compact_layout = platform::inventory_tooltip_layout(
        compact_shell, compact_source, false, compact_view,
        &mixed_width_measure, nullptr);
    ARPG_REQUIRE(compact_layout.valid);
    ARPG_REQUIRE(inside(compact_layout.backing,
        compact_shell.grid_viewport));

    auto fallback_shell = platform::inventory_shell_layout(1280, 720);
    const Rectangle fallback_right_source =
        platform::inventory_grid_item_rectangle(fallback_shell, 0U, 0.0F);
    fallback_shell.left_panel.width = 200.0F;
    const auto fallback_left = platform::inventory_tooltip_layout(
        fallback_shell, fallback_right_source, true, view,
        &mixed_width_measure, nullptr);
    ARPG_REQUIRE(fallback_left.valid);
    ARPG_REQUIRE(inside(fallback_left.backing, fallback_shell.content));
    ARPG_REQUIRE(!inside(fallback_left.backing, fallback_shell.left_panel));
    ARPG_REQUIRE(fallback_left.backing.x + fallback_left.backing.width
        + 8.0F * fallback_shell.geometry_scale <= fallback_right_source.x);

    fallback_shell = platform::inventory_shell_layout(1280, 720);
    const Rectangle fallback_left_source = fallback_shell.equipment_slots[
        static_cast<std::size_t>(items::ItemSlot::weapon)];
    fallback_shell.right_panel.width = 200.0F;
    const auto fallback_right = platform::inventory_tooltip_layout(
        fallback_shell, fallback_left_source, false, view,
        &mixed_width_measure, nullptr);
    ARPG_REQUIRE(fallback_right.valid);
    ARPG_REQUIRE(inside(fallback_right.backing, fallback_shell.content));
    ARPG_REQUIRE(!inside(fallback_right.backing, fallback_shell.right_panel));
    ARPG_REQUIRE(fallback_right.backing.x >= fallback_left_source.x
        + fallback_left_source.width
        + 8.0F * fallback_shell.geometry_scale);

    auto blocked_shell = platform::inventory_shell_layout(800, 450);
    blocked_shell.left_panel.width = 200.0F;
    const Rectangle blocked_source{blocked_shell.content.x + 220.0F,
        blocked_shell.content.y + 40.0F, 340.0F, 60.0F};
    ARPG_REQUIRE(!platform::inventory_tooltip_layout(blocked_shell,
        blocked_source, true, view, &mixed_width_measure, nullptr).valid);

    const auto shell = platform::inventory_shell_layout(1280, 720);
    const Rectangle source = platform::inventory_grid_item_rectangle(
        shell, 0U, 0.0F);
    std::size_t valid_count{};
    const std::uint64_t before = test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 10000U; ++iteration) {
        valid_count += platform::inventory_tooltip_layout(shell, source, true,
            view, &mixed_width_measure, nullptr).valid ? 1U : 0U;
    }
    const std::uint64_t after = test::allocation_count();
    std::printf("[inventory-tooltip-layout] iterations=10000 allocations=%llu\n",
        static_cast<unsigned long long>(after - before));
    ARPG_REQUIRE(valid_count == 10000U);
    ARPG_REQUIRE(after == before);
    return {};
}

test::Failure inventory_and_skill_text_boxes_use_disjoint_safe_areas() noexcept {
    constexpr std::array<std::array<int, 2>, 3> kSizes{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto size : kSizes) {
        const auto shell = platform::inventory_shell_layout(size[0], size[1]);
        const platform::ActiveSkillLoadoutLayout skill =
            platform::active_skill_loadout_layout(shell);
        ARPG_REQUIRE(inside(skill.panel, shell.content));
        for (const Rectangle main : skill.main_slots)
            ARPG_REQUIRE(inside(main, shell.left_panel));
        for (const Rectangle support : skill.support_slots)
            ARPG_REQUIRE(inside(support, shell.left_panel));
        for (const Rectangle inventory : skill.inventory_slots)
            ARPG_REQUIRE(inside(inventory, shell.right_panel));
        ARPG_REQUIRE(inside(skill.empty_bounds, shell.right_panel));
        ARPG_REQUIRE(test::near(skill.empty_bounds.x
            + skill.empty_bounds.width * 0.5F,
            shell.right_panel.x + shell.right_panel.width * 0.5F));
        ARPG_REQUIRE(test::near(skill.empty_bounds.y
            + skill.empty_bounds.height * 0.5F,
            shell.right_panel.y + shell.right_panel.height * 0.5F));

        constexpr const char* kLabels[] = {
            u8"装备", u8"技能石", u8"材料", u8"取出",
        };
        const Rectangle controls[] = {
            shell.tabs[0U], shell.tabs[1U], shell.tabs[2U],
            skill.remove_button,
        };
        for (std::size_t index = 0U; index < 4U; ++index) {
            std::array<char, 64U> fitted{};
            const float preferred = 18.0F * skill.scale;
            const auto centered = platform::fit_centered_ui_text(
                fitted.data(), fitted.size(), kLabels[index], controls[index],
                preferred, 12.0F * skill.scale, 0.5F * skill.scale,
                &linear_measure, nullptr);
            ARPG_REQUIRE(centered.drawable);
            ARPG_REQUIRE(!centered.truncated);
            ARPG_REQUIRE(std::strcmp(fitted.data(), kLabels[index]) == 0);
            ARPG_REQUIRE(test::near(centered.position.x
                    + centered.size.x * 0.5F,
                controls[index].x + controls[index].width * 0.5F, 0.01));
            ARPG_REQUIRE(test::near(centered.position.y
                    + centered.size.y * 0.5F,
                controls[index].y + controls[index].height * 0.5F, 0.01));
        }

        const Rectangle skill_name_bounds[] = {
            {skill.main_slots[0U].x + 50.0F * skill.scale,
                skill.main_slots[0U].y + 30.0F * skill.scale,
                skill.main_slots[0U].width - 58.0F * skill.scale,
                skill.main_slots[0U].height - 38.0F * skill.scale},
            {skill.inventory_slots[0U].x + 39.0F * skill.scale,
                skill.inventory_slots[0U].y + 10.0F * skill.scale,
                skill.inventory_slots[0U].width - 46.0F * skill.scale,
                skill.inventory_slots[0U].height - 20.0F * skill.scale},
        };
        for (const Rectangle bounds : skill_name_bounds) {
            std::array<char, 64U> fitted{};
            const auto name_fit = platform::fit_centered_ui_text(
                fitted.data(), fitted.size(), u8"极·鬼剑术（暴风式）",
                bounds, std::max(12.0F, 18.0F * skill.scale), 12.0F,
                0.5F * skill.scale, &mixed_width_measure, nullptr);
            ARPG_REQUIRE(name_fit.drawable);
            ARPG_REQUIRE(!name_fit.truncated);
            ARPG_REQUIRE(std::strcmp(
                fitted.data(), u8"极·鬼剑术（暴风式）") == 0);
        }
    }
    return {};
}

test::Failure full_hd_inventory_and_skill_geometry_scales_by_one_and_a_half()
    noexcept {
    const platform::InventoryShellLayout inventory_720 =
        platform::inventory_shell_layout(1280, 720);
    const platform::InventoryShellLayout inventory_1080 =
        platform::inventory_shell_layout(1920, 1080);
    const platform::ActiveSkillLoadoutLayout skill_720 =
        platform::active_skill_loadout_layout(inventory_720);
    const platform::ActiveSkillLoadoutLayout skill_1080 =
        platform::active_skill_loadout_layout(inventory_1080);
    ARPG_REQUIRE(test::near(inventory_720.geometry_scale, 1.0F));
    ARPG_REQUIRE(test::near(inventory_1080.geometry_scale, 1.5F));
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
        platform::InventoryPage::equipment, u8"右 Shift");
    const auto skills = platform::inventory_page_title(
        platform::InventoryPage::skill_stones, u8"右 Shift");
    const auto materials = platform::inventory_page_title(
        platform::InventoryPage::materials, u8"右 Shift");
    ARPG_REQUIRE(std::string_view(equipment.data())
        == u8"按键提示：右 Shift / ESC  ·  装备背包");
    ARPG_REQUIRE(std::string_view(skills.data())
        == u8"按键提示：右 Shift / ESC  ·  技能石背包");
    ARPG_REQUIRE(std::string_view(materials.data())
        == u8"按键提示：右 Shift / ESC  ·  材料背包");
    const auto unknown = platform::inventory_page_title(
        platform::InventoryPage::equipment, nullptr);
    ARPG_REQUIRE(std::string_view(unknown.data())
        == u8"按键提示：未知 / ESC  ·  装备背包");
    const auto longest = platform::inventory_page_title(
        platform::InventoryPage::equipment, u8"右 Shift");
    ARPG_REQUIRE(std::string_view(longest.data()).rfind(
        u8"按键提示：右 Shift / ESC", 0U) == 0U);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(equipment.data()));
    ARPG_REQUIRE(platform::ui_utf8_is_valid(skills.data()));
    ARPG_REQUIRE(platform::ui_utf8_is_valid(materials.data()));
    ARPG_REQUIRE(platform::ui_utf8_is_valid(unknown.data()));

    const auto extreme = platform::inventory_page_title(
        platform::InventoryPage::equipment,
        u8"超长中文绑定名称超长中文绑定名称超长中文绑定名称超长中文绑定名称");
    const std::string_view extreme_text{extreme.data()};
    constexpr std::string_view kEquipmentSuffix = u8" / ESC  ·  装备背包";
    ARPG_REQUIRE(platform::ui_utf8_is_valid(extreme.data()));
    ARPG_REQUIRE(extreme_text.find(u8"…") != std::string_view::npos);
    ARPG_REQUIRE(extreme_text.size() >= kEquipmentSuffix.size());
    ARPG_REQUIRE(extreme_text.substr(
        extreme_text.size() - kEquipmentSuffix.size()) == kEquipmentSuffix);
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
    {"inventory recipe visibility contract",
        &recipe_cache_rejects_incompatible_or_filtered_items},
    {"inventory maximum cache complexity",
        &view_cache_never_rescans_stable_maximum_inventory},
    {"inventory readable attribute labels",
        &attribute_labels_are_human_readable_and_scoped},
    {"inventory catalog attribute units",
        &all_catalog_affixes_use_semantic_value_units},
    {"inventory tooltip content boundaries",
        &tooltip_content_stays_inside_all_required_viewports},
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
