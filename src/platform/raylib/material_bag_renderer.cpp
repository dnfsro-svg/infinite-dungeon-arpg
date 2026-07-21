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

ReinforcementConfirmationLayout reinforcement_confirmation_layout(
    int width, int height) noexcept {
    const float panel_width = (std::min)(360.0F,
        (std::max)(180.0F, static_cast<float>(width) - 32.0F));
    const float panel_height = 124.0F;
    const Rectangle panel{(static_cast<float>(width) - panel_width) * 0.5F,
        (static_cast<float>(height) - panel_height) * 0.5F,
        panel_width, panel_height};
    constexpr float kModalInset = 14.0F;
    constexpr float kModalGap = 8.0F;
    const float button_width = (panel.width - kModalInset * 2.0F
        - kModalGap) * 0.5F;
    return {panel,
        {panel.x + kModalInset, panel.y + panel.height - 38.0F,
            button_width, 24.0F},
        {panel.x + kModalInset + button_width + kModalGap,
            panel.y + panel.height - 38.0F, button_width, 24.0F}};
}

bool MaterialBagRenderer::select_slot(std::size_t slot,
    const items::ItemOwnershipState& state) noexcept {
    if (slot >= state.materials.size() || state.materials[slot] == 0U) return false;
    selected_ = static_cast<items::MaterialId>(slot);
    return true;
}

bool MaterialBagRenderer::clear_selection() noexcept {
    if (!selected_.has_value()) return false;
    selected_.reset();
    return true;
}

bool MaterialBagRenderer::begin_reinforcement_confirmation(
    std::uint64_t item_id, std::uint32_t current) noexcept {
    if (item_id == 0U || current < 12U
            || reinforcement_confirmation_item_.has_value()) {
        return false;
    }
    reinforcement_confirmation_item_ = item_id;
    return true;
}

std::optional<std::uint64_t>
MaterialBagRenderer::resolve_reinforcement_confirmation(
    bool confirmed) noexcept {
    const auto item = reinforcement_confirmation_item_;
    reinforcement_confirmation_item_.reset();
    return confirmed ? item : std::nullopt;
}

std::optional<std::uint64_t>
MaterialBagRenderer::reinforcement_confirmation_item() const noexcept {
    return reinforcement_confirmation_item_;
}

void MaterialBagRenderer::sync_selection(
    const items::ItemOwnershipState& state) noexcept {
    if (!selected_.has_value()
            || state.materials[items::material_index(*selected_)] != 0U) {
        return;
    }
    selected_.reset();
}

bool MaterialBagRenderer::cycle_directed_category(
    Vector2 point, int width, int height) noexcept {
    if (selected_ != items::MaterialId::directed) return false;
    const MaterialBagLayout layout = material_bag_layout(width, height);
    const std::size_t index = items::material_index(items::MaterialId::directed);
    if (index >= layout.slots.size() || !contains(layout.slots[index], point)) {
        return false;
    }
    const auto next = static_cast<std::uint8_t>(directed_category_) + 1U;
    directed_category_ = next < static_cast<std::uint8_t>(items::DirectedCategory::count)
        ? static_cast<items::DirectedCategory>(next)
        : items::DirectedCategory::damage;
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

items::DirectedCategory MaterialBagRenderer::directed_category() const noexcept {
    return directed_category_;
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
        if (selected && id == items::MaterialId::directed) {
            const char* category = "Damage";
            switch (directed_category_) {
            case items::DirectedCategory::damage: break;
            case items::DirectedCategory::defense: category = "Defense"; break;
            case items::DirectedCategory::speed: category = "Speed"; break;
            case items::DirectedCategory::element: category = "Element"; break;
            case items::DirectedCategory::count: break;
            }
            DrawText(TextFormat("%s (R-click: cycle)", category),
                static_cast<int>(slot.x + 5.0F), static_cast<int>(slot.y + 16.0F),
                9, Color{255, 237, 154, 255});
        }
    }
}

void MaterialBagRenderer::draw_reinforcement_confirmation(
    int width, int height) const noexcept {
    if (!reinforcement_confirmation_item_.has_value()) return;
    const ReinforcementConfirmationLayout layout =
        reinforcement_confirmation_layout(width, height);
    DrawRectangleRounded(layout.panel, 0.04F, 5, Color{29, 15, 18, 252});
    DrawRectangleRoundedLinesEx(layout.panel, 0.04F, 5, 2.0F,
        Color{255, 113, 96, 255});
    DrawText("DANGER: FAILURE DESTROYS EQUIPMENT",
        static_cast<int>(layout.panel.x + 14.0F),
        static_cast<int>(layout.panel.y + 16.0F), 14,
        Color{255, 180, 160, 255});
    DrawText("Use one Reinforcement Stone?",
        static_cast<int>(layout.panel.x + 14.0F),
        static_cast<int>(layout.panel.y + 42.0F), 13, RAYWHITE);
    DrawRectangleRounded(layout.confirm, 0.10F, 4, Color{113, 39, 39, 255});
    DrawRectangleRounded(layout.cancel, 0.10F, 4, Color{44, 58, 74, 255});
    DrawText("CONFIRM", static_cast<int>(layout.confirm.x + 8.0F),
        static_cast<int>(layout.confirm.y + 5.0F), 13, RAYWHITE);
    DrawText("CANCEL", static_cast<int>(layout.cancel.x + 10.0F),
        static_cast<int>(layout.cancel.y + 5.0F), 13, RAYWHITE);
}

}  // namespace arpg::platform
