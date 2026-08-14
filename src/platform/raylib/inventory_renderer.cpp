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
#include "ui_utf8.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "direct_input_poison.hpp"

static_assert(arpg::platform::direct_input_poison::active,
    "direct input poison must be active in inventory_renderer.cpp");

namespace arpg::platform {
namespace {

constexpr Color kInventoryTextBacking{5, 9, 16, 255};
constexpr std::uint16_t kMaterialRejectionFrameCount{45U};

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
    Color opaque_backing = kInventoryTextBacking,
    const Rectangle* audit_container = nullptr) noexcept {
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
        const Rectangle container = audit_container != nullptr
            ? *audit_container : interaction_bounds;
        record_ui_text_bounds(audit_page, audit_role, font, layout.text,
            layout.position, layout.font_size, layout.spacing,
            container, minimum_size * scale);
    }
    draw_crisp_ui_text(font, layout.text, layout.position, layout.font_size,
        layout.spacing, color);
}

void draw_centered_shell_text(Font font, bool ready, const char* text,
    Rectangle interaction_bounds, float physical_size,
    float physical_minimum_size, Color color,
    UiTextAuditPage audit_page = UiTextAuditPage::count,
    UiTextAuditRole audit_role = UiTextAuditRole::count,
    Color opaque_backing = kInventoryTextBacking,
    const Rectangle* audit_container = nullptr) noexcept {
    const float viewport_scale = ui_viewport_scale(
        GetScreenWidth(), GetScreenHeight());
    draw_centered_hud_font_text(font, ready, text, interaction_bounds,
        physical_size / viewport_scale,
        physical_minimum_size / viewport_scale, color,
        audit_page, audit_role, opaque_backing, audit_container);
}

Rectangle centered_text_shadow_fit_bounds(Rectangle outer) noexcept {
    constexpr float kSnapMargin = 1.0F;
    const float trailing_margin = kSnapMargin
        + static_cast<float>(ui_text_contrast_style().shadow_pixels);
    return {outer.x + kSnapMargin, outer.y + kSnapMargin,
        std::max(0.0F, outer.width - kSnapMargin - trailing_margin),
        std::max(0.0F, outer.height - kSnapMargin - trailing_margin)};
}

void draw_opaque_skill_text_backing(Rectangle bounds) noexcept {
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) return;
    DrawRectangleRounded(bounds, 0.12F, 4, Color{5, 9, 16, 255});
}

void draw_shell_panel(Rectangle rectangle, const MaterialPack& assets,
    UiMaterialElement element) noexcept {
    DrawRectangleRec(rectangle, kInventoryTextBacking);
    if (!assets.draw_nine_slice(ui_material_sprite(element), rectangle)) {
        DrawRectangleRounded(rectangle, 0.025F, 5, Color{8, 12, 20, 255});
        DrawRectangleRoundedLinesEx(rectangle, 0.025F, 5, 1.0F,
            Color{72, 91, 120, 255});
    }
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
    if (opaque_label_plate) {
        const float scale = ui_viewport_scale(
            GetScreenWidth(), GetScreenHeight());
        draw_opaque_skill_text_backing({rectangle.x + 4.0F * scale,
            rectangle.y + 3.0F * scale,
            rectangle.width - 8.0F * scale,
            rectangle.height - 6.0F * scale});
    }
    draw_centered_hud_font_text(font, font_ready, label, rectangle,
        ui_typography().kInventoryBodyFontSize,
        ui_typography().kInventoryBodyFontSize, text,
        UiTextAuditPage::inventory, UiTextAuditRole::inventory_status, fill);
}

void draw_inventory_page_button(Rectangle rectangle, const char* label,
    bool active, bool material_target, Font font, bool font_ready,
    const MaterialPack& assets) noexcept {
    const InventoryControlTextStyle control = inventory_control_text_style();
    const Color backing = material_target ? Color{48, 35, 13, 255}
        : active ? control.active_backing : control.idle_backing;
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
    if (material_target) {
        DrawRectangleLinesEx(rectangle, 2.0F, Color{218, 172, 72, 255});
    }
    draw_centered_hud_font_text(font, font_ready, label, rectangle,
        ui_typography().inventory_tab_font_size,
        ui_typography().inventory_tab_font_size,
        material_target ? Color{255, 218, 132, 255}
        : active ? ui_text_contrast_style().interaction
               : ui_text_contrast_style().primary,
        UiTextAuditPage::inventory, UiTextAuditRole::inventory_status,
        backing);
}

void draw_active_skill_loadout(const dungeon::DungeonSnapshot& snapshot,
    const ActiveSkillLoadoutLayout& layout,
    const ActiveSkillLoadoutSelection& selection,
    const MaterialPack& assets,
    Font font, bool font_ready) noexcept {
    const float scale = layout.scale;
    const float skill_text_scale = scale / ui_viewport_scale(
        GetScreenWidth(), GetScreenHeight());
    const ActiveSkillLoadoutView view = make_active_skill_loadout_view(
        snapshot.skill_loadout, selection,
        snapshot.pending_save_kind.has_value());

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
            std::max(12.0F, ui_typography().kSkillDescriptionFontSize
                * skill_text_scale), 12.0F,
            slot.empty ? ui_text_contrast_style().muted
                       : Color{210, 241, 255, 255},
            UiTextAuditPage::skill, UiTextAuditRole::skill_description);
    }

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
            inventory_name_backing,
            std::max(12.0F, 18.0F * skill_text_scale), 12.0F,
            ui_text_contrast_style().primary, UiTextAuditPage::skill,
            UiTextAuditRole::skill_inventory_entry);
    }
    if (view.inventory_count == 0U) {
        draw_centered_hud_font_text(font, font_ready,
            ui_text(UiTextId::inventory_skill_empty),
            layout.empty_bounds, 18.0F, 12.0F,
            ui_text_contrast_style().muted, UiTextAuditPage::skill,
            UiTextAuditRole::skill_inventory_entry);
    }
}

bool equipment_equal(const items::EquipmentState& left,
    const items::EquipmentState& right) noexcept {
    return left.equipped_ids == right.equipped_ids;
}

const items::ItemInstance* find_item(const items::ItemOwnershipState& state,
    std::uint64_t item_id) noexcept {
    const auto found = std::find_if(state.items.begin(), state.items.end(),
        [item_id](const items::ItemInstance& item) noexcept {
            return item.id == item_id;
        });
    return found == state.items.end() ? nullptr : &*found;
}

bool same_font_identity(Font left, Font right) noexcept {
    return left.texture.id == right.texture.id
        && left.baseSize == right.baseSize
        && left.glyphCount == right.glyphCount;
}

