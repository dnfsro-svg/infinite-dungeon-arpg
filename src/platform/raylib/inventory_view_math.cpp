#include "inventory_view_math.hpp"

#include "items/item_catalog.hpp"
#include "ui_text_catalog.hpp"
#include "ui_text_layout.hpp"
#include "ui_typography.hpp"
#include "ui_utf8.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr double kDoubleClickSeconds = 0.30;

constexpr float kSkillMainWidth = 204.0F;
constexpr float kSkillMainHeight = 92.0F;
constexpr float kSkillMainGap = 12.0F;
constexpr float kSkillSupportSize = 54.0F;
constexpr float kSkillSupportGap = 12.0F;

bool contains(Rectangle rectangle, Vector2 point) noexcept {
    return point.x >= rectangle.x && point.x <= rectangle.x + rectangle.width
        && point.y >= rectangle.y && point.y <= rectangle.y + rectangle.height;
}

bool equipped(const items::EquipmentState& equipment,
    std::uint64_t id) noexcept {
    return std::find(equipment.equipped_ids.begin(),
        equipment.equipped_ids.end(), id) != equipment.equipped_ids.end();
}

bool same_filter(InventoryFilter left, InventoryFilter right) noexcept {
    return left.slot == right.slot && left.rarity == right.rarity;
}

bool same_recipe(const RecipeSelection& left,
    const RecipeSelection& right) noexcept {
    return left.count == right.count && left.ids == right.ids;
}

[[nodiscard]] bool skill_equipped(
    const skills::SkillLoadoutState& state,
    skills::ActiveSkillId id) noexcept {
    for (const skills::ActiveSkillSlot& slot : state.slots) {
        if (slot.active == id) return true;
    }
    return false;
}

void copy_skill_name(std::array<char, 48>& output,
    skills::ActiveSkillId id) noexcept {
    if (id == skills::ActiveSkillId::none) return;
    static_cast<void>(std::snprintf(output.data(), output.size(), "%s",
        ui_skill_name(id)));
    output.back() = '\0';
}

bool matches_inventory_filter(const items::ItemInstance& item,
    bool is_equipped, InventoryFilter filter) noexcept {
    if (is_equipped) return false;
    const items::BaseDefinition* const base =
        items::base_definition(item.base_id);
    if (base == nullptr) return false;
    return (!filter.slot.has_value() || base->slot == *filter.slot)
        && (!filter.rarity.has_value() || item.rarity == *filter.rarity);
}

const char* stat_name(modifiers::StatId stat) noexcept {
    switch (stat) {
    case modifiers::StatId::impulse_scale: return u8"击退";
    case modifiers::StatId::shield: return u8"护盾";
    case modifiers::StatId::melee_damage: return u8"近战伤害";
    case modifiers::StatId::physical_flat_damage: return u8"物理伤害";
    case modifiers::StatId::fire_flat_damage:
    case modifiers::StatId::fire_damage: return u8"火焰伤害";
    case modifiers::StatId::water_flat_damage:
    case modifiers::StatId::water_damage: return u8"水流伤害";
    case modifiers::StatId::lightning_flat_damage:
    case modifiers::StatId::lightning_damage: return u8"闪电伤害";
    case modifiers::StatId::chaos_flat_damage:
    case modifiers::StatId::chaos_damage: return u8"混沌伤害";
    case modifiers::StatId::fire_damage_reduction: return u8"火焰减伤";
    case modifiers::StatId::water_damage_reduction: return u8"水流减伤";
    case modifiers::StatId::lightning_damage_reduction: return u8"闪电减伤";
    case modifiers::StatId::chaos_damage_reduction: return u8"混沌减伤";
    case modifiers::StatId::fire_damage_reduction_cap:
        return u8"火焰减伤上限";
    case modifiers::StatId::water_damage_reduction_cap:
        return u8"水流减伤上限";
    case modifiers::StatId::lightning_damage_reduction_cap:
        return u8"闪电减伤上限";
    case modifiers::StatId::chaos_damage_reduction_cap:
        return u8"混沌减伤上限";
    case modifiers::StatId::armor: return u8"护甲";
    case modifiers::StatId::evasion: return u8"闪避";
    case modifiers::StatId::max_health:
    case modifiers::StatId::max_health_more: return u8"最大生命";
    case modifiers::StatId::max_barrier: return u8"最大护盾";
    case modifiers::StatId::damage_taken: return u8"承受伤害";
    case modifiers::StatId::move_speed: return u8"移动速度";
    case modifiers::StatId::attack_speed: return u8"攻击速度";
    case modifiers::StatId::jump_speed: return u8"跳跃速度";
    case modifiers::StatId::air_control: return u8"空中移动";
    case modifiers::StatId::count: break;
    }
    return u8"装备值";
}

const char* final_stat_name(modifiers::StatId stat) noexcept {
    switch (stat) {
    case modifiers::StatId::impulse_scale: return u8"最终击退";
    case modifiers::StatId::shield: return u8"最终护盾";
    case modifiers::StatId::melee_damage: return u8"最终近战伤害";
    case modifiers::StatId::physical_flat_damage: return u8"最终物理伤害";
    case modifiers::StatId::fire_flat_damage:
    case modifiers::StatId::fire_damage: return u8"最终火焰伤害";
    case modifiers::StatId::water_flat_damage:
    case modifiers::StatId::water_damage: return u8"最终水流伤害";
    case modifiers::StatId::lightning_flat_damage:
    case modifiers::StatId::lightning_damage: return u8"最终闪电伤害";
    case modifiers::StatId::chaos_flat_damage:
    case modifiers::StatId::chaos_damage: return u8"最终混沌伤害";
    case modifiers::StatId::fire_damage_reduction:
        return u8"最终火焰减伤";
    case modifiers::StatId::water_damage_reduction:
        return u8"最终水流减伤";
    case modifiers::StatId::lightning_damage_reduction:
        return u8"最终闪电减伤";
    case modifiers::StatId::chaos_damage_reduction:
        return u8"最终混沌减伤";
    case modifiers::StatId::fire_damage_reduction_cap:
        return u8"最终火焰减伤上限";
    case modifiers::StatId::water_damage_reduction_cap:
        return u8"最终水流减伤上限";
    case modifiers::StatId::lightning_damage_reduction_cap:
        return u8"最终闪电减伤上限";
    case modifiers::StatId::chaos_damage_reduction_cap:
        return u8"最终混沌减伤上限";
    case modifiers::StatId::armor: return u8"最终护甲";
    case modifiers::StatId::evasion: return u8"最终闪避";
    case modifiers::StatId::max_health:
    case modifiers::StatId::max_health_more: return u8"最终最大生命";
    case modifiers::StatId::max_barrier: return u8"最终最大护盾";
    case modifiers::StatId::damage_taken: return u8"最终承受伤害";
    case modifiers::StatId::move_speed: return u8"最终移动速度";
    case modifiers::StatId::attack_speed: return u8"最终攻击速度";
    case modifiers::StatId::jump_speed: return u8"最终跳跃速度";
    case modifiers::StatId::air_control: return u8"最终空中移动";
    case modifiers::StatId::count: break;
    }
    return u8"最终装备值";
}

bool percent_stat(modifiers::StatId stat) noexcept {
    return stat == modifiers::StatId::fire_damage_reduction
        || stat == modifiers::StatId::water_damage_reduction
        || stat == modifiers::StatId::lightning_damage_reduction
        || stat == modifiers::StatId::chaos_damage_reduction
        || stat == modifiers::StatId::fire_damage_reduction_cap
        || stat == modifiers::StatId::water_damage_reduction_cap
        || stat == modifiers::StatId::lightning_damage_reduction_cap
        || stat == modifiers::StatId::chaos_damage_reduction_cap;
}

bool finite_rectangle(Rectangle rectangle) noexcept {
    return std::isfinite(rectangle.x) && std::isfinite(rectangle.y)
        && std::isfinite(rectangle.width) && std::isfinite(rectangle.height)
        && rectangle.width > 0.0F && rectangle.height > 0.0F;
}

bool intersects(Rectangle left, Rectangle right) noexcept {
    return left.x < right.x + right.width && right.x < left.x + left.width
        && left.y < right.y + right.height && right.y < left.y + left.height;
}

bool rectangle_inside(Rectangle child, Rectangle parent) noexcept {
    return finite_rectangle(child) && finite_rectangle(parent)
        && child.x >= parent.x && child.y >= parent.y
        && child.x + child.width <= parent.x + parent.width
        && child.y + child.height <= parent.y + parent.height;
}

void tooltip_add_segment(InventoryTooltipView& view, const char* text,
    InventoryTooltipTone tone) noexcept {
    if (text == nullptr || text[0] == '\0'
        || view.line_count >= view.lines.size()) return;
    InventoryTooltipLine& line = view.lines[view.line_count++];
    line.segment_count = 1U;
    line.segments[0].tone = tone;
    static_cast<void>(ui_utf8_copy(line.segments[0].text.data(),
        line.segments[0].text.size(), text));
}

enum class TooltipDeltaUnit : std::uint8_t {
    integer,
    fixed_scalar,
    percentage_bp,
};

