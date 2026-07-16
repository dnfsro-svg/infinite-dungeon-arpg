#pragma once

#include "combat/combat_types.hpp"
#include "items/item_catalog.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace arpg::platform {

struct InventoryLayout final {
    Rectangle equipment{};
    Rectangle grid{};
    Rectangle detail{};
};

struct VisibleGridRange final {
    std::size_t first{};
    std::size_t count{};
};

struct InventoryFilter final {
    std::optional<items::ItemSlot> slot{};
    std::optional<items::ItemRarity> rarity{};
};

enum class InventoryClickKind : std::uint8_t {
    single,
    double_click,
};

struct InventoryClickTracker final {
    std::uint64_t last_item_id{};
    double last_click_seconds{-1.0};
};

struct RecipeSelection final {
    std::array<std::uint64_t, 3> ids{};
    std::size_t count{};
};

struct InventoryInputGate final {
    bool forward_actions{};
    bool forward_movement{};
    bool forward_descent{};
    bool forward_room_reset{};
    bool forward_passive_toggle{};
};

struct BuildDifference final {
    std::int64_t max_health{};
    std::int64_t max_barrier{};
    std::int64_t melee_damage{};
    std::int64_t impulse_scale{};
    std::int64_t move_speed{};
    std::int64_t attack_speed{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::array<std::int64_t, 5> flat_damage{};
    std::array<std::int64_t, 5> damage_increased{};
    std::array<std::int64_t, 4> damage_reduction{};
    std::array<std::int64_t, 4> damage_reduction_cap_bonus{};
    std::int64_t weapon_physical{};
    std::int64_t local_attack_speed_bp{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
};

struct InventoryViewCache final {
    static constexpr std::size_t kInvalidIndex = ~std::size_t{0U};

    std::uint64_t generation{~std::uint64_t{0U}};
    items::EquipmentState equipment{};
    InventoryFilter filter{};
    std::uint64_t selected_item_id{};
    RecipeSelection requested_recipe{};
    RecipeSelection resolved_recipe{};
    std::vector<std::size_t> filtered_indices{};
    std::size_t selected_index{kInvalidIndex};
    std::array<std::size_t, 3> recipe_indices{{
        kInvalidIndex, kInvalidIndex, kInvalidIndex}};
    bool recipe_ready{};
    std::size_t refresh_count{};
    std::size_t item_inspection_count{};
};

struct ItemAttributeLabel final {
    const char* name{};
    const char* scope{};
    const char* suffix{};
    const char* qualifier{};
};

[[nodiscard]] InventoryLayout inventory_layout(int width, int height) noexcept;
[[nodiscard]] VisibleGridRange visible_grid_range(
    std::size_t filtered_count, int columns, float scroll_rows,
    float viewport_height, float cell_height) noexcept;
[[nodiscard]] float clamp_inventory_scroll_rows(
    std::size_t filtered_count, int columns, float scroll_rows,
    float viewport_height, float cell_height) noexcept;
[[nodiscard]] std::vector<std::size_t> filtered_inventory_indices(
    const items::ItemOwnershipState& state, InventoryFilter filter);
[[nodiscard]] InventoryClickKind register_inventory_click(
    InventoryClickTracker& tracker, std::uint64_t item_id,
    double now_seconds) noexcept;
[[nodiscard]] bool toggle_recipe_selection(
    RecipeSelection& selection, std::uint64_t item_id) noexcept;
[[nodiscard]] Rectangle equipment_slot_rectangle(
    InventoryLayout layout, items::ItemSlot slot) noexcept;
[[nodiscard]] std::optional<items::ItemSlot> hit_test_equipped_slot(
    Vector2 point, InventoryLayout layout,
    const items::EquipmentState& equipment) noexcept;
[[nodiscard]] bool inventory_can_open(
    bool runtime_running, bool passive_overlay_open) noexcept;
[[nodiscard]] bool passive_overlay_can_toggle(bool inventory_open) noexcept;
[[nodiscard]] InventoryInputGate inventory_input_gate(
    bool inventory_open) noexcept;
[[nodiscard]] BuildDifference compare_player_builds(
    const combat::PlayerCombatBuild& current,
    const combat::PlayerCombatBuild& candidate) noexcept;
void refresh_inventory_view_cache(InventoryViewCache& cache,
    const items::ItemOwnershipState& state, std::uint64_t generation,
    InventoryFilter filter, std::uint64_t selected_item_id,
    RecipeSelection recipe);
[[nodiscard]] const items::ItemInstance* cached_selected_item(
    const InventoryViewCache& cache,
    const items::ItemOwnershipState& state) noexcept;
[[nodiscard]] bool cached_recipe_ready(
    const InventoryViewCache& cache) noexcept;
[[nodiscard]] ItemAttributeLabel item_attribute_label(
    items::ItemEffectKind effect, modifiers::StatId stat,
    modifiers::ModifierOperation operation, items::ItemSlot slot,
    std::uint8_t variant) noexcept;
[[nodiscard]] double item_attribute_display_value(
    std::int32_t raw_value, ItemAttributeLabel label) noexcept;
[[nodiscard]] Rectangle detail_line_rectangle(
    InventoryLayout layout, std::size_t line) noexcept;
[[nodiscard]] bool detail_content_fits(
    InventoryLayout layout, std::size_t line_count) noexcept;
[[nodiscard]] std::size_t detail_line_character_capacity(
    InventoryLayout layout) noexcept;
[[nodiscard]] bool detail_text_fits(InventoryLayout layout,
    std::string_view text) noexcept;

}  // namespace arpg::platform
