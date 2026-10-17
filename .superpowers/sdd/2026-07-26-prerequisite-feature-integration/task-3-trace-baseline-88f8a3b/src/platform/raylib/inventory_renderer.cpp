#include "inventory_renderer.hpp"

#include "host_input.hpp"

#include "dungeon/dungeon_session.hpp"
#include "dungeon_runtime.hpp"
#include "items/item_catalog.hpp"
#include "ui_material.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_bounds_audit.hpp"
#include "ui_text_renderer.hpp"
#include "ui_typography.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include "direct_input_poison.hpp"

static_assert(arpg::platform::direct_input_poison::active,
    "direct input poison must be active in inventory_renderer.cpp");

namespace arpg::platform {
namespace {

constexpr float kCellHeight = 58.0F;
constexpr float kGridTopInset = 82.0F;
constexpr float kGridBottomInset = 58.0F;

const char* slot_name(items::ItemSlot slot) noexcept {
    switch (slot) {
    case items::ItemSlot::weapon: return "Weapon";
    case items::ItemSlot::helmet: return "Helmet";
    case items::ItemSlot::chest: return "Chest";
    case items::ItemSlot::gloves: return "Gloves";
    case items::ItemSlot::boots: return "Boots";
    case items::ItemSlot::accessory: return "Accessory";
    case items::ItemSlot::count: break;
    }
    return "Unknown";
}

const char* rarity_name(items::ItemRarity rarity) noexcept {
    switch (rarity) {
    case items::ItemRarity::normal: return "Normal";
    case items::ItemRarity::magic: return "Magic";
    case items::ItemRarity::rare: return "Rare";
    }
    return "Unknown";
}

Color rarity_color(items::ItemRarity rarity) noexcept {
    switch (rarity) {
    case items::ItemRarity::normal: return Color{218, 225, 234, 255};
    case items::ItemRarity::magic: return Color{90, 154, 255, 255};
    case items::ItemRarity::rare: return Color{255, 199, 68, 255};
    }
    return RAYWHITE;
}

bool contains(Rectangle rectangle, Vector2 point) noexcept {
    return point.x >= rectangle.x && point.x <= rectangle.x + rectangle.width
        && point.y >= rectangle.y && point.y <= rectangle.y + rectangle.height;
}

void fit_inventory_text(Font font, const char* source, char* output,
    std::size_t capacity, float width, float size) noexcept {
    if (output == nullptr || capacity == 0U) return;
    static_cast<void>(std::snprintf(output, capacity, "%s",
        source == nullptr ? "" : source));
    output[capacity - 1U] = '\0';
    if (!(width > 0.0F)
            || MeasureTextEx(font, output, size, 0.5F).x <= width) return;
    std::size_t length = std::char_traits<char>::length(output);
    while (length > 0U) {
        do {
            --length;
        } while (length > 0U
            && (static_cast<unsigned char>(output[length]) & 0xC0U) == 0x80U);
        output[length] = '\0';
        if (length + 4U >= capacity) continue;
        static_cast<void>(std::snprintf(
            output + length, capacity - length, "..."));
        if (MeasureTextEx(font, output, size, 0.5F).x <= width) return;
        output[length] = '\0';
    }
}

void draw_hud_font_text(Font font, bool ready, const char* text,
    float x, float y, float size, Color color,
    UiTextAuditPage audit_page = UiTextAuditPage::count,
    UiTextAuditRole audit_role = UiTextAuditRole::count,
    Rectangle audit_container = {}, float minimum_size = 0.0F,
    const Rectangle* blockers = nullptr,
    std::size_t blocker_count = 0U) noexcept {
    if (text == nullptr || text[0] == '\0') return;
    const Font draw_font = ready && IsFontValid(font) ? font : GetFontDefault();
    const float viewport_scale = ui_viewport_scale(
        GetScreenWidth(), GetScreenHeight());
    size *= viewport_scale;
    minimum_size *= viewport_scale;
    char fitted[256]{};
    const bool audited = audit_page != UiTextAuditPage::count
        && audit_role != UiTextAuditRole::count
        && audit_container.width > 0.0F && audit_container.height > 0.0F;
    fit_inventory_text(draw_font, text, fitted, sizeof(fitted),
        audited ? audit_container.width - 6.0F : 0.0F, size);
    const char* const final_text = fitted;
    const UiTextContrastStyle style = ui_text_contrast_style();
    color.a = 255U;
    if (ui_luma_contrast_ratio(color, style.backing) < 4.5F) {
        color = style.muted;
    }
    constexpr float kSpacing = 0.5F;
    if (audited) {
        record_ui_text_bounds(audit_page, audit_role, draw_font, final_text,
            {x, y}, size, kSpacing, audit_container, minimum_size,
            blockers, blocker_count);
    }
    draw_crisp_ui_text(draw_font, final_text, {x, y}, size, kSpacing, color);
}

void draw_opaque_skill_text_backing(Rectangle bounds) noexcept {
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) return;
    DrawRectangleRounded(bounds, 0.12F, 4, Color{5, 9, 16, 255});
}

void draw_panel(Rectangle rectangle, const char* title,
    const MaterialPack& assets, UiMaterialElement element,
    Font font, bool font_ready, Rectangle title_bounds) noexcept {
    if (!assets.draw_nine_slice(ui_material_sprite(element), rectangle)) {
        DrawRectangleRounded(rectangle, 0.025F, 5, Color{8, 12, 20, 247});
        DrawRectangleRoundedLinesEx(rectangle, 0.025F, 5, 1.0F,
            Color{72, 91, 120, 255});
    }
    const Rectangle label_bounds{rectangle.x + 8.0F, rectangle.y + 5.0F,
        std::min(190.0F, rectangle.width - 16.0F), 30.0F};
    static_cast<void>(assets.draw_region_fit(
        ui_material_sprite(UiMaterialElement::label_plate),
        {4.0F, 30.0F, 120.0F, 67.0F}, label_bounds));
    draw_hud_font_text(font, font_ready, title,
        title_bounds.x + 2.0F, title_bounds.y + 2.0F,
        ui_typography().panel_title_font_size,
        ui_text_contrast_style().primary, UiTextAuditPage::inventory,
        UiTextAuditRole::inventory_page_title, title_bounds,
        ui_typography().panel_title_font_size);
}

void draw_button(Rectangle rectangle, const char* label,
    bool enabled, const MaterialPack& assets, Font font, bool font_ready,
    bool active = false, bool opaque_label_plate = false) noexcept {
    const Color fill = !enabled ? Color{42, 45, 52, 255}
        : active ? Color{42, 104, 139, 255} : Color{28, 47, 67, 255};
    const UiTextContrastStyle text_style = ui_text_contrast_style();
    const Color text = !enabled ? text_style.muted
        : active ? text_style.interaction : text_style.primary;
    const UiMaterialElement element = !enabled
        ? UiMaterialElement::inventory_button_disabled
        : active ? UiMaterialElement::inventory_button_active
                 : UiMaterialElement::inventory_button_idle;
    Rectangle source{4.0F, 40.0F, 120.0F, 47.0F};
    if (element == UiMaterialElement::inventory_button_active) {
        source = {4.0F, 42.0F, 120.0F, 44.0F};
    } else if (element == UiMaterialElement::inventory_button_disabled) {
        source = {4.0F, 41.0F, 120.0F, 46.0F};
    }
    if (!assets.draw_horizontal_slice(ui_material_sprite(element),
            source, 24.0F, rectangle)) {
        DrawRectangleRounded(rectangle, 0.12F, 4, fill);
        DrawRectangleRoundedLinesEx(rectangle, 0.12F, 4, 1.0F,
            enabled ? Color{90, 151, 190, 255} : Color{65, 68, 75, 255});
    }
    if (opaque_label_plate) {
        const float scale = ui_viewport_scale(
            GetScreenWidth(), GetScreenHeight());
        draw_opaque_skill_text_backing({rectangle.x + 4.0F * scale,
            rectangle.y + 3.0F * scale,
            rectangle.width - 8.0F * scale,
            rectangle.height - 6.0F * scale});
    }
    draw_hud_font_text(font, font_ready, label,
        rectangle.x + 9.0F, rectangle.y + 5.0F,
        ui_typography().kInventoryBodyFontSize, text,
        UiTextAuditPage::inventory, UiTextAuditRole::inventory_status,
        {rectangle.x + 5.0F, rectangle.y + 2.0F,
            rectangle.width - 10.0F, rectangle.height - 4.0F},
        ui_typography().kInventoryBodyFontSize);
}

void draw_inventory_page_button(Rectangle rectangle, const char* label,
    bool active, Font font, bool font_ready,
    const MaterialPack& assets) noexcept {
    if (!assets.draw_horizontal_slice(ui_material_sprite(active
            ? UiMaterialElement::inventory_tab_active
            : UiMaterialElement::inventory_tab_idle),
            active ? Rectangle{4.0F, 32.0F, 120.0F, 63.0F}
                   : Rectangle{4.0F, 34.0F, 120.0F, 59.0F},
            24.0F, rectangle)) {
        DrawRectangleRounded(rectangle, 0.14F, 4,
            active ? Color{42, 104, 139, 255} : Color{23, 39, 57, 255});
        DrawRectangleRoundedLinesEx(rectangle, 0.14F, 4, 1.0F,
            active ? Color{151, 225, 255, 255} : Color{75, 105, 137, 255});
    }
    draw_hud_font_text(font, font_ready, label,
        rectangle.x + 12.0F, rectangle.y + 4.0F,
        ui_typography().inventory_tab_font_size,
        active ? ui_text_contrast_style().interaction
               : ui_text_contrast_style().primary,
        UiTextAuditPage::inventory, UiTextAuditRole::inventory_status,
        {rectangle.x + 6.0F, rectangle.y + 2.0F,
            rectangle.width - 12.0F, rectangle.height - 4.0F},
        ui_typography().inventory_tab_font_size);
}

void draw_active_skill_loadout(const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& status,
    const ActiveSkillLoadoutSelection& selection,
    const MaterialPack& assets,
    Font font, bool font_ready) noexcept {
    const ActiveSkillLoadoutLayout layout = active_skill_loadout_layout(
        GetScreenWidth(), GetScreenHeight());
    const float scale = layout.scale;
    const InventoryTextSafeLayout text_layout = inventory_text_safe_layout(
        GetScreenWidth(), GetScreenHeight());
    const ActiveSkillLoadoutView view = make_active_skill_loadout_view(
        snapshot.skill_loadout, selection,
        snapshot.pending_save_kind.has_value());
    if (!assets.draw_nine_slice(
            ui_material_sprite(UiMaterialElement::skill_panel),
            layout.panel)) {
        DrawRectangleRounded(layout.panel, 0.025F, 5, Color{8, 12, 20, 247});
        DrawRectangleRoundedLinesEx(layout.panel, 0.025F, 5, 1.0F,
            Color{72, 91, 120, 255});
    }
    draw_hud_font_text(font, font_ready, u8"主动技能石槽",
        text_layout.skill_panel_title.x + 2.0F,
        text_layout.skill_panel_title.y + 2.0F, 23.0F,
        ui_text_contrast_style().primary, UiTextAuditPage::skill,
        UiTextAuditRole::skill_page_title, text_layout.skill_panel_title,
        18.0F);

    for (std::size_t index = 0U; index < view.slots.size(); ++index) {
        const ActiveSkillLoadoutSlotView& slot = view.slots[index];
        const Rectangle bounds = layout.main_slots[index];
        const UiMaterialElement slot_material = slot.selected
            ? UiMaterialElement::skill_slot_selected
            : slot.empty ? UiMaterialElement::skill_slot_empty
                         : UiMaterialElement::skill_slot_ready;
        if (!assets.draw_nine_slice(ui_material_sprite(slot_material), bounds)) {
            DrawRectangleRounded(bounds, 0.08F, 4,
                slot.selected ? Color{38, 83, 111, 255}
                              : Color{20, 30, 44, 255});
            DrawRectangleRoundedLinesEx(bounds, 0.08F, 4,
                slot.selected ? 3.0F : 1.0F,
                slot.selected ? Color{148, 225, 255, 255}
                              : Color{77, 105, 137, 255});
        }
        static_cast<void>(assets.draw(skill_stone_sprite(
            SkillStoneVisualKind::active),
            {bounds.x + 29.0F * scale,
             bounds.y + 31.0F * scale},
            false, 0.24F * scale,
            slot.empty ? Fade(WHITE, 0.30F) : WHITE));
        char number[8]{};
        static_cast<void>(std::snprintf(number, sizeof(number), "%u",
            static_cast<unsigned>(slot.slot_number)));
        draw_hud_font_text(font, font_ready, number,
            bounds.x + 55.0F * scale, bounds.y + 7.0F * scale,
            18.0F, WHITE,
            UiTextAuditPage::skill, UiTextAuditRole::skill_main_slot,
            {bounds.x + 50.0F * scale, bounds.y + 3.0F * scale,
                bounds.width - 54.0F * scale, 24.0F * scale}, 18.0F);
        const Rectangle name_backing{
            bounds.x + 8.0F * scale,
            bounds.y + 52.0F * scale,
            bounds.width - 16.0F * scale,
            32.0F * scale};
        draw_opaque_skill_text_backing(name_backing);
        draw_hud_font_text(font, font_ready,
            slot.empty ? u8"空主技能槽" : slot.name.data(),
            name_backing.x + 8.0F * scale,
            name_backing.y + 4.0F * scale,
            ui_typography().kSkillDescriptionFontSize,
            slot.empty ? ui_text_contrast_style().muted
                       : Color{210, 241, 255, 255},
            UiTextAuditPage::skill, UiTextAuditRole::skill_description,
            {name_backing.x + 4.0F * scale,
                name_backing.y + 2.0F * scale,
                name_backing.width - 8.0F * scale,
                name_backing.height - 4.0F * scale},
            ui_typography().kSkillDescriptionFontSize);
    }

    draw_hud_font_text(font, font_ready, u8"辅助技能石（只读）",
        text_layout.support_section_title.x + 2.0F,
        text_layout.support_section_title.y + 2.0F, 19.0F,
        ui_text_contrast_style().primary, UiTextAuditPage::skill,
        UiTextAuditRole::skill_description,
        text_layout.support_section_title, 18.0F);
    for (const Rectangle support : layout.support_slots) {
        if (!assets.draw_region_fit(ui_material_sprite(
                UiMaterialElement::skill_slot_support),
                {4.0F, 5.0F, 120.0F, 117.0F}, support)) {
            DrawRectangleRounded(support, 0.08F, 4, Color{17, 24, 35, 255});
            DrawRectangleRoundedLinesEx(support, 0.08F, 4, 1.0F,
                Color{61, 79, 101, 255});
        }
        static_cast<void>(assets.draw(skill_stone_sprite(
            SkillStoneVisualKind::support),
            {support.x + support.width * 0.5F,
             support.y + support.height * 0.5F},
            false, 0.22F * scale, Fade(WHITE, 0.48F)));
        draw_hud_font_text(font, font_ready, u8"空",
            support.x + 17.0F * scale, support.y + 16.0F * scale,
            ui_typography().kSkillDescriptionFontSize,
            ui_text_contrast_style().muted, UiTextAuditPage::skill,
            UiTextAuditRole::skill_description,
            {support.x + 12.0F * scale, support.y + 10.0F * scale,
                support.width - 24.0F * scale,
                support.height - 20.0F * scale}, 16.0F);
    }

    draw_hud_font_text(font, font_ready, u8"未装备技能石",
        text_layout.inventory_section_title.x + 2.0F,
        text_layout.inventory_section_title.y + 2.0F, 19.0F,
        ui_text_contrast_style().primary, UiTextAuditPage::skill,
        UiTextAuditRole::skill_description,
        text_layout.inventory_section_title, 18.0F);
    for (std::size_t index = 0U; index < view.inventory_count; ++index) {
        const ActiveSkillInventoryStoneView& stone = view.inventory[index];
        const Rectangle bounds = layout.inventory_slots[index];
        if (!assets.draw_horizontal_slice(ui_material_sprite(stone.selected
                ? UiMaterialElement::skill_slot_selected
                : UiMaterialElement::skill_slot_ready),
                stone.selected
                    ? Rectangle{5.0F, 4.0F, 118.0F, 120.0F}
                    : Rectangle{4.0F, 5.0F, 120.0F, 118.0F},
                28.0F, bounds)) {
            DrawRectangleRounded(bounds, 0.08F, 4,
                stone.selected ? Color{38, 83, 111, 255}
                               : Color{20, 30, 44, 255});
            DrawRectangleRoundedLinesEx(bounds, 0.08F, 4,
                stone.selected ? 3.0F : 1.0F,
                stone.selected ? Color{148, 225, 255, 255}
                               : Color{77, 105, 137, 255});
        }
        static_cast<void>(assets.draw(skill_stone_sprite(
            SkillStoneVisualKind::active),
            {bounds.x + 22.0F * scale, bounds.y + bounds.height * 0.5F},
            false, 0.22F * scale));
        const Rectangle inventory_name_backing{
            bounds.x + 39.0F * scale,
            bounds.y + 10.0F * scale,
            bounds.width - 46.0F * scale,
            bounds.height - 20.0F * scale};
        draw_opaque_skill_text_backing(inventory_name_backing);
        draw_hud_font_text(font, font_ready, stone.name.data(),
            inventory_name_backing.x + 5.0F * scale,
            inventory_name_backing.y + 9.0F * scale, 18.0F,
            ui_text_contrast_style().primary, UiTextAuditPage::skill,
            UiTextAuditRole::skill_inventory_entry,
            {inventory_name_backing.x + 3.0F * scale,
                inventory_name_backing.y + 2.0F * scale,
                inventory_name_backing.width - 6.0F * scale,
                inventory_name_backing.height - 4.0F * scale}, 18.0F);
    }
    if (view.inventory_count == 0U) {
        draw_hud_font_text(font, font_ready, u8"无",
            layout.inventory_slots[0U].x + 2.0F * scale,
            layout.inventory_slots[0U].y + 19.0F * scale, 18.0F,
            ui_text_contrast_style().muted, UiTextAuditPage::skill,
            UiTextAuditRole::skill_inventory_entry,
            layout.inventory_slots[0U], 18.0F);
    }

    const bool removable = !view.save_pending
        && selection.selected_slot < snapshot.skill_loadout.slots.size()
        && snapshot.skill_loadout.slots[selection.selected_slot].active
            != skills::ActiveSkillId::none;
    if (!assets.draw_horizontal_slice(ui_material_sprite(removable
            ? UiMaterialElement::inventory_button_active
            : UiMaterialElement::inventory_button_disabled),
            removable ? Rectangle{4.0F, 42.0F, 120.0F, 44.0F}
                      : Rectangle{4.0F, 41.0F, 120.0F, 46.0F},
            24.0F, layout.remove_button)) {
        DrawRectangleRounded(layout.remove_button, 0.12F, 4,
            removable ? Color{74, 53, 65, 255} : Color{42, 45, 52, 255});
        DrawRectangleRoundedLinesEx(layout.remove_button, 0.12F, 4, 1.0F,
            removable ? Color{219, 125, 151, 255}
                      : Color{65, 68, 75, 255});
    }
    const Rectangle remove_text_backing{
        layout.remove_button.x + 42.0F * scale,
        layout.remove_button.y + 5.0F * scale,
        layout.remove_button.width - 84.0F * scale,
        layout.remove_button.height - 10.0F * scale};
    draw_opaque_skill_text_backing(remove_text_backing);
    draw_hud_font_text(font, font_ready, u8"取出",
        remove_text_backing.x + 18.0F * scale,
        remove_text_backing.y + 3.0F * scale, 18.0F,
        removable ? ui_text_contrast_style().interaction
                  : ui_text_contrast_style().muted,
        UiTextAuditPage::skill, UiTextAuditRole::skill_description,
        {remove_text_backing.x + 2.0F * scale,
            remove_text_backing.y + 1.0F * scale,
            remove_text_backing.width - 4.0F * scale,
            remove_text_backing.height - 2.0F * scale}, 18.0F);

    if (view.save_pending) {
        draw_hud_font_text(font, font_ready, u8"正在保存",
            layout.panel.x + 18.0F,
            layout.panel.y + layout.panel.height - 32.0F,
            ui_typography().kSkillDescriptionFontSize,
            Color{255, 191, 96, 255});
    } else if (status.indicator == SaveIndicator::error) {
        draw_hud_font_text(font, font_ready, u8"保存失败",
            layout.panel.x + 18.0F,
            layout.panel.y + layout.panel.height - 32.0F,
            ui_typography().kSkillDescriptionFontSize,
            Color{255, 118, 118, 255});
    }
}

int grid_columns(Rectangle grid, float scale) noexcept {
    return std::max(1, static_cast<int>(
        (grid.width - 20.0F * scale) / (170.0F * scale)));
}

float grid_cell_width(Rectangle grid, int columns, float scale) noexcept {
    return (grid.width - 20.0F * scale) / static_cast<float>(columns);
}

Rectangle grid_viewport(Rectangle grid, float scale) noexcept {
    return {grid.x + 10.0F * scale, grid.y + kGridTopInset * scale,
        std::max(0.0F, grid.width - 20.0F * scale),
        std::max(0.0F, grid.height
            - (kGridTopInset + kGridBottomInset) * scale)};
}

Rectangle grid_item_rectangle(Rectangle viewport, int columns,
    std::size_t filtered_position, float scroll_rows, float scale) noexcept {
    const float width = viewport.width / static_cast<float>(columns);
    const std::size_t row = filtered_position / static_cast<std::size_t>(columns);
    const std::size_t column = filtered_position % static_cast<std::size_t>(columns);
    return {viewport.x + static_cast<float>(column) * width + 2.0F * scale,
        viewport.y + (static_cast<float>(row) - scroll_rows)
            * kCellHeight * scale + 2.0F * scale,
        width - 4.0F * scale, (kCellHeight - 4.0F) * scale};
}

Rectangle slot_filter_button(Rectangle grid, float scale) noexcept {
    return {grid.x + 10.0F * scale, grid.y + 40.0F * scale,
        std::max(80.0F * scale,
            grid.width * 0.48F - 15.0F * scale), 28.0F * scale};
}

Rectangle rarity_filter_button(Rectangle grid, float scale) noexcept {
    return {grid.x + grid.width * 0.50F + 5.0F * scale,
        grid.y + 40.0F * scale,
        std::max(80.0F * scale,
            grid.width * 0.50F - 15.0F * scale), 28.0F * scale};
}

Rectangle combine_button(Rectangle grid, float scale) noexcept {
    return {grid.x + 10.0F * scale,
        grid.y + grid.height - 42.0F * scale,
        grid.width - 20.0F * scale, 30.0F * scale};
}

bool equipment_equal(const items::EquipmentState& left,
    const items::EquipmentState& right) noexcept {
    return left.equipped_ids == right.equipped_ids;
}

std::size_t base_value_index(std::uint8_t item_level) noexcept {
    for (std::uint8_t tier = 1U; tier <= 8U; ++tier) {
        if (item_level >= items::tier_minimum_level(tier)) {
            return static_cast<std::size_t>(8U - tier);
        }
    }
    return 0U;
}

}  // namespace