template <std::size_t N>
bool tooltip_add_delta_group(InventoryTooltipView& view, const char* heading,
    const std::array<const char*, N>& labels,
    const std::array<std::int64_t, N>& values,
    const std::array<TooltipDeltaUnit, N>& units) noexcept {
    bool any{};
    for (const std::int64_t value : values) any = any || value != 0;
    if (!any || view.line_count >= view.lines.size()
        || N + 1U > kInventoryTooltipSegmentCapacity) return false;
    InventoryTooltipLine& line = view.lines[view.line_count++];
    line.segment_count = 1U;
    line.segments[0].tone = InventoryTooltipTone::primary;
    static_cast<void>(ui_utf8_copy(line.segments[0].text.data(),
        line.segments[0].text.size(), heading));
    for (std::size_t index{}; index < N; ++index) {
        if (values[index] == 0) continue;
        InventoryTooltipSegment& segment = line.segments[line.segment_count++];
        segment.tone = values[index] > 0 ? InventoryTooltipTone::positive
                                         : InventoryTooltipTone::negative;
        if (units[index] == TooltipDeltaUnit::fixed_scalar) {
            static_cast<void>(ui_utf8_format(segment.text.data(),
                segment.text.size(), "%s %+g", labels[index],
                static_cast<double>(values[index])
                    / static_cast<double>(modifiers::kFixedOne)));
        } else if (units[index] == TooltipDeltaUnit::percentage_bp) {
            static_cast<void>(ui_utf8_format(segment.text.data(),
                segment.text.size(), "%s %+g%%", labels[index],
                static_cast<double>(values[index]) / 100.0));
        } else {
            static_cast<void>(ui_utf8_format(segment.text.data(),
                segment.text.size(), "%s %+lld", labels[index],
                static_cast<long long>(values[index])));
        }
    }
    return true;
}

std::size_t tooltip_base_value_index(std::uint8_t item_level) noexcept {
    for (std::uint8_t tier = 1U; tier <= 8U; ++tier) {
        if (item_level >= items::tier_minimum_level(tier))
            return static_cast<std::size_t>(8U - tier);
    }
    return 0U;
}

InventoryTransientStatusPresentation transient_presentation(
    const InventoryTransientStatusState& state) noexcept {
    return {state.kind, state.receipt_success, state.receipt_destroyed,
        state.receipt_coupon, state.receipt_before, state.receipt_after};
}

}  // namespace

InventoryShellLayout inventory_shell_layout(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return {};
    const float viewport_width = static_cast<float>(width);
    const float viewport_height = static_cast<float>(height);
    const float geometry_scale = std::clamp(std::min(
        viewport_width / 1280.0F, viewport_height / 720.0F),
        0.625F, 1.5F);
    const UiTypography type = ui_typography();
    InventoryShellLayout layout{};
    layout.viewport = {0.0F, 0.0F, viewport_width, viewport_height};
    layout.geometry_scale = geometry_scale;
    layout.body_font_size = std::max(type.inventory_compact_min_font_size,
        type.kInventoryBodyFontSize * geometry_scale);
    layout.hint_font_size = std::max(type.inventory_compact_min_font_size,
        type.inventory_hint_font_size * geometry_scale);
    const float margin = 16.0F * geometry_scale;
    const float gap = 16.0F * geometry_scale;
    const float header_height = std::max(32.0F, 44.0F * geometry_scale);
    const float status_height = std::max(32.0F, 52.0F * geometry_scale);
    layout.header = {margin, margin, viewport_width - margin * 2.0F,
        header_height};
    layout.status_bar = {margin, viewport_height - margin - status_height,
        viewport_width - margin * 2.0F, status_height};
    layout.content = {margin, layout.header.y + layout.header.height + gap,
        viewport_width - margin * 2.0F,
        layout.status_bar.y - gap - (layout.header.y + layout.header.height + gap)};
    if (!finite_rectangle(layout.content) || layout.content.height < 240.0F) {
        return {};
    }
    const float maximum_left_width = std::min(
        layout.content.width - gap - 180.0F,
        (layout.content.width - gap) * 0.49F);
    if (maximum_left_width < 180.0F) return {};
    const float left_width = std::clamp(440.0F * geometry_scale,
        180.0F, maximum_left_width);
    if (left_width + gap + 180.0F > layout.content.width) return {};
    layout.left_panel = {layout.content.x, layout.content.y, left_width,
        layout.content.height};
    layout.right_panel = {layout.left_panel.x + layout.left_panel.width + gap,
        layout.content.y, layout.content.width - left_width - gap,
        layout.content.height};
    const float tab_height = header_height;
    const float tab_gap = std::max(4.0F, 6.0F * geometry_scale);
    const float title_reserve = std::max(190.0F,
        layout.header.width * 0.34F);
    const float tabs_x = layout.header.x + title_reserve + tab_gap;
    const float tabs_width = layout.header.x + layout.header.width - tabs_x;
    const float tab_width = (tabs_width - tab_gap * 2.0F) / 3.0F;
    for (std::size_t index{}; index < layout.tabs.size(); ++index) {
        layout.tabs[index] = {tabs_x + static_cast<float>(index)
                * (tab_width + tab_gap),
            layout.header.y + (layout.header.height - tab_height) * 0.5F,
            tab_width, tab_height};
    }
    const float panel_padding = std::max(5.0F, 10.0F * geometry_scale);
    const float available_slot_width =
        (layout.left_panel.width - panel_padding * 4.0F) / 3.0F;
    const float available_slot_height =
        (layout.left_panel.height - panel_padding * 4.0F) / 3.0F;
    const float slot_size = std::min({78.0F * geometry_scale,
        available_slot_width, available_slot_height});
    if (slot_size < 32.0F) return {};
    const float left_x = layout.left_panel.x + panel_padding;
    const float center_x = layout.left_panel.x
        + (layout.left_panel.width - slot_size) * 0.5F;
    const float right_x = layout.left_panel.x + layout.left_panel.width
        - panel_padding - slot_size;
    const float top_y = layout.left_panel.y + panel_padding;
    const float middle_y = layout.left_panel.y
        + (layout.left_panel.height - slot_size) * 0.5F;
    const float bottom_y = layout.left_panel.y + layout.left_panel.height
        - panel_padding - slot_size;
    layout.equipment_slots[static_cast<std::size_t>(items::ItemSlot::weapon)] =
        {left_x, top_y + slot_size * 0.60F, slot_size, slot_size};
    layout.equipment_slots[static_cast<std::size_t>(items::ItemSlot::helmet)] =
        {center_x, top_y, slot_size, slot_size};
    layout.equipment_slots[static_cast<std::size_t>(items::ItemSlot::chest)] =
        {center_x, middle_y, slot_size, slot_size};
    layout.equipment_slots[static_cast<std::size_t>(items::ItemSlot::gloves)] =
        {left_x, bottom_y, slot_size, slot_size};
    layout.equipment_slots[static_cast<std::size_t>(items::ItemSlot::boots)] =
        {center_x, bottom_y, slot_size, slot_size};
    layout.equipment_slots[static_cast<std::size_t>(items::ItemSlot::accessory)] =
        {right_x, top_y + slot_size * 0.60F, slot_size, slot_size};
    layout.silhouette = {center_x - slot_size * 0.18F,
        top_y + slot_size * 0.70F, slot_size * 1.36F,
        std::max(slot_size, bottom_y - top_y)};
    const float filter_height = std::max(36.0F, 42.0F * geometry_scale);
    layout.filter_bar = {layout.right_panel.x + panel_padding,
        layout.right_panel.y + panel_padding,
        layout.right_panel.width - panel_padding * 2.0F, filter_height};
    const float filter_gap = std::max(6.0F, 8.0F * geometry_scale);
    const float filter_width = (layout.filter_bar.width - filter_gap) * 0.5F;
    layout.slot_filter_bounds = {layout.filter_bar.x, layout.filter_bar.y,
        filter_width, layout.filter_bar.height};
    layout.rarity_filter_bounds = {layout.slot_filter_bounds.x
            + layout.slot_filter_bounds.width + filter_gap,
        layout.filter_bar.y, filter_width, layout.filter_bar.height};
    layout.grid_viewport = {layout.filter_bar.x,
        layout.filter_bar.y + layout.filter_bar.height + panel_padding,
        layout.filter_bar.width,
        layout.right_panel.y + layout.right_panel.height - panel_padding
            - (layout.filter_bar.y + layout.filter_bar.height + panel_padding)};
    layout.empty_state_bounds = layout.grid_viewport;
    layout.grid_cell_height = std::max(42.0F, 58.0F * geometry_scale);
    layout.grid_row_stride = layout.grid_cell_height + std::max(4.0F,
        8.0F * geometry_scale);
    const float action_width = std::max(112.0F, 144.0F * geometry_scale);
    const float action_height = std::min(layout.status_bar.height,
        std::max(32.0F, layout.status_bar.height - panel_padding));
    layout.action_bounds = {layout.status_bar.x + layout.status_bar.width
            - action_width - panel_padding,
        layout.status_bar.y
            + (layout.status_bar.height - action_height) * 0.5F,
        action_width, action_height};
    layout.hint_visible = width > 800;
    const float hint_width = layout.hint_visible
        ? std::max(120.0F, layout.status_bar.width * 0.27F) : 0.0F;
    layout.hint_bounds = {layout.action_bounds.x - gap - hint_width,
        layout.action_bounds.y, hint_width, layout.action_bounds.height};
    layout.status_bounds = {layout.status_bar.x + panel_padding,
        layout.action_bounds.y,
        layout.hint_visible ? layout.hint_bounds.x - gap
            - (layout.status_bar.x + panel_padding)
            : layout.action_bounds.x - gap - (layout.status_bar.x + panel_padding),
        layout.action_bounds.height};
    layout.valid = rectangle_inside(layout.header, layout.viewport)
        && rectangle_inside(layout.content, layout.viewport)
        && rectangle_inside(layout.left_panel, layout.content)
        && rectangle_inside(layout.right_panel, layout.content)
        && layout.right_panel.width > layout.left_panel.width
        && rectangle_inside(layout.filter_bar, layout.right_panel)
        && rectangle_inside(layout.slot_filter_bounds, layout.filter_bar)
        && rectangle_inside(layout.rarity_filter_bounds, layout.filter_bar)
        && rectangle_inside(layout.grid_viewport, layout.right_panel)
        && rectangle_inside(layout.empty_state_bounds, layout.right_panel)
        && rectangle_inside(layout.status_bar, layout.viewport)
        && rectangle_inside(layout.status_bounds, layout.status_bar)
        && rectangle_inside(layout.action_bounds, layout.status_bar)
        && rectangle_inside(layout.silhouette, layout.left_panel);
    for (const Rectangle tab : layout.tabs)
        layout.valid = layout.valid && rectangle_inside(tab, layout.header);
    for (const Rectangle slot : layout.equipment_slots)
        layout.valid = layout.valid && rectangle_inside(slot, layout.left_panel);
    if (layout.hint_visible)
        layout.valid = layout.valid
            && rectangle_inside(layout.hint_bounds, layout.status_bar);
    return layout;
}

