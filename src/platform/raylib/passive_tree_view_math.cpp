#include "passive_tree_view_math.hpp"

#include "passives/passive_tree_catalog.hpp"
#include "passives/passive_tree_rules.hpp"
#include "ui_text_catalog.hpp"
#include "ui_utf8.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace arpg::platform {
namespace {

constexpr float kDesignWidth = 1200.0F;
constexpr float kDesignHeight = 760.0F;
constexpr float kDesignCenterX = kDesignWidth * 0.5F;
constexpr float kDesignCenterY = kDesignHeight * 0.5F;

bool valid_viewport(float width, float height) noexcept {
    return width > 0.0F && height > 0.0F;
}

float projection_scale(float width, float height) noexcept {
    return std::min(width / kDesignWidth, height / kDesignHeight);
}

float node_radius(passives::PassiveNodeType type) noexcept {
    switch (type) {
    case passives::PassiveNodeType::start: return 17.0F;
    case passives::PassiveNodeType::connector: return 12.0F;
    case passives::PassiveNodeType::small: return 11.0F;
    case passives::PassiveNodeType::notable: return 17.0F;
    case passives::PassiveNodeType::keystone: return 23.0F;
    }
    return 0.0F;
}

PassiveScreenPoint route_layout_offset(passives::PassiveNodeId node) noexcept {
    if (node < 8U || node >= passives::kPassiveNodeCount) return {};
    switch ((node - 8U) / 14U) {
    case 0U: return {-200.0F, -175.0F};
    case 1U: return {200.0F, -175.0F};
    case 2U: return {-200.0F, 175.0F};
    case 3U: return {200.0F, 175.0F};
    default: return {};
    }
}

bool node_is_allocated(const passives::PassiveTreeState& state,
    passives::PassiveNodeId node) noexcept {
    return node < passives::kPassiveNodeCount
        && (state.allocated_bits & (1ULL << node)) != 0U;
}

bool has_allocated_neighbor(const passives::PassiveTreeState& state,
    const passives::PassiveNode& node) noexcept {
    for (std::size_t index = 0U; index < node.neighbor_count; ++index) {
        if (node_is_allocated(state, node.neighbors[index])) return true;
    }
    return false;
}

std::size_t route_index(passives::PassiveNodeId node) noexcept {
    return node < 8U ? 0U : static_cast<std::size_t>((node - 8U) / 14U);
}

const char* passive_node_type_text(passives::PassiveNodeType type) noexcept {
    switch (type) {
    case passives::PassiveNodeType::start: return u8"起始";
    case passives::PassiveNodeType::connector: return u8"连接";
    case passives::PassiveNodeType::small: return u8"小节点";
    case passives::PassiveNodeType::notable: return u8"重要节点";
    case passives::PassiveNodeType::keystone: return u8"核心";
    }
    return u8"节点";
}

const char* passive_benefit_source(passives::PassiveNodeId node) noexcept {
    constexpr const char* kFlat[] = {
        u8"收益：+3 火焰伤害", u8"收益：+3 水流伤害",
        u8"收益：+3 闪电伤害", u8"收益：+3 混沌伤害"};
    constexpr const char* kDamage[] = {
        u8"收益：+8% 火焰伤害", u8"收益：+8% 水流伤害",
        u8"收益：+8% 闪电伤害", u8"收益：+8% 混沌伤害"};
    constexpr const char* kReduction[] = {
        u8"收益：+7% 火焰减伤", u8"收益：+7% 水流减伤",
        u8"收益：+7% 闪电减伤", u8"收益：+7% 混沌减伤"};
    constexpr const char* kNotableDamage[] = {
        u8"收益：+8 火焰伤害，+15% 火焰伤害",
        u8"收益：+8 水流伤害，+15% 水流伤害",
        u8"收益：+8 闪电伤害，+15% 闪电伤害",
        u8"收益：+8 混沌伤害，+15% 混沌伤害"};
    constexpr const char* kNotableReduction[] = {
        u8"收益：+15% 火焰减伤，+8 最大护盾",
        u8"收益：+15% 水流减伤，+8 最大护盾",
        u8"收益：+15% 闪电减伤，+8 最大护盾",
        u8"收益：+15% 混沌减伤，+8 最大护盾"};
    constexpr const char* kKeystone[] = {
        u8"收益：最终火焰伤害 ×1.30",
        u8"收益：最终水流伤害 ×1.25，+24 最大护盾",
        u8"收益：最终闪电伤害 ×1.25，移动速度/攻击速度 +12%",
        u8"收益：最终混沌伤害 ×1.30"};
    switch (node) {
    case 0U: return u8"收益：起始节点";
    case 1U: return u8"收益：+20 最大生命";
    case 2U: return u8"收益：+10 最大护盾，承受伤害 -3%";
    case 3U: return u8"收益：近战伤害 +6%";
    case 4U: return u8"收益：攻击速度 +5%";
    case 5U: return u8"收益：击退与挑飞 +10%";
    case 6U: return u8"收益：移动速度 +6%";
    case 7U: return u8"收益：跳跃速度/空中移动 +10%";
    default: break;
    }
    if (node < 8U || node >= passives::kPassiveNodeCount) {
        return u8"收益：无";
    }
    const std::size_t route = route_index(node);
    switch ((node - 8U) % 14U) {
    case 0U:
    case 1U: return u8"收益：路线连接";
    case 2U:
    case 3U: return kFlat[route];
    case 4U:
    case 7U: return kDamage[route];
    case 5U:
    case 6U: return kReduction[route];
    case 8U: return u8"收益：近战伤害 +4%";
    case 9U: return u8"收益：承受伤害 -2%";
    case 10U: return kNotableDamage[route];
    case 11U: return kNotableReduction[route];
    case 12U: return u8"收益：+12 最大生命，移动速度 +3%";
    case 13U: return kKeystone[route];
    default: return u8"收益：无";
    }
}

const char* passive_cost_source(passives::PassiveNodeId node) noexcept {
    switch (node) {
    case 21U: return u8"代价：-20% 水流减伤";
    case 35U: return u8"代价：最终近战伤害 ×0.85";
    case 49U: return u8"代价：最终最大生命 ×0.80";
    case 63U: return u8"代价：最终承受伤害 ×1.15";
    default: return u8"代价：无";
    }
}

template <std::size_t Capacity>
bool copy_complete(std::array<char, Capacity>& destination,
    const char* source) noexcept {
    const UiUtf8CopyResult result = ui_utf8_copy(
        destination.data(), destination.size(), source);
    return result.input_was_valid && !result.truncated
        && destination[0] != '\0';
}

bool finite_rectangle(Rectangle rectangle) noexcept {
    return std::isfinite(rectangle.x) && std::isfinite(rectangle.y)
        && std::isfinite(rectangle.width) && std::isfinite(rectangle.height)
        && rectangle.width > 0.0F && rectangle.height > 0.0F
        && std::isfinite(rectangle.x + rectangle.width)
        && std::isfinite(rectangle.y + rectangle.height);
}

}  // namespace