Color outline_color(InventoryItemOutline outline) noexcept {
    switch (outline) {
    case InventoryItemOutline::hover: return Color{144, 170, 190, 255};
    case InventoryItemOutline::selected: return Color{87, 220, 244, 255};
    case InventoryItemOutline::recipe: return Color{237, 118, 212, 255};
    case InventoryItemOutline::material_target: return Color{218, 172, 72, 255};
    case InventoryItemOutline::disabled_target: return Color{105, 91, 67, 255};
    case InventoryItemOutline::rejected_target: return Color{255, 145, 118, 255};
    case InventoryItemOutline::none: break;
    }
    return BLANK;
}

float outline_thickness(InventoryItemOutline outline) noexcept {
    switch (outline) {
    case InventoryItemOutline::hover: return 1.0F;
    case InventoryItemOutline::selected: return 2.0F;
    case InventoryItemOutline::recipe: return 3.0F;
    case InventoryItemOutline::material_target:
    case InventoryItemOutline::disabled_target: return 2.0F;
    case InventoryItemOutline::rejected_target: return 3.0F;
    case InventoryItemOutline::none: return 0.0F;
    }
    return 0.0F;
}

void draw_inventory_item_overlay(Rectangle bounds,
    const InventoryItemVisualState& visual, float scale,
    Font font, bool font_ready) noexcept {
    const float thickness = outline_thickness(visual.outline);
    if (thickness > 0.0F) {
        DrawRectangleLinesEx(bounds, thickness, outline_color(visual.outline));
    }
    if (visual.selected_indicator
            && visual.outline == InventoryItemOutline::recipe) {
        DrawCircleV({bounds.x + bounds.width - 8.0F * scale,
            bounds.y + 8.0F * scale}, std::max(2.0F, 3.0F * scale),
            Color{87, 220, 244, 255});
    }
    if (visual.recipe_ordinal != 0U) {
        char ordinal[8]{};
        static_cast<void>(std::snprintf(ordinal, sizeof(ordinal), "%u/3",
            static_cast<unsigned>(visual.recipe_ordinal)));
        const Rectangle badge{bounds.x + bounds.width - 40.0F * scale,
            bounds.y + 3.0F * scale, 36.0F * scale, 20.0F * scale};
        DrawRectangleRec(badge, kInventoryTextBacking);
        draw_centered_hud_font_text(font, font_ready, ordinal, badge,
            12.0F, 12.0F, Color{255, 183, 244, 255});
    }
}

struct InventoryMaterialTargetContext final {
    std::optional<items::MaterialId> material{};
    std::optional<items::DirectedCategory> directed_category{};
    std::uint64_t root_seed{};
    std::uint64_t operation_nonce{};
    std::uint64_t rejected_item_id{};
    bool authority_enabled{};
    bool rejection_active{};
};

InventoryItemVisualInput inventory_item_visual_input_with_material(
    const items::ItemOwnershipState& state,
    const items::ItemInstance* item, std::uint64_t item_id,
    std::uint64_t hovered_item_id, std::uint64_t selected_item_id,
    const RecipeSelection& recipe,
    const InventoryMaterialTargetContext& material_target) noexcept {
    InventoryItemVisualInput output{};
    output.item_id = item_id;
    output.hovered_item_id = hovered_item_id;
    output.selected_item_id = selected_item_id;
    output.recipe = recipe;
    output.material_target_mode = material_target.material.has_value();
    output.material_target_enabled = output.material_target_mode
        && item != nullptr && inventory_material_target_enabled(state, *item,
            *material_target.material, material_target.directed_category,
            material_target.root_seed, material_target.operation_nonce,
            material_target.authority_enabled);
    output.material_target_rejected = material_target.rejection_active
        && material_target.rejected_item_id == item_id;
    return output;
}

void draw_character_silhouette(const InventoryShellLayout& shell) noexcept {
    const Rectangle body = shell.silhouette;
    const Color fill{21, 29, 41, 255};
    const Color line{62, 77, 96, 255};
    const float head_radius = std::max(8.0F, body.width * 0.12F);
    DrawCircleV({body.x + body.width * 0.5F,
        body.y + head_radius}, head_radius, fill);
    DrawCircleLines(static_cast<int>(body.x + body.width * 0.5F),
        static_cast<int>(body.y + head_radius), head_radius, line);
    const Rectangle torso{body.x + body.width * 0.33F,
        body.y + head_radius * 2.2F, body.width * 0.34F,
        body.height * 0.48F};
    DrawRectangleRounded(torso, 0.3F, 6, fill);
    DrawRectangleRoundedLinesEx(torso, 0.3F, 6, 1.0F, line);
    DrawLineEx({torso.x, torso.y + torso.height * 0.18F},
        {body.x + body.width * 0.12F, body.y + body.height * 0.58F},
        5.0F, line);
    DrawLineEx({torso.x + torso.width, torso.y + torso.height * 0.18F},
        {body.x + body.width * 0.88F, body.y + body.height * 0.58F},
        5.0F, line);
    DrawLineEx({torso.x + torso.width * 0.42F, torso.y + torso.height},
        {body.x + body.width * 0.32F, body.y + body.height}, 6.0F, line);
    DrawLineEx({torso.x + torso.width * 0.58F, torso.y + torso.height},
        {body.x + body.width * 0.68F, body.y + body.height}, 6.0F, line);
}

void draw_equipment_overview(const items::ItemOwnershipState& state,
    const InventoryShellLayout& shell, const MaterialPack& assets,
    std::uint64_t hovered_item_id, std::uint64_t selected_item_id,
    const RecipeSelection& recipe,
    const InventoryMaterialTargetContext& material_target,
    Font font, bool font_ready) noexcept {
    draw_character_silhouette(shell);
    for (std::size_t index{}; index < shell.equipment_slots.size(); ++index) {
        const auto slot = static_cast<items::ItemSlot>(index);
        const Rectangle bounds = shell.equipment_slots[index];
        const std::uint64_t item_id = state.equipment.equipped_ids[index];
        const items::ItemInstance* const item = find_item(state, item_id);
        const InventoryItemVisualState visual = inventory_item_visual_state(
            inventory_item_visual_input_with_material(state, item, item_id,
                hovered_item_id, selected_item_id, recipe, material_target));
        if (!assets.draw_horizontal_slice(ui_material_sprite(item_id != 0U
                ? UiMaterialElement::inventory_slot_selected
                : UiMaterialElement::inventory_slot_idle),
                {4.0F, 4.0F, 120.0F, 120.0F}, 28.0F, bounds)) {
            DrawRectangleRounded(bounds, 0.08F, 4, Color{21, 28, 40, 255});
            DrawRectangleRoundedLinesEx(bounds, 0.08F, 4, 1.0F,
                Color{75, 100, 133, 255});
        }
        static_cast<void>(assets.draw(ground_loot_item_sprite(slot),
            {bounds.x + bounds.width * 0.5F,
                bounds.y + bounds.height * 0.40F}, false,
            0.19F * shell.geometry_scale,
            item_id == 0U ? Fade(WHITE, 0.30F) : WHITE));
        draw_inventory_item_overlay(bounds, visual, shell.geometry_scale,
            font, font_ready);
        const auto label = inventory_equipment_slot_label(slot, item);
        const Rectangle label_container{bounds.x,
            bounds.y + bounds.height * 0.66F,
            bounds.width, bounds.height * 0.34F};
        const Rectangle label_fit = centered_text_shadow_fit_bounds(
            label_container);
        draw_centered_shell_text(font, font_ready, label.data(), label_fit,
            shell.body_font_size, 12.0F, ui_text_contrast_style().primary,
            UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_equipment_slot,
            kInventoryTextBacking, &label_container);
    }
}