int inventory_grid_columns(InventoryShellLayout layout) noexcept {
    return layout.valid ? 3 : 0;
}

Rectangle inventory_grid_item_rectangle(InventoryShellLayout layout,
    std::size_t filtered_position, float scroll_rows) noexcept {
    if (!layout.valid || !std::isfinite(scroll_rows)) return {};
    constexpr int kColumns = 3;
    const float gap = std::max(4.0F, 8.0F * layout.geometry_scale);
    const float width = (layout.grid_viewport.width - gap * 2.0F) / 3.0F;
    if (width <= 0.0F) return {};
    const std::size_t row = filtered_position / kColumns;
    const std::size_t column = filtered_position % kColumns;
    return {layout.grid_viewport.x + static_cast<float>(column) * (width + gap),
        layout.grid_viewport.y + (static_cast<float>(row) - scroll_rows)
            * layout.grid_row_stride,
        width, layout.grid_cell_height};
}

bool inventory_page_action_contains(InventoryShellLayout shell,
    InventoryPage page, Vector2 point) noexcept {
    const bool action_visible = page == InventoryPage::equipment
        || page == InventoryPage::skill_stones;
    return shell.valid && action_visible && contains(shell.action_bounds, point);
}

InventoryControlTextStyle inventory_control_text_style() noexcept {
    return {
        {5, 9, 16, 255},
        {18, 28, 40, 255},
        {12, 34, 48, 255},
        {22, 25, 31, 255},
        {36, 16, 24, 255},
    };
}

std::array<char, 96> inventory_page_title(
    InventoryPage page, const char* inventory_binding_label) noexcept {
    std::array<char, 96> title{};
    const char* const binding_source = inventory_binding_label != nullptr
            && inventory_binding_label[0] != '\0'
        ? inventory_binding_label : u8"未知";
    std::array<char, 48> binding{};
    static_cast<void>(ui_utf8_fit_with_ellipsis(
        binding.data(), binding.size(), binding_source));
    const char* const page_title = page == InventoryPage::equipment
        ? ui_text(UiTextId::equipment_title)
        : page == InventoryPage::skill_stones
        ? ui_text(UiTextId::inventory_page_skill_stones)
        : ui_text(UiTextId::inventory_page_materials);
    static_cast<void>(ui_utf8_format(
        title.data(), title.size(), u8"按键提示：%s / ESC  ·  %s背包",
        binding.data(), page_title));
    return title;
}

std::array<char, 64> inventory_equipment_slot_label(
    items::ItemSlot slot, const items::ItemInstance* item) noexcept {
    std::array<char, 64> label{};
    if (item == nullptr) {
        static_cast<void>(ui_utf8_copy(label.data(), label.size(),
            ui_item_slot_name(slot)));
        return label;
    }
    static_cast<void>(ui_utf8_format(label.data(), label.size(),
        u8"+%u · %s", static_cast<unsigned int>(item->reinforcement),
        ui_item_base_name(item->base_id)));
    return label;
}

std::array<char, 64> inventory_grid_item_metadata(
    const items::ItemInstance& item) noexcept {
    std::array<char, 64> metadata{};
    static_cast<void>(ui_utf8_format(metadata.data(), metadata.size(),
        u8"%s · %u级 · +%u", ui_item_rarity_name(item.rarity),
        static_cast<unsigned int>(item.item_level),
        static_cast<unsigned int>(item.reinforcement)));
    return metadata;
}

ActiveSkillLoadoutLayout active_skill_loadout_layout(
    InventoryShellLayout shell) noexcept {
    if (!shell.valid) return {};
    ActiveSkillLoadoutLayout layout{};
    layout.scale = shell.geometry_scale;
    layout.panel = shell.content;
    layout.remove_button = shell.action_bounds;

    const float padding = std::max(4.0F, 8.0F * layout.scale);
    const float main_gap = std::max(4.0F, kSkillMainGap * layout.scale);
    const float support_gap = std::max(3.0F, kSkillSupportGap * layout.scale);
    const float skill_title_reserve = 36.0F * layout.scale;
    const float support_label_reserve = 29.0F * layout.scale;
    const float left_width = shell.left_panel.width - padding * 2.0F;
    const float left_height = shell.left_panel.height - padding * 2.0F;
    const float support_size = std::min(kSkillSupportSize * layout.scale,
        (left_width - support_gap
            * static_cast<float>(skills::kSupportSlotsPerActive - 1U))
            / static_cast<float>(skills::kSupportSlotsPerActive));
    const float main_height = (left_height - skill_title_reserve
        - support_label_reserve - support_size
        - main_gap * static_cast<float>(skills::kActiveSkillSlotCount - 1U))
        / static_cast<float>(skills::kActiveSkillSlotCount);
    const float main_width = std::min(std::max(180.0F,
        kSkillMainWidth * layout.scale), left_width);
    if (main_width <= 0.0F || support_size <= 0.0F || main_height <= 0.0F) {
        return {};
    }
    for (std::size_t index = 0U; index < layout.main_slots.size(); ++index) {
        layout.main_slots[index] = {
            shell.left_panel.x + (shell.left_panel.width - main_width) * 0.5F,
            shell.left_panel.y + padding + skill_title_reserve
                + static_cast<float>(index) * (main_height + main_gap),
            main_width, main_height,
        };
    }
    layout.support_label_bounds = {
        shell.left_panel.x + padding,
        shell.left_panel.y + shell.left_panel.height - padding - support_size
            - support_label_reserve,
        left_width, support_label_reserve,
    };
    for (std::size_t index = 0U; index < layout.support_slots.size(); ++index) {
        layout.support_slots[index] = {
            shell.left_panel.x + padding + static_cast<float>(index)
                * (support_size + support_gap),
            shell.left_panel.y + shell.left_panel.height - padding - support_size,
            support_size, support_size,
        };
    }

    constexpr float kInventoryWidth = 180.0F;
    constexpr float kInventoryHeight = 62.0F;
    const float inventory_gap = std::max(4.0F, kSkillMainGap * layout.scale);
    const float inventory_width_target = std::max(160.0F,
        kInventoryWidth * layout.scale);
    const float inventory_height = kInventoryHeight * layout.scale;
    const float right_width = shell.right_panel.width - padding * 2.0F;
    const std::size_t inventory_columns = std::min(layout.inventory_slots.size(),
        static_cast<std::size_t>(std::max(1.0F, std::floor((right_width
            + inventory_gap) / (inventory_width_target + inventory_gap)))));
    const float inventory_width = std::min(inventory_width_target,
        (right_width - inventory_gap
            * static_cast<float>(inventory_columns - 1U))
            / static_cast<float>(inventory_columns));
    if (right_width <= 0.0F || inventory_width <= 0.0F
        || inventory_height <= 0.0F) {
        return {};
    }
    const float empty_height = std::min(shell.right_panel.height,
        std::max(32.0F, 44.0F * layout.scale));
    layout.empty_bounds = {shell.right_panel.x + padding,
        shell.right_panel.y
            + (shell.right_panel.height - empty_height) * 0.5F,
        right_width, empty_height};
    for (std::size_t index = 0U; index < layout.inventory_slots.size(); ++index) {
        const std::size_t column = index % inventory_columns;
        const std::size_t row = index / inventory_columns;
        layout.inventory_slots[index] = {
            shell.right_panel.x + padding + static_cast<float>(column)
                * (inventory_width + inventory_gap),
            shell.right_panel.y + padding + skill_title_reserve
                + static_cast<float>(row)
                * (inventory_height + inventory_gap),
            inventory_width, inventory_height,
        };
    }
    return layout;
}

ActiveSkillLoadoutView make_active_skill_loadout_view(
    const skills::SkillLoadoutState& state,
    const ActiveSkillLoadoutSelection& selection,
    bool save_pending) noexcept {
    ActiveSkillLoadoutView view{};
    view.save_pending = save_pending;
    for (std::size_t index = 0U; index < view.slots.size(); ++index) {
        ActiveSkillLoadoutSlotView& output = view.slots[index];
        const skills::ActiveSkillSlot& input = state.slots[index];
        output.slot_number = static_cast<std::uint8_t>(index + 1U);
        output.id = input.active;
        output.empty = output.id == skills::ActiveSkillId::none;
        output.selected = selection.selected_slot == index;
        copy_skill_name(output.name, output.id);
        for (std::size_t support = 0U;
             support < output.support_empty.size(); ++support) {
            output.support_empty[support] =
                input.supports[support] == skills::SupportSkillId::none;
        }
    }
    for (std::size_t raw_id = 0U; raw_id < skills::kActiveSkillCount; ++raw_id) {
        const auto id = static_cast<skills::ActiveSkillId>(raw_id);
        const std::uint64_t bit = 1ULL << raw_id;
        if ((state.owned_active_bits & bit) == 0U
                || skill_equipped(state, id)) {
            continue;
        }
        ActiveSkillInventoryStoneView& stone =
            view.inventory[view.inventory_count++];
        stone.id = id;
        stone.selected = selection.selected_inventory == id;
        copy_skill_name(stone.name, id);
    }
    return view;
}