bool passive_tree_can_open(const dungeon::DungeonSnapshot& snapshot) noexcept {
    return snapshot.phase == dungeon::RoomPhase::awaiting_exit
        && !snapshot.passive_save_pending;
}

bool passive_tree_can_stay_open(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    return snapshot.phase == dungeon::RoomPhase::awaiting_exit
        || (snapshot.phase == dungeon::RoomPhase::committing
            && snapshot.passive_save_pending);
}

PassiveTreeToggleAction passive_tree_toggle_action(
    const dungeon::DungeonSnapshot& snapshot,
    bool pressed,
    bool open_input_available,
    bool overlay_open) noexcept {
    if (!pressed) return PassiveTreeToggleAction::none;
    if (overlay_open) return PassiveTreeToggleAction::toggle;
    if (!open_input_available) return PassiveTreeToggleAction::none;
    if (passive_tree_can_open(snapshot)) {
        return PassiveTreeToggleAction::toggle;
    }
    return snapshot.has_active_room
            && snapshot.phase == dungeon::RoomPhase::combat
            && snapshot.remaining_targets != 0U
        ? PassiveTreeToggleAction::show_full_clear_requirement
        : PassiveTreeToggleAction::none;
}

PassiveNodeProjection project_passive_node(passives::PassiveNodeId node,
    float width, float height) noexcept {
    if (node >= passives::kPassiveNodeCount || !valid_viewport(width, height)) {
        return {};
    }
    const float scale = projection_scale(width, height);
    const float offset_x = (width - kDesignWidth * scale) * 0.5F;
    const float offset_y = (height - kDesignHeight * scale) * 0.5F;
    const passives::PassiveNode& source = passives::passive_nodes()[node];
    const PassiveScreenPoint route_offset = route_layout_offset(node);
    return {{offset_x + (kDesignCenterX + source.x + route_offset.x) * scale,
                offset_y + (kDesignCenterY + source.y + route_offset.y) * scale},
        node_radius(source.type) * scale, true};
}