Color tooltip_tone_color(InventoryTooltipTone tone,
    items::ItemRarity rarity) noexcept {
    switch (tone) {
    case InventoryTooltipTone::primary: return ui_text_contrast_style().primary;
    case InventoryTooltipTone::muted: return ui_text_contrast_style().muted;
    case InventoryTooltipTone::positive: return Color{147, 244, 169, 255};
    case InventoryTooltipTone::negative: return Color{255, 145, 118, 255};
    case InventoryTooltipTone::rarity: return rarity_color(rarity);
    }
    return ui_text_contrast_style().primary;
}

void draw_inventory_tooltip(const InventoryTooltipFrame& frame,
    const items::ItemInstance& item, Font font, bool font_ready) noexcept {
    if (!frame.valid || !font_ready || !IsFontValid(font)) return;
    DrawRectangleRec(frame.layout.backing, kInventoryTextBacking);
    DrawRectangleLinesEx(frame.layout.backing, 2.0F, rarity_color(item.rarity));
    draw_crisp_ui_text(font, frame.view.title.data(),
        frame.layout.title_position, frame.layout.title_font_size, 0.0F,
        rarity_color(item.rarity));
    for (std::size_t index{}; index < frame.layout.placement_count; ++index) {
        const InventoryTooltipPlacedSegment& placed =
            frame.layout.placements[index];
        if (placed.logical_line >= frame.view.line_count) continue;
        const InventoryTooltipLine& line = frame.view.lines[placed.logical_line];
        if (placed.segment >= line.segment_count) continue;
        const InventoryTooltipSegment& segment = line.segments[placed.segment];
        draw_crisp_ui_text(font, segment.text.data(), placed.position,
            frame.layout.font_size, 0.0F,
            tooltip_tone_color(segment.tone, item.rarity));
    }
}

Color status_tone_color(InventoryStatusTone tone) noexcept {
    switch (tone) {
    case InventoryStatusTone::normal: return ui_text_contrast_style().primary;
    case InventoryStatusTone::muted: return ui_text_contrast_style().muted;
    case InventoryStatusTone::positive: return Color{147, 244, 169, 255};
    case InventoryStatusTone::warning: return Color{255, 205, 112, 255};
    case InventoryStatusTone::negative: return Color{255, 145, 118, 255};
    }
    return ui_text_contrast_style().primary;
}

void draw_inventory_status(const InventoryShellLayout& shell,
    const InventoryStatusPresentation& presentation,
    const MaterialPack& assets, Font font, bool font_ready) noexcept {
    DrawRectangleRec(shell.status_bar, kInventoryTextBacking);
    DrawRectangleLinesEx(shell.status_bar, 1.0F, Color{72, 91, 120, 255});
    const Color color = status_tone_color(presentation.tone);
    const bool duplicate_wide_hint = shell.hint_visible
        && std::string_view{presentation.status.data()}
            == std::string_view{presentation.hint.data()};
    if (!duplicate_wide_hint) {
        draw_centered_shell_text(font, font_ready, presentation.status.data(),
            shell.status_bounds, shell.body_font_size, 12.0F, color,
            UiTextAuditPage::inventory, UiTextAuditRole::inventory_status);
    }
    if (presentation.action_visible) {
        draw_button(shell.action_bounds, presentation.action.data(),
            presentation.action_enabled, assets, font, font_ready,
            presentation.action_enabled, true);
    }
    if (shell.hint_visible) {
        const Color hint_color = duplicate_wide_hint
            ? color : ui_text_contrast_style().muted;
        draw_centered_shell_text(font, font_ready, presentation.hint.data(),
            shell.hint_bounds, shell.hint_font_size, 12.0F,
            hint_color, UiTextAuditPage::inventory,
            UiTextAuditRole::inventory_status);
    }
}

struct InventoryDirectItemHit final {
    const items::ItemInstance* item{};
    InventoryVisibleItemSource source{};
    std::optional<items::ItemSlot> equipped_slot{};
    bool grid_item{};
};

InventoryDirectItemHit inventory_direct_item_hit(
    const InventoryShellLayout& shell,
    const items::ItemOwnershipState& state,
    const InventoryViewCache& cache, float scroll_rows,
    Vector2 point) noexcept {
    for (std::size_t index{}; index < shell.equipment_slots.size(); ++index) {
        const std::uint64_t item_id = state.equipment.equipped_ids[index];
        if (item_id == 0U || !contains(shell.equipment_slots[index], point))
            continue;
        const items::ItemInstance* const item = find_item(state, item_id);
        if (item == nullptr) return {};
        return {item, {item_id, shell.equipment_slots[index], false, true},
            static_cast<items::ItemSlot>(index), false};
    }
    if (!contains(shell.grid_viewport, point)) return {};
    const VisibleGridRange visible = visible_grid_range(
        cache.filtered_indices.size(), inventory_grid_columns(shell),
        scroll_rows, shell.grid_viewport.height, shell.grid_row_stride);
    for (std::size_t offset{}; offset < visible.count; ++offset) {
        const std::size_t position = visible.first + offset;
        const Rectangle bounds = inventory_grid_item_rectangle(
            shell, position, scroll_rows);
        if (!contains(bounds, point)) continue;
        const std::size_t item_index = cache.filtered_indices[position];
        if (item_index >= state.items.size()) return {};
        const items::ItemInstance* const item = &state.items[item_index];
        return {item, {item->id, bounds, true, true}, std::nullopt, true};
    }
    return {};
}

bool material_card_contains(const MaterialBagLayout& layout,
    Vector2 point) noexcept {
    return std::any_of(layout.slots.begin(), layout.slots.end(),
        [point](Rectangle bounds) noexcept { return contains(bounds, point); });
}

bool active_skill_content_contains(const ActiveSkillLoadoutLayout& layout,
    Vector2 point) noexcept {
    return std::any_of(layout.main_slots.begin(), layout.main_slots.end(),
            [point](Rectangle bounds) noexcept { return contains(bounds, point); })
        || std::any_of(layout.inventory_slots.begin(),
            layout.inventory_slots.end(),
            [point](Rectangle bounds) noexcept { return contains(bounds, point); });
}

InventorySavePresentationState inventory_save_state(
    SaveIndicator indicator) noexcept {
    switch (indicator) {
    case SaveIndicator::saving: return InventorySavePresentationState::saving;
    case SaveIndicator::saved: return InventorySavePresentationState::saved;
    case SaveIndicator::recovered:
        return InventorySavePresentationState::recovered;
    case SaveIndicator::error: return InventorySavePresentationState::error;
    case SaveIndicator::none: return InventorySavePresentationState::none;
    }
    return InventorySavePresentationState::none;
}

}  // namespace

