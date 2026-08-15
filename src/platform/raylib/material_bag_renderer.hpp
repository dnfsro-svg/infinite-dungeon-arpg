#pragma once

#include "inventory_view_math.hpp"
#include "material_loot_view.hpp"
#include "material_pack.hpp"
#include "items/item_crafting.hpp"
#include "items/material_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::platform {

enum class SkillStoneVisualKind : std::uint8_t {
    active,
    support,
};

enum class MaterialBagSlotVisualState : std::uint8_t {
    disabled,
    idle,
    selected,
};

[[nodiscard]] MaterialSpriteId material_bag_sprite(
    items::MaterialId id) noexcept;

[[nodiscard]] const char* material_bag_display_name(
    items::MaterialId id) noexcept;

[[nodiscard]] Rectangle material_bag_text_backing(
    Rectangle slot, float scale) noexcept;

[[nodiscard]] MaterialBagSlotVisualState material_bag_slot_visual_state(
    std::uint64_t quantity, bool selected) noexcept;

[[nodiscard]] MaterialSpriteId skill_stone_sprite(
    SkillStoneVisualKind kind) noexcept;

struct MaterialBagLayout final {
    Rectangle panel{};
    std::array<Rectangle, items::kMaterialCount> slots{};

    [[nodiscard]] bool contains_all_slots() const noexcept;
};

struct ReinforcementConfirmationLayout final {
    Rectangle panel{};
    Rectangle confirm{};
    Rectangle cancel{};
};

[[nodiscard]] MaterialBagLayout material_bag_layout(
    Rectangle content, float scale) noexcept;
[[nodiscard]] ReinforcementConfirmationLayout
reinforcement_confirmation_layout(int width, int height) noexcept;

class MaterialBagRenderer final {
public:
    [[nodiscard]] bool process_click(Vector2,
        const items::ItemOwnershipState&,
        const MaterialBagLayout&) noexcept;
    [[nodiscard]] bool select_slot(std::size_t,
        const items::ItemOwnershipState&) noexcept;
    [[nodiscard]] bool clear_selection() noexcept;
    [[nodiscard]] bool begin_reinforcement_confirmation(
        std::uint64_t item_id, std::uint32_t current) noexcept;
    [[nodiscard]] bool begin_recipe_confirmation() noexcept;
    [[nodiscard]] std::optional<std::uint64_t>
    resolve_reinforcement_confirmation(bool confirmed) noexcept;
    [[nodiscard]] bool resolve_recipe_confirmation(bool confirmed) noexcept;
    [[nodiscard]] std::optional<std::uint64_t>
    reinforcement_confirmation_item() const noexcept;
    [[nodiscard]] InventoryConfirmationKind confirmation_kind() const noexcept;
    void dismiss_confirmation() noexcept;
    void sync_selection(const items::ItemOwnershipState&) noexcept;
    [[nodiscard]] bool cycle_directed_category(
        Vector2, const MaterialBagLayout&) noexcept;
    [[nodiscard]] std::optional<items::MaterialId> selected_material() const noexcept;
    [[nodiscard]] items::DirectedCategory directed_category() const noexcept;
    void draw(const items::ItemOwnershipState&, const MaterialPack&,
        Font font, bool font_ready, const MaterialBagLayout&,
        float scale) const noexcept;
    void draw_confirmation(const MaterialPack&,
        Font font, bool font_ready, int width, int height) const noexcept;

private:
    std::optional<items::MaterialId> selected_{};
    std::uint64_t reinforcement_confirmation_item_{};
    items::DirectedCategory directed_category_{items::DirectedCategory::damage};
    InventoryConfirmationKind confirmation_kind_{
        InventoryConfirmationKind::none};
};

}  // namespace arpg::platform
