#include "material_bag_renderer.hpp"

#include "material_loot_view.hpp"

#include <raylib.h>

#include <algorithm>

namespace arpg::platform {
namespace {

constexpr float kInset = 10.0F;
constexpr float kGap = 4.0F;
constexpr std::size_t kColumns = 2U;

bool contains(Rectangle rectangle, Vector2 point) noexcept {
    return point.x >= rectangle.x && point.x <= rectangle.x + rectangle.width
        && point.y >= rectangle.y && point.y <= rectangle.y + rectangle.height;
}

bool inside(Rectangle child, Rectangle parent) noexcept {
    return child.width > 0.0F && child.height > 0.0F
        && child.x >= parent.x && child.y >= parent.y
        && child.x + child.width <= parent.x + parent.width
        && child.y + child.height <= parent.y + parent.height;
}

}  // namespace

bool MaterialBagLayout::contains_all_slots() const noexcept {
    if (panel.width <= 0.0F || panel.height <= 0.0F) return false;
    for (const Rectangle slot : slots) {
        if (!inside(slot, panel)) return false;
    }
    return true;
}

MaterialBagLayout material_bag_layout(int width, int height) noexcept {
    MaterialBagLayout result{};
    const InventoryLayout inventory = inventory_layout(width, height);
    constexpr float kPanelHeight = 224.0F;
    result.panel = {inventory.detail.x, inventory.detail.y + inventory.detail.height
            - (std::min)(inventory.detail.height, kPanelHeight),
        inventory.detail.width, (std::min)(inventory.detail.height, kPanelHeight)};
    if (result.panel.width <= kInset * 2.0F || result.panel.height <= 42.0F) {
        return result;
    }
    const std::size_t rows = (items::kMaterialCount + kColumns - 1U) / kColumns;
    const float cell_width = (result.panel.width - kInset * 2.0F - kGap)
        / static_cast<float>(kColumns);
    const float cell_height = (result.panel.height - 42.0F - kInset - kGap
        * static_cast<float>(rows - 1U)) / static_cast<float>(rows);
    if (cell_width <= 0.0F || cell_height <= 0.0F) return result;
    for (std::size_t index = 0U; index < result.slots.size(); ++index) {
        const std::size_t row = index / kColumns;
        const std::size_t column = index % kColumns;
        result.slots[index] = {result.panel.x + kInset
                + static_cast<float>(column) * (cell_width + kGap),
            result.panel.y + 33.0F + static_cast<float>(row) * (cell_height + kGap),
            cell_width, cell_height};
    }
    return result;
}

Rectangle material_bag_detail_bounds(int width, int height) noexcept {
    const InventoryLayout inventory = inventory_layout(width, height);
    const MaterialBagLayout bag = material_bag_layout(width, height);
    constexpr float kDetailTopInset = 36.0F;
    constexpr float kDetailBagGap = 4.0F;
    const float top = inventory.detail.y + kDetailTopInset;
    const float bottom = (std::max)(top, bag.panel.y - kDetailBagGap);
    return {inventory.detail.x, top, inventory.detail.width, bottom - top};
}

bool MaterialBagRenderer::select_slot(std::size_t slot,
    const items::ItemOwnershipState& state) noexcept {
    if (slot >= state.materials.size() || state.materials[slot] == 0U) return false;
    selected_ = static_cast<items::MaterialId>(slot);
    return true;
}

bool MaterialBagRenderer::process_click(Vector2 point,
    const items::ItemOwnershipState& state, int width, int height) noexcept {
    const MaterialBagLayout layout = material_bag_layout(width, height);
    for (std::size_t index = 0U; index < layout.slots.size(); ++index) {
        if (contains(layout.slots[index], point)) return select_slot(index, state);
    }
    return false;
}

std::optional<items::MaterialId> MaterialBagRenderer::selected_material() const noexcept {
    return selected_;
}

void MaterialBagRenderer::draw(const items::ItemOwnershipState& state,
    int width, int height) const noexcept {
    const MaterialBagLayout layout = material_bag_layout(width, height);
    if (!layout.contains_all_slots()) return;
    DrawRectangleRounded(layout.panel, 0.025F, 5, Color{8, 12, 20, 247});
    DrawRectangleRoundedLinesEx(layout.panel, 0.025F, 5, 1.0F,
        Color{72, 91, 120, 255});
    DrawText("MATERIAL BAG", static_cast<int>(layout.panel.x + kInset),
        static_cast<int>(layout.panel.y + 9.0F), 16, Color{131, 211, 255, 255});
    for (std::size_t index = 0U; index < layout.slots.size(); ++index) {
        const items::MaterialId id = static_cast<items::MaterialId>(index);
        const bool selected = selected_.has_value() && *selected_ == id;
        const Color color = [&] {
            const Rgba8 rgba = material_color(id);
            return Color{rgba.r, rgba.g, rgba.b, rgba.a};
        }();
        const Rectangle slot = layout.slots[index];
        DrawRectangleRounded(slot, 0.08F, 4,
            selected ? Color{34, 68, 88, 255} : Color{21, 28, 39, 255});
        DrawRectangleRoundedLinesEx(slot, 0.08F, 4,
            selected ? 2.0F : 1.0F, color);
        const items::MaterialDefinition* const definition = items::material_definition(id);
        DrawText(definition == nullptr ? "Invalid" : definition->name.data(),
            static_cast<int>(slot.x + 5.0F), static_cast<int>(slot.y + 3.0F), 10, color);
        DrawText(TextFormat("x%llu", static_cast<unsigned long long>(state.materials[index])),
            static_cast<int>(slot.x + 5.0F), static_cast<int>(slot.y + slot.height - 13.0F),
            11, RAYWHITE);
    }
}

}  // namespace arpg::platform
