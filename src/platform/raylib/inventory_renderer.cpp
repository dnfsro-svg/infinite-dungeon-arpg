#include "inventory_renderer.hpp"

#include "host_input.hpp"

#include "dungeon/dungeon_session.hpp"
#include "dungeon_runtime.hpp"
#include "items/item_catalog.hpp"
#include "ui_material.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_bounds_audit.hpp"
#include "ui_text_catalog.hpp"
#include "ui_text_layout.hpp"
#include "ui_text_renderer.hpp"
#include "ui_typography.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "direct_input_poison.hpp"

static_assert(arpg::platform::direct_input_poison::active,
    "direct input poison must be active in inventory_renderer.cpp");

namespace arpg::platform {
namespace {

constexpr float kCellHeight = 58.0F;
constexpr float kGridTopInset = 82.0F;
constexpr float kGridBottomInset = 58.0F;
constexpr Color kInventoryTextBacking{5, 9, 16, 255};

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

UiTextExtent measure_inventory_text(const char* text, float font_size,
    float spacing, void* context) noexcept {
    if (context == nullptr) return {};
    const Font font = *static_cast<const Font*>(context);
    if (!IsFontValid(font)) return {};
    const Vector2 extent = MeasureTextEx(font, text, font_size, spacing);
    return {extent.x, extent.y};
}

void draw_hud_font_text(Font font, bool ready, const char* text,
    float x, float y, float size, Color color,
    UiTextAuditPage audit_page = UiTextAuditPage::count,
    UiTextAuditRole audit_role = UiTextAuditRole::count,
    Rectangle audit_container = Rectangle(), float minimum_size = 0.0F,
    const Rectangle* blockers = nullptr,
    std::size_t blocker_count = 0U,
    Color opaque_backing = kInventoryTextBacking) noexcept {
    if (!ready || !IsFontValid(font)) return;
    if (text == nullptr || text[0] == '\0') return;
    if (opaque_backing.a != 255U) return;
    const float viewport_scale = ui_viewport_scale(
        GetScreenWidth(), GetScreenHeight());
    size *= viewport_scale;
    minimum_size *= viewport_scale;
    char fitted[256]{};
    const bool audited = audit_page != UiTextAuditPage::count
        && audit_role != UiTextAuditRole::count
        && audit_container.width > 0.0F && audit_container.height > 0.0F;
    const Rectangle fit_bounds = audited ? audit_container : Rectangle{
        x, y, std::max(1.0F, static_cast<float>(GetScreenWidth()) - x),
        std::max(1.0F, size * 1.5F)};
    const UiCenteredTextLayout layout = fit_centered_ui_text(
        fitted, sizeof(fitted), text, fit_bounds, size,
        minimum_size > 0.0F ? minimum_size : size,
        0.5F * viewport_scale, &measure_inventory_text, &font);
    if (!layout.drawable) return;
    const UiTextContrastStyle style = ui_text_contrast_style();
    color.a = 255U;
    if (ui_luma_contrast_ratio(color, opaque_backing) < 4.5F) {
        color = style.primary;
        if (ui_luma_contrast_ratio(color, opaque_backing) < 4.5F) return;
    }
    const Vector2 position{std::max(x, fit_bounds.x),
        std::max(y, fit_bounds.y)};
    if (audited) {
        record_ui_text_bounds(audit_page, audit_role, font, layout.text,
            position, layout.font_size, layout.spacing, audit_container,
            minimum_size,
            blockers, blocker_count);
    }
    draw_crisp_ui_text(font, layout.text, position, layout.font_size,
        layout.spacing, color);
}

void draw_centered_hud_font_text(Font font, bool ready, const char* text,
    Rectangle interaction_bounds, float size, float minimum_size, Color color,
    UiTextAuditPage audit_page = UiTextAuditPage::count,
    UiTextAuditRole audit_role = UiTextAuditRole::count,
    Color opaque_backing = kInventoryTextBacking) noexcept {
    if (!ready || !IsFontValid(font)) return;
    if (text == nullptr || text[0] == '\0') return;
    if (opaque_backing.a != 255U) return;
    const float scale = ui_viewport_scale(GetScreenWidth(), GetScreenHeight());
    char fitted[256]{};
    const UiCenteredTextLayout layout = fit_centered_ui_text(
        fitted, sizeof(fitted), text, interaction_bounds, size * scale,
        minimum_size * scale, 0.5F * scale,
        &measure_inventory_text, &font);
    if (!layout.drawable) return;
    color.a = 255U;
    if (ui_luma_contrast_ratio(color, opaque_backing) < 4.5F) {
        color = ui_text_contrast_style().primary;
        if (ui_luma_contrast_ratio(color, opaque_backing) < 4.5F) return;
    }
    if (audit_page != UiTextAuditPage::count
            && audit_role != UiTextAuditRole::count) {
        record_ui_text_bounds(audit_page, audit_role, font, layout.text,
            layout.position, layout.font_size, layout.spacing,
            interaction_bounds, minimum_size * scale);
    }
    draw_crisp_ui_text(font, layout.text, layout.position, layout.font_size,
        layout.spacing, color);
}

void draw_opaque_skill_text_backing(Rectangle bounds) noexcept {
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) return;
    DrawRectangleRounded(bounds, 0.12F, 4, Color{5, 9, 16, 255});
}

void draw_panel(Rectangle rectangle, const char* title,
    const MaterialPack& assets, UiMaterialElement element,
    Font font, bool font_ready, Rectangle title_bounds) noexcept {
    DrawRectangleRec(rectangle, kInventoryTextBacking);
    if (!assets.draw_nine_slice(ui_material_sprite(element), rectangle)) {
        DrawRectangleRounded(rectangle, 0.025F, 5, Color{8, 12, 20, 255});
        DrawRectangleRoundedLinesEx(rectangle, 0.025F, 5, 1.0F,
            Color{72, 91, 120, 255});
    }
    const Rectangle label_bounds{rectangle.x + 8.0F, rectangle.y + 5.0F,
        std::min(190.0F, rectangle.width - 16.0F), 30.0F};
    static_cast<void>(assets.draw_region_fit(
        ui_material_sprite(UiMaterialElement::label_plate),
        {4.0F, 30.0F, 120.0F, 67.0F}, label_bounds));
    draw_centered_hud_font_text(font, font_ready, title, title_bounds,
        ui_typography().panel_title_font_size,
        ui_typography().panel_title_font_size,
        ui_text_contrast_style().primary, UiTextAuditPage::inventory,
        UiTextAuditRole::inventory_page_title);
}

void draw_button(Rectangle rectangle, const char* label,
    bool enabled, const MaterialPack& assets, Font font, bool font_ready,
    bool active = false, bool opaque_label_plate = false) noexcept {
    const InventoryControlTextStyle control = inventory_control_text_style();
    const Color fill = !enabled ? control.disabled_backing
        : active ? control.active_backing : control.idle_backing;
    const UiTextContrastStyle text_style = ui_text_contrast_style();
    const Color text = !enabled ? text_style.muted
        : active ? text_style.interaction : text_style.primary;
    const UiMaterialElement element = !enabled
        ? UiMaterialElement::inventory_button_disabled
        : active ? UiMaterialElement::inventory_button_active
                 : UiMaterialElement::inventory_button_idle;
    DrawRectangleRec(rectangle, fill);
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
    static_cast<void>(opaque_label_plate);
    draw_centered_hud_font_text(font, font_ready, label, rectangle,
        ui_typography().kInventoryBodyFontSize,
        ui_typography().kInventoryBodyFontSize, text,
        UiTextAuditPage::inventory, UiTextAuditRole::inventory_status, fill);
}

void draw_inventory_page_button(Rectangle rectangle, const char* label,
    bool active, Font font, bool font_ready,
    const MaterialPack& assets) noexcept {
    const InventoryControlTextStyle control = inventory_control_text_style();
    const Color backing = active ? control.active_backing
                                 : control.idle_backing;
    DrawRectangleRec(rectangle, backing);
    if (!assets.draw_horizontal_slice(ui_material_sprite(active
            ? UiMaterialElement::inventory_tab_active
            : UiMaterialElement::inventory_tab_idle),
            active ? Rectangle{4.0F, 32.0F, 120.0F, 63.0F}
                   : Rectangle{4.0F, 34.0F, 120.0F, 59.0F},
            24.0F, rectangle)) {
        DrawRectangleRounded(rectangle, 0.14F, 4,
            backing);
        DrawRectangleRoundedLinesEx(rectangle, 0.14F, 4, 1.0F,
            active ? Color{151, 225, 255, 255} : Color{75, 105, 137, 255});
    }
    draw_centered_hud_font_text(font, font_ready, label, rectangle,
        ui_typography().inventory_tab_font_size,
        ui_typography().inventory_tab_font_size,
        active ? ui_text_contrast_style().interaction
               : ui_text_contrast_style().primary,
        UiTextAuditPage::inventory, UiTextAuditRole::inventory_status,
        backing);
}

void draw_active_skill_loadout(const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& status,
    const ActiveSkillLoadoutSelection& selection,
    const MaterialPack& assets,
    Font font, bool font_ready) noexcept {
    const ActiveSkillLoadoutLayout layout = active_skill_loadout_layout(
        GetScreenWidth(), GetScreenHeight());
    const float scale = layout.scale;
    const float skill_text_scale = scale / ui_viewport_scale(
        GetScreenWidth(), GetScreenHeight());
    const InventoryTextSafeLayout text_layout = inventory_text_safe_layout(
        GetScreenWidth(), GetScreenHeight());
    const ActiveSkillLoadoutView view = make_active_skill_loadout_view(
        snapshot.skill_loadout, selection,
        snapshot.pending_save_kind.has_value());
    DrawRectangleRec(layout.panel, kInventoryTextBacking);
    if (!assets.draw_nine_slice(
            ui_material_sprite(UiMaterialElement::skill_panel),
            layout.panel)) {
        DrawRectangleRounded(layout.panel, 0.025F, 5, Color{8, 12, 20, 255});
        DrawRectangleRoundedLinesEx(layout.panel, 0.025F, 5, 1.0F,
            Color{72, 91, 120, 255});
    }
    draw_centered_hud_font_text(font, font_ready, u8"主动技能石槽",
        text_layout.skill_panel_title, 23.0F, 18.0F,
        ui_text_contrast_style().primary, UiTextAuditPage::skill,
        UiTextAuditRole::skill_page_title);

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
            bounds.x + 50.0F * scale,
            bounds.y + 30.0F * scale,
            bounds.width - 58.0F * scale,
            bounds.height - 38.0F * scale};
        draw_opaque_skill_text_backing(name_backing);
        draw_centered_hud_font_text(font, font_ready,
            slot.empty ? u8"空主技能槽" : slot.name.data(),
            name_backing,
            ui_typography().kSkillDescriptionFontSize * skill_text_scale,
            9.0F,
            slot.empty ? ui_text_contrast_style().muted
                       : Color{210, 241, 255, 255},
            UiTextAuditPage::skill, UiTextAuditRole::skill_description);
    }

    draw_centered_hud_font_text(font, font_ready, u8"辅助技能石（只读）",
        text_layout.support_section_title, 19.0F, 18.0F,
        ui_text_contrast_style().primary, UiTextAuditPage::skill,
        UiTextAuditRole::skill_description);
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

    draw_centered_hud_font_text(font, font_ready, u8"未装备技能石",
        text_layout.inventory_section_title, 19.0F, 18.0F,
        ui_text_contrast_style().primary, UiTextAuditPage::skill,
        UiTextAuditRole::skill_description);
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
        draw_centered_hud_font_text(font, font_ready, stone.name.data(),
            inventory_name_backing, 18.0F * skill_text_scale, 9.0F,
            ui_text_contrast_style().primary, UiTextAuditPage::skill,
            UiTextAuditRole::skill_inventory_entry);
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
    const InventoryControlTextStyle control = inventory_control_text_style();
    const Color remove_backing = removable ? control.remove_backing
                                           : control.disabled_backing;
    DrawRectangleRec(layout.remove_button, remove_backing);
    if (!assets.draw_horizontal_slice(ui_material_sprite(removable
            ? UiMaterialElement::inventory_button_active
            : UiMaterialElement::inventory_button_disabled),
            removable ? Rectangle{4.0F, 42.0F, 120.0F, 44.0F}
                      : Rectangle{4.0F, 41.0F, 120.0F, 46.0F},
            24.0F, layout.remove_button)) {
        DrawRectangleRounded(layout.remove_button, 0.12F, 4,
            remove_backing);
        DrawRectangleRoundedLinesEx(layout.remove_button, 0.12F, 4, 1.0F,
            removable ? Color{219, 125, 151, 255}
                      : Color{65, 68, 75, 255});
    }
    draw_centered_hud_font_text(font, font_ready, u8"取出",
        layout.remove_button, 18.0F, 18.0F,
        removable ? ui_text_contrast_style().interaction
                  : ui_text_contrast_style().muted,
        UiTextAuditPage::skill, UiTextAuditRole::skill_description,
        remove_backing);

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

