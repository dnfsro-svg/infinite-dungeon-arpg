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

bool same_filter(InventoryFilter left, InventoryFilter right) noexcept {
    return left.slot == right.slot && left.rarity == right.rarity;
}

bool same_recipe(const RecipeSelection& left,
    const RecipeSelection& right) noexcept {
    return left.count == right.count && left.ids == right.ids;
}

bool matches_inventory_filter(const items::ItemInstance& item,
    bool is_equipped, InventoryFilter filter) noexcept {
    if (is_equipped) return false;
    const items::BaseDefinition* const base =
        items::base_definition(item.base_id);
    if (base == nullptr) return false;
    return (!filter.slot.has_value() || base->slot == *filter.slot)
        && (!filter.rarity.has_value() || item.rarity == *filter.rarity);
}

const char* stat_name(modifiers::StatId stat) noexcept {
    switch (stat) {
    case modifiers::StatId::impulse_scale: return "Knockback";
    case modifiers::StatId::shield: return "Shield";
    case modifiers::StatId::melee_damage: return "Melee damage";
    case modifiers::StatId::physical_flat_damage: return "Physical damage";
    case modifiers::StatId::fire_flat_damage: return "Fire damage";
    case modifiers::StatId::water_flat_damage: return "Water damage";
    case modifiers::StatId::lightning_flat_damage: return "Lightning damage";
    case modifiers::StatId::chaos_flat_damage: return "Chaos damage";
    case modifiers::StatId::fire_damage: return "Fire damage";
    case modifiers::StatId::water_damage: return "Water damage";
    case modifiers::StatId::lightning_damage: return "Lightning damage";
    case modifiers::StatId::chaos_damage: return "Chaos damage";
    case modifiers::StatId::fire_damage_reduction: return "Fire DR";
    case modifiers::StatId::water_damage_reduction: return "Water DR";
    case modifiers::StatId::lightning_damage_reduction: return "Lightning DR";
    case modifiers::StatId::chaos_damage_reduction: return "Chaos DR";
    case modifiers::StatId::fire_damage_reduction_cap: return "Fire DR cap";
    case modifiers::StatId::water_damage_reduction_cap: return "Water DR cap";
    case modifiers::StatId::lightning_damage_reduction_cap: return "Lightning DR cap";
    case modifiers::StatId::chaos_damage_reduction_cap: return "Chaos DR cap";
    case modifiers::StatId::armor: return "Armor";
    case modifiers::StatId::evasion: return "Evasion";
    case modifiers::StatId::max_health: return "Maximum health";
    case modifiers::StatId::max_health_more: return "Maximum health";
    case modifiers::StatId::max_barrier: return "Maximum barrier";
    case modifiers::StatId::damage_taken: return "Damage taken";
    case modifiers::StatId::move_speed: return "Movement speed";
    case modifiers::StatId::attack_speed: return "Attack speed";
    case modifiers::StatId::jump_speed: return "Jump speed";
    case modifiers::StatId::air_control: return "Air control";
    case modifiers::StatId::count: break;
    }
    return "Attribute";
}

bool percent_stat(modifiers::StatId stat) noexcept {
    return stat == modifiers::StatId::fire_damage_reduction
        || stat == modifiers::StatId::water_damage_reduction
        || stat == modifiers::StatId::lightning_damage_reduction
        || stat == modifiers::StatId::chaos_damage_reduction
        || stat == modifiers::StatId::fire_damage_reduction_cap
        || stat == modifiers::StatId::water_damage_reduction_cap
        || stat == modifiers::StatId::lightning_damage_reduction_cap
        || stat == modifiers::StatId::chaos_damage_reduction_cap;
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
        if (matches_inventory_filter(item,
                equipped(state.equipment, item.id), filter)) {
            result.push_back(index);
        }
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
    result.impulse_scale = candidate.values.impulse_scale
        - current.values.impulse_scale;
    result.move_speed = candidate.values.movement_speed
        - current.values.movement_speed;
    result.attack_speed = candidate.values.attack_speed
        - current.values.attack_speed;
    result.armor = candidate.values.armor - current.values.armor;
    result.evasion = candidate.values.evasion - current.values.evasion;
    for (std::size_t index = 0U; index < result.flat_damage.size(); ++index) {
        result.flat_damage[index] = candidate.values.flat_damage[index]
            - current.values.flat_damage[index];
        result.damage_increased[index] =
            static_cast<std::int64_t>(candidate.values.damage_increased[index])
            - current.values.damage_increased[index];
    }
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
    result.armor_reduction_bp = modifiers::rating_to_basis_points(
        candidate.values.armor) - modifiers::rating_to_basis_points(
            current.values.armor);
    result.evasion_rate_bp = modifiers::rating_to_basis_points(
        candidate.values.evasion) - modifiers::rating_to_basis_points(
            current.values.evasion);
    return result;
}