std::optional<passives::PassiveNodeId> hit_test_passive_node(
    PassiveScreenPoint screen, float width, float height) noexcept {
    std::optional<passives::PassiveNodeId> closest{};
    float closest_distance_squared{};
    for (std::size_t index = 0U; index < passives::kPassiveNodeCount; ++index) {
        const auto node = static_cast<passives::PassiveNodeId>(index);
        const PassiveNodeProjection projection = project_passive_node(node, width, height);
        if (!projection.visible) continue;
        const float dx = screen.x - projection.center.x;
        const float dy = screen.y - projection.center.y;
        const float distance_squared = dx * dx + dy * dy;
        const float hit_radius = std::max(12.0F,
            projection.radius + kPassiveNodeVisibleRingExtension);
        if (!(distance_squared <= hit_radius * hit_radius)) continue;
        if (!closest.has_value()
            || distance_squared < closest_distance_squared) {
            closest = node;
            closest_distance_squared = distance_squared;
        }
    }
    return closest;
}

PassiveNodeVisualState passive_node_visual_state(
    const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node) noexcept {
    if (node >= passives::kPassiveNodeCount) return PassiveNodeVisualState::locked;
    if (snapshot.passive_save_pending) return PassiveNodeVisualState::pending;
    if (node_is_allocated(snapshot.passive_tree, node)) {
        return PassiveNodeVisualState::allocated;
    }
    const passives::PassiveNode& candidate = passives::passive_nodes()[node];
    return passive_tree_can_open(snapshot)
            && snapshot.progression.unspent_passive_points > 0U
            && has_allocated_neighbor(snapshot.passive_tree, candidate)
        ? PassiveNodeVisualState::available : PassiveNodeVisualState::locked;
}

PassiveNodeVisualCue passive_node_visual_cue(
    PassiveNodeVisualState state) noexcept {
    switch (state) {
    case PassiveNodeVisualState::locked:
        return {u8"锁定", PassiveNodeStateMark::locked_cross};
    case PassiveNodeVisualState::available:
        return {u8"可分配", PassiveNodeStateMark::available_ring};
    case PassiveNodeVisualState::allocated:
        return {u8"已分配", PassiveNodeStateMark::allocated_dot};
    case PassiveNodeVisualState::pending:
        return {u8"保存中", PassiveNodeStateMark::pending_hourglass};
    }
    return {u8"锁定", PassiveNodeStateMark::locked_cross};
}

const char* passive_node_action_text(
    const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node) noexcept {
    if (node >= passives::kPassiveNodeCount) return u8"节点不可用";
    if (snapshot.passive_save_pending) return u8"被动星图保存中…";
    if (node_is_allocated(snapshot.passive_tree, node)) {
        if (node == 0U) return u8"起始节点不可退款";
        passives::PassiveTreeState tree = snapshot.passive_tree;
        progression::ProgressionState progression_state = snapshot.progression;
        const passives::PassiveTreeResult refund = passives::refund_node(
            tree, progression_state, node);
        return refund.error == passives::PassiveTreeError::none
            ? u8"点击退款并保存"
            : passive_tree_status_view(
                {false, false, refund.error}).text;
    }
    if (!passive_tree_can_open(snapshot)) return u8"被动星图不可用";
    if (snapshot.progression.unspent_passive_points == 0U) {
        return u8"未分配点：0";
    }
    return has_allocated_neighbor(snapshot.passive_tree,
               passives::passive_nodes()[node])
        ? u8"点击分配并保存"
        : u8"请分配相邻节点";
}

PassiveNodeTextView passive_node_text_view(
    const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node) noexcept {
    PassiveNodeTextView view{};
    if (node >= passives::kPassiveNodeCount) return view;
    const passives::PassiveNode& source = passives::passive_nodes()[node];
    const PassiveNodeVisualCue cue = passive_node_visual_cue(
        passive_node_visual_state(snapshot, node));
    const UiUtf8CopyResult type_state = ui_utf8_format(
        view.type_state.data(), view.type_state.size(), u8"%s / 状态：%s",
        passive_node_type_text(source.type), cue.text);
    view.valid = copy_complete(view.name, ui_passive_node_name(node))
        && type_state.input_was_valid && !type_state.truncated
        && view.type_state[0] != '\0'
        && copy_complete(view.benefit, passive_benefit_source(node))
        && copy_complete(view.cost, passive_cost_source(node))
        && copy_complete(view.action,
            passive_node_action_text(snapshot, node));
    if (!view.valid) return {};
    return view;
}