std::optional<ActiveSkillLoadoutCommand>
active_skill_loadout_command_after_click(
    const skills::SkillLoadoutState& state,
    const ActiveSkillLoadoutLayout& layout,
    Vector2 point,
    ActiveSkillLoadoutSelection& selection,
    bool save_pending) noexcept {
    if (save_pending) return std::nullopt;
    const ActiveSkillLoadoutView view = make_active_skill_loadout_view(
        state, selection, false);
    for (std::size_t index = 0U; index < layout.main_slots.size(); ++index) {
        if (!contains(layout.main_slots[index], point)) continue;
        if (selection.selected_inventory != skills::ActiveSkillId::none
                && state.slots[index].active == skills::ActiveSkillId::none) {
            return ActiveSkillLoadoutCommand{
                ActiveSkillLoadoutActionKind::equip,
                static_cast<std::uint8_t>(index), 0xFFU,
                selection.selected_inventory};
        }
        if (selection.selected_slot < state.slots.size()
                && selection.selected_slot != index
                && state.slots[selection.selected_slot].active
                    != skills::ActiveSkillId::none) {
            return ActiveSkillLoadoutCommand{
                ActiveSkillLoadoutActionKind::swap,
                static_cast<std::uint8_t>(selection.selected_slot),
                static_cast<std::uint8_t>(index),
                skills::ActiveSkillId::none};
        }
        selection.selected_slot = index;
        selection.selected_inventory = skills::ActiveSkillId::none;
        return ActiveSkillLoadoutCommand{
            ActiveSkillLoadoutActionKind::select,
            static_cast<std::uint8_t>(index), 0xFFU,
            state.slots[index].active};
    }

    if (contains(layout.remove_button, point)
            && selection.selected_slot < state.slots.size()
            && state.slots[selection.selected_slot].active
                != skills::ActiveSkillId::none) {
        return ActiveSkillLoadoutCommand{
            ActiveSkillLoadoutActionKind::remove,
            static_cast<std::uint8_t>(selection.selected_slot), 0xFFU,
            state.slots[selection.selected_slot].active};
    }

    for (std::size_t index = 0U; index < view.inventory_count; ++index) {
        if (!contains(layout.inventory_slots[index], point)) continue;
        selection.selected_slot = kNoActiveSkillLoadoutSelection;
        selection.selected_inventory = view.inventory[index].id;
        return ActiveSkillLoadoutCommand{
            ActiveSkillLoadoutActionKind::select, 0xFFU, 0xFFU,
            view.inventory[index].id};
    }
    return std::nullopt;
}

void advance_active_skill_loadout_selection(
    ActiveSkillLoadoutSelection& selection,
    const ActiveSkillLoadoutCommand& accepted_command) noexcept {
    if (accepted_command.kind == ActiveSkillLoadoutActionKind::equip) {
        selection.selected_slot = accepted_command.slot;
        selection.selected_inventory = skills::ActiveSkillId::none;
    } else if (accepted_command.kind
            == ActiveSkillLoadoutActionKind::swap) {
        selection.selected_slot = accepted_command.other_slot;
    }
}

float clamp_inventory_scroll_rows(std::size_t filtered_count, int columns,
    float scroll_rows, float viewport_height, float cell_height) noexcept {
    if (columns <= 0 || viewport_height <= 0.0F || cell_height <= 0.0F
        || !std::isfinite(scroll_rows)) return 0.0F;
    const float total_rows = std::ceil(static_cast<float>(filtered_count)
        / static_cast<float>(columns));
    const float visible_rows = viewport_height / cell_height;
    return std::clamp(scroll_rows, 0.0F,
        std::max(0.0F, total_rows - visible_rows));
}

VisibleGridRange visible_grid_range(std::size_t filtered_count, int columns,
    float scroll_rows, float viewport_height, float cell_height) noexcept {
    if (filtered_count == 0U || columns <= 0 || viewport_height <= 0.0F
        || cell_height <= 0.0F) return {};
    const float clamped = clamp_inventory_scroll_rows(filtered_count, columns,
        scroll_rows, viewport_height, cell_height);
    const std::size_t first_row = static_cast<std::size_t>(std::floor(clamped));
    const float row_fraction = clamped - static_cast<float>(first_row);
    const std::size_t visible_rows = static_cast<std::size_t>(
        std::ceil(row_fraction + viewport_height / cell_height));
    const std::size_t first = std::min(filtered_count,
        first_row * static_cast<std::size_t>(columns));
    const std::size_t requested = visible_rows * static_cast<std::size_t>(columns);
    return {first, std::min(requested, filtered_count - first)};
}

std::vector<std::size_t> filtered_inventory_indices(
    const items::ItemOwnershipState& state, InventoryFilter filter) {
    std::vector<std::size_t> result;
    result.reserve(state.items.size());
    for (std::size_t index = 0U; index < state.items.size(); ++index) {
        const items::ItemInstance& item = state.items[index];
        if (matches_inventory_filter(item,
                equipped(state.equipment, item.id), filter)) {
            result.push_back(index);
        }
    }
    return result;
}

InventoryClickKind register_inventory_click(InventoryClickTracker& tracker,
    std::uint64_t item_id, double now_seconds) noexcept {
    const bool repeated = item_id != 0U && tracker.last_item_id == item_id
        && now_seconds >= tracker.last_click_seconds
        && now_seconds - tracker.last_click_seconds <= kDoubleClickSeconds;
    tracker.last_item_id = repeated ? 0U : item_id;
    tracker.last_click_seconds = repeated ? -1.0 : now_seconds;
    return repeated ? InventoryClickKind::double_click
                    : InventoryClickKind::single;
}

bool toggle_recipe_selection(RecipeSelection& selection,
    std::uint64_t item_id) noexcept {
    if (item_id == 0U) return false;
    for (std::size_t index = 0U; index < selection.count; ++index) {
        if (selection.ids[index] != item_id) continue;
        for (std::size_t move = index + 1U; move < selection.count; ++move) {
            selection.ids[move - 1U] = selection.ids[move];
        }
        selection.ids[--selection.count] = 0U;
        return true;
    }
    if (selection.count >= selection.ids.size()) return false;
    selection.ids[selection.count++] = item_id;
    return true;
}

std::optional<items::ItemSlot> hit_test_equipped_slot(Vector2 point,
    InventoryShellLayout layout,
    const items::EquipmentState& equipment) noexcept {
    if (!layout.valid) return std::nullopt;
    for (std::size_t index{}; index < layout.equipment_slots.size(); ++index) {
        if (equipment.equipped_ids[index] == 0U) continue;
        if (contains(layout.equipment_slots[index], point))
            return static_cast<items::ItemSlot>(index);
    }
    return std::nullopt;
}

bool inventory_can_open(bool runtime_running,
    bool passive_overlay_open) noexcept {
    return runtime_running && !passive_overlay_open;
}

bool passive_overlay_can_toggle(bool inventory_open) noexcept {
    return !inventory_open;
}

InventoryInputGate inventory_input_gate(bool inventory_open) noexcept {
    const bool forward = !inventory_open;
    return {forward, forward, forward, forward, forward};
}

BuildDifference compare_player_builds(const combat::PlayerCombatBuild& current,
    const combat::PlayerCombatBuild& candidate) noexcept {
    BuildDifference result{};
    result.max_health = static_cast<std::int64_t>(candidate.values.max_health)
        - current.values.max_health;
    result.max_barrier = static_cast<std::int64_t>(candidate.values.max_barrier)
        - current.values.max_barrier;
    result.melee_damage = candidate.values.melee_damage
        - current.values.melee_damage;
    result.impulse_scale = candidate.values.impulse_scale
        - current.values.impulse_scale;
    result.move_speed = candidate.values.movement_speed
        - current.values.movement_speed;
    result.attack_speed = candidate.values.attack_speed
        - current.values.attack_speed;
    result.armor = candidate.values.armor - current.values.armor;
    result.evasion = candidate.values.evasion - current.values.evasion;
    for (std::size_t index = 0U; index < result.flat_damage.size(); ++index) {
        result.flat_damage[index] = candidate.values.flat_damage[index]
            - current.values.flat_damage[index];
        result.damage_increased[index] =
            static_cast<std::int64_t>(candidate.values.damage_increased[index])
            - current.values.damage_increased[index];
    }
    for (std::size_t index = 0U; index < result.damage_reduction.size(); ++index) {
        result.damage_reduction[index] =
            static_cast<std::int64_t>(candidate.values.damage_reduction[index])
            - current.values.damage_reduction[index];
        result.damage_reduction_cap_bonus[index] = static_cast<std::int64_t>(
            candidate.values.damage_reduction_cap_bonus[index])
            - current.values.damage_reduction_cap_bonus[index];
    }
    result.weapon_physical = candidate.weapon_physical - current.weapon_physical;
    result.local_attack_speed_bp = static_cast<std::int64_t>(
        candidate.local_attack_speed_bp) - current.local_attack_speed_bp;
    result.armor_reduction_bp = modifiers::rating_to_basis_points(
        candidate.values.armor) - modifiers::rating_to_basis_points(
            current.values.armor);
    result.evasion_rate_bp = modifiers::rating_to_basis_points(
        candidate.values.evasion) - modifiers::rating_to_basis_points(
            current.values.evasion);
    return result;
}

