#include "material_bag_renderer.hpp"

#include "material_loot_view.hpp"
#include "ui_material.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_bounds_audit.hpp"
#include "ui_text_catalog.hpp"
#include "ui_text_layout.hpp"
#include "ui_text_renderer.hpp"
#include "ui_typography.hpp"
#include "ui_utf8.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr float kInset = 10.0F;
constexpr float kGap = 4.0F;
constexpr std::size_t kColumns = 2U;
constexpr Color kMaterialTextBacking{5, 9, 16, 255};
constexpr Color kDangerBacking{29, 15, 18, 255};
constexpr Color kConfirmBacking{74, 24, 27, 255};
constexpr Color kCancelBacking{30, 42, 58, 255};

void draw_opaque_material_text_backing(Rectangle bounds) noexcept {
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) return;
    DrawRectangleRounded(bounds, 0.08F, 4, kMaterialTextBacking);
}

UiTextExtent measure_material_text(const char* text, float font_size,
    float spacing, void* context) noexcept {
    if (context == nullptr) return {};
    const Font font = *static_cast<const Font*>(context);
    if (!IsFontValid(font)) return {};
    const Vector2 extent = MeasureTextEx(font, text, font_size, spacing);
    return {extent.x, extent.y};
}

void draw_material_text(Font font, bool ready, const char* text,
    Rectangle bounds, float size, float minimum_size, Color color,
    Color opaque_backing) noexcept {
    if (!ready || !IsFontValid(font)) return;
    if (text == nullptr || text[0] == '\0'
            || bounds.width <= 0.0F || bounds.height <= 0.0F
            || opaque_backing.a != 255U) {
        return;
    }
    const float viewport_scale = ui_viewport_scale(
        GetScreenWidth(), GetScreenHeight());
    const float scaled_size = size * viewport_scale;
    const float scaled_minimum = minimum_size * viewport_scale;
    char fitted[128]{};
    const UiCenteredTextLayout layout = fit_centered_ui_text(
        fitted, sizeof(fitted), text, bounds, scaled_size, scaled_minimum,
        0.5F * viewport_scale, &measure_material_text, &font);
    if (!layout.drawable) return;
    color.a = 255U;
    if (ui_luma_contrast_ratio(color, opaque_backing) < 4.5F) {
        color = ui_text_contrast_style().primary;
        if (ui_luma_contrast_ratio(color, opaque_backing) < 4.5F) return;
    }
    record_ui_text_bounds(UiTextAuditPage::inventory,
        UiTextAuditRole::inventory_material_entry, font, layout.text,
        layout.position, layout.font_size, layout.spacing, bounds,
        scaled_minimum);
    draw_crisp_ui_text(font, layout.text, layout.position,
        layout.font_size, layout.spacing, color);
}

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

MaterialSpriteId skill_stone_sprite(SkillStoneVisualKind kind) noexcept {
    switch (kind) {
    case SkillStoneVisualKind::active:
        return MaterialSpriteId::skill_stone_active;
    case SkillStoneVisualKind::support:
        return MaterialSpriteId::skill_stone_support;
    }
    return MaterialSpriteId::missing;
}

MaterialSpriteId material_bag_sprite(items::MaterialId id) noexcept {
    return material_loot_sprite(id);
}

const char* material_bag_display_name(items::MaterialId id) noexcept {
    return ui_material_name(id);
}

Rectangle material_bag_text_backing(Rectangle slot, float scale) noexcept {
    const float left = 34.0F * scale;
    const float horizontal_inset = 4.0F * scale;
    const float vertical_inset = 3.0F * scale;
    return {slot.x + left, slot.y + vertical_inset,
        (std::max)(0.0F, slot.width - left - horizontal_inset),
        (std::max)(0.0F, slot.height - vertical_inset * 2.0F)};
}

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
    const float scale = inventory.scale;
    const float panel_height = 280.0F * scale;
    result.panel = {inventory.detail.x, inventory.detail.y + inventory.detail.height
            - (std::min)(inventory.detail.height, panel_height),
        inventory.detail.width, (std::min)(inventory.detail.height, panel_height)};
    if (result.panel.width <= kInset * 2.0F * scale
            || result.panel.height <= 42.0F * scale) {
        return result;
    }
    const std::size_t rows = (items::kMaterialCount + kColumns - 1U) / kColumns;
    const float cell_width = (result.panel.width
        - kInset * 2.0F * scale - kGap * scale)
        / static_cast<float>(kColumns);
    const float cell_height = (result.panel.height - 42.0F * scale
        - kInset * scale - kGap * scale
        * static_cast<float>(rows - 1U)) / static_cast<float>(rows);
    if (cell_width <= 0.0F || cell_height <= 0.0F) return result;
    for (std::size_t index = 0U; index < result.slots.size(); ++index) {
        const std::size_t row = index / kColumns;
        const std::size_t column = index % kColumns;
        result.slots[index] = {result.panel.x + kInset * scale
                + static_cast<float>(column)
                    * (cell_width + kGap * scale),
            result.panel.y + 33.0F * scale
                + static_cast<float>(row)
                    * (cell_height + kGap * scale),
            cell_width, cell_height};
    }
    return result;
}