PassiveTreeHeaderText passive_tree_header_text(
    std::uint32_t unspent_points,
    const char* passive_tree_binding_label) noexcept {
    PassiveTreeHeaderText view{};
    std::array<char, 32U> binding{};
    const char* source = passive_tree_binding_label != nullptr
            && passive_tree_binding_label[0] != '\0'
        ? passive_tree_binding_label : ui_text(UiTextId::unknown);
    UiUtf8CopyResult binding_result = ui_utf8_fit_with_ellipsis(
        binding.data(), binding.size(), source);
    if (!binding_result.input_was_valid || binding[0] == '\0') {
        binding_result = ui_utf8_copy(binding.data(), binding.size(),
            ui_text(UiTextId::unknown));
    }
    const UiUtf8CopyResult points_result = ui_utf8_format(
        view.points.data(), view.points.size(), u8"%s：%u",
        ui_text(UiTextId::passive_points),
        static_cast<unsigned>(unspent_points));
    const UiUtf8CopyResult close_result = ui_utf8_format(
        view.close.data(), view.close.size(), u8"%s %s",
        binding.data(), ui_text(UiTextId::passive_close));
    view.valid = binding_result.input_was_valid
        && points_result.input_was_valid && !points_result.truncated
        && close_result.input_was_valid && !close_result.truncated
        && view.points[0] != '\0' && view.close[0] != '\0';
    return view;
}

PassiveTreeOverlayLayout passive_tree_overlay_layout(
    int width, int height) noexcept {
    if (width < 320 || height < 240) return {};
    const float screen_width = static_cast<float>(width);
    const float screen_height = static_cast<float>(height);
    const float margin = std::clamp(screen_width * 0.03F, 16.0F, 48.0F);
    const float top = std::clamp(screen_height * 0.035F, 16.0F, 38.0F);
    const float row_height = std::clamp(
        screen_height * 0.065F, 28.0F, 50.0F);
    const float gap = std::clamp(screen_width * 0.012F, 8.0F, 20.0F);
    const float usable_width = screen_width - margin * 2.0F;
    const float side_width = usable_width * 0.29F;
    const float title_width = usable_width - side_width * 2.0F - gap * 2.0F;
    if (!(title_width > 0.0F)) return {};
    PassiveTreeOverlayLayout layout{};
    layout.points = {margin, top, side_width, row_height};
    layout.title = {layout.points.x + layout.points.width + gap,
        top, title_width, row_height};
    layout.close = {layout.title.x + layout.title.width + gap,
        top, side_width, row_height};
    layout.status = {margin, top + row_height + 4.0F,
        side_width, std::max(20.0F, row_height * 0.72F)};
    layout.valid = finite_rectangle(layout.points)
        && finite_rectangle(layout.title) && finite_rectangle(layout.close)
        && finite_rectangle(layout.status)
        && layout.close.x + layout.close.width <= screen_width
        && layout.status.y + layout.status.height <= screen_height;
    return layout.valid ? layout : PassiveTreeOverlayLayout{};
}