void refresh_inventory_view_cache(InventoryViewCache& cache,
    const items::ItemOwnershipState& state, std::uint64_t generation,
    InventoryFilter filter, std::uint64_t selected_item_id,
    RecipeSelection recipe) {
    if (cache.generation == generation
        && cache.equipment.equipped_ids == state.equipment.equipped_ids
        && same_filter(cache.filter, filter)
        && cache.selected_item_id == selected_item_id
        && same_recipe(cache.requested_recipe, recipe)) return;
    cache.generation = generation;
    cache.equipment = state.equipment;
    cache.filter = filter;
    cache.selected_item_id = selected_item_id;
    cache.requested_recipe = recipe;
    cache.resolved_recipe = {};
    cache.selected_index = InventoryViewCache::kInvalidIndex;
    cache.recipe_indices.fill(InventoryViewCache::kInvalidIndex);
    cache.recipe_ready = false;
    cache.filtered_indices.clear();
    cache.filtered_indices.reserve(state.items.size());
    ++cache.refresh_count;
    for (std::size_t index = 0U; index < state.items.size(); ++index) {
        ++cache.item_inspection_count;
        const items::ItemInstance& item = state.items[index];
        const bool is_equipped = equipped(state.equipment, item.id);
        if (item.id == selected_item_id) cache.selected_index = index;
        const bool matches_filter =
            matches_inventory_filter(item, is_equipped, filter);
        if (matches_filter) {
            for (std::size_t selected = 0U; selected < recipe.count; ++selected) {
                if (item.id == recipe.ids[selected]) {
                    cache.recipe_indices[selected] = index;
                }
            }
            cache.filtered_indices.push_back(index);
        }
    }
    for (std::size_t selected = 0U; selected < recipe.count; ++selected) {
        if (cache.recipe_indices[selected] == InventoryViewCache::kInvalidIndex) {
            continue;
        }
        cache.resolved_recipe.ids[cache.resolved_recipe.count++] =
            recipe.ids[selected];
    }
    if (cache.resolved_recipe.count != 3U) return;
    if (cache.resolved_recipe.ids[0] == cache.resolved_recipe.ids[1]
        || cache.resolved_recipe.ids[0] == cache.resolved_recipe.ids[2]
        || cache.resolved_recipe.ids[1] == cache.resolved_recipe.ids[2]) return;
    const items::ItemInstance& first =
        state.items[cache.recipe_indices[0]];
    if (items::base_definition(first.base_id) == nullptr) return;
    for (std::size_t selected = 1U; selected < 3U; ++selected) {
        const items::ItemInstance& item =
            state.items[cache.recipe_indices[selected]];
        if (items::base_definition(item.base_id) == nullptr
            || item.base_id != first.base_id
            || item.rarity != first.rarity) return;
    }
    cache.recipe_ready = true;
}

const items::ItemInstance* cached_selected_item(
    const InventoryViewCache& cache,
    const items::ItemOwnershipState& state) noexcept {
    if (cache.selected_index >= state.items.size()) return nullptr;
    const items::ItemInstance& item = state.items[cache.selected_index];
    return item.id == cache.selected_item_id ? &item : nullptr;
}

bool cached_recipe_ready(const InventoryViewCache& cache) noexcept {
    return cache.recipe_ready;
}

InventoryVisibleItemSource inventory_visible_item_source(
    InventoryShellLayout shell, InventoryPage page,
    const items::ItemOwnershipState& state, const InventoryViewCache& cache,
    std::uint64_t item_id, float scroll_rows) noexcept {
    if (!shell.valid || page != InventoryPage::equipment || item_id == 0U
        || !std::isfinite(scroll_rows)) return {};
    for (std::size_t slot{}; slot < state.equipment.equipped_ids.size(); ++slot) {
        if (state.equipment.equipped_ids[slot] != item_id) continue;
        const auto found = std::find_if(state.items.begin(), state.items.end(),
            [item_id](const items::ItemInstance& item) noexcept {
                return item.id == item_id;
            });
        if (found != state.items.end())
            return {item_id, shell.equipment_slots[slot], false, true};
        return {};
    }
    const VisibleGridRange visible = visible_grid_range(cache.filtered_indices.size(),
        inventory_grid_columns(shell), scroll_rows, shell.grid_viewport.height,
        shell.grid_row_stride);
    for (std::size_t position = visible.first;
         position < visible.first + visible.count; ++position) {
        const std::size_t index = cache.filtered_indices[position];
        if (index >= state.items.size() || state.items[index].id != item_id) continue;
        const Rectangle bounds = inventory_grid_item_rectangle(shell, position,
            scroll_rows);
        if (finite_rectangle(bounds) && intersects(bounds, shell.grid_viewport))
            return {item_id, bounds, true, true};
    }
    return {};
}

InventoryEmptyPresentation inventory_empty_presentation(
    const items::ItemOwnershipState& state, std::size_t filtered_count,
    std::optional<items::MaterialId> selected_material) noexcept {
    InventoryEmptyPresentation output{};
    if (filtered_count != 0U) return output;
    if (state.items.empty()) {
        output.state = InventoryEmptyState::inventory_empty;
        output.title = ui_text(UiTextId::inventory_empty_title);
        static_cast<void>(ui_utf8_copy(output.hint.data(), output.hint.size(),
            ui_text(UiTextId::inventory_empty_hint)));
        return output;
    }
    bool any_unequipped{};
    for (const items::ItemInstance& item : state.items) {
        if (!equipped(state.equipment, item.id)) {
            any_unequipped = true;
            break;
        }
    }
    if (!any_unequipped) {
        output.state = InventoryEmptyState::all_equipped;
        output.title = ui_text(UiTextId::inventory_all_equipped_title);
        if (selected_material.has_value()) {
            static_cast<void>(ui_utf8_format(output.hint.data(),
                output.hint.size(),
                ui_text(UiTextId::inventory_material_target_hint_format),
                ui_material_name(*selected_material)));
        } else {
            static_cast<void>(ui_utf8_copy(output.hint.data(), output.hint.size(),
                ui_text(UiTextId::inventory_all_equipped_hint)));
        }
        return output;
    }
    output.state = InventoryEmptyState::filter_empty;
    output.title = ui_text(UiTextId::inventory_filter_empty_title);
    static_cast<void>(ui_utf8_copy(output.hint.data(), output.hint.size(),
        ui_text(UiTextId::inventory_filter_empty_hint)));
    return output;
}

InventoryStatusPresentation inventory_status_presentation(
    const InventoryStatusInput& input) noexcept {
    InventoryStatusPresentation output{};
    output.tone = InventoryStatusTone::muted;
    if (input.page == InventoryPage::equipment) {
        static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
            ui_text(UiTextId::inventory_context_hint)));
        static_cast<void>(ui_utf8_copy(output.hint.data(), output.hint.size(),
            ui_text(UiTextId::inventory_context_hint)));
        if (!input.selected_material.has_value()) {
            static_cast<void>(ui_utf8_format(output.action.data(), output.action.size(),
                ui_text(UiTextId::inventory_recipe_action_format),
                static_cast<unsigned int>(input.recipe_count)));
            output.action_kind = InventoryActionKind::combine;
            output.action_visible = true;
            output.action_enabled = input.recipe_ready && input.authority_enabled;
        }
        if (input.reinforced_recipe_input) {
            static_cast<void>(ui_utf8_copy(output.status.data(),
                output.status.size(), ui_text(UiTextId::inventory_recipe_danger)));
            output.tone = InventoryStatusTone::negative;
        } else if (input.recipe_count != 0U) {
            output.tone = input.recipe_ready ? InventoryStatusTone::positive
                                             : InventoryStatusTone::warning;
        }
    } else if (input.page == InventoryPage::skill_stones) {
        const UiTextId status_id = input.active_skill_removable
            ? UiTextId::inventory_skill_swap_hint
            : UiTextId::inventory_skill_equip_hint;
        const UiTextId hint_id = input.active_skill_removable
            ? UiTextId::inventory_skill_remove_hint
            : UiTextId::inventory_skill_equip_hint;
        static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
            ui_text(status_id)));
        static_cast<void>(ui_utf8_copy(output.hint.data(), output.hint.size(),
            ui_text(hint_id)));
        static_cast<void>(ui_utf8_copy(output.action.data(), output.action.size(),
            ui_text(UiTextId::inventory_action_remove)));
        output.action_kind = InventoryActionKind::remove_active_skill;
        output.action_visible = true;
        output.action_enabled = input.active_skill_removable
            && input.authority_enabled;
        if (input.active_skill_removable)
            output.tone = InventoryStatusTone::positive;
    } else if (input.page == InventoryPage::materials) {
        static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
            ui_text(UiTextId::inventory_page_materials)));
        if (input.selected_material.has_value()) {
            static_cast<void>(ui_utf8_copy(output.hint.data(), output.hint.size(),
                ui_material_description(*input.selected_material)));
        }
    }
    if (input.save_state == InventorySavePresentationState::saving) {
        static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
            ui_text(UiTextId::save_saving)));
        output.tone = InventoryStatusTone::warning;
        output.action_enabled = false;
        return output;
    }
    if (input.save_state == InventorySavePresentationState::error
        || input.recovery_required) {
        static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
            ui_text(UiTextId::save_error)));
        output.tone = InventoryStatusTone::negative;
        output.action_enabled = false;
        return output;
    }
    if (input.material_target_rejected) {
        static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
            ui_text(UiTextId::inventory_material_rejected)));
        output.tone = InventoryStatusTone::negative;
        return output;
    }
    if (input.transient.kind == InventoryTransientStatusKind::reinforcement_receipt) {
        UiTextId format = UiTextId::inventory_reinforcement_success_format;
        output.tone = InventoryStatusTone::positive;
        if (!input.transient.receipt_success) {
            format = input.transient.receipt_destroyed
                ? UiTextId::inventory_reinforcement_destroyed_format
                : UiTextId::inventory_reinforcement_failure_format;
            output.tone = InventoryStatusTone::negative;
        } else if (input.transient.receipt_coupon) {
            format = UiTextId::inventory_coupon_success_format;
        }
        if (format == UiTextId::inventory_reinforcement_destroyed_format) {
            static_cast<void>(ui_utf8_format(output.status.data(),
                output.status.size(), ui_text(format), input.transient.receipt_before));
        } else {
            static_cast<void>(ui_utf8_format(output.status.data(),
                output.status.size(), ui_text(format), input.transient.receipt_before,
                input.transient.receipt_after));
        }
        return output;
    }
    if (input.transient.kind == InventoryTransientStatusKind::saved
        || input.transient.kind == InventoryTransientStatusKind::recovered) {
        const UiTextId id = input.transient.kind
                == InventoryTransientStatusKind::saved
            ? UiTextId::save_saved : UiTextId::save_recovered;
        static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
            ui_text(id)));
        output.tone = InventoryStatusTone::positive;
        return output;
    }
    if (input.selected_material.has_value()
            && input.page != InventoryPage::skill_stones) {
        if (input.page == InventoryPage::materials
                && *input.selected_material == items::MaterialId::directed) {
            static_cast<void>(ui_utf8_copy(output.status.data(),
                output.status.size(),
                ui_text(UiTextId::inventory_directed_material_hint)));
        } else {
            static_cast<void>(ui_utf8_format(output.status.data(),
                output.status.size(),
                ui_text(input.page == InventoryPage::equipment
                        ? UiTextId::inventory_material_target_hint_format
                        : UiTextId::inventory_material_selected_format),
                ui_material_name(*input.selected_material)));
        }
        output.tone = InventoryStatusTone::warning;
        return output;
    }
    return output;
}