void InventoryRenderer::open(const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot) {
    open_ = true;
    page_ = InventoryPage::equipment;
    active_skill_selection_ = {};
    scroll_rows_ = 0.0F;
    selected_item_id_ = 0U;
    recipe_ = {};
    click_tracker_ = {};
    view_cache_ = {};
    frame_layout_ = {};
    tooltip_state_ = {};
    transient_status_state_ = {};
    clear_material_rejection();
    tooltip_measure_font_ = {};
    tooltip_measure_font_ready_ = false;
    comparison_generation_ = ~std::uint64_t{0U};
    comparison_item_id_ = 0U;
    comparison_equipment_ = {};
    current_build_.reset();
    difference_.reset();
    sync(session, snapshot);
}

void InventoryRenderer::close() noexcept {
    open_ = false;
    page_ = InventoryPage::equipment;
    active_skill_selection_ = {};
    scroll_rows_ = 0.0F;
    selected_item_id_ = 0U;
    recipe_ = {};
    click_tracker_ = {};
    view_cache_ = {};
    frame_layout_ = {};
    tooltip_state_ = {};
    transient_status_state_ = {};
    clear_material_rejection();
    tooltip_measure_font_ = {};
    tooltip_measure_font_ready_ = false;
    comparison_generation_ = ~std::uint64_t{0U};
    comparison_item_id_ = 0U;
    comparison_equipment_ = {};
    current_build_.reset();
    difference_.reset();
    static_cast<void>(material_bag_.clear_selection());
    static_cast<void>(material_bag_.resolve_reinforcement_confirmation(false));
}

void InventoryRenderer::show_skill_stones_page() noexcept {
    if (!open_) return;
    page_ = InventoryPage::skill_stones;
    tooltip_state_ = {};
    clear_material_rejection();
    if (active_skill_selection_.selected_slot
            == kNoActiveSkillLoadoutSelection) {
        active_skill_selection_.selected_slot = 0U;
    }
}

bool InventoryRenderer::is_open() const noexcept { return open_; }

void InventoryRenderer::clear_material_rejection() noexcept {
    rejected_material_item_id_ = 0U;
    material_rejection_frames_ = 0U;
}

void InventoryRenderer::show_material_rejection(
    std::uint64_t item_id) noexcept {
    rejected_material_item_id_ = item_id;
    material_rejection_frames_ = kMaterialRejectionFrameCount;
}

void InventoryRenderer::refresh_frame_layout() noexcept {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    if (frame_layout_.valid && frame_layout_.width == width
            && frame_layout_.height == height) {
        return;
    }
    InventoryFrameLayout next{};
    next.width = width;
    next.height = height;
    next.shell = inventory_shell_layout(width, height);
    if (next.shell.valid) {
        next.skills = active_skill_loadout_layout(next.shell);
        next.materials = material_bag_layout(next.shell.right_panel,
            next.shell.geometry_scale);
        next.valid = next.skills.panel.width > 0.0F
            && next.skills.panel.height > 0.0F
            && next.materials.contains_all_slots();
    }
    tooltip_state_ = {};
    tooltip_measure_font_ = {};
    tooltip_measure_font_ready_ = false;
    frame_layout_ = next;
}