void refresh_inventory_view_cache(InventoryViewCache& cache,
    const items::ItemOwnershipState& state, std::uint64_t generation,
    InventoryFilter filter, std::uint64_t selected_item_id,
    RecipeSelection recipe) {
    if (cache.generation == generation
        && cache.equipment.equipped_ids == state.equipment.equipped_ids
        && same_filter(cache.filter, filter)
        && cache.selected_item_id == selected_item_id
        && same_recipe(cache.requested_recipe, recipe)) return;
    cache.generation = generation;
    cache.equipment = state.equipment;
    cache.filter = filter;
    cache.selected_item_id = selected_item_id;
    cache.requested_recipe = recipe;
    cache.resolved_recipe = {};
    cache.selected_index = InventoryViewCache::kInvalidIndex;
    cache.recipe_indices.fill(InventoryViewCache::kInvalidIndex);
    cache.recipe_ready = false;
    cache.filtered_indices.clear();
    cache.filtered_indices.reserve(state.items.size());
    ++cache.refresh_count;
    for (std::size_t index = 0U; index < state.items.size(); ++index) {
        ++cache.item_inspection_count;
        const items::ItemInstance& item = state.items[index];
        const bool is_equipped = equipped(state.equipment, item.id);
        if (item.id == selected_item_id) cache.selected_index = index;
        for (std::size_t selected = 0U; selected < recipe.count; ++selected) {
            if (!is_equipped && item.id == recipe.ids[selected]) {
                cache.recipe_indices[selected] = index;
            }
        }
        if (matches_inventory_filter(item, is_equipped, filter)) {
            cache.filtered_indices.push_back(index);
        }
    }
    for (std::size_t selected = 0U; selected < recipe.count; ++selected) {
        if (cache.recipe_indices[selected] == InventoryViewCache::kInvalidIndex) {
            continue;
        }
        cache.resolved_recipe.ids[cache.resolved_recipe.count++] =
            recipe.ids[selected];
    }
    if (cache.resolved_recipe.count != 3U) return;
    if (cache.resolved_recipe.ids[0] == cache.resolved_recipe.ids[1]
        || cache.resolved_recipe.ids[0] == cache.resolved_recipe.ids[2]
        || cache.resolved_recipe.ids[1] == cache.resolved_recipe.ids[2]) return;
    const items::ItemInstance& first =
        state.items[cache.recipe_indices[0]];
    if (items::base_definition(first.base_id) == nullptr) return;
    for (std::size_t selected = 1U; selected < 3U; ++selected) {
        const items::ItemInstance& item =
            state.items[cache.recipe_indices[selected]];
        if (items::base_definition(item.base_id) == nullptr
            || item.base_id != first.base_id
            || item.rarity != first.rarity) return;
    }
    cache.recipe_ready = true;
}

const items::ItemInstance* cached_selected_item(
    const InventoryViewCache& cache,
    const items::ItemOwnershipState& state) noexcept {
    if (cache.selected_index >= state.items.size()) return nullptr;
    const items::ItemInstance& item = state.items[cache.selected_index];
    return item.id == cache.selected_item_id ? &item : nullptr;
}

bool cached_recipe_ready(const InventoryViewCache& cache) noexcept {
    return cache.recipe_ready;
}