InventoryTransientStatusFrameResult inventory_transient_status_for_frame(
    InventoryTransientStatusState state,
    const InventoryTransientStatusInput& input) noexcept {
    InventoryTransientStatusFrameResult result{};
    if (!state.initialized) {
        state.initialized = true;
        state.last_commit_generation = input.commit_generation;
        state.last_receipt_generation = input.receipt_valid
            ? input.receipt_commit_generation : 0U;
        result.next = state;
        return result;
    }
    if (input.commit_generation < state.last_commit_generation
        || (input.receipt_valid && input.receipt_commit_generation
            < state.last_receipt_generation)) {
        state.kind = InventoryTransientStatusKind::none;
        state.remaining_frames = 0U;
        state.last_commit_generation = input.commit_generation;
        if (input.receipt_valid)
            state.last_receipt_generation = input.receipt_commit_generation;
        else
            state.last_receipt_generation = 0U;
        result.next = state;
        return result;
    }
    const bool new_commit = input.commit_generation > state.last_commit_generation;
    const bool new_receipt = input.receipt_valid
        && input.receipt_commit_generation > state.last_receipt_generation;
    state.last_commit_generation = std::max(state.last_commit_generation,
        input.commit_generation);
    if (input.receipt_valid) state.last_receipt_generation = std::max(
        state.last_receipt_generation, input.receipt_commit_generation);
    if (new_receipt) {
        state.kind = InventoryTransientStatusKind::reinforcement_receipt;
        state.remaining_frames = kInventoryTransientStatusFrameCount;
        state.receipt_success = input.receipt_success;
        state.receipt_destroyed = input.receipt_destroyed;
        state.receipt_coupon = input.receipt_coupon;
        state.receipt_before = input.receipt_before;
        state.receipt_after = input.receipt_after;
    } else if (new_commit && input.save_state == InventorySavePresentationState::saved) {
        state.kind = InventoryTransientStatusKind::saved;
        state.remaining_frames = kInventoryTransientStatusFrameCount;
    } else if (new_commit
            && input.save_state == InventorySavePresentationState::recovered) {
        state.kind = InventoryTransientStatusKind::recovered;
        state.remaining_frames = kInventoryTransientStatusFrameCount;
    }
    if (state.remaining_frames != 0U) {
        result.presentation = transient_presentation(state);
        --state.remaining_frames;
        if (state.remaining_frames == 0U)
            state.kind = InventoryTransientStatusKind::none;
    }
    result.next = state;
    return result;
}

InventoryItemVisualState inventory_item_visual_state(
    const InventoryItemVisualInput& input) noexcept {
    InventoryItemVisualState output{};
    output.selected_indicator = input.item_id != 0U
        && input.item_id == input.selected_item_id;
    if (input.item_id == 0U) return output;
    if (input.material_target_mode && input.material_target_rejected) {
        output.outline = InventoryItemOutline::rejected_target;
        return output;
    }
    if (input.material_target_mode) {
        output.outline = input.material_target_enabled
            ? InventoryItemOutline::material_target
            : InventoryItemOutline::disabled_target;
        return output;
    }
    for (std::size_t index{}; index < input.recipe.count; ++index) {
        if (input.recipe.ids[index] == input.item_id) {
            output.outline = InventoryItemOutline::recipe;
            output.recipe_ordinal = static_cast<std::uint8_t>(index + 1U);
            return output;
        }
    }
    if (output.selected_indicator) {
        output.outline = InventoryItemOutline::selected;
    } else if (input.item_id == input.hovered_item_id) {
        output.outline = InventoryItemOutline::hover;
    }
    return output;
}

bool inventory_material_target_enabled(
    const items::ItemOwnershipState& state,
    const items::ItemInstance& item, items::MaterialId material,
    std::optional<items::DirectedCategory> directed_category,
    std::uint64_t root_seed, std::uint64_t operation_nonce,
    bool authority_enabled) noexcept {
    const std::size_t material_slot = items::material_index(material);
    if (!authority_enabled || material_slot >= state.materials.size()
            || state.materials[material_slot] == 0U) {
        return false;
    }
    if (material == items::MaterialId::reinforcement_stone) {
        return items::reinforcement_preview(item).can_attempt;
    }
    if (items::material_is_coupon(material)) {
        return items::apply_coupon(item, material).has_value();
    }
    if (!items::material_is_crafting_currency(material)) return false;
    const items::CraftResult preview = items::craft_item({root_seed,
        operation_nonce, item, material, directed_category});
    return preview.applied && preview.consumed && preview.item.id == item.id;
}

