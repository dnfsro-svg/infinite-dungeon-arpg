#include "inventory_renderer.hpp"

#include "host_input.hpp"

#include "dungeon/dungeon_session.hpp"
#include "dungeon_runtime.hpp"
#include "items/item_catalog.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {
namespace {

constexpr float kCellHeight = 58.0F;
constexpr float kGridTopInset = 82.0F;
constexpr float kGridBottomInset = 58.0F;

const char* slot_name(items::ItemSlot slot) noexcept {
    switch (slot) {
    case items::ItemSlot::weapon: return "Weapon";
    case items::ItemSlot::helmet: return "Helmet";
    case items::ItemSlot::chest: return "Chest";
    case items::ItemSlot::gloves: return "Gloves";
    case items::ItemSlot::boots: return "Boots";
    case items::ItemSlot::accessory: return "Accessory";
    case items::ItemSlot::count: break;
    }
    return "Unknown";
}

const char* rarity_name(items::ItemRarity rarity) noexcept {
    switch (rarity) {
    case items::ItemRarity::normal: return "Normal";
    case items::ItemRarity::magic: return "Magic";
    case items::ItemRarity::rare: return "Rare";
    }
    return "Unknown";
}

Color rarity_color(items::ItemRarity rarity) noexcept {
    switch (rarity) {
    case items::ItemRarity::normal: return Color{218, 225, 234, 255};
    case items::ItemRarity::magic: return Color{90, 154, 255, 255};
    case items::ItemRarity::rare: return Color{255, 199, 68, 255};
    }
    return RAYWHITE;
}

bool contains(Rectangle rectangle, Vector2 point) noexcept {
    return point.x >= rectangle.x && point.x <= rectangle.x + rectangle.width
        && point.y >= rectangle.y && point.y <= rectangle.y + rectangle.height;
}

void draw_panel(Rectangle rectangle, const char* title) noexcept {
    DrawRectangleRounded(rectangle, 0.025F, 5, Color{8, 12, 20, 247});
    DrawRectangleRoundedLinesEx(rectangle, 0.025F, 5, 1.0F,
        Color{72, 91, 120, 255});
    DrawText(title, static_cast<int>(rectangle.x + 12.0F),
        static_cast<int>(rectangle.y + 10.0F), 18,
        Color{131, 211, 255, 255});
}

void draw_button(Rectangle rectangle, const char* label,
    bool enabled, bool active = false) noexcept {
    const Color fill = !enabled ? Color{42, 45, 52, 255}
        : active ? Color{42, 104, 139, 255} : Color{28, 47, 67, 255};
    const Color text = enabled ? RAYWHITE : Color{118, 123, 133, 255};
    DrawRectangleRounded(rectangle, 0.12F, 4, fill);
    DrawRectangleRoundedLinesEx(rectangle, 0.12F, 4, 1.0F,
        enabled ? Color{90, 151, 190, 255} : Color{65, 68, 75, 255});
    DrawText(label, static_cast<int>(rectangle.x + 7.0F),
        static_cast<int>(rectangle.y + 6.0F), 14, text);
}

int grid_columns(Rectangle grid) noexcept {
    return std::max(1, static_cast<int>((grid.width - 20.0F) / 112.0F));
}

float grid_cell_width(Rectangle grid, int columns) noexcept {
    return (grid.width - 20.0F) / static_cast<float>(columns);
}

Rectangle grid_viewport(Rectangle grid) noexcept {
    return {grid.x + 10.0F, grid.y + kGridTopInset,
        std::max(0.0F, grid.width - 20.0F),
        std::max(0.0F, grid.height - kGridTopInset - kGridBottomInset)};
}

Rectangle grid_item_rectangle(Rectangle viewport, int columns,
    std::size_t filtered_position, float scroll_rows) noexcept {
    const float width = viewport.width / static_cast<float>(columns);
    const std::size_t row = filtered_position / static_cast<std::size_t>(columns);
    const std::size_t column = filtered_position % static_cast<std::size_t>(columns);
    return {viewport.x + static_cast<float>(column) * width + 2.0F,
        viewport.y + (static_cast<float>(row) - scroll_rows) * kCellHeight + 2.0F,
        width - 4.0F, kCellHeight - 4.0F};
}

Rectangle slot_filter_button(Rectangle grid) noexcept {
    return {grid.x + 10.0F, grid.y + 40.0F,
        std::max(80.0F, grid.width * 0.48F - 15.0F), 28.0F};
}

Rectangle rarity_filter_button(Rectangle grid) noexcept {
    return {grid.x + grid.width * 0.50F + 5.0F, grid.y + 40.0F,
        std::max(80.0F, grid.width * 0.50F - 15.0F), 28.0F};
}

Rectangle combine_button(Rectangle grid) noexcept {
    return {grid.x + 10.0F, grid.y + grid.height - 42.0F,
        grid.width - 20.0F, 30.0F};
}

bool equipment_equal(const items::EquipmentState& left,
    const items::EquipmentState& right) noexcept {
    return left.equipped_ids == right.equipped_ids;
}

std::size_t base_value_index(std::uint8_t item_level) noexcept {
    for (std::uint8_t tier = 1U; tier <= 8U; ++tier) {
        if (item_level >= items::tier_minimum_level(tier)) {
            return static_cast<std::size_t>(8U - tier);
        }
    }
    return 0U;
}

}  // namespace