void InventoryRenderer::open(const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot) {
    open_ = true;
    page_ = InventoryPage::equipment_materials;
    active_skill_selection_ = {};
    sync(session, snapshot);
}

void InventoryRenderer::close() noexcept {
    open_ = false;
    page_ = InventoryPage::equipment_materials;
    active_skill_selection_ = {};
    click_tracker_ = {};
    static_cast<void>(material_bag_.resolve_reinforcement_confirmation(false));
}

void InventoryRenderer::show_skill_stones_page() noexcept {
    if (!open_) return;
    page_ = InventoryPage::skill_stones;
    if (active_skill_selection_.selected_slot
            == kNoActiveSkillLoadoutSelection) {
        active_skill_selection_.selected_slot = 0U;
    }
}

bool InventoryRenderer::is_open() const noexcept { return open_; }

const items::ItemInstance* InventoryRenderer::selected_item(
    const items::ItemOwnershipState& state) const noexcept {
    return cached_selected_item(view_cache_, state);
}

void InventoryRenderer::refresh_comparison(
    const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot) {
    const items::ItemOwnershipState& state = session.item_state();
    if (comparison_generation_ == snapshot.commit_generation
        && comparison_item_id_ == selected_item_id_
        && equipment_equal(comparison_equipment_, state.equipment)) return;
    comparison_generation_ = snapshot.commit_generation;
    comparison_item_id_ = selected_item_id_;
    comparison_equipment_ = state.equipment;
    current_build_ = session.preview_equipment_build(state.equipment);
    difference_.reset();
    const items::ItemInstance* const item = selected_item(state);
    const items::BaseDefinition* const base = item == nullptr
        ? nullptr : items::base_definition(item->base_id);
    if (!current_build_.has_value() || item == nullptr || base == nullptr) return;
    items::EquipmentState candidate = state.equipment;
    candidate.equipped_ids[static_cast<std::size_t>(base->slot)] = item->id;
    const auto candidate_build = session.preview_equipment_build(candidate);
    if (candidate_build.has_value()) {
        difference_ = compare_player_builds(*current_build_, *candidate_build);
    }
}

