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

constexpr float kMargin = 12.0F;
constexpr float kTop = 54.0F;
constexpr float kGap = 10.0F;
constexpr float kEquipmentFraction = 0.27F;
constexpr float kGridFraction = 0.42F;
constexpr double kDoubleClickSeconds = 0.30;

constexpr float kSkillMainWidth = 204.0F;
constexpr float kSkillMainHeight = 92.0F;
constexpr float kSkillMainGap = 12.0F;
constexpr float kSkillSupportSize = 54.0F;
constexpr float kSkillSupportGap = 12.0F;
constexpr float kPageButtonWidth = 136.0F;
constexpr float kPageButtonHeight = 30.0F;
constexpr float kDetailTop = 40.0F;
constexpr float kDetailLineHeight = 20.0F;
constexpr float kDetailBottomInset = 4.0F;
constexpr std::size_t kMaximumDetailLineCount = 21U;

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

void tooltip_add_delta(InventoryTooltipView& view, const char* label,
    std::int64_t value) noexcept {
    if (value == 0 || view.line_count >= view.lines.size()) return;
    InventoryTooltipLine& line = view.lines[view.line_count++];
    line.segment_count = 1U;
    line.segments[0].tone = value > 0 ? InventoryTooltipTone::positive
                                      : InventoryTooltipTone::negative;
    static_cast<void>(ui_utf8_format(line.segments[0].text.data(),
        line.segments[0].text.size(), "%s %+lld", label,
        static_cast<long long>(value)));
}

InventoryTransientStatusPresentation transient_presentation(
    const InventoryTransientStatusState& state) noexcept {
    return {state.kind, state.receipt_success, state.receipt_destroyed,
        state.receipt_coupon, state.receipt_before, state.receipt_after};
}

}  // namespace

InventoryLayout inventory_layout(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return {};
    const float viewport_width = static_cast<float>(width);
    const float viewport_height = static_cast<float>(height);
    const float scale = ui_viewport_scale(width, height);
    const float margin = kMargin * scale;
    const float top = kTop * scale;
    const float gap = kGap * scale;
    const float content_width = std::max(0.0F,
        viewport_width - margin * 2.0F - gap * 2.0F);
    const float content_height = std::max(0.0F,
        viewport_height - top - margin);
    const float equipment_width = std::floor(content_width * kEquipmentFraction);
    const float grid_width = std::floor(content_width * kGridFraction);
    const float detail_width = std::max(0.0F,
        content_width - equipment_width - grid_width);
    const Rectangle equipment{margin, top, equipment_width, content_height};
    const Rectangle grid{equipment.x + equipment.width + gap, top,
        grid_width, content_height};
    return {equipment, grid,
        {grid.x + grid.width + gap, top, detail_width, content_height}, scale};
}

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
    const float margin = 12.0F * geometry_scale;
    const float gap = 10.0F * geometry_scale;
    const float header_height = std::max(40.0F, 54.0F * geometry_scale);
    const float status_height = std::max(46.0F, 58.0F * geometry_scale);
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
    const float left_width = std::max(180.0F,
        std::floor(layout.content.width * 0.30F));
    if (left_width + gap + 180.0F > layout.content.width) return {};
    layout.left_panel = {layout.content.x, layout.content.y, left_width,
        layout.content.height};
    layout.right_panel = {layout.left_panel.x + layout.left_panel.width + gap,
        layout.content.y, layout.content.width - left_width - gap,
        layout.content.height};
    const float tab_height = std::max(32.0F, 36.0F * geometry_scale);
    const float tab_gap = std::max(4.0F, 6.0F * geometry_scale);
    const float tab_width = (layout.header.width - tab_gap * 2.0F) / 3.0F;
    for (std::size_t index{}; index < layout.tabs.size(); ++index) {
        layout.tabs[index] = {layout.header.x + static_cast<float>(index)
                * (tab_width + tab_gap),
            layout.header.y + (layout.header.height - tab_height) * 0.5F,
            tab_width, tab_height};
    }
    const float panel_padding = 10.0F * geometry_scale;
    const float slot_height = std::max(32.0F, 38.0F * geometry_scale);
    const float slot_gap = std::max(4.0F, 7.0F * geometry_scale);
    for (std::size_t index{}; index < layout.equipment_slots.size(); ++index) {
        layout.equipment_slots[index] = {layout.left_panel.x + panel_padding,
            layout.left_panel.y + panel_padding + static_cast<float>(index)
                * (slot_height + slot_gap),
            layout.left_panel.width - panel_padding * 2.0F, slot_height};
    }
    layout.silhouette = {layout.left_panel.x + layout.left_panel.width * 0.25F,
        layout.equipment_slots.back().y + layout.equipment_slots.back().height
            + panel_padding,
        layout.left_panel.width * 0.5F,
        std::max(0.0F, layout.left_panel.y + layout.left_panel.height
            - (layout.equipment_slots.back().y
                + layout.equipment_slots.back().height + panel_padding))};
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
    layout.action_bounds = {layout.status_bar.x + layout.status_bar.width
            - action_width - panel_padding,
        layout.status_bar.y + panel_padding * 0.5F, action_width,
        layout.status_bar.height - panel_padding};
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
    layout.valid = finite_rectangle(layout.header)
        && finite_rectangle(layout.left_panel) && finite_rectangle(layout.right_panel)
        && finite_rectangle(layout.status_bar) && finite_rectangle(layout.grid_viewport)
        && layout.status_bounds.width > 0.0F;
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

