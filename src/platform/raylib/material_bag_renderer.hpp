#pragma once

#include "inventory_view_math.hpp"
#include "material_loot_view.hpp"
#include "material_pack.hpp"
#include "items/item_crafting.hpp"
#include "items/material_catalog.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace arpg::platform {

enum class SkillStoneVisualKind : std::uint8_t {
    active,
    support,
};

[[nodiscard]] MaterialSpriteId material_bag_sprite(
    items::MaterialId id) noexcept;

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

[[nodiscard]] MaterialBagLayout material_bag_layout(int width, int height) noexcept;
[[nodiscard]] Rectangle material_bag_detail_bounds(int width, int height) noexcept;
[[nodiscard]] ReinforcementConfirmationLayout
reinforcement_confirmation_layout(int width, int height) noexcept;

class MaterialBagRenderer final {
public:
    [[nodiscard]] bool process_click(Vector2,
        const items::ItemOwnershipState&, int width, int height) noexcept;
    [[nodiscard]] bool select_slot(std::size_t,
        const items::ItemOwnershipState&) noexcept;
    [[nodiscard]] bool clear_selection() noexcept;
    [[nodiscard]] bool begin_reinforcement_confirmation(
        std::uint64_t item_id, std::uint32_t current) noexcept;
    [[nodiscard]] std::optional<std::uint64_t>
    resolve_reinforcement_confirmation(bool confirmed) noexcept;
    [[nodiscard]] std::optional<std::uint64_t>
    reinforcement_confirmation_item() const noexcept;
    void sync_selection(const items::ItemOwnershipState&) noexcept;
    [[nodiscard]] bool cycle_directed_category(
        Vector2, int width, int height) noexcept;
    [[nodiscard]] std::optional<items::MaterialId> selected_material() const noexcept;
    [[nodiscard]] items::DirectedCategory directed_category() const noexcept;
    void draw(const items::ItemOwnershipState&, const MaterialPack&,
        int width, int height) const noexcept;
    void draw_reinforcement_confirmation(const MaterialPack&,
        int width, int height) const noexcept;

private:
    std::optional<items::MaterialId> selected_{};
    std::optional<std::uint64_t> reinforcement_confirmation_item_{};
    items::DirectedCategory directed_category_{items::DirectedCategory::damage};
};

}  // namespace arpg::platform
