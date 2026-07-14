#include "passive_tree_view_math.hpp"

#include "passives/passive_tree_catalog.hpp"

#include <algorithm>

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

}  // namespace

bool passive_tree_can_open(const dungeon::DungeonSnapshot& snapshot) noexcept {
    return snapshot.phase == dungeon::RoomPhase::awaiting_exit
        && !snapshot.passive_save_pending;
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
    for (std::size_t index = 0U; index < passives::kPassiveNodeCount; ++index) {
        const auto node = static_cast<passives::PassiveNodeId>(index);
        const PassiveNodeProjection projection = project_passive_node(node, width, height);
        if (!projection.visible) return std::nullopt;
        const float dx = screen.x - projection.center.x;
        const float dy = screen.y - projection.center.y;
        if (dx * dx + dy * dy <= projection.radius * projection.radius) return node;
    }
    return std::nullopt;
}

PassiveNodeVisualState passive_node_visual_state(
    const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node) noexcept {
    if (node >= passives::kPassiveNodeCount) return PassiveNodeVisualState::locked;
    if (snapshot.passive_save_pending) return PassiveNodeVisualState::pending;
    if (snapshot.passive_tree_error != passives::PassiveTreeError::none) {
        return PassiveNodeVisualState::rejected;
    }
    if (node_is_allocated(snapshot.passive_tree, node)) {
        return PassiveNodeVisualState::allocated;
    }
    const passives::PassiveNode& candidate = passives::passive_nodes()[node];
    return passive_tree_can_open(snapshot)
            && snapshot.progression.unspent_passive_points > 0U
            && has_allocated_neighbor(snapshot.passive_tree, candidate)
        ? PassiveNodeVisualState::available : PassiveNodeVisualState::locked;
}

PassiveOverlayInputGate passive_overlay_input_gate(bool overlay_open) noexcept {
    return { !overlay_open, !overlay_open, !overlay_open };
}

}  // namespace arpg::platform