ActiveSkillLoadoutLayout active_skill_loadout_layout(
    int width, int height) noexcept {
    if (width <= 0 || height <= 0) return {};
    const float screen_width = static_cast<float>(width);
    const float screen_height = static_cast<float>(height);
    const float viewport_scale = ui_viewport_scale(width, height);
    const float unscaled_main_width =
        static_cast<float>(skills::kActiveSkillSlotCount) * kSkillMainWidth
        + static_cast<float>(skills::kActiveSkillSlotCount - 1U)
            * kSkillMainGap;
    const float width_fit_scale = std::max(0.0F, screen_width - 40.0F)
        / unscaled_main_width;
    const float scale = std::min(viewport_scale, width_fit_scale);
    ActiveSkillLoadoutLayout layout{};
    layout.scale = scale;
    layout.panel = {40.0F * scale, 70.0F * scale,
        std::max(0.0F, screen_width - 80.0F * scale),
        std::max(0.0F, screen_height - 100.0F * scale)};
    layout.equipment_page_button = {
        screen_width * 0.5F - (kPageButtonWidth + 6.0F) * scale,
        18.0F * scale, kPageButtonWidth * scale, kPageButtonHeight * scale,
    };
    layout.skill_stones_page_button = {
        screen_width * 0.5F + 6.0F * scale,
        18.0F * scale, kPageButtonWidth * scale, kPageButtonHeight * scale,
    };

    const float main_width = scale * unscaled_main_width;
    const float main_x = (screen_width - main_width) * 0.5F;
    const float main_y = layout.panel.y + 62.0F * scale;
    for (std::size_t index = 0U; index < layout.main_slots.size(); ++index) {
        layout.main_slots[index] = {
            main_x + static_cast<float>(index)
                * (kSkillMainWidth + kSkillMainGap) * scale,
            main_y, kSkillMainWidth * scale, kSkillMainHeight * scale,
        };
    }

    const float support_width = scale * (
        static_cast<float>(skills::kSupportSlotsPerActive) * kSkillSupportSize
        + static_cast<float>(skills::kSupportSlotsPerActive - 1U)
            * kSkillSupportGap);
    const float support_x = (screen_width - support_width) * 0.5F;
    const float support_y = main_y + (kSkillMainHeight + 78.0F) * scale;
    for (std::size_t index = 0U; index < layout.support_slots.size(); ++index) {
        layout.support_slots[index] = {
            support_x + static_cast<float>(index)
                * (kSkillSupportSize + kSkillSupportGap) * scale,
            support_y, kSkillSupportSize * scale, kSkillSupportSize * scale,
        };
    }

    constexpr float kInventoryWidth = 180.0F;
    constexpr float kInventoryHeight = 62.0F;
    constexpr float kInventoryGap = 18.0F;
    const float inventory_total = scale * (
        static_cast<float>(skills::kActiveSkillCount) * kInventoryWidth
        + static_cast<float>(skills::kActiveSkillCount - 1U) * kInventoryGap);
    const float inventory_x = (screen_width - inventory_total) * 0.5F;
    const float inventory_y = support_y + (kSkillSupportSize + 82.0F) * scale;
    for (std::size_t index = 0U; index < layout.inventory_slots.size(); ++index) {
        layout.inventory_slots[index] = {
            inventory_x + static_cast<float>(index)
                * (kInventoryWidth + kInventoryGap) * scale,
            inventory_y, kInventoryWidth * scale, kInventoryHeight * scale,
        };
    }
    layout.remove_button = {
        (screen_width - 164.0F * scale) * 0.5F,
        std::min(inventory_y + (kInventoryHeight + 42.0F) * scale,
            screen_height - 62.0F * scale),
        164.0F * scale, 38.0F * scale,
    };
    return layout;
}