InventoryTooltipView inventory_tooltip_view(const items::ItemInstance& item,
    const std::optional<BuildDifference>& difference) noexcept {
    InventoryTooltipView view{};
    const items::BaseDefinition* const base = items::base_definition(item.base_id);
    if (item.id == 0U || base == nullptr) return view;
    static_cast<void>(ui_utf8_copy(view.title.data(), view.title.size(),
        ui_item_base_name(item.base_id)));
    tooltip_add_segment(view, ui_item_rarity_name(item.rarity),
        InventoryTooltipTone::rarity);
    std::array<char, 64> line{};
    if (view.line_count < view.lines.size()) {
        InventoryTooltipLine& metadata = view.lines[view.line_count++];
        metadata.segment_count = 3U;
        for (InventoryTooltipSegment& segment : metadata.segments)
            segment.tone = InventoryTooltipTone::muted;
        static_cast<void>(ui_utf8_format(metadata.segments[0].text.data(),
            metadata.segments[0].text.size(), u8"物品等级 %u",
            static_cast<unsigned int>(item.item_level)));
        static_cast<void>(ui_utf8_format(metadata.segments[1].text.data(),
            metadata.segments[1].text.size(), u8"需要等级 %u",
            static_cast<unsigned int>(item.required_level)));
        static_cast<void>(ui_utf8_format(metadata.segments[2].text.data(),
            metadata.segments[2].text.size(), u8"强化 +%u",
            static_cast<unsigned int>(item.reinforcement)));
    }
    for (std::size_t index{}; index < base->effect_count; ++index) {
        const items::BaseEffect& effect = base->effects[index];
        const ItemAttributeLabel label = item_attribute_label(effect.effect,
            effect.stat, effect.operation, base->slot, 0xFFU);
        const double value = item_attribute_display_value(
            effect.values[tooltip_base_value_index(item.item_level)], label);
        if (label.multiplier) {
            static_cast<void>(ui_utf8_format(line.data(), line.size(),
                u8"基础 %s ×%.2f", label.name, value));
        } else {
            static_cast<void>(ui_utf8_format(line.data(), line.size(),
                u8"基础 %s %+g%s", label.name, value, label.suffix));
        }
        tooltip_add_segment(view, line.data(), InventoryTooltipTone::primary);
    }
    const std::size_t affix_count = std::min<std::size_t>(item.affix_count,
        item.affixes.size());
    for (std::size_t index{}; index < affix_count; ++index) {
        const items::AffixRoll& roll = item.affixes[index];
        const auto value = items::affix_roll_value(roll);
        const items::AffixDefinition* const affix =
            items::affix_definition(roll.affix_id);
        if (affix == nullptr || !value.has_value()) continue;
        const ItemAttributeLabel label = item_attribute_label(affix->effect,
            affix->stat, affix->operation, base->slot, roll.variant);
        const double display_value = item_attribute_display_value(*value, label);
        if (label.multiplier) {
            static_cast<void>(ui_utf8_format(line.data(), line.size(),
                u8"%u级 %s %s ×%.2f",
                static_cast<unsigned int>(roll.tier),
                affix->kind == items::AffixKind::prefix
                    ? u8"前缀" : u8"后缀",
                ui_item_affix_name(roll.affix_id), display_value));
        } else {
            static_cast<void>(ui_utf8_format(line.data(), line.size(),
                u8"%u级 %s %s %+g%s",
                static_cast<unsigned int>(roll.tier),
                affix->kind == items::AffixKind::prefix
                    ? u8"前缀" : u8"后缀",
                ui_item_affix_name(roll.affix_id), display_value,
                label.suffix == nullptr ? "" : label.suffix));
        }
        tooltip_add_segment(view, line.data(), InventoryTooltipTone::primary);
    }
    if (difference.has_value()) {
        const BuildDifference& delta = *difference;
        tooltip_add_segment(view, ui_text(UiTextId::inventory_comparison_title),
            InventoryTooltipTone::rarity);
        bool any_difference{};
        any_difference = tooltip_add_delta_group(view, u8"生存",
            std::array<const char*, 2>{{u8"生命", u8"护盾"}},
            std::array<std::int64_t, 2>{{delta.max_health,
                delta.max_barrier}},
            std::array<TooltipDeltaUnit, 2>{{
                TooltipDeltaUnit::fixed_scalar,
                TooltipDeltaUnit::fixed_scalar}}) || any_difference;
        any_difference = tooltip_add_delta_group(view, u8"基础伤害",
            std::array<const char*, 5>{{u8"物理", u8"火焰", u8"水流",
                u8"闪电", u8"混沌"}}, delta.flat_damage,
            std::array<TooltipDeltaUnit, 5>{{
                TooltipDeltaUnit::fixed_scalar,
                TooltipDeltaUnit::fixed_scalar,
                TooltipDeltaUnit::fixed_scalar,
                TooltipDeltaUnit::fixed_scalar,
                TooltipDeltaUnit::fixed_scalar}}) || any_difference;
        any_difference = tooltip_add_delta_group(view, u8"伤害提高",
            std::array<const char*, 5>{{u8"物理", u8"火焰", u8"水流",
                u8"闪电", u8"混沌"}}, delta.damage_increased,
            std::array<TooltipDeltaUnit, 5>{{
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp}}) || any_difference;
        any_difference = tooltip_add_delta_group(view, u8"近战",
            std::array<const char*, 2>{{u8"伤害", u8"击退"}},
            std::array<std::int64_t, 2>{{delta.melee_damage,
                delta.impulse_scale}},
            std::array<TooltipDeltaUnit, 2>{{
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp}}) || any_difference;
        any_difference = tooltip_add_delta_group(view, u8"速度",
            std::array<const char*, 2>{{u8"移动", u8"攻击"}},
            std::array<std::int64_t, 2>{{delta.move_speed,
                delta.attack_speed}},
            std::array<TooltipDeltaUnit, 2>{{
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp}}) || any_difference;
        any_difference = tooltip_add_delta_group(view, u8"武器",
            std::array<const char*, 2>{{u8"物理", u8"局部攻速"}},
            std::array<std::int64_t, 2>{{delta.weapon_physical,
                delta.local_attack_speed_bp}},
            std::array<TooltipDeltaUnit, 2>{{TooltipDeltaUnit::integer,
                TooltipDeltaUnit::percentage_bp}}) || any_difference;
        any_difference = tooltip_add_delta_group(view, u8"防御",
            std::array<const char*, 4>{{u8"护甲", u8"减伤", u8"闪避",
                u8"闪避率"}},
            std::array<std::int64_t, 4>{{delta.armor,
                delta.armor_reduction_bp, delta.evasion,
                delta.evasion_rate_bp}},
            std::array<TooltipDeltaUnit, 4>{{TooltipDeltaUnit::integer,
                TooltipDeltaUnit::percentage_bp, TooltipDeltaUnit::integer,
                TooltipDeltaUnit::percentage_bp}}) || any_difference;
        any_difference = tooltip_add_delta_group(view, u8"元素减伤",
            std::array<const char*, 4>{{u8"火", u8"水", u8"雷", u8"混"}},
            delta.damage_reduction,
            std::array<TooltipDeltaUnit, 4>{{
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp}}) || any_difference;
        any_difference = tooltip_add_delta_group(view, u8"减伤上限",
            std::array<const char*, 4>{{u8"火", u8"水", u8"雷", u8"混"}},
            delta.damage_reduction_cap_bonus,
            std::array<TooltipDeltaUnit, 4>{{
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp,
                TooltipDeltaUnit::percentage_bp}}) || any_difference;
        if (!any_difference) tooltip_add_segment(view,
            ui_text(UiTextId::inventory_no_change), InventoryTooltipTone::muted);
    }
    view.valid = view.title[0] != '\0' && view.line_count != 0U;
    return view;
}

InventoryTooltipLayout inventory_tooltip_layout(InventoryShellLayout shell,
    Rectangle source, bool source_is_in_right_panel,
    const InventoryTooltipView& view, UiTextMeasureFn measure,
    void* measure_context) noexcept {
    if (!shell.valid || !finite_rectangle(source) || !view.valid
        || measure == nullptr) return {};
    const float font_size = std::max(12.0F, shell.body_font_size);
    const float title_font_size = std::max(12.0F,
        font_size + 2.0F * shell.geometry_scale);
    const float line_height = std::max(13.0F,
        18.0F * shell.geometry_scale);
    const float padding = std::max(4.0F, 6.0F * shell.geometry_scale);
    const float title_gap = std::max(1.0F, 1.5F * shell.geometry_scale);
    const float source_gap = 8.0F * shell.geometry_scale;
    const float content_right = shell.content.x + shell.content.width;
    const float source_right = source.x + source.width;
    const Rectangle full_preferred = source_is_in_right_panel
        ? shell.left_panel : shell.right_panel;
    const Rectangle preferred = source_is_in_right_panel
        ? full_preferred : shell.grid_viewport;
    const float opposite_y = source_is_in_right_panel
        ? shell.content.y : shell.grid_viewport.y;
    const float opposite_height = source_is_in_right_panel
        ? shell.content.height : shell.grid_viewport.height;
    const Rectangle opposite_side = source_is_in_right_panel
        ? Rectangle{shell.content.x, opposite_y,
            std::max(0.0F, source.x - source_gap - shell.content.x),
            opposite_height}
        : Rectangle{source_right + source_gap, opposite_y,
            std::max(0.0F, content_right - source_right - source_gap),
            opposite_height};
    const std::array<Rectangle, 3U> candidates{{
        preferred, full_preferred, opposite_side}};

    for (const Rectangle target_panel : candidates) {
        InventoryTooltipLayout output{};
        output.font_size = font_size;
        output.title_font_size = title_font_size;
        output.line_height = line_height;
        if (!finite_rectangle(target_panel)
                || target_panel.height <= padding * 2.0F) {
            continue;
        }
        const float backing_width = std::min(target_panel.width,
            std::clamp(360.0F * shell.geometry_scale, 224.0F, 630.0F));
        if (backing_width < 224.0F) continue;
        const float maximum_width = backing_width - padding * 2.0F;
        if (maximum_width <= 0.0F) continue;
        const UiTextExtent title_extent = measure(view.title.data(),
            output.title_font_size, 0.0F, measure_context);
        if (!std::isfinite(title_extent.width)
                || !std::isfinite(title_extent.height)
                || title_extent.width > maximum_width) {
            continue;
        }
        bool candidate_valid = true;
        std::size_t visual_lines{};
        float preview_x{};
        for (std::size_t line{}; line < view.line_count; ++line) {
            const InventoryTooltipLine& logical = view.lines[line];
            for (std::size_t segment{}; segment < logical.segment_count;
                    ++segment) {
                const UiTextExtent extent = measure(
                    logical.segments[segment].text.data(), output.font_size,
                    0.0F, measure_context);
                if (!std::isfinite(extent.width)
                        || !std::isfinite(extent.height)
                        || extent.width > maximum_width) {
                    candidate_valid = false;
                    break;
                }
                if (preview_x != 0.0F
                        && preview_x + extent.width > maximum_width) {
                    ++visual_lines;
                    preview_x = 0.0F;
                }
                preview_x += extent.width + 4.0F;
            }
            if (!candidate_valid) break;
            ++visual_lines;
            preview_x = 0.0F;
        }
        if (!candidate_valid) continue;
        const float requested_height = padding * 2.0F + title_extent.height
            + title_gap + output.line_height
                * static_cast<float>(visual_lines);
        if (requested_height > target_panel.height) continue;
        const float x = target_panel.x
            + (target_panel.width - backing_width) * 0.5F;
        const float y = std::clamp(source.y, target_panel.y,
            target_panel.y + target_panel.height - requested_height);
        output.backing = {x, y, backing_width, requested_height};
        const bool separated_from_source =
            output.backing.x + output.backing.width + source_gap <= source.x
            || source_right + source_gap <= output.backing.x;
        if (!rectangle_inside(output.backing, shell.content)
                || !separated_from_source) {
            continue;
        }
        output.content_bounds = {x + padding, y + padding, maximum_width,
            requested_height - padding * 2.0F};
        output.title_position = {output.content_bounds.x,
            output.content_bounds.y};
        output.retention_corridor = {std::min(source.x, output.backing.x),
            std::min(source.y, output.backing.y),
            std::max(source_right, output.backing.x + output.backing.width)
                - std::min(source.x, output.backing.x),
            std::max(source.y + source.height,
                output.backing.y + output.backing.height)
                - std::min(source.y, output.backing.y)};
        float cursor_x = output.content_bounds.x;
        float cursor_y = output.content_bounds.y + title_extent.height
            + title_gap;
        for (std::size_t line{}; line < view.line_count; ++line) {
            const InventoryTooltipLine& logical = view.lines[line];
            for (std::size_t segment{}; segment < logical.segment_count;
                    ++segment) {
                const InventoryTooltipSegment& text = logical.segments[segment];
                const UiTextExtent extent = measure(text.text.data(),
                    output.font_size, 0.0F, measure_context);
                if (!std::isfinite(extent.width)
                        || !std::isfinite(extent.height)
                        || extent.width > output.content_bounds.width
                        || output.placement_count
                            >= output.placements.size()) {
                    candidate_valid = false;
                    break;
                }
                if (cursor_x != output.content_bounds.x
                        && cursor_x + extent.width
                            > output.content_bounds.x
                                + output.content_bounds.width) {
                    cursor_x = output.content_bounds.x;
                    cursor_y += output.line_height;
                    ++output.visual_line_count;
                }
                if (cursor_y + output.line_height
                        > output.content_bounds.y
                            + output.content_bounds.height) {
                    candidate_valid = false;
                    break;
                }
                output.placements[output.placement_count++] = {
                    static_cast<std::uint8_t>(line),
                    static_cast<std::uint8_t>(segment),
                    {cursor_x, cursor_y}};
                cursor_x += extent.width + 4.0F;
            }
            if (!candidate_valid) break;
            cursor_x = output.content_bounds.x;
            cursor_y += output.line_height;
            ++output.visual_line_count;
        }
        if (!candidate_valid) continue;
        output.blocks_pointer = true;
        output.valid = output.placement_count != 0U;
        if (output.valid) return output;
    }
    return {};
}