void InventoryRenderer::open(const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot) {
    open_ = true;
    sync(session, snapshot);
}

void InventoryRenderer::close() noexcept {
    open_ = false;
    click_tracker_ = {};
}

bool InventoryRenderer::is_open() const noexcept { return open_; }

const items::ItemInstance* InventoryRenderer::selected_item(
    const items::ItemOwnershipState& state) const noexcept {
    return cached_selected_item(view_cache_, state);
}

void InventoryRenderer::refresh_comparison(
    const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot) {
    const items::ItemOwnershipState& state = session.item_state();
    if (comparison_generation_ == snapshot.commit_generation
        && comparison_item_id_ == selected_item_id_
        && equipment_equal(comparison_equipment_, state.equipment)) return;
    comparison_generation_ = snapshot.commit_generation;
    comparison_item_id_ = selected_item_id_;
    comparison_equipment_ = state.equipment;
    current_build_ = session.preview_equipment_build(state.equipment);
    difference_.reset();
    const items::ItemInstance* const item = selected_item(state);
    const items::BaseDefinition* const base = item == nullptr
        ? nullptr : items::base_definition(item->base_id);
    if (!current_build_.has_value() || item == nullptr || base == nullptr) return;
    items::EquipmentState candidate = state.equipment;
    candidate.equipped_ids[static_cast<std::size_t>(base->slot)] = item->id;
    const auto candidate_build = session.preview_equipment_build(candidate);
    if (candidate_build.has_value()) {
        difference_ = compare_player_builds(*current_build_, *candidate_build);
    }
}

void InventoryRenderer::sync(const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot) {
    const items::ItemOwnershipState& state = session.item_state();
    refresh_inventory_view_cache(view_cache_, state,
        snapshot.commit_generation, filter_, selected_item_id_, recipe_);
    if (selected_item_id_ != 0U && selected_item(state) == nullptr) {
        selected_item_id_ = 0U;
        view_cache_.selected_item_id = 0U;
    }
    if (recipe_.ids != view_cache_.resolved_recipe.ids
        || recipe_.count != view_cache_.resolved_recipe.count) {
        recipe_ = view_cache_.resolved_recipe;
        view_cache_.requested_recipe = recipe_;
    }
    refresh_comparison(session, snapshot);
}

bool InventoryRenderer::recipe_ready() const noexcept {
    return cached_recipe_ready(view_cache_);
}