InventoryTextSafeLayout inventory_text_safe_layout(
    int width, int height) noexcept {
    if (width <= 0 || height <= 0) return {};
    const InventoryLayout panels = inventory_layout(width, height);
    const ActiveSkillLoadoutLayout skill = active_skill_loadout_layout(
        width, height);
    const float scale = panels.scale;
    const auto panel_title = [scale](Rectangle panel) noexcept {
        return Rectangle{panel.x + 40.0F * scale, panel.y + 10.0F * scale,
            std::max(0.0F, panel.width - 56.0F * scale), 26.0F * scale};
    };
    return {
        {14.0F * scale, 24.0F * scale,
            std::max(0.0F, skill.equipment_page_button.x - 28.0F * scale),
            34.0F * scale},
        panel_title(panels.equipment),
        panel_title(panels.grid),
        panel_title(panels.detail),
        {skill.panel.x + 40.0F * scale, skill.panel.y + 14.0F * scale,
            std::max(0.0F, skill.panel.width - 80.0F * scale), 28.0F * scale},
        {skill.support_slots[0U].x,
            skill.support_slots[0U].y - 29.0F * scale,
            240.0F * scale, 23.0F * scale},
        {skill.inventory_slots[0U].x,
            skill.inventory_slots[0U].y - 29.0F * scale,
            240.0F * scale, 23.0F * scale},
    };
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

Rectangle equipment_slot_rectangle(InventoryLayout layout,
    items::ItemSlot slot) noexcept {
    constexpr float kInset = 12.0F;
    constexpr float kSlotHeight = 30.0F;
    constexpr float kSlotGap = 4.0F;
    const std::size_t index = static_cast<std::size_t>(slot);
    if (index >= static_cast<std::size_t>(items::ItemSlot::count)) return {};
    const float scale = layout.scale;
    return {layout.equipment.x + kInset * scale,
        layout.equipment.y + 36.0F * scale
            + static_cast<float>(index) * (kSlotHeight + kSlotGap) * scale,
        std::max(0.0F, layout.equipment.width - kInset * 2.0F * scale),
        kSlotHeight * scale};
}

std::optional<items::ItemSlot> hit_test_equipped_slot(Vector2 point,
    InventoryLayout layout, const items::EquipmentState& equipment) noexcept {
    for (std::size_t index = 0U; index < equipment.equipped_ids.size(); ++index) {
        if (equipment.equipped_ids[index] == 0U) continue;
        const auto slot = static_cast<items::ItemSlot>(index);
        if (contains(equipment_slot_rectangle(layout, slot), point)) return slot;
    }
    return std::nullopt;
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
        for (std::size_t selected = 0U; selected < recipe.count; ++selected) {
            if (!is_equipped && item.id == recipe.ids[selected]) {
                cache.recipe_indices[selected] = index;
            }
        }
        if (matches_inventory_filter(item, is_equipped, filter)) {
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
    const items::ItemOwnershipState& state, std::size_t filtered_count) noexcept {
    if (filtered_count != 0U) return {};
    if (state.items.empty()) return {InventoryEmptyState::inventory_empty,
        ui_text(UiTextId::inventory_empty_title),
        ui_text(UiTextId::inventory_empty_hint)};
    bool any_unequipped{};
    for (const items::ItemInstance& item : state.items) {
        if (!equipped(state.equipment, item.id)) {
            any_unequipped = true;
            break;
        }
    }
    if (!any_unequipped) return {InventoryEmptyState::all_equipped,
        ui_text(UiTextId::inventory_all_equipped_title),
        ui_text(UiTextId::inventory_all_equipped_hint)};
    return {InventoryEmptyState::filter_empty,
        ui_text(UiTextId::inventory_filter_empty_title),
        ui_text(UiTextId::inventory_filter_empty_hint)};
}

InventoryStatusPresentation inventory_status_presentation(
    const InventoryStatusInput& input) noexcept {
    InventoryStatusPresentation output{};
    static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
        ui_text(UiTextId::inventory_context_hint)));
    static_cast<void>(ui_utf8_copy(output.hint.data(), output.hint.size(),
        ui_text(UiTextId::inventory_context_hint)));
    output.tone = InventoryStatusTone::muted;
    if (input.save_state == InventorySavePresentationState::saving) {
        static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
            ui_text(UiTextId::save_saving)));
        output.tone = InventoryStatusTone::warning;
        return output;
    }
    if (input.save_state == InventorySavePresentationState::error
        || input.recovery_required) {
        static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
            ui_text(UiTextId::save_error)));
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
    if (input.selected_material.has_value()) {
        static_cast<void>(ui_utf8_format(output.status.data(), output.status.size(),
            ui_text(UiTextId::inventory_material_selected_format),
            ui_material_name(*input.selected_material)));
        output.tone = InventoryStatusTone::warning;
        return output;
    }
    if (input.page == InventoryPage::skill_stones && input.active_skill_removable) {
        static_cast<void>(ui_utf8_copy(output.action.data(), output.action.size(),
            ui_text(UiTextId::inventory_action_remove)));
        output.action_kind = InventoryActionKind::remove_active_skill;
        output.action_visible = true;
        output.action_enabled = input.authority_enabled;
        output.tone = InventoryStatusTone::positive;
        return output;
    }
    if (input.recipe_count != 0U) {
        static_cast<void>(ui_utf8_format(output.action.data(), output.action.size(),
            ui_text(UiTextId::inventory_recipe_action_format),
            static_cast<unsigned int>(input.recipe_count)));
        output.action_kind = InventoryActionKind::combine;
        output.action_visible = true;
        output.action_enabled = input.recipe_ready && input.authority_enabled;
        if (input.reinforced_recipe_input) {
            static_cast<void>(ui_utf8_copy(output.status.data(), output.status.size(),
                ui_text(UiTextId::inventory_recipe_danger)));
            output.tone = InventoryStatusTone::negative;
        } else {
            output.tone = input.recipe_ready ? InventoryStatusTone::positive
                                             : InventoryStatusTone::warning;
        }
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
        state.last_receipt_generation = input.receipt_commit_generation;
        result.next = state;
        return result;
    }
    if (input.commit_generation < state.last_commit_generation
        || input.receipt_commit_generation < state.last_receipt_generation) {
        state.kind = InventoryTransientStatusKind::none;
        state.remaining_frames = 0U;
        state.last_commit_generation = input.commit_generation;
        state.last_receipt_generation = input.receipt_commit_generation;
        result.next = state;
        return result;
    }
    const bool new_commit = input.commit_generation > state.last_commit_generation;
    const bool new_receipt = input.receipt_valid
        && input.receipt_commit_generation > state.last_receipt_generation;
    state.last_commit_generation = std::max(state.last_commit_generation,
        input.commit_generation);
    state.last_receipt_generation = std::max(state.last_receipt_generation,
        input.receipt_commit_generation);
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