InventoryTooltipInteractionState inventory_tooltip_state_with_retained_source(
    InventoryTooltipInteractionState state,
    InventoryVisibleItemSource source,
    Rectangle retention_corridor) noexcept {
    const std::uint64_t retained_item_id = state.frozen_item_id != 0U
        ? state.frozen_item_id : state.hovered_item_id;
    if (!state.visible || !source.valid || source.item_id != retained_item_id
        || !finite_rectangle(source.bounds)
        || !finite_rectangle(retention_corridor)) return state;
    state.hovered_item_id = source.item_id;
    state.frozen_item_id = source.item_id;
    state.source = source.bounds;
    state.retention_corridor = retention_corridor;
    state.source_is_in_right_panel = source.source_is_in_right_panel;
    return state;
}

InventoryTooltipInteractionResult inventory_tooltip_transition(
    InventoryTooltipInteractionState state,
    const InventoryTooltipInteractionInput& input) noexcept {
    InventoryTooltipInteractionResult result{};
    result.blocks_pointer = input.pointer_inside_tooltip;
    if (input.source_invalidated) {
        result.blocks_pointer = false;
        result.state = {};
        return result;
    }
    const bool in_corridor = finite_rectangle(state.retention_corridor)
        && contains(state.retention_corridor, input.pointer);
    if (input.close_requested || input.background_clear_requested) {
        const std::uint64_t closing = state.frozen_item_id != 0U
            ? state.frozen_item_id : state.hovered_item_id;
        state.hovered_item_id = 0U;
        state.frozen_item_id = 0U;
        state.visible = false;
        state.suppressed_item_id = closing;
        result.clear_persistent_selection = input.background_clear_requested
            || input.clear_persistent_selection_requested;
        result.state = state;
        return result;
    }
    const bool candidate_valid = input.hover_candidate_item_id != 0U
        && finite_rectangle(input.hover_candidate_source);
    if (state.suppressed_item_id != 0U) {
        if (in_corridor) {
            result.state = state;
            return result;
        }
        state.suppressed_item_id = 0U;
    }
    if (candidate_valid && input.hover_candidate_item_id
            != state.suppressed_item_id) {
        state.hovered_item_id = input.hover_candidate_item_id;
        state.frozen_item_id = input.hover_candidate_item_id;
        state.source = input.hover_candidate_source;
        state.retention_corridor = input.hover_candidate_corridor;
        state.source_is_in_right_panel =
            input.hover_candidate_source_is_in_right_panel;
        state.visible = true;
    } else if (state.visible && in_corridor) {
        state.frozen_item_id = state.hovered_item_id;
    } else if (!in_corridor) {
        state.hovered_item_id = 0U;
        state.frozen_item_id = 0U;
        state.visible = false;
    }
    result.state = state;
    return result;
}

float inventory_grid_scroll_after_wheel(InventoryPage page,
    InventoryShellLayout shell, Vector2 pointer, bool pointer_blocked,
    float wheel_delta, std::size_t filtered_count,
    float current_scroll_rows) noexcept {
    if (!shell.valid || !std::isfinite(wheel_delta)
        || !std::isfinite(current_scroll_rows) || pointer_blocked
        || page != InventoryPage::equipment || wheel_delta == 0.0F
        || !contains(shell.grid_viewport, pointer)) return current_scroll_rows;
    return clamp_inventory_scroll_rows(filtered_count, inventory_grid_columns(shell),
        current_scroll_rows - wheel_delta, shell.grid_viewport.height,
        shell.grid_row_stride);
}

InventoryPointerLayer inventory_pointer_layer(
    const InventoryPointerPriorityInput& input) noexcept {
    if (input.confirmation) return InventoryPointerLayer::confirmation;
    if (input.tab) return InventoryPointerLayer::tab;
    if (input.tooltip) return InventoryPointerLayer::tooltip;
    if (input.directed_material) return InventoryPointerLayer::directed_material;
    if (input.material_cancel_requested)
        return InventoryPointerLayer::material_cancel;
    if (input.material_select) return InventoryPointerLayer::material_select;
    if (input.material_target) return InventoryPointerLayer::material_target;
    if (input.page_action && (input.page != InventoryPage::equipment
            || !input.material_selected))
        return InventoryPointerLayer::page_action;
    if (input.filter) return InventoryPointerLayer::filter;
    if (input.skill_content) return InventoryPointerLayer::skill_content;
    if (input.recipe) return InventoryPointerLayer::recipe;
    if (input.equipment) return InventoryPointerLayer::equipment;
    return input.background_clear ? InventoryPointerLayer::background_clear
                                  : InventoryPointerLayer::none;
}

ItemAttributeLabel item_attribute_label(items::ItemEffectKind effect,
    modifiers::StatId stat, modifiers::ModifierOperation operation,
    items::ItemSlot slot, std::uint8_t variant) noexcept {
    const bool local = effect == items::ItemEffectKind::local_weapon_physical_flat
        || effect == items::ItemEffectKind::local_weapon_physical_increased
        || (effect == items::ItemEffectKind::slot_dependent_attack_speed
            && slot == items::ItemSlot::weapon);
    const bool multiplier = operation == modifiers::ModifierOperation::more;
    const char* name = multiplier ? final_stat_name(stat) : stat_name(stat);
    if (effect == items::ItemEffectKind::local_weapon_physical_flat
        || effect == items::ItemEffectKind::local_weapon_physical_increased) {
        name = multiplier ? u8"最终武器物理伤害" : u8"武器物理伤害";
    } else if (effect == items::ItemEffectKind::slot_dependent_attack_speed) {
        name = multiplier ? u8"最终攻击速度" : u8"攻击速度";
    } else if (effect == items::ItemEffectKind::all_element_damage_reduction) {
        name = multiplier ? u8"最终全元素减伤" : u8"全元素减伤";
    } else if (effect == items::ItemEffectKind::tri_element_damage_reduction) {
        name = multiplier ? u8"最终三元素减伤" : u8"三元素减伤";
    } else if (effect
            == items::ItemEffectKind::variant_element_damage_reduction_cap) {
        switch (variant) {
        case 0U: name = multiplier ? u8"最终火焰减伤上限"
                                   : u8"火焰减伤上限"; break;
        case 1U: name = multiplier ? u8"最终水流减伤上限"
                                   : u8"水流减伤上限"; break;
        case 2U: name = multiplier ? u8"最终闪电减伤上限"
                                   : u8"闪电减伤上限"; break;
        case 3U: name = multiplier ? u8"最终混沌减伤上限"
                                   : u8"混沌减伤上限"; break;
        default: name = multiplier ? u8"最终元素减伤上限"
                                   : u8"元素减伤上限"; break;
        }
    }
    const bool semantic_increased = effect
        == items::ItemEffectKind::local_weapon_physical_increased;
    const bool semantic_percent = semantic_increased
        || effect == items::ItemEffectKind::slot_dependent_attack_speed;
    const bool percent = !multiplier && (semantic_percent
        || operation != modifiers::ModifierOperation::flat
        || percent_stat(stat)
        || effect == items::ItemEffectKind::all_element_damage_reduction
        || effect == items::ItemEffectKind::tri_element_damage_reduction
        || effect
            == items::ItemEffectKind::variant_element_damage_reduction_cap);
    const char* qualifier = "";
    if (semantic_increased
        || operation == modifiers::ModifierOperation::increased) {
        qualifier = u8"提高";
    } else if (operation == modifiers::ModifierOperation::conversion) {
        qualifier = u8"转化";
    }
    return {name, local ? u8"局部" : u8"全局",
        percent ? "%" : "", qualifier, multiplier};
}

double item_attribute_display_value(std::int32_t raw_value,
    ItemAttributeLabel label) noexcept {
    return label.multiplier
        ? static_cast<double>(raw_value)
            / static_cast<double>(modifiers::kFixedOne)
        : label.suffix != nullptr && label.suffix[0] == '%'
        ? static_cast<double>(raw_value) / 100.0
        : static_cast<double>(raw_value);
}

}  // namespace arpg::platform