void InventoryRenderer::refresh_comparison(
    const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    std::uint64_t presentation_item_id) {
    const items::ItemOwnershipState& state = session.item_state();
    if (comparison_generation_ == snapshot.commit_generation
        && comparison_item_id_ == presentation_item_id
        && equipment_equal(comparison_equipment_, state.equipment)) return;
    comparison_generation_ = snapshot.commit_generation;
    comparison_item_id_ = presentation_item_id;
    comparison_equipment_ = state.equipment;
    current_build_ = session.preview_equipment_build(state.equipment);
    difference_.reset();
    const items::ItemInstance* const item = find_item(state,
        presentation_item_id);
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

InventoryTooltipFrame InventoryRenderer::build_tooltip_frame(
    const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    std::uint64_t presentation_item_id,
    Rectangle source, bool source_is_in_right_panel,
    Font measure_font, bool measure_font_ready) {
    InventoryTooltipFrame frame{};
    refresh_comparison(session, snapshot, presentation_item_id);
    if (!frame_layout_.valid || presentation_item_id == 0U
            || !measure_font_ready || !IsFontValid(measure_font)) {
        return frame;
    }
    const items::ItemOwnershipState& state = session.item_state();
    const items::ItemInstance* const item = find_item(state,
        presentation_item_id);
    if (item == nullptr || comparison_item_id_ != presentation_item_id)
        return frame;
    frame.item_id = presentation_item_id;
    frame.view = inventory_tooltip_view(*item, difference_);
    frame.layout = inventory_tooltip_layout(frame_layout_.shell, source,
        source_is_in_right_panel, frame.view, &measure_inventory_text,
        &measure_font);
    frame.valid = frame.view.valid && frame.layout.valid;
    return frame;
}

void InventoryRenderer::sync(const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot) {
    const items::ItemOwnershipState& state = session.item_state();
    material_bag_.sync_selection(state);
    refresh_inventory_view_cache(view_cache_, state,
        snapshot.commit_generation, filter_, selected_item_id_, recipe_);
    if (selected_item_id_ != 0U
            && find_item(state, selected_item_id_) == nullptr) {
        selected_item_id_ = 0U;
        view_cache_.selected_item_id = 0U;
    }
    if (recipe_.ids != view_cache_.resolved_recipe.ids
        || recipe_.count != view_cache_.resolved_recipe.count) {
        recipe_ = view_cache_.resolved_recipe;
        view_cache_.requested_recipe = recipe_;
    }
}

bool InventoryRenderer::recipe_ready() const noexcept {
    return cached_recipe_ready(view_cache_);
}

bool InventoryRenderer::process_input(DungeonRuntime& runtime,
    const dungeon::DungeonSnapshot& snapshot,
    const HostFrameInput& input) {
    if (!open_ || runtime.session() == nullptr) return false;
    refresh_frame_layout();
    if (!frame_layout_.valid) return false;
    dungeon::DungeonSession& session = *runtime.session();
    sync(session, snapshot);
    const items::ItemOwnershipState& state = session.item_state();
    const bool left_pressed = input.mouse_left_pressed;
    const bool right_pressed = input.mouse_right_pressed;
    const Vector2 mouse = input.mouse_position;
    const bool requests_enabled = !snapshot.pending_save_kind.has_value()
        && runtime.authority_requests_enabled();

    const InventoryVisibleItemSource frozen_source =
        inventory_visible_item_source(frame_layout_.shell, page_, state,
            view_cache_, tooltip_state_.frozen_item_id, scroll_rows_);
    const InventoryVisibleItemSource selected_source =
        inventory_visible_item_source(frame_layout_.shell, page_, state,
            view_cache_, selected_item_id_, scroll_rows_);
    const bool frozen_invalid = tooltip_state_.frozen_item_id != 0U
        && !frozen_source.valid;
    const bool selected_invalid = tooltip_state_.frozen_item_id == 0U
        && selected_item_id_ != 0U && !selected_source.valid;
    if (frozen_invalid || selected_invalid) {
        InventoryTooltipInteractionInput invalid{};
        invalid.pointer = mouse;
        invalid.source_invalidated = true;
        tooltip_state_ = inventory_tooltip_transition(
            tooltip_state_, invalid).state;
    }

    const InventoryVisibleItemSource retained_source = frozen_source.valid
        ? frozen_source : selected_source;
    InventoryTooltipFrame retained_frame{};
    if (retained_source.valid && tooltip_measure_font_ready_) {
        retained_frame = build_tooltip_frame(session, snapshot,
            retained_source.item_id, retained_source.bounds,
            retained_source.source_is_in_right_panel,
            tooltip_measure_font_, tooltip_measure_font_ready_);
    }
    if (retained_frame.valid) {
        tooltip_state_ = inventory_tooltip_state_with_retained_source(
            tooltip_state_, retained_source,
            retained_frame.layout.retention_corridor);
    }
    const bool pointer_inside_retained_tooltip = retained_frame.valid
        && contains(retained_frame.layout.backing, mouse);
    const bool retained_backing_blocks = tooltip_state_.visible
        && pointer_inside_retained_tooltip;

    InventoryDirectItemHit direct_hit{};
    if (!retained_backing_blocks && page_ == InventoryPage::equipment) {
        direct_hit = inventory_direct_item_hit(frame_layout_.shell, state,
            view_cache_, scroll_rows_, mouse);
    }
    InventoryVisibleItemSource transition_source{};
    if (direct_hit.source.valid) {
        transition_source = direct_hit.source;
    } else if (selected_source.valid
            && (tooltip_state_.frozen_item_id == 0U
                || tooltip_state_.frozen_item_id == selected_source.item_id)) {
        transition_source = selected_source;
    }
    InventoryTooltipFrame transition_frame{};
    if (!retained_backing_blocks && transition_source.valid
            && tooltip_measure_font_ready_) {
        transition_frame = build_tooltip_frame(session, snapshot,
            transition_source.item_id, transition_source.bounds,
            transition_source.source_is_in_right_panel,
            tooltip_measure_font_, tooltip_measure_font_ready_);
    }
    if (!tooltip_measure_font_ready_) {
        tooltip_state_ = {};
    } else {
        InventoryTooltipInteractionInput transition{};
        if (!retained_backing_blocks && transition_frame.valid) {
            transition.hover_candidate_item_id = transition_frame.item_id;
            transition.hover_candidate_source = transition_source.bounds;
            transition.hover_candidate_corridor =
                transition_frame.layout.retention_corridor;
            transition.hover_candidate_source_is_in_right_panel =
                transition_source.source_is_in_right_panel;
        }
        transition.pointer = mouse;
        transition.pointer_inside_tooltip =
            pointer_inside_retained_tooltip;
        tooltip_state_ = inventory_tooltip_transition(
            tooltip_state_, transition).state;
    }

    const bool confirmation_open = material_bag_
        .reinforcement_confirmation_item().has_value();
    scroll_rows_ = inventory_grid_scroll_after_wheel(page_,
        frame_layout_.shell, mouse,
        confirmation_open || retained_backing_blocks,
        input.mouse_wheel, view_cache_.filtered_indices.size(), scroll_rows_);
    if (!left_pressed && !right_pressed) return false;
    clear_material_rejection();

    std::size_t tab_index = kInventoryPageCount;
    if (left_pressed) {
        for (std::size_t index{}; index < frame_layout_.shell.tabs.size();
             ++index) {
            if (contains(frame_layout_.shell.tabs[index], mouse)) {
                tab_index = index;
                break;
            }
        }
    }
    const auto selected_material = material_bag_.selected_material();
    const std::size_t directed_index = items::material_index(
        items::MaterialId::directed);
    const bool directed_material = right_pressed
        && page_ == InventoryPage::materials
        && selected_material == items::MaterialId::directed
        && directed_index < frame_layout_.materials.slots.size()
        && contains(frame_layout_.materials.slots[directed_index], mouse);
    InventoryPointerPriorityInput priority{};
    priority.page = page_;
    priority.confirmation = confirmation_open;
    priority.tab = tab_index < kInventoryPageCount;
    priority.tooltip = retained_backing_blocks;
    priority.directed_material = directed_material;
    priority.material_cancel_requested = right_pressed
        && selected_material.has_value() && !directed_material;
    priority.material_select = left_pressed
        && page_ == InventoryPage::materials
        && material_card_contains(frame_layout_.materials, mouse);
    priority.material_target = left_pressed
        && page_ == InventoryPage::equipment
        && selected_material.has_value() && direct_hit.item != nullptr;
    priority.material_selected = selected_material.has_value();
    priority.page_action = left_pressed
        && inventory_page_action_contains(frame_layout_.shell, page_, mouse);
    priority.filter = left_pressed && page_ == InventoryPage::equipment
        && (contains(frame_layout_.shell.slot_filter_bounds, mouse)
            || contains(frame_layout_.shell.rarity_filter_bounds, mouse));
    priority.skill_content = left_pressed
        && page_ == InventoryPage::skill_stones
        && active_skill_content_contains(frame_layout_.skills, mouse);
    priority.recipe = page_ == InventoryPage::equipment
        && direct_hit.grid_item && !selected_material.has_value()
        && (right_pressed || (left_pressed && input.control_down));
    priority.equipment = left_pressed
        && page_ == InventoryPage::equipment && direct_hit.item != nullptr;
    priority.background_clear = left_pressed;

    switch (inventory_pointer_layer(priority)) {
    case InventoryPointerLayer::confirmation: {
        const ReinforcementConfirmationLayout confirmation =
            reinforcement_confirmation_layout(frame_layout_.width,
                frame_layout_.height);
        if (right_pressed || (left_pressed
                && contains(confirmation.cancel, mouse))) {
            static_cast<void>(material_bag_
                .resolve_reinforcement_confirmation(false));
            return false;
        }
        if (left_pressed && contains(confirmation.confirm, mouse)) {
            const auto item = material_bag_
                .resolve_reinforcement_confirmation(true);
            if (requests_enabled && item.has_value()
                    && runtime.request_reinforcement(*item)
                        == dungeon::RequestResult::accepted) {
                runtime.service_pending_save();
                return true;
            }
        }
        return false;
    }
    case InventoryPointerLayer::tab:
        page_ = static_cast<InventoryPage>(tab_index);
        tooltip_state_ = {};
        if (page_ == InventoryPage::skill_stones
                && active_skill_selection_.selected_slot
                    == kNoActiveSkillLoadoutSelection
                && active_skill_selection_.selected_inventory
                    == skills::ActiveSkillId::none) {
            active_skill_selection_.selected_slot = 0U;
        }
        return false;
    case InventoryPointerLayer::tooltip:
        if (right_pressed && selected_material.has_value()) {
            static_cast<void>(material_bag_.clear_selection());
            return false;
        } else {
            InventoryTooltipInteractionInput close{};
            close.pointer = mouse;
            close.pointer_inside_tooltip = pointer_inside_retained_tooltip;
            close.close_requested = true;
            close.clear_persistent_selection_requested = left_pressed;
            const InventoryTooltipInteractionResult closed =
                inventory_tooltip_transition(tooltip_state_, close);
            tooltip_state_ = closed.state;
            if (closed.clear_persistent_selection) selected_item_id_ = 0U;
            return false;
        }
    case InventoryPointerLayer::directed_material:
        static_cast<void>(material_bag_.cycle_directed_category(
            mouse, frame_layout_.materials));
        return false;
    case InventoryPointerLayer::material_cancel:
        static_cast<void>(material_bag_.clear_selection());
        return false;
    case InventoryPointerLayer::material_select:
        static_cast<void>(material_bag_.process_click(mouse, state,
            frame_layout_.materials));
        return false;
    case InventoryPointerLayer::material_target: {
        const items::ItemInstance& item = *direct_hit.item;
        const auto category = *selected_material
                == items::MaterialId::directed
            ? std::optional<items::DirectedCategory>{
                material_bag_.directed_category()}
            : std::nullopt;
        if (!inventory_material_target_enabled(state, item,
                *selected_material, category, snapshot.root_seed,
                snapshot.commit_generation, requests_enabled)) {
            show_material_rejection(item.id);
            return false;
        }
        dungeon::RequestResult request = dungeon::RequestResult::rejected;
        if (*selected_material == items::MaterialId::reinforcement_stone) {
            if (item.reinforcement >= 12U) {
                static_cast<void>(material_bag_
                    .begin_reinforcement_confirmation(item.id,
                        item.reinforcement));
                return false;
            }
            request = runtime.request_reinforcement(item.id);
        } else if (items::material_is_coupon(*selected_material)) {
            request = runtime.request_coupon(*selected_material, item.id);
        } else {
            request = runtime.request_craft(*selected_material, item.id,
                category);
        }
        if (request == dungeon::RequestResult::accepted) {
            selected_item_id_ = item.id;
            runtime.service_pending_save();
            return true;
        }
        show_material_rejection(item.id);
        return false;
    }
    case InventoryPointerLayer::page_action:
        if (page_ == InventoryPage::equipment) {
            if (requests_enabled && !selected_material.has_value() && recipe_ready()
                    && runtime.request_recipe(recipe_.ids)
                        == dungeon::RequestResult::accepted) {
                runtime.service_pending_save();
                return true;
            }
            return false;
        }
        [[fallthrough]];
    case InventoryPointerLayer::skill_content: {
        const auto command = active_skill_loadout_command_after_click(
            snapshot.skill_loadout, frame_layout_.skills, mouse,
            active_skill_selection_, snapshot.pending_save_kind.has_value());
        if (!command.has_value()
                || command->kind == ActiveSkillLoadoutActionKind::select) {
            return false;
        }
        if (!requests_enabled) return false;
        dungeon::RequestResult request = dungeon::RequestResult::rejected;
        switch (command->kind) {
        case ActiveSkillLoadoutActionKind::select: return false;
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
        advance_active_skill_loadout_selection(
            active_skill_selection_, *command);
        runtime.service_pending_save();
        return true;
    }
    case InventoryPointerLayer::filter:
        if (contains(frame_layout_.shell.slot_filter_bounds, mouse)) {
            if (!filter_.slot.has_value()) filter_.slot = items::ItemSlot::weapon;
            else if (*filter_.slot == items::ItemSlot::accessory)
                filter_.slot.reset();
            else filter_.slot = static_cast<items::ItemSlot>(
                static_cast<std::uint8_t>(*filter_.slot) + 1U);
        } else {
            if (!filter_.rarity.has_value())
                filter_.rarity = items::ItemRarity::normal;
            else if (*filter_.rarity == items::ItemRarity::rare)
                filter_.rarity.reset();
            else filter_.rarity = static_cast<items::ItemRarity>(
                static_cast<std::uint8_t>(*filter_.rarity) + 1U);
        }
        sync(session, snapshot);
        scroll_rows_ = 0.0F;
        return false;
    case InventoryPointerLayer::recipe:
        selected_item_id_ = direct_hit.item->id;
        static_cast<void>(toggle_recipe_selection(recipe_,
            direct_hit.item->id));
        sync(session, snapshot);
        click_tracker_ = {};
        return false;
    case InventoryPointerLayer::equipment:
        if (direct_hit.equipped_slot.has_value()) {
            if (requests_enabled
                    && runtime.request_unequip(*direct_hit.equipped_slot)
                        == dungeon::RequestResult::accepted) {
                runtime.service_pending_save();
                return true;
            }
            return false;
        }
        selected_item_id_ = direct_hit.item->id;
        sync(session, snapshot);
        if (register_inventory_click(click_tracker_, direct_hit.item->id,
                GetTime()) == InventoryClickKind::double_click
                && requests_enabled
                && runtime.request_equip(direct_hit.item->id)
                    == dungeon::RequestResult::accepted) {
            runtime.service_pending_save();
            return true;
        }
        return false;
    case InventoryPointerLayer::background_clear: {
        InventoryTooltipInteractionInput clear{};
        clear.pointer = mouse;
        clear.background_clear_requested = true;
        const InventoryTooltipInteractionResult cleared =
            inventory_tooltip_transition(tooltip_state_, clear);
        tooltip_state_ = cleared.state;
        if (cleared.clear_persistent_selection) selected_item_id_ = 0U;
        return false;
    }
    case InventoryPointerLayer::none:
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
    if (!open_) return;
    refresh_frame_layout();
    if (!frame_layout_.valid) return;

    const bool current_font_ready = hud_font_ready && IsFontValid(hud_font);
    bool font_identity_changed{};
    if (!current_font_ready) {
        tooltip_state_ = {};
        tooltip_measure_font_ = {};
        tooltip_measure_font_ready_ = false;
    } else if (!tooltip_measure_font_ready_
            || !same_font_identity(tooltip_measure_font_, hud_font)) {
        tooltip_state_ = {};
        tooltip_measure_font_ = hud_font;
        tooltip_measure_font_ready_ = true;
        font_identity_changed = true;
    } else {
        tooltip_measure_font_ = hud_font;
    }

    sync(session, snapshot);
    const items::ItemOwnershipState& state = session.item_state();
    const InventoryShellLayout& shell = frame_layout_.shell;
    const auto selected_material = material_bag_.selected_material();
    DrawRectangle(0, 0, frame_layout_.width, frame_layout_.height,
        Color{3, 5, 9, 255});

    InventoryTransientStatusInput transient_input{};
    transient_input.commit_generation = snapshot.commit_generation;
    transient_input.save_state = inventory_save_state(status.indicator);
    transient_input.receipt_valid = snapshot.reinforcement_receipt.valid;
    transient_input.receipt_success = snapshot.reinforcement_receipt.success;
    transient_input.receipt_destroyed =
        snapshot.reinforcement_receipt.destroyed;
    transient_input.receipt_coupon = snapshot.reinforcement_receipt.coupon;
    transient_input.receipt_commit_generation =
        snapshot.reinforcement_receipt.commit_generation;
    transient_input.receipt_before = snapshot.reinforcement_receipt.before;
    transient_input.receipt_after = snapshot.reinforcement_receipt.after;
    const InventoryTransientStatusFrameResult transient_result =
        inventory_transient_status_for_frame(transient_status_state_,
            transient_input);
    transient_status_state_ = transient_result.next;

    bool reinforced_recipe_input{};
    for (std::size_t recipe_index{}; recipe_index < recipe_.count;
         ++recipe_index) {
        const items::ItemInstance* const recipe_item =
            find_item(state, recipe_.ids[recipe_index]);
        if (recipe_item != nullptr && recipe_item->reinforcement != 0U) {
            reinforced_recipe_input = true;
            break;
        }
    }
    const bool authority_enabled = !snapshot.pending_save_kind.has_value()
        && !status.recovery_required && !status.faulted;
    InventoryMaterialTargetContext material_target{};
    if (page_ == InventoryPage::equipment) {
        material_target.material = selected_material;
        if (selected_material == items::MaterialId::directed) {
            material_target.directed_category = material_bag_.directed_category();
        }
    }
    material_target.root_seed = snapshot.root_seed;
    material_target.operation_nonce = snapshot.commit_generation;
    material_target.authority_enabled = authority_enabled;
    material_target.rejection_active = material_rejection_frames_ != 0U;
    material_target.rejected_item_id = rejected_material_item_id_;
    const bool active_skill_removable =
        active_skill_selection_.selected_slot
            < snapshot.skill_loadout.slots.size()
        && snapshot.skill_loadout.slots[
            active_skill_selection_.selected_slot].active
                != skills::ActiveSkillId::none;
    InventoryStatusInput status_input{};
    status_input.page = page_;
    status_input.save_state = inventory_save_state(status.indicator);
    status_input.recovery_required = status.recovery_required;
    status_input.transient = transient_result.presentation;
    status_input.selected_material = selected_material;
    status_input.material_target_rejected =
        material_rejection_frames_ != 0U
        && rejected_material_item_id_ != 0U;
    status_input.recipe_count = recipe_.count;
    status_input.recipe_ready = recipe_ready();
    status_input.reinforced_recipe_input = reinforced_recipe_input;
    status_input.active_skill_removable = active_skill_removable;
    status_input.authority_enabled = authority_enabled;
    const InventoryStatusPresentation status_presentation =
        inventory_status_presentation(status_input);

    DrawRectangleRec(shell.header, kInventoryTextBacking);
    DrawRectangleLinesEx(shell.header, 1.0F, Color{72, 91, 120, 255});
    const Rectangle header_title_bounds{shell.header.x + 8.0F,
        shell.header.y + 4.0F,
        std::max(40.0F, shell.tabs[0U].x - shell.header.x - 16.0F),
        shell.header.height - 8.0F};
    static_cast<void>(material_pack.draw_region_fit(
        ui_material_sprite(UiMaterialElement::label_plate),
        {4.0F, 30.0F, 120.0F, 67.0F}, header_title_bounds));
    const auto page_title = inventory_page_title(page_,
        inventory_binding_label);
    draw_centered_shell_text(hud_font, current_font_ready,
        page_title.data(), header_title_bounds,
        std::max(16.0F, shell.body_font_size + 4.0F), 12.0F,
        ui_text_contrast_style().primary,
        page_ == InventoryPage::skill_stones
            ? UiTextAuditPage::skill : UiTextAuditPage::inventory,
        page_ == InventoryPage::skill_stones
            ? UiTextAuditRole::skill_page_title
            : UiTextAuditRole::inventory_page_title);
    const std::array<const char*, kInventoryPageCount> tab_labels{{
        ui_text(UiTextId::equipment_title),
        ui_text(UiTextId::inventory_page_skill_stones),
        ui_text(UiTextId::inventory_page_materials),
    }};
    for (std::size_t index{}; index < shell.tabs.size(); ++index) {
        const bool material_target_tab = index
                == static_cast<std::size_t>(InventoryPage::equipment)
            && selected_material.has_value();
        draw_inventory_page_button(shell.tabs[index], tab_labels[index],
            page_ == static_cast<InventoryPage>(index), material_target_tab,
            hud_font,
            current_font_ready, material_pack);
    }

    draw_shell_panel(shell.left_panel, material_pack,
        page_ == InventoryPage::skill_stones
            ? UiMaterialElement::skill_panel
            : UiMaterialElement::inventory_panel_equipment);
    if (page_ != InventoryPage::materials) {
        draw_shell_panel(shell.right_panel, material_pack,
            page_ == InventoryPage::skill_stones
                ? UiMaterialElement::skill_panel
                : UiMaterialElement::inventory_panel_grid);
    }

    const std::uint64_t hovered_item_id =
        tooltip_state_.frozen_item_id != 0U
        ? tooltip_state_.frozen_item_id : tooltip_state_.hovered_item_id;
    if (page_ == InventoryPage::equipment
            || page_ == InventoryPage::materials) {
        draw_equipment_overview(state, shell, material_pack, hovered_item_id,
            selected_item_id_, recipe_, material_target, hud_font,
            current_font_ready);
    }

    if (page_ == InventoryPage::equipment) {
        std::array<char, 64> slot_filter_text{};
        std::array<char, 64> rarity_filter_text{};
        static_cast<void>(ui_utf8_format(slot_filter_text.data(),
            slot_filter_text.size(), ui_text(UiTextId::inventory_slot_filter_format),
            filter_.slot.has_value() ? ui_item_slot_name(*filter_.slot)
                                     : ui_text(UiTextId::inventory_filter_all)));
        static_cast<void>(ui_utf8_format(rarity_filter_text.data(),
            rarity_filter_text.size(),
            ui_text(UiTextId::inventory_rarity_filter_format),
            filter_.rarity.has_value() ? ui_item_rarity_name(*filter_.rarity)
                                       : ui_text(UiTextId::inventory_filter_all)));
        draw_button(shell.slot_filter_bounds, slot_filter_text.data(), true,
            material_pack, hud_font, current_font_ready);
        draw_button(shell.rarity_filter_bounds, rarity_filter_text.data(), true,
            material_pack, hud_font, current_font_ready);

        DrawRectangleRec(shell.grid_viewport, Color{11, 16, 25, 255});
        scroll_rows_ = clamp_inventory_scroll_rows(
            view_cache_.filtered_indices.size(),
            inventory_grid_columns(shell), scroll_rows_,
            shell.grid_viewport.height, shell.grid_row_stride);
        const VisibleGridRange visible = visible_grid_range(
            view_cache_.filtered_indices.size(),
            inventory_grid_columns(shell), scroll_rows_,
            shell.grid_viewport.height, shell.grid_row_stride);
        BeginScissorMode(static_cast<int>(shell.grid_viewport.x),
            static_cast<int>(shell.grid_viewport.y),
            static_cast<int>(shell.grid_viewport.width),
            static_cast<int>(shell.grid_viewport.height));
        for (std::size_t offset{}; offset < visible.count; ++offset) {
            const std::size_t position = visible.first + offset;
            const std::size_t item_index =
                view_cache_.filtered_indices[position];
            if (item_index >= state.items.size()) continue;
            const items::ItemInstance& item = state.items[item_index];
            const items::BaseDefinition* const base =
                items::base_definition(item.base_id);
            const Rectangle cell = inventory_grid_item_rectangle(
                shell, position, scroll_rows_);
            const InventoryItemVisualState visual =
                inventory_item_visual_state(
                    inventory_item_visual_input_with_material(state, &item,
                        item.id, hovered_item_id, selected_item_id_, recipe_,
                        material_target));
            if (!material_pack.draw_horizontal_slice(ui_material_sprite(
                    visual.outline == InventoryItemOutline::selected
                        ? UiMaterialElement::inventory_slot_selected
                        : UiMaterialElement::inventory_slot_idle),
                    {4.0F, 4.0F, 120.0F, 120.0F}, 28.0F, cell)) {
                DrawRectangleRounded(cell, 0.08F, 4,
                    Color{21, 28, 39, 255});
                DrawRectangleRoundedLinesEx(cell, 0.08F, 4, 1.0F,
                    rarity_color(item.rarity));
            }
            if (base != nullptr) {
                static_cast<void>(material_pack.draw(
                    ground_loot_item_sprite(base->slot),
                    {cell.x + 22.0F * shell.geometry_scale,
                        cell.y + cell.height * 0.5F},
                    false, 0.20F * shell.geometry_scale));
            }
            draw_inventory_item_overlay(cell, visual, shell.geometry_scale,
                hud_font, current_font_ready);
            const Rectangle name_bounds{cell.x + 42.0F * shell.geometry_scale,
                cell.y + 3.0F, cell.width - 46.0F * shell.geometry_scale,
                cell.height * 0.46F};
            draw_centered_shell_text(hud_font, current_font_ready,
                ui_item_base_name(item.base_id), name_bounds,
                shell.body_font_size, 12.0F, rarity_color(item.rarity),
                UiTextAuditPage::inventory,
                UiTextAuditRole::inventory_grid_entry);
            const auto item_meta = inventory_grid_item_metadata(item);
            const Rectangle meta_bounds{name_bounds.x,
                cell.y + cell.height * 0.50F, name_bounds.width,
                cell.height * 0.44F};
            draw_centered_shell_text(hud_font, current_font_ready,
                item_meta.data(), meta_bounds, shell.body_font_size, 12.0F,
                ui_text_contrast_style().muted,
                UiTextAuditPage::inventory,
                UiTextAuditRole::inventory_grid_entry);
        }
        EndScissorMode();

        const InventoryEmptyPresentation empty =
            inventory_empty_presentation(state,
                view_cache_.filtered_indices.size());
        if (empty.state != InventoryEmptyState::none) {
            const Rectangle title_bounds{shell.empty_state_bounds.x,
                shell.empty_state_bounds.y
                    + shell.empty_state_bounds.height * 0.38F,
                shell.empty_state_bounds.width,
                shell.empty_state_bounds.height * 0.10F};
            const Rectangle hint_bounds{shell.empty_state_bounds.x,
                shell.empty_state_bounds.y
                    + shell.empty_state_bounds.height * 0.50F,
                shell.empty_state_bounds.width,
                shell.empty_state_bounds.height * 0.10F};
            draw_centered_shell_text(hud_font, current_font_ready,
                empty.title, title_bounds,
                std::max(14.0F, shell.body_font_size + 2.0F), 12.0F,
                ui_text_contrast_style().primary,
                UiTextAuditPage::inventory,
                UiTextAuditRole::inventory_status);
            draw_centered_shell_text(hud_font, current_font_ready,
                empty.hint, hint_bounds, shell.hint_font_size, 12.0F,
                ui_text_contrast_style().muted,
                UiTextAuditPage::inventory,
                UiTextAuditRole::inventory_status);
        }
    } else if (page_ == InventoryPage::skill_stones) {
        draw_active_skill_loadout(snapshot, frame_layout_.skills,
            active_skill_selection_, material_pack, hud_font,
            current_font_ready);
    } else if (page_ == InventoryPage::materials) {
        material_bag_.draw(state, material_pack, hud_font,
            current_font_ready, frame_layout_.materials,
            shell.geometry_scale);
    }

    if (!font_identity_changed && current_font_ready
            && tooltip_state_.visible) {
        const InventoryVisibleItemSource frozen_source =
            inventory_visible_item_source(shell, page_, state, view_cache_,
                tooltip_state_.frozen_item_id, scroll_rows_);
        const InventoryVisibleItemSource selected_source =
            inventory_visible_item_source(shell, page_, state, view_cache_,
                selected_item_id_, scroll_rows_);
        const InventoryVisibleItemSource source = frozen_source.valid
            ? frozen_source : selected_source;
        if (!source.valid) {
            InventoryTooltipInteractionInput invalid{};
            invalid.source_invalidated = true;
            tooltip_state_ = inventory_tooltip_transition(
                tooltip_state_, invalid).state;
        } else if (tooltip_state_.suppressed_item_id != source.item_id) {
            const InventoryTooltipFrame tooltip = build_tooltip_frame(
                session, snapshot, source.item_id, source.bounds,
                source.source_is_in_right_panel, hud_font,
                current_font_ready);
            const items::ItemInstance* const tooltip_item =
                find_item(state, source.item_id);
            if (tooltip.valid && tooltip_item != nullptr) {
                draw_inventory_tooltip(tooltip, *tooltip_item, hud_font,
                    current_font_ready);
            }
        }
    }

    draw_inventory_status(shell, status_presentation, material_pack,
        hud_font, current_font_ready);
    material_bag_.draw_reinforcement_confirmation(material_pack,
        hud_font, current_font_ready, frame_layout_.width,
        frame_layout_.height);
    if (material_rejection_frames_ != 0U) {
        --material_rejection_frames_;
        if (material_rejection_frames_ == 0U) {
            rejected_material_item_id_ = 0U;
        }
    }
}

}  // namespace arpg::platform
