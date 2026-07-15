#include "inventory_renderer.hpp"

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

bool item_equipped(const items::EquipmentState& equipment,
    std::uint64_t id) noexcept {
    return std::find(equipment.equipped_ids.begin(),
        equipment.equipped_ids.end(), id) != equipment.equipped_ids.end();
}

std::size_t base_value_index(std::uint8_t item_level) noexcept {
    for (std::uint8_t tier = 1U; tier <= 8U; ++tier) {
        if (item_level >= items::tier_minimum_level(tier)) {
            return static_cast<std::size_t>(8U - tier);
        }
    }
    return 0U;
}

const char* effect_scope(items::ItemEffectKind effect,
    items::ItemSlot slot) noexcept {
    return effect == items::ItemEffectKind::local_weapon_physical_flat
            || effect == items::ItemEffectKind::local_weapon_physical_increased
            || (effect == items::ItemEffectKind::slot_dependent_attack_speed
                && slot == items::ItemSlot::weapon)
        ? "LOCAL" : "GLOBAL";
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
    for (const items::ItemInstance& item : state.items) {
        if (item.id == selected_item_id_) return &item;
    }
    return nullptr;
}

void InventoryRenderer::prune_selection(
    const items::ItemOwnershipState& state) noexcept {
    if (selected_item(state) == nullptr) selected_item_id_ = 0U;
    for (std::size_t index = 0U; index < recipe_.count;) {
        bool exists = false;
        for (const items::ItemInstance& item : state.items) {
            if (item.id == recipe_.ids[index]
                && !item_equipped(state.equipment, item.id)) {
                exists = true;
                break;
            }
        }
        if (exists) {
            ++index;
            continue;
        }
        for (std::size_t move = index + 1U; move < recipe_.count; ++move) {
            recipe_.ids[move - 1U] = recipe_.ids[move];
        }
        recipe_.ids[--recipe_.count] = 0U;
    }
}

void InventoryRenderer::rebuild_filter(
    const items::ItemOwnershipState& state) {
    filtered_indices_ = filtered_inventory_indices(state, filter_);
    filtered_equipment_ = state.equipment;
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
    const bool stable_state_changed =
        filtered_generation_ != snapshot.commit_generation
        || !equipment_equal(filtered_equipment_, state.equipment);
    if (stable_state_changed) {
        prune_selection(state);
        rebuild_filter(state);
        filtered_generation_ = snapshot.commit_generation;
    }
    refresh_comparison(session, snapshot);
}

bool InventoryRenderer::recipe_ready(
    const items::ItemOwnershipState& state) const noexcept {
    if (recipe_.count != 3U) return false;
    const items::ItemInstance* first = nullptr;
    items::ItemSlot slot = items::ItemSlot::count;
    for (std::size_t selected = 0U; selected < recipe_.count; ++selected) {
        const items::ItemInstance* found = nullptr;
        for (const items::ItemInstance& item : state.items) {
            if (item.id == recipe_.ids[selected]) {
                found = &item;
                break;
            }
        }
        if (found == nullptr || item_equipped(state.equipment, found->id)) return false;
        const items::BaseDefinition* const base = items::base_definition(found->base_id);
        if (base == nullptr) return false;
        if (first == nullptr) {
            first = found;
            slot = base->slot;
        } else if (found->rarity != first->rarity || base->slot != slot) {
            return false;
        }
    }
    return true;
}