ItemAttributeLabel item_attribute_label(items::ItemEffectKind effect,
    modifiers::StatId stat, modifiers::ModifierOperation operation,
    items::ItemSlot slot, std::uint8_t variant) noexcept {
    const bool local = effect == items::ItemEffectKind::local_weapon_physical_flat
        || effect == items::ItemEffectKind::local_weapon_physical_increased
        || (effect == items::ItemEffectKind::slot_dependent_attack_speed
            && slot == items::ItemSlot::weapon);
    const char* name = stat_name(stat);
    if (effect == items::ItemEffectKind::local_weapon_physical_flat
        || effect == items::ItemEffectKind::local_weapon_physical_increased) {
        name = "Weapon physical";
    } else if (effect == items::ItemEffectKind::slot_dependent_attack_speed) {
        name = "Attack speed";
    } else if (effect == items::ItemEffectKind::all_element_damage_reduction) {
        name = "All element DR";
    } else if (effect == items::ItemEffectKind::tri_element_damage_reduction) {
        name = "Fire/Water/Lightning DR";
    } else if (effect
            == items::ItemEffectKind::variant_element_damage_reduction_cap) {
        switch (variant) {
        case 0U: name = "Fire DR cap"; break;
        case 1U: name = "Water DR cap"; break;
        case 2U: name = "Lightning DR cap"; break;
        case 3U: name = "Chaos DR cap"; break;
        default: name = "Element DR cap"; break;
        }
    }
    const bool semantic_increased = effect
        == items::ItemEffectKind::local_weapon_physical_increased;
    const bool semantic_percent = semantic_increased
        || effect == items::ItemEffectKind::slot_dependent_attack_speed;
    const bool percent = semantic_percent
        || operation != modifiers::ModifierOperation::flat
        || percent_stat(stat)
        || effect == items::ItemEffectKind::all_element_damage_reduction
        || effect == items::ItemEffectKind::tri_element_damage_reduction
        || effect
            == items::ItemEffectKind::variant_element_damage_reduction_cap;
    const char* qualifier = "";
    if (semantic_increased
        || operation == modifiers::ModifierOperation::increased) {
        qualifier = " inc";
    } else if (operation == modifiers::ModifierOperation::more) {
        qualifier = " more";
    } else if (operation == modifiers::ModifierOperation::conversion) {
        qualifier = " converted";
    }
    return {name, local ? "LOCAL" : "GLOBAL",
        percent ? "%" : "", qualifier};
}

double item_attribute_display_value(std::int32_t raw_value,
    ItemAttributeLabel label) noexcept {
    return label.suffix != nullptr && label.suffix[0] == '%'
        ? static_cast<double>(raw_value) / 100.0
        : static_cast<double>(raw_value);
}

Rectangle detail_line_rectangle(InventoryLayout layout,
    std::size_t line) noexcept {
    constexpr float kInsetX = 12.0F;
    constexpr float kDetailTop = 40.0F;
    constexpr float kLineHeight = 12.0F;
    return {layout.detail.x + kInsetX,
        layout.detail.y + kDetailTop + static_cast<float>(line) * kLineHeight,
        std::max(0.0F, layout.detail.width - kInsetX * 2.0F), 11.0F};
}

bool detail_content_fits(InventoryLayout layout,
    std::size_t line_count) noexcept {
    if (line_count == 0U) return true;
    const Rectangle last = detail_line_rectangle(layout, line_count - 1U);
    return last.x >= layout.detail.x && last.y >= layout.detail.y
        && last.x + last.width <= layout.detail.x + layout.detail.width
        && last.y + last.height <= layout.detail.y + layout.detail.height;
}

std::size_t detail_line_character_capacity(InventoryLayout layout) noexcept {
    const Rectangle line = detail_line_rectangle(layout, 0U);
    constexpr float kConservativeCharacterWidth = 5.2F;
    return static_cast<std::size_t>(std::floor(
        line.width / kConservativeCharacterWidth));
}

bool detail_text_fits(InventoryLayout layout,
    std::string_view text) noexcept {
    return text.size() <= detail_line_character_capacity(layout);
}

}  // namespace arpg::platform