Rectangle material_bag_detail_bounds(int width, int height) noexcept {
    const InventoryLayout inventory = inventory_layout(width, height);
    const MaterialBagLayout bag = material_bag_layout(width, height);
    constexpr float kDetailTopInset = 36.0F;
    constexpr float kDetailBagGap = 4.0F;
    const float top = inventory.detail.y + kDetailTopInset * inventory.scale;
    const float bottom = (std::max)(
        top, bag.panel.y - kDetailBagGap * inventory.scale);
    return {inventory.detail.x, top, inventory.detail.width, bottom - top};
}

ReinforcementConfirmationLayout reinforcement_confirmation_layout(
    int width, int height) noexcept {
    const float scale = ui_viewport_scale(width, height);
    const float panel_width = (std::min)(360.0F * scale,
        (std::max)(180.0F * scale,
            static_cast<float>(width) - 32.0F * scale));
    const float panel_height = 124.0F * scale;
    const Rectangle panel{(static_cast<float>(width) - panel_width) * 0.5F,
        (static_cast<float>(height) - panel_height) * 0.5F,
        panel_width, panel_height};
    const float modal_inset = 14.0F * scale;
    const float modal_gap = 8.0F * scale;
    const float button_width = (panel.width - modal_inset * 2.0F
        - modal_gap) * 0.5F;
    return {panel,
        {panel.x + modal_inset,
            panel.y + panel.height - 38.0F * scale,
            button_width, 24.0F * scale},
        {panel.x + modal_inset + button_width + modal_gap,
            panel.y + panel.height - 38.0F * scale,
            button_width, 24.0F * scale}};
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
    const MaterialPack& assets, Font font, bool font_ready,
    int width, int height) const noexcept {
    const MaterialBagLayout layout = material_bag_layout(width, height);
    const float scale = ui_viewport_scale(width, height);
    if (!layout.contains_all_slots()) return;
    DrawRectangleRec(layout.panel, kMaterialTextBacking);
    if (!assets.draw_nine_slice(ui_material_sprite(
            UiMaterialElement::inventory_panel_detail), layout.panel)) {
        DrawRectangleRounded(layout.panel, 0.025F, 5, Color{8, 12, 20, 255});
        DrawRectangleRoundedLinesEx(layout.panel, 0.025F, 5, 1.0F,
            Color{72, 91, 120, 255});
    }
    const Rectangle title_bounds{layout.panel.x + 36.0F * scale,
            layout.panel.y + 4.0F * scale,
            layout.panel.width - 48.0F * scale, 25.0F * scale};
    draw_material_text(font, font_ready, u8"材料袋", title_bounds,
        18.0F, 12.0F, ui_text_contrast_style().primary,
        kMaterialTextBacking);
    static_cast<void>(assets.draw(MaterialSpriteId::bag_frame_nw,
        {layout.panel.x + 13.0F, layout.panel.y + 13.0F}, false, 0.16F));
    static_cast<void>(assets.draw(MaterialSpriteId::bag_frame_ne,
        {layout.panel.x + layout.panel.width - 13.0F, layout.panel.y + 13.0F},
        false, 0.16F));
    static_cast<void>(assets.draw(MaterialSpriteId::bag_frame_sw,
        {layout.panel.x + 13.0F, layout.panel.y + layout.panel.height - 13.0F},
        false, 0.16F));
    static_cast<void>(assets.draw(MaterialSpriteId::bag_frame_se,
        {layout.panel.x + layout.panel.width - 13.0F,
         layout.panel.y + layout.panel.height - 13.0F}, false, 0.16F));
    for (std::size_t index = 0U; index < layout.slots.size(); ++index) {
        const items::MaterialId id = static_cast<items::MaterialId>(index);
        const bool selected = selected_.has_value() && *selected_ == id;
        const Color color = [&] {
            const Rgba8 rgba = material_color(id);
            return Color{rgba.r, rgba.g, rgba.b, rgba.a};
        }();
        const Rectangle slot = layout.slots[index];
        if (!assets.draw_horizontal_slice(ui_material_sprite(selected
                ? UiMaterialElement::inventory_slot_selected
                : UiMaterialElement::inventory_slot_idle),
                {4.0F, 4.0F, 120.0F, 120.0F}, 28.0F, slot)) {
            DrawRectangleRounded(slot, 0.08F, 4,
                selected ? Color{34, 68, 88, 255} : Color{21, 28, 39, 255});
            DrawRectangleRoundedLinesEx(slot, 0.08F, 4,
                selected ? 2.0F : 1.0F, color);
        }
        const float icon_scale = (std::min)(0.22F * scale,
            (std::max)(0.08F, slot.height * 0.72F / 128.0F));
        const float icon_width = 128.0F * icon_scale;
        static_cast<void>(assets.draw(material_bag_sprite(id),
            {slot.x + 4.0F + icon_width * 0.5F,
             slot.y + slot.height * 0.5F}, false, icon_scale));
        const Rectangle text_container = material_bag_text_backing(slot, scale);
        draw_opaque_material_text_backing(text_container);
        const char* display_name = material_bag_display_name(id);
        if (selected && id == items::MaterialId::directed) {
            switch (directed_category_) {
            case items::DirectedCategory::damage: display_name = u8"伤害核心"; break;
            case items::DirectedCategory::defense: display_name = u8"防御核心"; break;
            case items::DirectedCategory::speed: display_name = u8"速度核心"; break;
            case items::DirectedCategory::element: display_name = u8"元素核心"; break;
            case items::DirectedCategory::count: break;
            }
        }
        const float font_size = ui_typography().kInventoryBodyFontSize;
        char display_text[64]{};
        const UiUtf8CopyResult formatted = ui_utf8_format(
            display_text, sizeof(display_text), u8"%s x%llu", display_name,
            static_cast<unsigned long long>(state.materials[index]));
        if (!formatted.input_was_valid || formatted.truncated) continue;
        draw_material_text(font, font_ready,
            display_text, text_container, font_size, 9.0F, color,
            kMaterialTextBacking);
    }
}