InventoryTooltipView inventory_tooltip_view(const items::ItemInstance& item,
    const std::optional<BuildDifference>& difference) noexcept {
    InventoryTooltipView view{};
    if (item.id == 0U || items::base_definition(item.base_id) == nullptr)
        return view;
    static_cast<void>(ui_utf8_copy(view.title.data(), view.title.size(),
        ui_item_base_name(item.base_id)));
    tooltip_add_segment(view, ui_item_rarity_name(item.rarity),
        InventoryTooltipTone::rarity);
    std::array<char, 64> line{};
    static_cast<void>(ui_utf8_format(line.data(), line.size(), u8"物品等级 %u",
        static_cast<unsigned int>(item.item_level)));
    tooltip_add_segment(view, line.data(), InventoryTooltipTone::muted);
    const std::size_t affix_count = std::min<std::size_t>(item.affix_count,
        item.affixes.size());
    for (std::size_t index{}; index < affix_count; ++index) {
        const items::AffixRoll& roll = item.affixes[index];
        const auto value = items::affix_roll_value(roll);
        static_cast<void>(ui_utf8_format(line.data(), line.size(), u8"%s %+d",
            ui_item_affix_name(roll.affix_id), value.value_or(0)));
        tooltip_add_segment(view, line.data(), InventoryTooltipTone::primary);
    }
    if (difference.has_value()) {
        const BuildDifference& delta = *difference;
        tooltip_add_delta(view, u8"生命", delta.max_health);
        tooltip_add_delta(view, u8"护盾", delta.max_barrier);
        tooltip_add_delta(view, u8"近战伤害", delta.melee_damage);
        tooltip_add_delta(view, u8"击退", delta.impulse_scale);
        tooltip_add_delta(view, u8"移动速度", delta.move_speed);
        tooltip_add_delta(view, u8"攻击速度", delta.attack_speed);
        tooltip_add_delta(view, u8"护甲", delta.armor);
        tooltip_add_delta(view, u8"闪避", delta.evasion);
        tooltip_add_delta(view, u8"武器物理", delta.weapon_physical);
        for (std::size_t index{}; index < delta.flat_damage.size(); ++index)
            tooltip_add_delta(view, u8"元素伤害", delta.flat_damage[index]);
        for (std::size_t index{}; index < delta.damage_increased.size(); ++index)
            tooltip_add_delta(view, u8"元素提高", delta.damage_increased[index]);
        for (std::size_t index{}; index < delta.damage_reduction.size(); ++index)
            tooltip_add_delta(view, u8"元素减伤", delta.damage_reduction[index]);
    }
    view.valid = view.title[0] != '\0' && view.line_count != 0U;
    return view;
}

