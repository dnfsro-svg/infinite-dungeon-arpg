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
    const char* const format = page == InventoryPage::equipment_materials
        ? u8"按键提示：%s / ESC  ·  装备背包"
        : u8"按键提示：%s / ESC  ·  技能石背包";
    static_cast<void>(ui_utf8_format(
        title.data(), title.size(), format, binding.data()));
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