PassiveTreeTooltipLayout passive_tree_tooltip_layout(
    int width, int height) noexcept {
    if (width < 480 || height < 320) return {};
    const float screen_width = static_cast<float>(width);
    const float screen_height = static_cast<float>(height);
    const float margin = std::clamp(screen_width * 0.03F, 16.0F, 48.0F);
    const float panel_width = std::clamp(
        screen_width * 0.425F, 340.0F, 520.0F);
    const float panel_height = std::clamp(
        screen_height * 0.42F, 184.0F, 260.0F);
    if (panel_width > screen_width - margin * 2.0F
        || panel_height > screen_height - margin * 2.0F) {
        return {};
    }
    PassiveTreeTooltipLayout layout{};
    layout.panel = {screen_width - margin - panel_width,
        screen_height - margin - panel_height, panel_width, panel_height};
    const float inset = std::clamp(panel_height * 0.055F, 10.0F, 16.0F);
    constexpr float kGap = 4.0F;
    const float content_height = panel_height - inset * 2.0F - kGap * 4.0F;
    const float name_height = content_height * 0.18F;
    const float type_height = content_height * 0.14F;
    const float benefit_height = content_height * 0.24F;
    const float cost_height = content_height * 0.18F;
    const float action_height = content_height - name_height - type_height
        - benefit_height - cost_height;
    const float content_x = layout.panel.x + inset;
    const float content_width = layout.panel.width - inset * 2.0F;
    float y = layout.panel.y + inset;
    layout.name = {content_x, y, content_width, name_height};
    y += name_height + kGap;
    layout.type_state = {content_x, y, content_width, type_height};
    y += type_height + kGap;
    layout.benefit = {content_x, y, content_width, benefit_height};
    y += benefit_height + kGap;
    layout.cost = {content_x, y, content_width, cost_height};
    y += cost_height + kGap;
    layout.action = {content_x, y, content_width, action_height};
    layout.valid = finite_rectangle(layout.panel)
        && finite_rectangle(layout.name)
        && finite_rectangle(layout.type_state)
        && finite_rectangle(layout.benefit)
        && finite_rectangle(layout.cost)
        && finite_rectangle(layout.action)
        && layout.panel.x >= 0.0F && layout.panel.y >= 0.0F
        && layout.panel.x + layout.panel.width <= screen_width
        && layout.panel.y + layout.panel.height <= screen_height
        && layout.action.y + layout.action.height
            <= layout.panel.y + layout.panel.height;
    return layout.valid ? layout : PassiveTreeTooltipLayout{};
}

PassiveNodeNameBounds passive_node_name_bounds(
    passives::PassiveNodeId node, int width, int height) noexcept {
    if (width <= 0 || height <= 0) return {};
    const PassiveNodeProjection projection = project_passive_node(node,
        static_cast<float>(width), static_cast<float>(height));
    if (!projection.visible) return {};
    const float scale = projection_scale(
        static_cast<float>(width), static_cast<float>(height));
    const float label_width = std::clamp(64.0F * scale, 36.0F, 96.0F);
    const float label_height = std::clamp(24.0F * scale, 16.0F, 32.0F);
    const Rectangle bounds{projection.center.x - label_width * 0.5F,
        projection.center.y - label_height * 0.5F,
        label_width, label_height};
    const bool valid = finite_rectangle(bounds)
        && bounds.x >= 0.0F && bounds.y >= 0.0F
        && bounds.x + bounds.width <= static_cast<float>(width)
        && bounds.y + bounds.height <= static_cast<float>(height);
    return valid ? PassiveNodeNameBounds{bounds, true}
                 : PassiveNodeNameBounds{};
}

PassiveTreeStatusView passive_tree_status_view(
    PassiveTreeStatusInput input) noexcept {
    if (input.save_error) {
        return {u8"保存状态：错误", PassiveTreeStatusTone::error};
    }
    if (input.saving) {
        return {u8"保存状态：保存中", PassiveTreeStatusTone::saving};
    }
    switch (input.rule_error) {
    case passives::PassiveTreeError::none:
        return {u8"保存状态：就绪", PassiveTreeStatusTone::ready};
    case passives::PassiveTreeError::unknown_node:
        return {u8"未知被动节点", PassiveTreeStatusTone::error};
    case passives::PassiveTreeError::already_allocated:
        return {u8"节点已分配", PassiveTreeStatusTone::error};
    case passives::PassiveTreeError::not_allocated:
        return {u8"节点不可退款", PassiveTreeStatusTone::error};
    case passives::PassiveTreeError::no_points:
        return {u8"未分配点：0", PassiveTreeStatusTone::error};
    case passives::PassiveTreeError::not_adjacent:
        return {u8"请分配相邻节点", PassiveTreeStatusTone::error};
    case passives::PassiveTreeError::disconnects_tree:
        return {u8"退款错误：将断链", PassiveTreeStatusTone::error};
    case passives::PassiveTreeError::invalid_state:
        return {u8"被动星图状态错误", PassiveTreeStatusTone::error};
    }
    return {u8"被动星图错误", PassiveTreeStatusTone::error};
}

PassiveOverlayInputGate passive_overlay_input_gate(bool overlay_open) noexcept {
    return { !overlay_open, !overlay_open, !overlay_open };
}

}  // namespace arpg::platform