InventoryTooltipLayout inventory_tooltip_layout(InventoryShellLayout shell,
    Rectangle source, bool source_is_in_right_panel,
    const InventoryTooltipView& view, UiTextMeasureFn measure,
    void* measure_context) noexcept {
    InventoryTooltipLayout output{};
    if (!shell.valid || !finite_rectangle(source) || !view.valid
        || measure == nullptr) return output;
    output.font_size = std::max(12.0F, shell.body_font_size);
    output.title_font_size = std::max(12.0F, output.font_size + 2.0F);
    output.line_height = std::max(14.0F, output.font_size * 1.25F);
    constexpr float kPadding = 10.0F;
    const float maximum_width = std::min(224.0F, shell.content.width - 2.0F * kPadding);
    if (maximum_width < 80.0F) return output;
    const UiTextExtent title_extent = measure(view.title.data(),
        output.title_font_size, 0.0F, measure_context);
    if (!std::isfinite(title_extent.width) || !std::isfinite(title_extent.height)
        || title_extent.width > maximum_width) return output;
    std::size_t visual_lines{};
    float preview_x{};
    for (std::size_t line{}; line < view.line_count; ++line) {
        const InventoryTooltipLine& logical = view.lines[line];
        for (std::size_t segment{}; segment < logical.segment_count; ++segment) {
            const UiTextExtent extent = measure(logical.segments[segment].text.data(),
                output.font_size, 0.0F, measure_context);
            if (!std::isfinite(extent.width) || !std::isfinite(extent.height)
                || extent.width > maximum_width) return {};
            if (preview_x != 0.0F && preview_x + extent.width > maximum_width) {
                ++visual_lines;
                preview_x = 0.0F;
            }
            preview_x += extent.width + 4.0F;
        }
        ++visual_lines;
        preview_x = 0.0F;
    }
    const float requested_height = kPadding * 2.0F + title_extent.height
        + output.line_height * (static_cast<float>(visual_lines) + 0.25F);
    const float backing_height = std::min(requested_height,
        std::max(48.0F, shell.content.height - kPadding * 2.0F));
    if (backing_height < kPadding * 2.0F + title_extent.height) return output;
    const float backing_width = maximum_width + kPadding * 2.0F;
    float x = source_is_in_right_panel
        ? source.x - backing_width - kPadding : source.x + source.width + kPadding;
    x = std::clamp(x, shell.viewport.x + kPadding,
        shell.viewport.x + shell.viewport.width - backing_width - kPadding);
    const float y = std::clamp(source.y, shell.content.y + kPadding,
        shell.content.y + shell.content.height - backing_height - kPadding);
    output.backing = {x, y, backing_width, backing_height};
    output.content_bounds = {x + kPadding, y + kPadding, maximum_width,
        backing_height - kPadding * 2.0F};
    output.title_position = {output.content_bounds.x,
        output.content_bounds.y};
    output.retention_corridor = {std::min(source.x, output.backing.x),
        std::min(source.y, output.backing.y),
        std::max(source.x + source.width, output.backing.x + output.backing.width)
            - std::min(source.x, output.backing.x),
        std::max(source.y + source.height, output.backing.y + output.backing.height)
            - std::min(source.y, output.backing.y)};
    float cursor_x = output.content_bounds.x;
    float cursor_y = output.content_bounds.y + title_extent.height
        + output.line_height * 0.25F;
    for (std::size_t line{}; line < view.line_count; ++line) {
        const InventoryTooltipLine& logical = view.lines[line];
        for (std::size_t segment{}; segment < logical.segment_count; ++segment) {
            const InventoryTooltipSegment& text = logical.segments[segment];
            const UiTextExtent extent = measure(text.text.data(), output.font_size,
                0.0F, measure_context);
            if (!std::isfinite(extent.width) || !std::isfinite(extent.height)
                || extent.width > output.content_bounds.width
                || output.placement_count >= output.placements.size()) return {};
            if (cursor_x != output.content_bounds.x
                && cursor_x + extent.width > output.content_bounds.x
                    + output.content_bounds.width) {
                cursor_x = output.content_bounds.x;
                cursor_y += output.line_height;
                ++output.visual_line_count;
            }
            if (cursor_y + output.line_height > output.content_bounds.y
                    + output.content_bounds.height) return {};
            output.placements[output.placement_count++] = {
                static_cast<std::uint8_t>(line), static_cast<std::uint8_t>(segment),
                {cursor_x, cursor_y}};
            cursor_x += extent.width + 4.0F;
        }
        cursor_x = output.content_bounds.x;
        cursor_y += output.line_height;
        ++output.visual_line_count;
    }
    output.blocks_pointer = true;
    output.valid = output.placement_count != 0U;
    return output;
}

