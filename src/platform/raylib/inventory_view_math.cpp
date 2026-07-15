#include "inventory_view_math.hpp"

#include "items/item_catalog.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::platform {
namespace {

constexpr float kMargin = 12.0F;
constexpr float kTop = 54.0F;
constexpr float kGap = 10.0F;
constexpr float kEquipmentFraction = 0.27F;
constexpr float kGridFraction = 0.42F;
constexpr double kDoubleClickSeconds = 0.30;

bool contains(Rectangle rectangle, Vector2 point) noexcept {
    return point.x >= rectangle.x && point.x <= rectangle.x + rectangle.width
        && point.y >= rectangle.y && point.y <= rectangle.y + rectangle.height;
}

bool equipped(const items::EquipmentState& equipment,
    std::uint64_t id) noexcept {
    return std::find(equipment.equipped_ids.begin(),
        equipment.equipped_ids.end(), id) != equipment.equipped_ids.end();
}

}  // namespace

InventoryLayout inventory_layout(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return {};
    const float viewport_width = static_cast<float>(width);
    const float viewport_height = static_cast<float>(height);
    const float content_width = std::max(0.0F,
        viewport_width - kMargin * 2.0F - kGap * 2.0F);
    const float content_height = std::max(0.0F,
        viewport_height - kTop - kMargin);
    const float equipment_width = std::floor(content_width * kEquipmentFraction);
    const float grid_width = std::floor(content_width * kGridFraction);
    const float detail_width = std::max(0.0F,
        content_width - equipment_width - grid_width);
    const Rectangle equipment{kMargin, kTop, equipment_width, content_height};
    const Rectangle grid{equipment.x + equipment.width + kGap, kTop,
        grid_width, content_height};
    return {equipment, grid,
        {grid.x + grid.width + kGap, kTop, detail_width, content_height}};
}

float clamp_inventory_scroll_rows(std::size_t filtered_count, int columns,
    float scroll_rows, float viewport_height, float cell_height) noexcept {
    if (columns <= 0 || viewport_height <= 0.0F || cell_height <= 0.0F
        || !std::isfinite(scroll_rows)) return 0.0F;
    const float total_rows = std::ceil(static_cast<float>(filtered_count)
        / static_cast<float>(columns));
    const float visible_rows = viewport_height / cell_height;
    return std::clamp(scroll_rows, 0.0F,
        std::max(0.0F, total_rows - visible_rows));
}

VisibleGridRange visible_grid_range(std::size_t filtered_count, int columns,
    float scroll_rows, float viewport_height, float cell_height) noexcept {
    if (filtered_count == 0U || columns <= 0 || viewport_height <= 0.0F
        || cell_height <= 0.0F) return {};
    const float clamped = clamp_inventory_scroll_rows(filtered_count, columns,
        scroll_rows, viewport_height, cell_height);
    const std::size_t first_row = static_cast<std::size_t>(std::floor(clamped));
    const float row_fraction = clamped - static_cast<float>(first_row);
    const std::size_t visible_rows = static_cast<std::size_t>(
        std::ceil(row_fraction + viewport_height / cell_height));
    const std::size_t first = std::min(filtered_count,
        first_row * static_cast<std::size_t>(columns));
    const std::size_t requested = visible_rows * static_cast<std::size_t>(columns);
    return {first, std::min(requested, filtered_count - first)};
}

std::vector<std::size_t> filtered_inventory_indices(
    const items::ItemOwnershipState& state, InventoryFilter filter) {
    std::vector<std::size_t> result;
    result.reserve(state.items.size());
    for (std::size_t index = 0U; index < state.items.size(); ++index) {
        const items::ItemInstance& item = state.items[index];
        if (equipped(state.equipment, item.id)) continue;
        const items::BaseDefinition* base = items::base_definition(item.base_id);
        if (base == nullptr) continue;
        if (filter.slot.has_value() && base->slot != *filter.slot) continue;
        if (filter.rarity.has_value() && item.rarity != *filter.rarity) continue;
        result.push_back(index);
    }
    return result;
}