void InventoryRenderer::sync(const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot) {
    const items::ItemOwnershipState& state = session.item_state();
    material_bag_.sync_selection(state);
    refresh_inventory_view_cache(view_cache_, state,
        snapshot.commit_generation, filter_, selected_item_id_, recipe_);
    if (selected_item_id_ != 0U && selected_item(state) == nullptr) {
        selected_item_id_ = 0U;
        view_cache_.selected_item_id = 0U;
    }
    if (recipe_.ids != view_cache_.resolved_recipe.ids
        || recipe_.count != view_cache_.resolved_recipe.count) {
        recipe_ = view_cache_.resolved_recipe;
        view_cache_.requested_recipe = recipe_;
    }
    refresh_comparison(session, snapshot);
}

bool InventoryRenderer::recipe_ready() const noexcept {
    return cached_recipe_ready(view_cache_);
}

bool InventoryRenderer::process_input(DungeonRuntime& runtime,
    const dungeon::DungeonSnapshot& snapshot,
    const HostFrameInput& input) {
    if (!open_ || runtime.session() == nullptr) return false;
    dungeon::DungeonSession& session = *runtime.session();
    sync(session, snapshot);
    const items::ItemOwnershipState& state = session.item_state();
    const InventoryLayout layout = inventory_layout(GetScreenWidth(), GetScreenHeight());
    const Rectangle viewport = grid_viewport(layout.grid, layout.scale);
    const int columns = grid_columns(layout.grid, layout.scale);
    const float wheel = input.mouse_wheel;
    if (wheel != 0.0F) {
        scroll_rows_ = clamp_inventory_scroll_rows(view_cache_.filtered_indices.size(), columns,
            scroll_rows_ - wheel, viewport.height,
            kCellHeight * layout.scale);
    }
    const bool left_pressed = input.mouse_left_pressed;
    const bool right_pressed = input.mouse_right_pressed;
    if (!left_pressed && !right_pressed) return false;
    const Vector2 mouse = input.mouse_position;
    const bool requests_enabled = !snapshot.pending_save_kind.has_value()
        && runtime.state() == DungeonRuntimeState::running;
    const ActiveSkillLoadoutLayout skill_layout = active_skill_loadout_layout(
        GetScreenWidth(), GetScreenHeight());
    if (left_pressed && contains(skill_layout.equipment_page_button, mouse)) {
        page_ = InventoryPage::equipment_materials;
        return false;
    }
    if (left_pressed && contains(skill_layout.skill_stones_page_button, mouse)) {
        page_ = InventoryPage::skill_stones;
        if (active_skill_selection_.selected_slot
                == kNoActiveSkillLoadoutSelection
            && active_skill_selection_.selected_inventory
                == skills::ActiveSkillId::none) {
            active_skill_selection_.selected_slot = 0U;
        }
        return false;
    }
    if (page_ == InventoryPage::skill_stones) {
        if (!left_pressed) return false;
        const auto command = active_skill_loadout_command_after_click(
            snapshot.skill_loadout, skill_layout, mouse,
            active_skill_selection_, !requests_enabled);
        if (!command.has_value()
                || command->kind == ActiveSkillLoadoutActionKind::select) {
            return false;
        }
        dungeon::RequestResult request = dungeon::RequestResult::rejected;
        switch (command->kind) {
        case ActiveSkillLoadoutActionKind::select:
            return false;
        case ActiveSkillLoadoutActionKind::remove:
            request = session.request_remove_active_skill(command->slot);
            break;
        case ActiveSkillLoadoutActionKind::equip:
            request = session.request_equip_active_skill(
                command->skill, command->slot);
            break;
        case ActiveSkillLoadoutActionKind::swap:
            request = session.request_swap_active_skill_slots(
                command->slot, command->other_slot);
            break;
        }
        if (request != dungeon::RequestResult::accepted) return false;
        const std::uint64_t generation_before = snapshot.commit_generation;
        runtime.service_pending_save();
        const dungeon::DungeonSnapshot after = session.snapshot();
        if (after.commit_generation != generation_before) {
            if (command->kind == ActiveSkillLoadoutActionKind::equip) {
                active_skill_selection_.selected_slot = command->slot;
                active_skill_selection_.selected_inventory =
                    skills::ActiveSkillId::none;
            } else if (command->kind == ActiveSkillLoadoutActionKind::swap) {
                active_skill_selection_.selected_slot = command->other_slot;
            }
        }
        return true;
    }
    if (material_bag_.reinforcement_confirmation_item().has_value()) {
        const ReinforcementConfirmationLayout confirmation =
            reinforcement_confirmation_layout(GetScreenWidth(), GetScreenHeight());
        if (right_pressed || (left_pressed
                && contains(confirmation.cancel, mouse))) {
            static_cast<void>(
                material_bag_.resolve_reinforcement_confirmation(false));
            return false;
        }
        if (left_pressed && contains(confirmation.confirm, mouse)) {
            const auto confirmed =
                material_bag_.resolve_reinforcement_confirmation(true);
            if (requests_enabled && confirmed.has_value()
                    && runtime.request_reinforcement(*confirmed)
                        == dungeon::RequestResult::accepted) {
                runtime.service_pending_save();
                return true;
            }
            return false;
        }
        return false;
    }
    if (right_pressed && material_bag_.cycle_directed_category(
            mouse, GetScreenWidth(), GetScreenHeight())) {
        return false;
    }
    if (left_pressed && material_bag_.process_click(mouse, state,
            GetScreenWidth(), GetScreenHeight())) {
        return false;
    }
    if (right_pressed && material_bag_.clear_selection()) return false;
    if (left_pressed
            && contains(slot_filter_button(layout.grid, layout.scale), mouse)) {
        if (!filter_.slot.has_value()) filter_.slot = items::ItemSlot::weapon;
        else if (*filter_.slot == items::ItemSlot::accessory) filter_.slot.reset();
        else filter_.slot = static_cast<items::ItemSlot>(
            static_cast<std::uint8_t>(*filter_.slot) + 1U);
        sync(session, snapshot);
        scroll_rows_ = 0.0F;
        return false;
    }
    if (left_pressed
            && contains(rarity_filter_button(layout.grid, layout.scale), mouse)) {
        if (!filter_.rarity.has_value()) filter_.rarity = items::ItemRarity::normal;
        else if (*filter_.rarity == items::ItemRarity::rare) filter_.rarity.reset();
        else filter_.rarity = static_cast<items::ItemRarity>(
            static_cast<std::uint8_t>(*filter_.rarity) + 1U);
        sync(session, snapshot);
        scroll_rows_ = 0.0F;
        return false;
    }
    if (left_pressed) {
        if (const auto slot = hit_test_equipped_slot(
                mouse, layout, state.equipment)) {
            if (requests_enabled
                && runtime.request_unequip(*slot)
                    == dungeon::RequestResult::accepted) {
                runtime.service_pending_save();
                return true;
            }
            return false;
        }
    }
    if (left_pressed
            && contains(combine_button(layout.grid, layout.scale), mouse)) {
        if (requests_enabled && recipe_ready()
            && runtime.request_recipe(recipe_.ids) == dungeon::RequestResult::accepted) {
            runtime.service_pending_save();
            return true;
        }
        return false;
    }
    const VisibleGridRange visible = visible_grid_range(view_cache_.filtered_indices.size(),
        columns, scroll_rows_, viewport.height, kCellHeight * layout.scale);
    for (std::size_t offset = 0U; offset < visible.count; ++offset) {
        const std::size_t filtered_position = visible.first + offset;
        const Rectangle cell = grid_item_rectangle(viewport, columns,
            filtered_position, scroll_rows_, layout.scale);
        if (!contains(cell, mouse)) continue;
        const items::ItemInstance& item = state.items[
            view_cache_.filtered_indices[filtered_position]];
        selected_item_id_ = item.id;
        if (left_pressed && requests_enabled) {
            const auto selected_material = material_bag_.selected_material();
            if (selected_material.has_value()) {
                if (*selected_material == items::MaterialId::reinforcement_stone) {
                    if (item.reinforcement >= 12U) {
                        static_cast<void>(
                            material_bag_.begin_reinforcement_confirmation(
                                item.id, item.reinforcement));
                        return false;
                    }
                    if (runtime.request_reinforcement(item.id)
                            == dungeon::RequestResult::accepted) {
                        runtime.service_pending_save();
                        return true;
                    }
                    return false;
                }
                if (items::material_is_coupon(*selected_material)) {
                    if (runtime.request_coupon(*selected_material, item.id)
                            == dungeon::RequestResult::accepted) {
                        runtime.service_pending_save();
                        return true;
                    }
                    return false;
                }
                const auto category = *selected_material == items::MaterialId::directed
                    ? std::optional<items::DirectedCategory>{
                        material_bag_.directed_category()}
                    : std::nullopt;
                if (runtime.request_craft(*selected_material, item.id, category)
                        == dungeon::RequestResult::accepted) {
                    runtime.service_pending_save();
                    return true;
                }
                return false;
            }
        }
        const bool recipe_toggle = right_pressed || input.control_down;
        if (recipe_toggle) {
            static_cast<void>(toggle_recipe_selection(recipe_, item.id));
        }
        comparison_generation_ = ~std::uint64_t{0U};
        sync(session, snapshot);
        if (recipe_toggle) {
            click_tracker_ = {};
            return false;
        }
        if (register_inventory_click(click_tracker_, item.id, GetTime())
                == InventoryClickKind::double_click
            && requests_enabled
            && runtime.request_equip(item.id) == dungeon::RequestResult::accepted) {
            runtime.service_pending_save();
            return true;
        }
        return false;
    }
    return false;
}