void MaterialBagRenderer::draw_reinforcement_confirmation(
    const MaterialPack& assets, Font font, bool font_ready,
    int width, int height) const noexcept {
    if (!reinforcement_confirmation_item_.has_value()) return;
    const ReinforcementConfirmationLayout layout =
        reinforcement_confirmation_layout(width, height);
    const float scale = ui_viewport_scale(width, height);
    DrawRectangleRec(layout.panel, kDangerBacking);
    if (!assets.draw_nine_slice(
            ui_material_sprite(UiMaterialElement::warning_modal),
            layout.panel)) {
        DrawRectangleRounded(layout.panel, 0.04F, 5, kDangerBacking);
    }
    DrawRectangleLinesEx(layout.panel, 2.0F, Color{255, 113, 96, 255});
    const Rectangle danger_bounds{layout.panel.x + 14.0F * scale,
        layout.panel.y + 10.0F * scale,
        layout.panel.width - 28.0F * scale, 25.0F * scale};
    draw_material_text(font, font_ready,
        u8"危险：强化失败，装备丢失", danger_bounds, 16.0F, 10.0F,
        ui_text_contrast_style().danger, kDangerBacking);
    const Rectangle question_bounds{layout.panel.x + 14.0F * scale,
        layout.panel.y + 38.0F * scale,
        layout.panel.width - 28.0F * scale, 22.0F * scale};
    draw_material_text(font, font_ready, u8"确认强化石？", question_bounds,
        ui_typography().kInventoryBodyFontSize, 9.0F,
        ui_text_contrast_style().primary, kDangerBacking);
    DrawRectangleRec(layout.confirm, kConfirmBacking);
    if (!assets.draw_horizontal_slice(ui_material_sprite(
            UiMaterialElement::reinforcement_confirm),
            {4.0F, 22.0F, 120.0F, 84.0F}, 24.0F, layout.confirm)) {
        DrawRectangleRounded(layout.confirm, 0.10F, 4,
            kConfirmBacking);
    }
    DrawRectangleRoundedLinesEx(layout.confirm, 0.10F, 4, 2.0F,
        Color{255, 113, 96, 255});
    DrawRectangleRec(layout.cancel, kCancelBacking);
    if (!assets.draw_horizontal_slice(ui_material_sprite(
            UiMaterialElement::reinforcement_cancel),
            {4.0F, 30.0F, 120.0F, 67.0F}, 24.0F, layout.cancel)) {
        DrawRectangleRounded(layout.cancel, 0.10F, 4,
            kCancelBacking);
    }
    DrawRectangleRoundedLinesEx(layout.cancel, 0.10F, 4, 1.0F,
        Color{112, 145, 180, 255});
    draw_material_text(font, font_ready, u8"确认", layout.confirm,
        ui_typography().kInventoryBodyFontSize, 9.0F,
        ui_text_contrast_style().primary, kConfirmBacking);
    draw_material_text(font, font_ready, u8"取消", layout.cancel,
        ui_typography().kInventoryBodyFontSize, 9.0F,
        ui_text_contrast_style().primary, kCancelBacking);
}

}  // namespace arpg::platform