InventoryClickKind register_inventory_click(InventoryClickTracker& tracker,
    std::uint64_t item_id, double now_seconds) noexcept {
    const bool repeated = item_id != 0U && tracker.last_item_id == item_id
        && now_seconds >= tracker.last_click_seconds
        && now_seconds - tracker.last_click_seconds <= kDoubleClickSeconds;
    tracker.last_item_id = repeated ? 0U : item_id;
    tracker.last_click_seconds = repeated ? -1.0 : now_seconds;
    return repeated ? InventoryClickKind::double_click
                    : InventoryClickKind::single;
}

bool toggle_recipe_selection(RecipeSelection& selection,
    std::uint64_t item_id) noexcept {
    if (item_id == 0U) return false;
    for (std::size_t index = 0U; index < selection.count; ++index) {
        if (selection.ids[index] != item_id) continue;
        for (std::size_t move = index + 1U; move < selection.count; ++move) {
            selection.ids[move - 1U] = selection.ids[move];
        }
        selection.ids[--selection.count] = 0U;
        return true;
    }
    if (selection.count >= selection.ids.size()) return false;
    selection.ids[selection.count++] = item_id;
    return true;
}

Rectangle equipment_slot_rectangle(InventoryLayout layout,
    items::ItemSlot slot) noexcept {
    constexpr float kInset = 12.0F;
    constexpr float kSlotHeight = 30.0F;
    constexpr float kSlotGap = 4.0F;
    const std::size_t index = static_cast<std::size_t>(slot);
    if (index >= static_cast<std::size_t>(items::ItemSlot::count)) return {};
    return {layout.equipment.x + kInset,
        layout.equipment.y + 36.0F
            + static_cast<float>(index) * (kSlotHeight + kSlotGap),
        std::max(0.0F, layout.equipment.width - kInset * 2.0F), kSlotHeight};
}

std::optional<items::ItemSlot> hit_test_equipped_slot(Vector2 point,
    InventoryLayout layout, const items::EquipmentState& equipment) noexcept {
    for (std::size_t index = 0U; index < equipment.equipped_ids.size(); ++index) {
        if (equipment.equipped_ids[index] == 0U) continue;
        const auto slot = static_cast<items::ItemSlot>(index);
        if (contains(equipment_slot_rectangle(layout, slot), point)) return slot;
    }
    return std::nullopt;
}

bool inventory_can_open(bool runtime_running,
    bool passive_overlay_open) noexcept {
    return runtime_running && !passive_overlay_open;
}

bool passive_overlay_can_toggle(bool inventory_open) noexcept {
    return !inventory_open;
}

InventoryInputGate inventory_input_gate(bool inventory_open) noexcept {
    const bool forward = !inventory_open;
    return {forward, forward, forward, forward, forward};
}

BuildDifference compare_player_builds(const combat::PlayerCombatBuild& current,
    const combat::PlayerCombatBuild& candidate) noexcept {
    BuildDifference result{};
    result.max_health = static_cast<std::int64_t>(candidate.values.max_health)
        - current.values.max_health;
    result.max_barrier = static_cast<std::int64_t>(candidate.values.max_barrier)
        - current.values.max_barrier;
    result.melee_damage = candidate.values.melee_damage
        - current.values.melee_damage;
    result.move_speed = candidate.values.movement_speed
        - current.values.movement_speed;
    result.attack_speed = candidate.values.attack_speed
        - current.values.attack_speed;
    result.armor = candidate.values.armor - current.values.armor;
    result.evasion = candidate.values.evasion - current.values.evasion;
    for (std::size_t index = 0U; index < result.damage_reduction.size(); ++index) {
        result.damage_reduction[index] =
            static_cast<std::int64_t>(candidate.values.damage_reduction[index])
            - current.values.damage_reduction[index];
        result.damage_reduction_cap_bonus[index] = static_cast<std::int64_t>(
            candidate.values.damage_reduction_cap_bonus[index])
            - current.values.damage_reduction_cap_bonus[index];
    }
    result.weapon_physical = candidate.weapon_physical - current.weapon_physical;
    result.local_attack_speed_bp = static_cast<std::int64_t>(
        candidate.local_attack_speed_bp) - current.local_attack_speed_bp;
    return result;
}

}  // namespace arpg::platform
