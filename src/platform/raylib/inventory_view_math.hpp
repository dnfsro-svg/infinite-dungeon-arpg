#pragma once

#include "combat/combat_types.hpp"
#include "items/item_types.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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
    std::int64_t move_speed{};
    std::int64_t attack_speed{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::array<std::int64_t, 4> damage_reduction{};
    std::array<std::int64_t, 4> damage_reduction_cap_bonus{};
    std::int64_t weapon_physical{};
    std::int64_t local_attack_speed_bp{};
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

}  // namespace arpg::platform