bool InventoryRenderer::process_input(DungeonRuntime& runtime,
    const dungeon::DungeonSnapshot& snapshot,
    const HostFrameInput& input) {
    if (!open_ || runtime.session() == nullptr) return false;
    dungeon::DungeonSession& session = *runtime.session();
    sync(session, snapshot);
    const items::ItemOwnershipState& state = session.item_state();
    const InventoryLayout layout = inventory_layout(GetScreenWidth(), GetScreenHeight());
    const Rectangle viewport = grid_viewport(layout.grid);
    const int columns = grid_columns(layout.grid);
    const float wheel = input.mouse_wheel;
    if (wheel != 0.0F) {
        scroll_rows_ = clamp_inventory_scroll_rows(view_cache_.filtered_indices.size(), columns,
            scroll_rows_ - wheel, viewport.height, kCellHeight);
    }
    const bool left_pressed = input.mouse_left_pressed;
    const bool right_pressed = input.mouse_right_pressed;
    if (!left_pressed && !right_pressed) return false;
    const Vector2 mouse = input.mouse_position;
    if (left_pressed && contains(slot_filter_button(layout.grid), mouse)) {
        if (!filter_.slot.has_value()) filter_.slot = items::ItemSlot::weapon;
        else if (*filter_.slot == items::ItemSlot::accessory) filter_.slot.reset();
        else filter_.slot = static_cast<items::ItemSlot>(
            static_cast<std::uint8_t>(*filter_.slot) + 1U);
        sync(session, snapshot);
        scroll_rows_ = 0.0F;
        return false;
    }
    if (left_pressed && contains(rarity_filter_button(layout.grid), mouse)) {
        if (!filter_.rarity.has_value()) filter_.rarity = items::ItemRarity::normal;
        else if (*filter_.rarity == items::ItemRarity::rare) filter_.rarity.reset();
        else filter_.rarity = static_cast<items::ItemRarity>(
            static_cast<std::uint8_t>(*filter_.rarity) + 1U);
        sync(session, snapshot);
        scroll_rows_ = 0.0F;
        return false;
    }
    const bool requests_enabled = !snapshot.pending_save_kind.has_value()
        && runtime.state() == DungeonRuntimeState::running;
    if (left_pressed) {
        if (const auto slot = hit_test_equipped_slot(
                mouse, layout, state.equipment)) {
            if (requests_enabled
                && runtime.request_unequip(*slot)
                    == dungeon::RequestResult::accepted) {
                runtime.service_pending_save();
                return true;
            }
            return false;
        }
    }
    if (left_pressed && contains(combine_button(layout.grid), mouse)) {
        if (requests_enabled && recipe_ready()
            && runtime.request_recipe(recipe_.ids) == dungeon::RequestResult::accepted) {
            runtime.service_pending_save();
            return true;
        }
        return false;
    }
    const VisibleGridRange visible = visible_grid_range(view_cache_.filtered_indices.size(),
        columns, scroll_rows_, viewport.height, kCellHeight);
    for (std::size_t offset = 0U; offset < visible.count; ++offset) {
        const std::size_t filtered_position = visible.first + offset;
        const Rectangle cell = grid_item_rectangle(viewport, columns,
            filtered_position, scroll_rows_);
        if (!contains(cell, mouse)) continue;
        const items::ItemInstance& item = state.items[
            view_cache_.filtered_indices[filtered_position]];
        selected_item_id_ = item.id;
        const bool recipe_toggle = right_pressed || input.control_down;
        if (recipe_toggle) {
            static_cast<void>(toggle_recipe_selection(recipe_, item.id));
        }
        comparison_generation_ = ~std::uint64_t{0U};
        sync(session, snapshot);
        if (recipe_toggle) {
            click_tracker_ = {};
            return false;
        }
        if (register_inventory_click(click_tracker_, item.id, GetTime())
                == InventoryClickKind::double_click
            && requests_enabled
            && runtime.request_equip(item.id) == dungeon::RequestResult::accepted) {
            runtime.service_pending_save();
            return true;
        }
        return false;
    }
    return false;
}