bool InventoryRenderer::process_input(DungeonRuntime& runtime,
    const dungeon::DungeonSnapshot& snapshot) {
    if (!open_ || runtime.session() == nullptr) return false;
    dungeon::DungeonSession& session = *runtime.session();
    sync(session, snapshot);
    const items::ItemOwnershipState& state = session.item_state();
    const InventoryLayout layout = inventory_layout(GetScreenWidth(), GetScreenHeight());
    const Rectangle viewport = grid_viewport(layout.grid);
    const int columns = grid_columns(layout.grid);
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0F) {
        scroll_rows_ = clamp_inventory_scroll_rows(filtered_indices_.size(), columns,
            scroll_rows_ - wheel, viewport.height, kCellHeight);
    }
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
    const Vector2 mouse = GetMousePosition();
    if (contains(slot_filter_button(layout.grid), mouse)) {
        if (!filter_.slot.has_value()) filter_.slot = items::ItemSlot::weapon;
        else if (*filter_.slot == items::ItemSlot::accessory) filter_.slot.reset();
        else filter_.slot = static_cast<items::ItemSlot>(
            static_cast<std::uint8_t>(*filter_.slot) + 1U);
        rebuild_filter(state);
        filtered_generation_ = snapshot.commit_generation;
        scroll_rows_ = 0.0F;
        return false;
    }
    if (contains(rarity_filter_button(layout.grid), mouse)) {
        if (!filter_.rarity.has_value()) filter_.rarity = items::ItemRarity::normal;
        else if (*filter_.rarity == items::ItemRarity::rare) filter_.rarity.reset();
        else filter_.rarity = static_cast<items::ItemRarity>(
            static_cast<std::uint8_t>(*filter_.rarity) + 1U);
        rebuild_filter(state);
        filtered_generation_ = snapshot.commit_generation;
        scroll_rows_ = 0.0F;
        return false;
    }
    const bool requests_enabled = !snapshot.pending_save_kind.has_value()
        && runtime.state() == DungeonRuntimeState::running;
    if (const auto slot = hit_test_equipped_slot(mouse, layout, state.equipment)) {
        if (requests_enabled
            && runtime.request_unequip(*slot) == dungeon::RequestResult::accepted) {
            runtime.service_pending_save();
            return true;
        }
        return false;
    }
    if (contains(combine_button(layout.grid), mouse)) {
        if (requests_enabled && recipe_ready(state)
            && runtime.request_recipe(recipe_.ids) == dungeon::RequestResult::accepted) {
            runtime.service_pending_save();
            return true;
        }
        return false;
    }
    const VisibleGridRange visible = visible_grid_range(filtered_indices_.size(),
        columns, scroll_rows_, viewport.height, kCellHeight);
    for (std::size_t offset = 0U; offset < visible.count; ++offset) {
        const std::size_t filtered_position = visible.first + offset;
        const Rectangle cell = grid_item_rectangle(viewport, columns,
            filtered_position, scroll_rows_);
        if (!contains(cell, mouse)) continue;
        const items::ItemInstance& item = state.items[
            filtered_indices_[filtered_position]];
        selected_item_id_ = item.id;
        comparison_generation_ = ~std::uint64_t{0U};
        refresh_comparison(session, snapshot);
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            static_cast<void>(toggle_recipe_selection(recipe_, item.id));
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
    scroll_rows_ = clamp_inventory_scroll_rows(filtered_indices_.size(), columns,
        scroll_rows_, viewport.height, kCellHeight);
    const VisibleGridRange visible = visible_grid_range(filtered_indices_.size(),
        columns, scroll_rows_, viewport.height, kCellHeight);
    for (std::size_t offset = 0U; offset < visible.count; ++offset) {
        const std::size_t filtered_position = visible.first + offset;
        const items::ItemInstance& item = state.items[
            filtered_indices_[filtered_position]];
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
        && recipe_ready(state);
    draw_button(combine_button(layout.grid), TextFormat("Combine (%u/3)",
        static_cast<unsigned>(recipe_.count)), enabled);
    if (snapshot.pending_save_kind.has_value()) {
        DrawText("SAVE PENDING - ACTIONS DISABLED",
            static_cast<int>(layout.grid.x + 12.0F),
            static_cast<int>(layout.grid.y + 13.0F), 12,
            Color{255, 191, 96, 255});
    }

    const items::ItemInstance* const item = selected_item(state);
    int detail_y = static_cast<int>(layout.detail.y + 42.0F);
    const int detail_x = static_cast<int>(layout.detail.x + 12.0F);
    if (item == nullptr) {
        DrawText("Click an item to inspect.", detail_x, detail_y, 15,
            Color{166, 175, 191, 255});
        return;
    }
    const items::BaseDefinition* const base = items::base_definition(item->base_id);
    if (base == nullptr) return;
    DrawText(TextFormat("%s %s", rarity_name(item->rarity), base->name.data()),
        detail_x, detail_y, 18, rarity_color(item->rarity));
    detail_y += 25;
    DrawText(TextFormat("Item level %u   Required level %u",
        static_cast<unsigned>(item->item_level),
        static_cast<unsigned>(item->required_level)), detail_x, detail_y, 14,
        RAYWHITE);
    detail_y += 22;
    DrawText(TextFormat("INHERENT [%s] %+d", effect_scope(base->effect, base->slot),
        base->values[base_value_index(item->item_level)]),
        detail_x, detail_y, 14, Color{150, 210, 255, 255});
    detail_y += 24;
    for (std::size_t index = 0U; index < item->affix_count; ++index) {
        const items::AffixRoll& roll = item->affixes[index];
        const items::AffixDefinition* const affix = items::affix_definition(roll.affix_id);
        if (affix == nullptr) continue;
        const char* const kind = affix->kind == items::AffixKind::prefix
            ? "P" : "S";
        if (roll.variant < 4U) {
            DrawText(TextFormat("T%u %s#%u [%s] %+d E%u",
                static_cast<unsigned>(roll.tier), kind,
                static_cast<unsigned>(roll.affix_id),
                effect_scope(affix->effect, base->slot),
                affix->values[static_cast<std::size_t>(8U - roll.tier)],
                static_cast<unsigned>(roll.variant)),
                detail_x, detail_y, 12, Color{216, 222, 234, 255});
        } else {
            DrawText(TextFormat("T%u %s#%u [%s] %+d",
            static_cast<unsigned>(roll.tier),
                kind, static_cast<unsigned>(roll.affix_id),
                effect_scope(affix->effect, base->slot),
                affix->values[static_cast<std::size_t>(8U - roll.tier)]),
                detail_x, detail_y, 12, Color{216, 222, 234, 255});
        }
        detail_y += 19;
    }
    detail_y += 8;
    DrawText("FULL BUILD DIFFERENCE", detail_x, detail_y, 15,
        Color{242, 183, 255, 255});
    detail_y += 21;
    if (!difference_.has_value()) {
        DrawText("Preview unavailable", detail_x, detail_y, 13,
            Color{255, 126, 126, 255});
        return;
    }
    const BuildDifference& diff = *difference_;
    DrawText(TextFormat("HP %+lld  Barrier %+lld  Armor %+lld  Evasion %+lld",
        static_cast<long long>(diff.max_health / modifiers::kFixedOne),
        static_cast<long long>(diff.max_barrier / modifiers::kFixedOne),
        static_cast<long long>(diff.armor), static_cast<long long>(diff.evasion)),
        detail_x, detail_y, 13, RAYWHITE);
    detail_y += 19;
    DrawText(TextFormat("Move %+g%%  Attack %+g%%  Weapon %+lld",
        diff.move_speed / 100.0, diff.attack_speed / 100.0,
        static_cast<long long>(diff.weapon_physical)),
        detail_x, detail_y, 13, RAYWHITE);
    detail_y += 19;
    DrawText(TextFormat("F/W/L/C DR %+g/%+g/%+g/%+g%%",
        diff.damage_reduction[0] / 100.0, diff.damage_reduction[1] / 100.0,
        diff.damage_reduction[2] / 100.0, diff.damage_reduction[3] / 100.0),
        detail_x, detail_y, 13, RAYWHITE);
    detail_y += 19;
    DrawText(TextFormat("F/W cap %+g/%+g%%",
        diff.damage_reduction_cap_bonus[0] / 100.0,
        diff.damage_reduction_cap_bonus[1] / 100.0),
        detail_x, detail_y, 12, RAYWHITE);
    detail_y += 18;
    DrawText(TextFormat("L/C cap %+g/%+g%%",
        diff.damage_reduction_cap_bonus[2] / 100.0,
        diff.damage_reduction_cap_bonus[3] / 100.0),
        detail_x, detail_y, 12, RAYWHITE);
}

}  // namespace arpg::platform
