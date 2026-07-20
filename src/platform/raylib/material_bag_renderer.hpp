#pragma once

#include "inventory_view_math.hpp"
#include "items/material_catalog.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace arpg::platform {

struct MaterialBagLayout final {
    Rectangle panel{};
    std::array<Rectangle, items::kMaterialCount> slots{};

    [[nodiscard]] bool contains_all_slots() const noexcept;
};

[[nodiscard]] MaterialBagLayout material_bag_layout(int width, int height) noexcept;
[[nodiscard]] Rectangle material_bag_detail_bounds(int width, int height) noexcept;

class MaterialBagRenderer final {
public:
    [[nodiscard]] bool process_click(Vector2,
        const items::ItemOwnershipState&, int width, int height) noexcept;
    [[nodiscard]] bool select_slot(std::size_t,
        const items::ItemOwnershipState&) noexcept;
    [[nodiscard]] std::optional<items::MaterialId> selected_material() const noexcept;
    void draw(const items::ItemOwnershipState&, int width, int height) const noexcept;

private:
    std::optional<items::MaterialId> selected_{};
};

}  // namespace arpg::platform