void draw_detail_row(Font font, bool ready, InventoryLayout inventory_layout,
    std::size_t line, const char* label, const char* value,
    Color color) noexcept {
    if (!ready || !IsFontValid(font) || label == nullptr
            || label[0] == '\0' || value == nullptr || value[0] == '\0') {
        return;
    }
    const InventoryDetailRowLayout row = inventory_detail_row_layout(
        inventory_layout, line);
    if (!row.valid) return;
    const float preferred = ui_typography().kInventoryDetailFontSize
        * inventory_layout.scale;
    const float minimum = 9.0F * inventory_layout.scale;
    char fitted_label[128]{};
    char fitted_value[96]{};
    const UiCenteredTextLayout label_layout = fit_centered_ui_text(
        fitted_label, sizeof(fitted_label), label, row.label_bounds,
        preferred, minimum, 0.5F * inventory_layout.scale,
        &measure_inventory_text, &font);
    const UiCenteredTextLayout value_layout = fit_centered_ui_text(
        fitted_value, sizeof(fitted_value), value, row.value_bounds,
        preferred, minimum, 0.5F * inventory_layout.scale,
        &measure_inventory_text, &font);
    if (!label_layout.drawable || !value_layout.drawable) return;
    color.a = 255U;
    if (ui_luma_contrast_ratio(color, kInventoryTextBacking) < 4.5F) {
        color = ui_text_contrast_style().muted;
    }
    const Vector2 label_position{row.label_bounds.x,
        label_layout.position.y};
    const float value_x = row.value_bounds.x + row.value_bounds.width
        - value_layout.size.x;
    const Vector2 value_position{value_x, value_layout.position.y};
    record_ui_text_bounds(UiTextAuditPage::inventory,
        UiTextAuditRole::inventory_detail, font, label_layout.text,
        label_position, label_layout.font_size, label_layout.spacing,
        row.label_bounds, minimum);
    record_ui_text_bounds(UiTextAuditPage::inventory,
        UiTextAuditRole::inventory_detail, font, value_layout.text,
        value_position, value_layout.font_size, value_layout.spacing,
        row.value_bounds, minimum);
    draw_crisp_ui_text(font, label_layout.text, label_position,
        label_layout.font_size, label_layout.spacing, color);
    draw_crisp_ui_text(font, value_layout.text, value_position,
        value_layout.font_size, value_layout.spacing, color);
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
    recipe_ = {};
    click_tracker_ = {};
    static_cast<void>(material_bag_.clear_selection());
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
        && runtime.authority_requests_enabled();
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
        // Selection is presentation-only. Advance it when authority accepts
        // the exact request so asynchronous persistence cannot strand the UI
        // on the pre-request selection while the mutation is pending.
        advance_active_skill_loadout_selection(
            active_skill_selection_, *command);
        runtime.service_pending_save();
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
    const char* inventory_binding_label,
    Font hud_font, bool hud_font_ready) {
    if (!open_ || !hud_font_ready || !IsFontValid(hud_font)) return;
    sync(session, snapshot);
    const items::ItemOwnershipState& state = session.item_state();
    const InventoryLayout layout = inventory_layout(GetScreenWidth(), GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{3, 5, 9, 255});
    const ActiveSkillLoadoutLayout skill_layout = active_skill_loadout_layout(
        GetScreenWidth(), GetScreenHeight());
    const InventoryTextSafeLayout text_layout = inventory_text_safe_layout(
        GetScreenWidth(), GetScreenHeight());
    const auto page_title = inventory_page_title(
        page_, inventory_binding_label);
    draw_centered_hud_font_text(hud_font, hud_font_ready,
        page_title.data(), text_layout.page_title,
        page_ == InventoryPage::equipment_materials
            ? ui_typography().page_title_font_size : 23.0F,
        page_ == InventoryPage::equipment_materials
            ? ui_typography().page_title_font_size : 18.0F,
        ui_text_contrast_style().primary,
        page_ == InventoryPage::equipment_materials
            ? UiTextAuditPage::inventory : UiTextAuditPage::skill,
        page_ == InventoryPage::equipment_materials
            ? UiTextAuditRole::inventory_page_title
            : UiTextAuditRole::skill_page_title);
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
    draw_panel(layout.equipment, ui_text(UiTextId::equipment_title), material_pack,
        UiMaterialElement::inventory_panel_equipment,
        hud_font, hud_font_ready, text_layout.equipment_panel_title);
    draw_panel(layout.grid, ui_text(UiTextId::inventory_title), material_pack,
        UiMaterialElement::inventory_panel_grid,
        hud_font, hud_font_ready, text_layout.grid_panel_title);
    draw_panel(layout.detail, ui_text(UiTextId::item_detail_title), material_pack,
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
            {rectangle.x + 24.0F * layout.scale,
                rectangle.y + rectangle.height * 0.5F},
            false, 0.24F * layout.scale,
            state.equipment.equipped_ids[index] == 0U
                 ? Fade(WHITE, 0.30F) : WHITE));
        const std::uint64_t id = state.equipment.equipped_ids[index];
        char slot_text[64]{};
        static_cast<void>(std::snprintf(slot_text, sizeof(slot_text),
            id == 0U ? u8"%s · 空" : u8"%s · 已装备",
            ui_item_slot_name(slot)));
        const Rectangle slot_content{
            rectangle.x + 41.0F * layout.scale,
            rectangle.y + 3.0F * layout.scale,
            rectangle.width - 45.0F * layout.scale,
            rectangle.height - 6.0F * layout.scale};
        draw_centered_hud_font_text(hud_font, hud_font_ready, slot_text,
            slot_content, ui_typography().kInventoryBodyFontSize,
            ui_typography().kInventoryBodyFontSize,
            ui_text_contrast_style().primary,
            UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_equipment_slot);
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
        draw_stat(TextFormat(u8"生命 %d/%d · 护盾 %d/%d", player.hp,
            player.max_hp, player.barrier, player.max_barrier));
        draw_stat(TextFormat(u8"护甲 %lld（减伤 %g%%）",
            static_cast<long long>(player.armor),
            player.armor_reduction_bp / 100.0F));
        draw_stat(TextFormat(u8"闪避 %lld（闪避率 %g%%）",
            static_cast<long long>(player.evasion),
            player.evasion_rate_bp / 100.0F));
        draw_stat(TextFormat(u8"火焰 %g/%g · 水流 %g/%g%%",
            player.damage_reduction[0] / 100.0F,
            player.damage_reduction_cap[0] / 100.0F,
            player.damage_reduction[1] / 100.0F,
            player.damage_reduction_cap[1] / 100.0F));
        draw_stat(TextFormat(u8"闪电 %g/%g · 混沌 %g/%g%%",
            player.damage_reduction[2] / 100.0F,
            player.damage_reduction_cap[2] / 100.0F,
            player.damage_reduction[3] / 100.0F,
            player.damage_reduction_cap[3] / 100.0F));
    }
    if (current_build_.has_value()) {
        stat_y += 8;
        draw_stat(TextFormat(u8"移动速度 %g%% · 攻击速度 %g%%",
            current_build_->values.movement_speed / 100.0F,
            current_build_->values.attack_speed / 100.0F));
        draw_stat(TextFormat(u8"武器物理伤害 +%lld",
            static_cast<long long>(current_build_->weapon_physical)));
        draw_stat(TextFormat(u8"近战伤害 %g%%",
            current_build_->values.melee_damage / 100.0F));
    }

    const Rectangle slot_button = slot_filter_button(layout.grid, layout.scale);
    const Rectangle rarity_button = rarity_filter_button(
        layout.grid, layout.scale);
    draw_button(slot_button, TextFormat(u8"装备槽：%s",
        filter_.slot.has_value()
            ? ui_item_slot_name(*filter_.slot) : u8"全部"), true,
        material_pack, hud_font, hud_font_ready);
    draw_button(rarity_button, TextFormat(u8"稀有度：%s",
        filter_.rarity.has_value()
            ? ui_item_rarity_name(*filter_.rarity) : u8"全部"), true,
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
            u8"过滤：无",
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
        if (base != nullptr) {
            static_cast<void>(material_pack.draw(
                ground_loot_item_sprite(base->slot),
                {cell.x + 21.0F * layout.scale,
                    cell.y + cell.height * 0.5F},
                false, 0.20F * layout.scale));
        }
        const Rectangle item_name_bounds{
            cell.x + 40.0F * layout.scale,
            cell.y + 2.0F * layout.scale,
            cell.width - 43.0F * layout.scale,
            24.0F * layout.scale};
        draw_centered_hud_font_text(hud_font, hud_font_ready,
            ui_item_base_name(item.base_id), item_name_bounds,
            ui_typography().kInventoryBodyFontSize,
            ui_typography().kInventoryBodyFontSize, rarity_color(item.rarity),
            UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_grid_entry);
        const Rectangle item_meta_bounds{
            cell.x + 40.0F * layout.scale,
            cell.y + 27.0F * layout.scale,
            cell.width - 43.0F * layout.scale,
            cell.height - 29.0F * layout.scale};
        draw_centered_hud_font_text(hud_font, hud_font_ready,
            TextFormat(u8"%s · 物品等级 %u",
            ui_item_rarity_name(item.rarity),
            static_cast<unsigned>(item.item_level)), item_meta_bounds,
            ui_typography().kInventoryBodyFontSize,
            ui_typography().kInventoryBodyFontSize,
            ui_text_contrast_style().muted,
            UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_grid_entry);
    }
    EndScissorMode();
    const bool enabled = !snapshot.pending_save_kind.has_value()
        && recipe_ready();
    draw_button(combine_button(layout.grid, layout.scale),
        TextFormat(u8"三合一（%u/3）",
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
            u8"危险：三合一取消强化",
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
        draw_hud_font_text(hud_font, hud_font_ready,
            TextFormat(u8"已选择：%s · 选择物品应用（右键取消）",
            ui_material_name(*selected_material)),
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
            u8"正在保存，已锁定",
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
        const char* const result = receipt.destroyed ? u8"装备丢失"
            : receipt.success ? u8"成功" : u8"失败";
        const char* const action = receipt.coupon ? u8"强化券" : u8"强化";
        const Color color = receipt.success ? Color{147, 244, 169, 255}
                                            : Color{255, 145, 118, 255};
        if (receipt.destroyed) {
            draw_hud_font_text(hud_font, hud_font_ready,
                TextFormat(u8"%s结果：%s / +%u", action, result,
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
                TextFormat(u8"%s结果：%s / +%u → +%u", action, result,
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
    if (item == nullptr) {
        draw_centered_hud_font_text(hud_font, hud_font_ready,
            u8"选择物品", first_detail_line,
            ui_typography().kInventoryDetailFontSize,
            ui_typography().kInventoryDetailFontSize,
            ui_text_contrast_style().muted,
            UiTextAuditPage::inventory, UiTextAuditRole::inventory_detail);
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
    const auto draw_heading = [&](const char* text, Color color) noexcept {
        const Rectangle line = detail_line_rectangle(layout, detail_line++);
        draw_hud_font_text(hud_font, hud_font_ready, text,
            line.x + 2.0F, line.y + 2.0F,
            ui_typography().kInventoryDetailFontSize, color,
            UiTextAuditPage::inventory, UiTextAuditRole::inventory_detail,
            line, ui_typography().kInventoryDetailFontSize);
    };
    const auto draw_row = [&](const char* label, const char* value,
                              Color color) noexcept {
        draw_detail_row(hud_font, hud_font_ready, layout, detail_line++,
            label, value, color);
    };
    char detail_label[160]{};
    char detail_value[128]{};
    const auto format_attribute_value = [&](std::int32_t raw_value,
                                             ItemAttributeLabel label) noexcept {
        const double displayed = item_attribute_display_value(raw_value, label);
        if (label.multiplier) {
            static_cast<void>(std::snprintf(detail_value,
                sizeof(detail_value), u8"%s ×%.2f", label.name, displayed));
        } else if (label.qualifier != nullptr && label.qualifier[0] != '\0') {
            static_cast<void>(std::snprintf(detail_value,
                sizeof(detail_value), u8"%s %+g%s %s", label.name,
                displayed, label.suffix, label.qualifier));
        } else {
            static_cast<void>(std::snprintf(detail_value,
                sizeof(detail_value), u8"%s %+g%s", label.name,
                displayed, label.suffix));
        }
        detail_value[sizeof(detail_value) - 1U] = '\0';
    };

    static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
        u8"%s · %s", ui_item_rarity_name(item->rarity),
        ui_item_base_name(item->base_id)));
    draw_row(u8"装备", detail_value, rarity_color(item->rarity));
    static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
        u8"%u / 需要等级 %u", static_cast<unsigned>(item->item_level),
        static_cast<unsigned>(item->required_level)));
    draw_row(u8"物品等级", detail_value, RAYWHITE);
    static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
        "+%u", static_cast<unsigned>(item->reinforcement)));
    draw_row(u8"强化", detail_value, Color{255, 225, 123, 255});
    for (std::size_t effect_index = 0U;
         effect_index < base->effect_count; ++effect_index) {
        const items::BaseEffect& effect = base->effects[effect_index];
        const ItemAttributeLabel inherent = item_attribute_label(effect.effect,
            effect.stat, effect.operation, base->slot, 0xFFU);
        const std::int32_t inherent_raw =
            effect.values[base_value_index(item->item_level)];
        static_cast<void>(std::snprintf(detail_label, sizeof(detail_label),
            u8"基础 [%s]", inherent.scope));
        format_attribute_value(inherent_raw, inherent);
        draw_row(detail_label, detail_value, Color{150, 210, 255, 255});
    }
    for (std::size_t index = 0U; index < item->affix_count; ++index) {
        const items::AffixRoll& roll = item->affixes[index];
        const items::AffixDefinition* const affix = items::affix_definition(roll.affix_id);
        if (affix == nullptr) continue;
        const ItemAttributeLabel label = item_attribute_label(affix->effect,
            affix->stat, affix->operation, base->slot, roll.variant);
        const std::int32_t affix_raw =
            affix->values[static_cast<std::size_t>(8U - roll.tier)];
        static_cast<void>(std::snprintf(detail_label, sizeof(detail_label),
            u8"T%u %s %s [%s]", static_cast<unsigned>(roll.tier),
            affix->kind == items::AffixKind::prefix ? u8"前缀" : u8"后缀",
            ui_item_affix_name(roll.affix_id), label.scope));
        format_attribute_value(affix_raw, label);
        draw_row(detail_label, detail_value, Color{216, 222, 234, 255});
    }
    draw_heading(u8"全部装备变化", Color{242, 183, 255, 255});
    if (!difference_.has_value()) {
        draw_row(u8"预览", u8"不可用", Color{255, 126, 126, 255});
    } else {
        const BuildDifference& diff = *difference_;
        static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
            "%+g / %+g",
            diff.max_health / static_cast<double>(modifiers::kFixedOne),
            diff.max_barrier / static_cast<double>(modifiers::kFixedOne)));
        draw_row(u8"生命 / 护盾", detail_value, RAYWHITE);
        static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
            "%+g/%+g/%+g/%+g/%+g",
            diff.flat_damage[0] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[1] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[2] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[3] / static_cast<double>(modifiers::kFixedOne),
            diff.flat_damage[4] / static_cast<double>(modifiers::kFixedOne)));
        draw_row(u8"伤害", detail_value, RAYWHITE);
        static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
            "%+g/%+g/%+g/%+g/%+g%%",
            diff.damage_increased[0] / 100.0, diff.damage_increased[1] / 100.0,
            diff.damage_increased[2] / 100.0,
            diff.damage_increased[3] / 100.0,
            diff.damage_increased[4] / 100.0));
        draw_row(u8"伤害提高", detail_value, RAYWHITE);
        static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
            "%+g/%+g%%", diff.melee_damage / 100.0,
            diff.impulse_scale / 100.0));
        draw_row(u8"近战 / 击退", detail_value, RAYWHITE);
        static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
            "%+g/%+g%%", diff.move_speed / 100.0,
            diff.attack_speed / 100.0));
        draw_row(u8"移动 / 攻击", detail_value, RAYWHITE);
        static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
            "%+g%% / %+g",
            diff.local_attack_speed_bp / 100.0,
            static_cast<double>(diff.weapon_physical)));
        draw_row(u8"局部攻击 / 武器物理", detail_value, RAYWHITE);
        static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
            "%+g/%+g%%/%+g/%+g%%",
            static_cast<double>(diff.armor), diff.armor_reduction_bp / 100.0,
            static_cast<double>(diff.evasion),
            diff.evasion_rate_bp / 100.0));
        draw_row(u8"护甲/减伤/闪避/闪避率", detail_value, RAYWHITE);
        static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
            "%+g/%+g/%+g/%+g%%",
            diff.damage_reduction[0] / 100.0,
            diff.damage_reduction[1] / 100.0,
            diff.damage_reduction[2] / 100.0,
            diff.damage_reduction[3] / 100.0));
        draw_row(u8"元素减伤", detail_value, RAYWHITE);
        static_cast<void>(std::snprintf(detail_value, sizeof(detail_value),
            "%+g/%+g/%+g/%+g%%",
            diff.damage_reduction_cap_bonus[0] / 100.0,
            diff.damage_reduction_cap_bonus[1] / 100.0,
            diff.damage_reduction_cap_bonus[2] / 100.0,
            diff.damage_reduction_cap_bonus[3] / 100.0));
        draw_row(u8"元素减伤上限", detail_value, RAYWHITE);
    }
    EndScissorMode();
    material_bag_.draw_reinforcement_confirmation(material_pack,
        hud_font, hud_font_ready, GetScreenWidth(), GetScreenHeight());
}

}  // namespace arpg::platform