void InventoryRenderer::draw(const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& status,
    const MaterialPack& material_pack,
    Font hud_font, bool hud_font_ready) {
    if (!open_) return;
    sync(session, snapshot);
    const items::ItemOwnershipState& state = session.item_state();
    const InventoryLayout layout = inventory_layout(GetScreenWidth(), GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{3, 5, 9, 255});
    const ActiveSkillLoadoutLayout skill_layout = active_skill_loadout_layout(
        GetScreenWidth(), GetScreenHeight());
    const InventoryTextSafeLayout text_layout = inventory_text_safe_layout(
        GetScreenWidth(), GetScreenHeight());
    if (page_ == InventoryPage::equipment_materials) {
        draw_hud_font_text(hud_font, hud_font_ready,
            "EQUIPMENT INVENTORY  ·  I / ESC CLOSE",
            text_layout.page_title.x + 2.0F,
            text_layout.page_title.y + 2.0F,
            ui_typography().page_title_font_size,
            ui_text_contrast_style().primary, UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_page_title, text_layout.page_title,
            ui_typography().page_title_font_size);
    } else {
        draw_hud_font_text(hud_font, hud_font_ready,
            u8"技能石背包  ·  I / ESC 关闭",
            text_layout.page_title.x + 2.0F,
            text_layout.page_title.y + 2.0F, 23.0F,
            ui_text_contrast_style().primary, UiTextAuditPage::skill,
            UiTextAuditRole::skill_page_title, text_layout.page_title, 18.0F);
    }
    draw_inventory_page_button(skill_layout.equipment_page_button,
        u8"装备 / 材料", page_ == InventoryPage::equipment_materials,
        hud_font, hud_font_ready, material_pack);
    draw_inventory_page_button(skill_layout.skill_stones_page_button,
        u8"技能石", page_ == InventoryPage::skill_stones,
        hud_font, hud_font_ready, material_pack);
    if (page_ == InventoryPage::skill_stones) {
        draw_active_skill_loadout(snapshot, status, active_skill_selection_,
            material_pack, hud_font, hud_font_ready);
        return;
    }
    draw_panel(layout.equipment, "EQUIPMENT", material_pack,
        UiMaterialElement::inventory_panel_equipment,
        hud_font, hud_font_ready, text_layout.equipment_panel_title);
    draw_panel(layout.grid, "INVENTORY", material_pack,
        UiMaterialElement::inventory_panel_grid,
        hud_font, hud_font_ready, text_layout.grid_panel_title);
    draw_panel(layout.detail, "ITEM DETAIL", material_pack,
        UiMaterialElement::inventory_panel_detail,
        hud_font, hud_font_ready, text_layout.detail_panel_title);
    material_bag_.draw(state, material_pack, hud_font, hud_font_ready,
        GetScreenWidth(), GetScreenHeight());

    for (std::size_t index = 0U; index < state.equipment.equipped_ids.size(); ++index) {
        const auto slot = static_cast<items::ItemSlot>(index);
        const Rectangle rectangle = equipment_slot_rectangle(layout, slot);
        const bool equipped = state.equipment.equipped_ids[index] != 0U;
        if (!material_pack.draw_horizontal_slice(ui_material_sprite(equipped
                ? UiMaterialElement::inventory_slot_selected
                : UiMaterialElement::inventory_slot_idle),
                equipped ? Rectangle{4.0F, 4.0F, 120.0F, 120.0F}
                         : Rectangle{4.0F, 4.0F, 120.0F, 120.0F},
                28.0F, rectangle)) {
            DrawRectangleRounded(rectangle, 0.08F, 4,
                Color{21, 28, 40, 255});
            DrawRectangleRoundedLinesEx(rectangle, 0.08F, 4, 1.0F,
                Color{75, 100, 133, 255});
        }
        static_cast<void>(material_pack.draw(ground_loot_item_sprite(slot),
            {rectangle.x + 24.0F, rectangle.y + rectangle.height * 0.5F},
            false, 0.24F, state.equipment.equipped_ids[index] == 0U
                ? Fade(WHITE, 0.30F) : WHITE));
        const std::uint64_t id = state.equipment.equipped_ids[index];
        if (id == 0U) {
            draw_hud_font_text(hud_font, hud_font_ready,
                TextFormat("%s  [empty]", slot_name(slot)),
                rectangle.x + 45.0F, rectangle.y + 6.0F,
                ui_typography().kInventoryBodyFontSize,
                ui_text_contrast_style().primary,
                UiTextAuditPage::inventory,
                UiTextAuditRole::inventory_equipment_slot,
                {rectangle.x + 41.0F, rectangle.y + 3.0F,
                    rectangle.width - 45.0F, rectangle.height - 6.0F},
                ui_typography().kInventoryBodyFontSize);
        } else {
            draw_hud_font_text(hud_font, hud_font_ready,
                TextFormat("%s  #%llu", slot_name(slot),
                static_cast<unsigned long long>(id)),
                rectangle.x + 45.0F, rectangle.y + 6.0F,
                ui_typography().kInventoryBodyFontSize,
                ui_text_contrast_style().primary,
                UiTextAuditPage::inventory,
                UiTextAuditRole::inventory_equipment_slot,
                {rectangle.x + 41.0F, rectangle.y + 3.0F,
                    rectangle.width - 45.0F, rectangle.height - 6.0F},
                ui_typography().kInventoryBodyFontSize);
        }
    }
    const float scale = layout.scale;
    int stat_y = static_cast<int>(layout.equipment.y + 248.0F * scale);
    const auto draw_stat = [&](const char* text) noexcept {
        const Rectangle line{layout.equipment.x + 8.0F,
            static_cast<float>(stat_y) - 2.0F * scale,
            layout.equipment.width - 16.0F * scale, 21.0F * scale};
        draw_hud_font_text(hud_font, hud_font_ready, text,
            line.x + 4.0F * scale, static_cast<float>(stat_y),
            ui_typography().kInventoryBodyFontSize,
            ui_text_contrast_style().primary,
            UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_statistics, line,
            ui_typography().kInventoryBodyFontSize);
        stat_y += static_cast<int>(21.0F * scale);
    };
    if (snapshot.combat.has_value()) {
        const combat::PlayerSnapshot& player = snapshot.combat->player;
        draw_stat(TextFormat("HP %d/%d  Barrier %d/%d", player.hp,
            player.max_hp, player.barrier, player.max_barrier));
        draw_stat(TextFormat("Armor %lld (%g%%)",
            static_cast<long long>(player.armor),
            player.armor_reduction_bp / 100.0F));
        draw_stat(TextFormat("Evasion %lld (%g%%)",
            static_cast<long long>(player.evasion),
            player.evasion_rate_bp / 100.0F));
        draw_stat(TextFormat("F %g/%g  W %g/%g%%",
            player.damage_reduction[0] / 100.0F,
            player.damage_reduction_cap[0] / 100.0F,
            player.damage_reduction[1] / 100.0F,
            player.damage_reduction_cap[1] / 100.0F));
        draw_stat(TextFormat("L %g/%g  C %g/%g%%",
            player.damage_reduction[2] / 100.0F,
            player.damage_reduction_cap[2] / 100.0F,
            player.damage_reduction[3] / 100.0F,
            player.damage_reduction_cap[3] / 100.0F));
    }
    if (current_build_.has_value()) {
        stat_y += 8;
        draw_stat(TextFormat("Move %g%%  Attack %g%%",
            current_build_->values.movement_speed / 100.0F,
            current_build_->values.attack_speed / 100.0F));
        draw_stat(TextFormat("Weapon physical +%lld",
            static_cast<long long>(current_build_->weapon_physical)));
        draw_stat(TextFormat("Melee %g%%",
            current_build_->values.melee_damage / 100.0F));
    }

    const Rectangle slot_button = slot_filter_button(layout.grid, layout.scale);
    const Rectangle rarity_button = rarity_filter_button(
        layout.grid, layout.scale);
    draw_button(slot_button, TextFormat("Slot: %s",
        filter_.slot.has_value() ? slot_name(*filter_.slot) : "All"), true,
        material_pack, hud_font, hud_font_ready);
    draw_button(rarity_button, TextFormat("Rarity: %s",
        filter_.rarity.has_value() ? rarity_name(*filter_.rarity) : "All"), true,
        material_pack, hud_font, hud_font_ready);
    const Rectangle viewport = grid_viewport(layout.grid, layout.scale);
    DrawRectangleRec(viewport, Color{11, 16, 25, 255});
    BeginScissorMode(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
        static_cast<int>(viewport.width), static_cast<int>(viewport.height));
    const int columns = grid_columns(layout.grid, layout.scale);
    scroll_rows_ = clamp_inventory_scroll_rows(view_cache_.filtered_indices.size(), columns,
        scroll_rows_, viewport.height, kCellHeight * layout.scale);
    const VisibleGridRange visible = visible_grid_range(view_cache_.filtered_indices.size(),
        columns, scroll_rows_, viewport.height, kCellHeight * layout.scale);
    if (visible.count == 0U) {
        draw_hud_font_text(hud_font, hud_font_ready,
            "No items match this filter.",
            viewport.x + 14.0F * layout.scale,
            viewport.y + 12.0F * layout.scale,
            ui_typography().kInventoryBodyFontSize,
            ui_text_contrast_style().muted,
            UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_grid_entry,
            {viewport.x + 8.0F * layout.scale,
                viewport.y + 6.0F * layout.scale,
                viewport.width - 16.0F * layout.scale,
                28.0F * layout.scale},
            ui_typography().kInventoryBodyFontSize);
    }
    for (std::size_t offset = 0U; offset < visible.count; ++offset) {
        const std::size_t filtered_position = visible.first + offset;
        const items::ItemInstance& item = state.items[
            view_cache_.filtered_indices[filtered_position]];
        const items::BaseDefinition* const base = items::base_definition(item.base_id);
        const Rectangle cell = grid_item_rectangle(viewport, columns,
            filtered_position, scroll_rows_, layout.scale);
        const bool selected = item.id == selected_item_id_;
        const bool recipe_selected = std::find(recipe_.ids.begin(),
            recipe_.ids.begin() + static_cast<std::ptrdiff_t>(recipe_.count),
            item.id) != recipe_.ids.begin() + static_cast<std::ptrdiff_t>(recipe_.count);
        if (!material_pack.draw_horizontal_slice(ui_material_sprite(
                selected || recipe_selected
                    ? UiMaterialElement::inventory_slot_selected
                    : UiMaterialElement::inventory_slot_idle),
                {4.0F, 4.0F, 120.0F, 120.0F}, 28.0F, cell)) {
            DrawRectangleRounded(cell, 0.08F, 4,
                selected ? Color{34, 68, 88, 255} : Color{21, 28, 39, 255});
            DrawRectangleRoundedLinesEx(cell, 0.08F, 4,
                recipe_selected ? 3.0F : 1.0F,
                recipe_selected ? Color{237, 118, 212, 255}
                                : rarity_color(item.rarity));
        }
        draw_hud_font_text(hud_font, hud_font_ready,
            base == nullptr ? "Invalid" : base->name.data(),
            cell.x + 7.0F * layout.scale,
            cell.y + 5.0F * layout.scale,
            ui_typography().kInventoryBodyFontSize, rarity_color(item.rarity),
            UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_grid_entry,
            {cell.x + 3.0F * layout.scale,
                cell.y + 2.0F * layout.scale,
                cell.width - 6.0F * layout.scale,
                23.0F * layout.scale},
            ui_typography().kInventoryBodyFontSize);
        draw_hud_font_text(hud_font, hud_font_ready,
            TextFormat("%s  i%u", rarity_name(item.rarity),
            static_cast<unsigned>(item.item_level)),
            cell.x + 7.0F * layout.scale,
            cell.y + 30.0F * layout.scale,
            ui_typography().kInventoryBodyFontSize,
            ui_text_contrast_style().muted,
            UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_grid_entry,
            {cell.x + 3.0F * layout.scale,
                cell.y + 27.0F * layout.scale,
                cell.width - 6.0F * layout.scale,
                cell.height - 29.0F * layout.scale},
            ui_typography().kInventoryBodyFontSize);
    }
    EndScissorMode();
    const bool enabled = !snapshot.pending_save_kind.has_value()
        && recipe_ready();
    draw_button(combine_button(layout.grid, layout.scale),
        TextFormat("Combine (%u/3)",
        static_cast<unsigned>(recipe_.count)), enabled, material_pack,
        hud_font, hud_font_ready, false, true);
    bool reinforced_recipe_input = false;
    for (std::size_t index = 0U; index < recipe_.count; ++index) {
        for (const items::ItemInstance& recipe_item : state.items) {
            if (recipe_item.id == recipe_.ids[index]
                    && recipe_item.reinforcement != 0U) {
                reinforced_recipe_input = true;
                break;
            }
        }
    }
    if (reinforced_recipe_input) {
        draw_hud_font_text(hud_font, hud_font_ready,
            "WARNING: Combine removes reinforcement.",
            layout.grid.x + 12.0F * layout.scale,
            layout.grid.y + layout.grid.height - 58.0F * layout.scale,
            ui_typography().kInventoryBodyFontSize,
            ui_text_contrast_style().warning,
            UiTextAuditPage::inventory, UiTextAuditRole::inventory_status,
            {layout.grid.x + 8.0F * layout.scale,
                layout.grid.y + layout.grid.height - 62.0F * layout.scale,
                layout.grid.width - 16.0F * layout.scale,
                24.0F * layout.scale},
            ui_typography().kInventoryBodyFontSize);
    }
    if (const auto selected_material = material_bag_.selected_material();
            selected_material.has_value()) {
        const items::MaterialDefinition* const definition =
            items::material_definition(*selected_material);
        draw_hud_font_text(hud_font, hud_font_ready,
            TextFormat("Selected: %s - click an item to use (R-click cancels)",
            definition == nullptr ? "Invalid" : definition->name.data()),
            layout.grid.x + 12.0F * layout.scale,
            layout.grid.y + 42.0F * layout.scale,
            ui_typography().kInventoryBodyFontSize,
            ui_text_contrast_style().warning,
            UiTextAuditPage::inventory, UiTextAuditRole::inventory_status,
            {layout.grid.x + 8.0F * layout.scale,
                layout.grid.y + 38.0F * layout.scale,
                layout.grid.width - 16.0F * layout.scale,
                24.0F * layout.scale},
            ui_typography().kInventoryBodyFontSize);
    }
    if (snapshot.pending_save_kind.has_value()) {
        draw_hud_font_text(hud_font, hud_font_ready,
            "SAVE PENDING - ACTIONS DISABLED",
            layout.grid.x + 12.0F * layout.scale,
            layout.grid.y + 42.0F * layout.scale,
            ui_typography().kInventoryBodyFontSize,
            Color{255, 191, 96, 255}, UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_status,
            {layout.grid.x + 8.0F * layout.scale,
                layout.grid.y + 38.0F * layout.scale,
                layout.grid.width - 16.0F * layout.scale,
                24.0F * layout.scale},
            ui_typography().kInventoryBodyFontSize);
    }
    if (snapshot.reinforcement_receipt.valid) {
        const dungeon::ReinforcementReceipt& receipt =
            snapshot.reinforcement_receipt;
        const char* const result = receipt.destroyed ? "DESTROYED"
            : receipt.success ? "SUCCESS" : "FAILED";
        const char* const action = receipt.coupon ? "Coupon" : "Reinforcement";
        const Color color = receipt.success ? Color{147, 244, 169, 255}
                                            : Color{255, 145, 118, 255};
        if (receipt.destroyed) {
            draw_hud_font_text(hud_font, hud_font_ready,
                TextFormat("%s %s: +%u -> DESTROYED", action, result,
                static_cast<unsigned>(receipt.before)),
                layout.grid.x + 12.0F * layout.scale,
                layout.grid.y + 42.0F * layout.scale,
                ui_typography().kInventoryBodyFontSize, color,
                UiTextAuditPage::inventory, UiTextAuditRole::inventory_status,
                {layout.grid.x + 8.0F * layout.scale,
                    layout.grid.y + 38.0F * layout.scale,
                    layout.grid.width - 16.0F * layout.scale,
                    24.0F * layout.scale},
                ui_typography().kInventoryBodyFontSize);
        } else {
            draw_hud_font_text(hud_font, hud_font_ready,
                TextFormat("%s %s: +%u -> +%u", action, result,
                static_cast<unsigned>(receipt.before),
                static_cast<unsigned>(receipt.after)),
                layout.grid.x + 12.0F * layout.scale,
                layout.grid.y + 42.0F * layout.scale,
                ui_typography().kInventoryBodyFontSize, color,
                UiTextAuditPage::inventory, UiTextAuditRole::inventory_status,
                {layout.grid.x + 8.0F * layout.scale,
                    layout.grid.y + 38.0F * layout.scale,
                    layout.grid.width - 16.0F * layout.scale,
                    24.0F * layout.scale},
                ui_typography().kInventoryBodyFontSize);
        }
    }

    const items::ItemInstance* const item = selected_item(state);
    const Rectangle first_detail_line = detail_line_rectangle(layout, 0U);
    const int detail_x = static_cast<int>(first_detail_line.x);
    const int detail_y = static_cast<int>(first_detail_line.y);
    if (item == nullptr) {
        draw_hud_font_text(hud_font, hud_font_ready,
            "Click an item to inspect.", static_cast<float>(detail_x) + 2.0F,
            static_cast<float>(detail_y) + 2.0F,
            ui_typography().kInventoryDetailFontSize,
            ui_text_contrast_style().muted,
            UiTextAuditPage::inventory, UiTextAuditRole::inventory_detail,
            first_detail_line, ui_typography().kInventoryDetailFontSize);
        return;
    }
    const items::BaseDefinition* const base = items::base_definition(item->base_id);
    if (base == nullptr) return;
    const Rectangle detail_bounds = material_bag_detail_bounds(
        GetScreenWidth(), GetScreenHeight());
    BeginScissorMode(static_cast<int>(detail_bounds.x),
        static_cast<int>(detail_bounds.y), static_cast<int>(detail_bounds.width),
        static_cast<int>(detail_bounds.height));
    std::size_t detail_line = 0U;
    const auto draw_detail_line = [&](const char* text, Color color) noexcept {
        const Rectangle line = detail_line_rectangle(layout, detail_line++);
        draw_hud_font_text(hud_font, hud_font_ready, text,
            line.x + 2.0F, line.y + 2.0F,
            ui_typography().kInventoryDetailFontSize, color,
            UiTextAuditPage::inventory, UiTextAuditRole::inventory_detail,
            line, ui_typography().kInventoryDetailFontSize);
    };
    draw_detail_line(TextFormat("%s %s", rarity_name(item->rarity),
        base->name.data()), rarity_color(item->rarity));
    draw_detail_line(TextFormat("iLvl %u  Required %u",
        static_cast<unsigned>(item->item_level),
        static_cast<unsigned>(item->required_level)), RAYWHITE);
    draw_detail_line(TextFormat("Reinforcement +%u",
        static_cast<unsigned>(item->reinforcement)), Color{255, 225, 123, 255});
    for (std::size_t effect_index = 0U;
         effect_index < base->effect_count; ++effect_index) {
        const items::BaseEffect& effect = base->effects[effect_index];
        const ItemAttributeLabel inherent = item_attribute_label(effect.effect,
            effect.stat, effect.operation, base->slot, 0xFFU);
        const std::int32_t inherent_raw =
            effect.values[base_value_index(item->item_level)];
        draw_detail_line(TextFormat("Base [%s] %s %+g%s%s", inherent.scope,
            inherent.name, item_attribute_display_value(inherent_raw, inherent),
            inherent.suffix, inherent.qualifier), Color{150, 210, 255, 255});
    }
    for (std::size_t index = 0U; index < item->affix_count; ++index) {
        const items::AffixRoll& roll = item->affixes[index];
        const items::AffixDefinition* const affix = items::affix_definition(roll.affix_id);
        if (affix == nullptr) continue;
        const ItemAttributeLabel label = item_attribute_label(affix->effect,
            affix->stat, affix->operation, base->slot, roll.variant);
        const std::int32_t affix_raw =
            affix->values[static_cast<std::size_t>(8U - roll.tier)];
        draw_detail_line(TextFormat("T%u %s [%s] %s %+g%s%s",
            static_cast<unsigned>(roll.tier),
            affix->kind == items::AffixKind::prefix ? "Pre" : "Suf",
            label.scope, label.name,
            item_attribute_display_value(affix_raw, label),
            label.suffix, label.qualifier), Color{216, 222, 234, 255});
    }
    draw_detail_line("FULL BUILD DIFFERENCE", Color{242, 183, 255, 255});
    if (!difference_.has_value()) {
        draw_detail_line("Preview unavailable", Color{255, 126, 126, 255});
    } else {
        const BuildDifference& diff = *difference_;
        draw_detail_line(TextFormat("HP %+g  Barrier %+g",
            diff.max_health / static_cast<double>(modifiers::kFixedOne),
            diff.max_barrier / static_cast<double>(modifiers::kFixedOne)), RAYWHITE);
        draw_detail_line(TextFormat("Flat P/F/W/L/C %+g/%+g/%+g/%+g/%+g",
            diff.flat_damage[0] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[1] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[2] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[3] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[4] / static_cast<double>(modifiers::kFixedOne)), RAYWHITE);
        draw_detail_line(TextFormat("Inc P/F/W/L/C %+g/%+g/%+g/%+g/%+g%%",
            diff.damage_increased[0] / 100.0, diff.damage_increased[1] / 100.0,
            diff.damage_increased[2] / 100.0,
            diff.damage_increased[3] / 100.0,
            diff.damage_increased[4] / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("Melee/Knock %+g/%+g%%",
            diff.melee_damage / 100.0, diff.impulse_scale / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("Move/Attack %+g/%+g%%",
            diff.move_speed / 100.0, diff.attack_speed / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("LocalAtk %+g%%  Weapon %+g",
            diff.local_attack_speed_bp / 100.0,
            static_cast<double>(diff.weapon_physical)), RAYWHITE);
        draw_detail_line(TextFormat("Armor %+g/DR %+g%%  Eva %+g/Chance %+g%%",
            static_cast<double>(diff.armor), diff.armor_reduction_bp / 100.0,
            static_cast<double>(diff.evasion),
            diff.evasion_rate_bp / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("F/W/L/C DR %+g/%+g/%+g/%+g%%",
            diff.damage_reduction[0] / 100.0,
            diff.damage_reduction[1] / 100.0,
            diff.damage_reduction[2] / 100.0,
            diff.damage_reduction[3] / 100.0), RAYWHITE);
        draw_detail_line(TextFormat("F/W/L/C cap %+g/%+g/%+g/%+g%%",
            diff.damage_reduction_cap_bonus[0] / 100.0,
            diff.damage_reduction_cap_bonus[1] / 100.0,
            diff.damage_reduction_cap_bonus[2] / 100.0,
            diff.damage_reduction_cap_bonus[3] / 100.0), RAYWHITE);
    }
    EndScissorMode();
    material_bag_.draw_reinforcement_confirmation(material_pack,
        hud_font, hud_font_ready, GetScreenWidth(), GetScreenHeight());
}

}  // namespace arpg::platform