void InventoryRenderer::draw(const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus&) {
    if (!open_) return;
    sync(session, snapshot);
    const items::ItemOwnershipState& state = session.item_state();
    const InventoryLayout layout = inventory_layout(GetScreenWidth(), GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{3, 5, 9, 225});
    DrawText("EQUIPMENT INVENTORY - I / ESC CLOSE", 14, 16, 24,
        Color{141, 221, 255, 255});
    draw_panel(layout.equipment, "EQUIPMENT");
    draw_panel(layout.grid, "INVENTORY");
    draw_panel(layout.detail, "ITEM DETAIL");

    for (std::size_t index = 0U; index < state.equipment.equipped_ids.size(); ++index) {
        const auto slot = static_cast<items::ItemSlot>(index);
        const Rectangle rectangle = equipment_slot_rectangle(layout, slot);
        DrawRectangleRounded(rectangle, 0.08F, 4, Color{21, 28, 40, 255});
        DrawRectangleRoundedLinesEx(rectangle, 0.08F, 4, 1.0F,
            Color{75, 100, 133, 255});
        const std::uint64_t id = state.equipment.equipped_ids[index];
        if (id == 0U) {
            DrawText(TextFormat("%s  [empty]", slot_name(slot)),
                static_cast<int>(rectangle.x + 8.0F),
                static_cast<int>(rectangle.y + 8.0F), 13, RAYWHITE);
        } else {
            DrawText(TextFormat("%s  #%llu", slot_name(slot),
                static_cast<unsigned long long>(id)),
                static_cast<int>(rectangle.x + 8.0F),
                static_cast<int>(rectangle.y + 8.0F), 13, RAYWHITE);
        }
    }
    int stat_y = static_cast<int>(layout.equipment.y + 248.0F);
    if (snapshot.combat.has_value()) {
        const combat::PlayerSnapshot& player = snapshot.combat->player;
        DrawText(TextFormat("HP %d/%d  Barrier %d/%d", player.hp, player.max_hp,
            player.barrier, player.max_barrier),
            static_cast<int>(layout.equipment.x + 12.0F), stat_y, 12, RAYWHITE);
        stat_y += 17;
        DrawText(TextFormat("Armor %lld (%g%%)",
            static_cast<long long>(player.armor), player.armor_reduction_bp / 100.0F),
            static_cast<int>(layout.equipment.x + 12.0F), stat_y, 12, RAYWHITE);
        stat_y += 17;
        DrawText(TextFormat("Evasion %lld (%g%%)",
            static_cast<long long>(player.evasion), player.evasion_rate_bp / 100.0F),
            static_cast<int>(layout.equipment.x + 12.0F), stat_y, 12, RAYWHITE);
        stat_y += 17;
        DrawText(TextFormat("F %g/%g  W %g/%g%%",
            player.damage_reduction[0] / 100.0F,
            player.damage_reduction_cap[0] / 100.0F,
            player.damage_reduction[1] / 100.0F,
            player.damage_reduction_cap[1] / 100.0F),
            static_cast<int>(layout.equipment.x + 12.0F), stat_y, 12, RAYWHITE);
        stat_y += 17;
        DrawText(TextFormat("L %g/%g  C %g/%g%%",
            player.damage_reduction[2] / 100.0F,
            player.damage_reduction_cap[2] / 100.0F,
            player.damage_reduction[3] / 100.0F,
            player.damage_reduction_cap[3] / 100.0F),
            static_cast<int>(layout.equipment.x + 12.0F), stat_y, 12, RAYWHITE);
    }
    if (current_build_.has_value()) {
        stat_y += 22;
        DrawText(TextFormat("Move %g%%  Attack %g%%",
            current_build_->values.movement_speed / 100.0F,
            current_build_->values.attack_speed / 100.0F),
            static_cast<int>(layout.equipment.x + 12.0F), stat_y, 12,
            Color{155, 211, 255, 255});
        stat_y += 17;
        DrawText(TextFormat("Weapon physical +%lld",
            static_cast<long long>(current_build_->weapon_physical)),
            static_cast<int>(layout.equipment.x + 12.0F), stat_y, 12,
            Color{155, 211, 255, 255});
        stat_y += 17;
        DrawText(TextFormat("Melee %g%%",
            current_build_->values.melee_damage / 100.0F),
            static_cast<int>(layout.equipment.x + 12.0F), stat_y, 12,
            Color{155, 211, 255, 255});
    }

    const Rectangle slot_button = slot_filter_button(layout.grid);
    const Rectangle rarity_button = rarity_filter_button(layout.grid);
    draw_button(slot_button, TextFormat("Slot: %s",
        filter_.slot.has_value() ? slot_name(*filter_.slot) : "All"), true);
    draw_button(rarity_button, TextFormat("Rarity: %s",
        filter_.rarity.has_value() ? rarity_name(*filter_.rarity) : "All"), true);
    const Rectangle viewport = grid_viewport(layout.grid);
    DrawRectangleRec(viewport, Color{11, 16, 25, 255});
    BeginScissorMode(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
        static_cast<int>(viewport.width), static_cast<int>(viewport.height));
    const int columns = grid_columns(layout.grid);
    scroll_rows_ = clamp_inventory_scroll_rows(view_cache_.filtered_indices.size(), columns,
        scroll_rows_, viewport.height, kCellHeight);
    const VisibleGridRange visible = visible_grid_range(view_cache_.filtered_indices.size(),
        columns, scroll_rows_, viewport.height, kCellHeight);
    for (std::size_t offset = 0U; offset < visible.count; ++offset) {
        const std::size_t filtered_position = visible.first + offset;
        const items::ItemInstance& item = state.items[
            view_cache_.filtered_indices[filtered_position]];
        const items::BaseDefinition* const base = items::base_definition(item.base_id);
        const Rectangle cell = grid_item_rectangle(viewport, columns,
            filtered_position, scroll_rows_);
        const bool selected = item.id == selected_item_id_;
        const bool recipe_selected = std::find(recipe_.ids.begin(),
            recipe_.ids.begin() + static_cast<std::ptrdiff_t>(recipe_.count),
            item.id) != recipe_.ids.begin() + static_cast<std::ptrdiff_t>(recipe_.count);
        DrawRectangleRounded(cell, 0.08F, 4,
            selected ? Color{34, 68, 88, 255} : Color{21, 28, 39, 255});
        DrawRectangleRoundedLinesEx(cell, 0.08F, 4,
            recipe_selected ? 3.0F : 1.0F,
            recipe_selected ? Color{237, 118, 212, 255} : rarity_color(item.rarity));
        DrawText(base == nullptr ? "Invalid" : base->name.data(),
            static_cast<int>(cell.x + 5.0F), static_cast<int>(cell.y + 7.0F),
            13, rarity_color(item.rarity));
        DrawText(TextFormat("%s  i%u", rarity_name(item.rarity),
            static_cast<unsigned>(item.item_level)),
            static_cast<int>(cell.x + 5.0F), static_cast<int>(cell.y + 31.0F),
            12, Color{174, 183, 198, 255});
    }
    EndScissorMode();
    const bool enabled = !snapshot.pending_save_kind.has_value()
        && recipe_ready();
    draw_button(combine_button(layout.grid), TextFormat("Combine (%u/3)",
        static_cast<unsigned>(recipe_.count)), enabled);
    if (snapshot.pending_save_kind.has_value()) {
        DrawText("SAVE PENDING - ACTIONS DISABLED",
            static_cast<int>(layout.grid.x + 12.0F),
            static_cast<int>(layout.grid.y + 13.0F), 12,
            Color{255, 191, 96, 255});
    }

    const items::ItemInstance* const item = selected_item(state);
    const Rectangle first_detail_line = detail_line_rectangle(layout, 0U);
    const int detail_x = static_cast<int>(first_detail_line.x);
    const int detail_y = static_cast<int>(first_detail_line.y);
    if (item == nullptr) {
        DrawText("Click an item to inspect.", detail_x, detail_y, 15,
            Color{166, 175, 191, 255});
        return;
    }
    const items::BaseDefinition* const base = items::base_definition(item->base_id);
    if (base == nullptr) return;
    BeginScissorMode(static_cast<int>(layout.detail.x),
        static_cast<int>(layout.detail.y), static_cast<int>(layout.detail.width),
        static_cast<int>(layout.detail.height));
    std::size_t detail_line = 0U;
    const auto draw_detail_line = [&](const char* text, Color color) noexcept {
        const Rectangle line = detail_line_rectangle(layout, detail_line++);
        DrawText(text, static_cast<int>(line.x), static_cast<int>(line.y),
            10, color);
    };
    draw_detail_line(TextFormat("%s %s", rarity_name(item->rarity),
        base->name.data()), rarity_color(item->rarity));
    draw_detail_line(TextFormat("iLvl %u  Required %u",
        static_cast<unsigned>(item->item_level),
        static_cast<unsigned>(item->required_level)), RAYWHITE);
    const ItemAttributeLabel inherent = item_attribute_label(base->effect,
        base->stat, base->operation, base->slot, 0xFFU);
    const std::int32_t inherent_raw =
        base->values[base_value_index(item->item_level)];
    draw_detail_line(TextFormat("Base [%s] %s %+g%s%s", inherent.scope,
        inherent.name, item_attribute_display_value(inherent_raw, inherent),
        inherent.suffix, inherent.qualifier), Color{150, 210, 255, 255});
    for (std::size_t index = 0U; index < item->affix_count; ++index) {
        const items::AffixRoll& roll = item->affixes[index];
        const items::AffixDefinition* const affix = items::affix_definition(roll.affix_id);
        if (affix == nullptr) continue;
        const ItemAttributeLabel label = item_attribute_label(affix->effect,
            affix->stat, affix->operation, base->slot, roll.variant);
        const std::int32_t affix_raw =
            affix->values[static_cast<std::size_t>(8U - roll.tier)];
        draw_detail_line(TextFormat("T%u %s [%s] %s %+g%s%s",
            static_cast<unsigned>(roll.tier),
            affix->kind == items::AffixKind::prefix ? "Pre" : "Suf",
            label.scope, label.name,
            item_attribute_display_value(affix_raw, label),
            label.suffix, label.qualifier), Color{216, 222, 234, 255});
    }
    draw_detail_line("FULL BUILD DIFFERENCE", Color{242, 183, 255, 255});
    if (!difference_.has_value()) {
        draw_detail_line("Preview unavailable", Color{255, 126, 126, 255});
    } else {
        const BuildDifference& diff = *difference_;
        draw_detail_line(TextFormat("HP %+g  Barrier %+g",
            diff.max_health / static_cast<double>(modifiers::kFixedOne),
            diff.max_barrier / static_cast<double>(modifiers::kFixedOne)), RAYWHITE);
        draw_detail_line(TextFormat("Flat P/F/W %+g/%+g/%+g",
            diff.flat_damage[0] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[1] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[2] / static_cast<double>(modifiers::kFixedOne)), RAYWHITE);
        draw_detail_line(TextFormat("Flat L/C %+g/%+g",
            diff.flat_damage[3] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[4] / static_cast<double>(modifiers::kFixedOne)), RAYWHITE);
        draw_detail_line(TextFormat("Inc P/F/W %+g/%+g/%+g%%",
            diff.damage_increased[0] / 100.0, diff.damage_increased[1] / 100.0,
            diff.damage_increased[2] / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("Inc L/C %+g/%+g%%",
            diff.damage_increased[3] / 100.0,
            diff.damage_increased[4] / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("Melee/Knock %+g/%+g%%",
            diff.melee_damage / 100.0, diff.impulse_scale / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("Move/Attack %+g/%+g%%",
            diff.move_speed / 100.0, diff.attack_speed / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("LocalAtk %+g%%  Weapon %+g",
            diff.local_attack_speed_bp / 100.0,
            static_cast<double>(diff.weapon_physical)), RAYWHITE);
        draw_detail_line(TextFormat("Armor %+g  DR %+g%%",
            static_cast<double>(diff.armor), diff.armor_reduction_bp / 100.0),
            RAYWHITE);
        draw_detail_line(TextFormat("Evasion %+g  Chance %+g%%",
            static_cast<double>(diff.evasion), diff.evasion_rate_bp / 100.0),
            RAYWHITE);
        draw_detail_line(TextFormat("F/W DR %+g/%+g%%",
            diff.damage_reduction[0] / 100.0,
            diff.damage_reduction[1] / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("L/C DR %+g/%+g%%",
            diff.damage_reduction[2] / 100.0,
            diff.damage_reduction[3] / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("F/W cap %+g/%+g%%",
            diff.damage_reduction_cap_bonus[0] / 100.0,
            diff.damage_reduction_cap_bonus[1] / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("L/C cap %+g/%+g%%",
            diff.damage_reduction_cap_bonus[2] / 100.0,
            diff.damage_reduction_cap_bonus[3] / 100.0), RAYWHITE);
    }
    EndScissorMode();
}

}  // namespace arpg::platform