InventoryTooltipInteractionResult inventory_tooltip_transition(
    InventoryTooltipInteractionState state,
    const InventoryTooltipInteractionInput& input) noexcept {
    InventoryTooltipInteractionResult result{};
    result.blocks_pointer = state.visible || input.pointer_inside_tooltip;
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
        if (candidate_valid && input.hover_candidate_item_id
                == state.suppressed_item_id && in_corridor) {
            result.state = state;
            return result;
        }
        if (!in_corridor) state.suppressed_item_id = 0U;
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
    if (input.page_action) return InventoryPointerLayer::page_action;
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

Rectangle detail_line_rectangle(InventoryLayout layout,
    std::size_t line) noexcept {
    constexpr float kInsetX = 12.0F;
    const float required_height = kDetailTop
        + kDetailLineHeight * static_cast<float>(kMaximumDetailLineCount);
    const float available_height = std::max(0.0F,
        layout.detail.height - kDetailBottomInset * layout.scale);
    const float vertical_scale = std::min(layout.scale,
        available_height / required_height);
    return {layout.detail.x + kInsetX * layout.scale,
        layout.detail.y + kDetailTop * vertical_scale
            + static_cast<float>(line) * kDetailLineHeight * vertical_scale,
        std::max(0.0F,
            layout.detail.width - kInsetX * 2.0F * layout.scale),
        kDetailLineHeight * vertical_scale};
}

InventoryDetailRowLayout inventory_detail_row_layout(
    InventoryLayout layout, std::size_t line) noexcept {
    const UiLabelValueRowLayout shared = ui_label_value_row_layout(
        detail_line_rectangle(layout, line), 10.0F * layout.scale);
    return {shared.label_bounds, shared.value_bounds, shared.gap, shared.valid};
}

bool detail_content_fits(InventoryLayout layout,
    std::size_t line_count) noexcept {
    if (line_count == 0U) return true;
    const Rectangle last = detail_line_rectangle(layout, line_count - 1U);
    return last.x >= layout.detail.x && last.y >= layout.detail.y
        && last.x + last.width <= layout.detail.x + layout.detail.width
        && last.y + last.height <= layout.detail.y + layout.detail.height;
}

std::size_t detail_line_character_capacity(InventoryLayout layout) noexcept {
    const Rectangle line = detail_line_rectangle(layout, 0U);
    const float conservative_character_width = 8.0F * layout.scale;
    return static_cast<std::size_t>(std::floor(
        line.width / conservative_character_width));
}

bool detail_text_fits(InventoryLayout layout,
    std::string_view text) noexcept {
    std::size_t codepoint_count{};
    for (const unsigned char byte : text) {
        if ((byte & 0xC0U) != 0x80U) ++codepoint_count;
    }
    return codepoint_count <= detail_line_character_capacity(layout);
}

}  // namespace arpg::platform
