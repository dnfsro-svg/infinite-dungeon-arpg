#pragma once

#include "dungeon/dungeon_types.hpp"
#include "passives/passive_tree_types.hpp"

#include <cstdint>
#include <optional>

namespace arpg::platform {

struct PassiveScreenPoint final {
    float x{};
    float y{};
};

struct PassiveNodeProjection final {
    PassiveScreenPoint center{};
    float radius{};
    bool visible{};
};

enum class PassiveNodeVisualState : std::uint8_t {
    locked,
    available,
    allocated,
    rejected,
    pending,
};

enum class PassiveTreeToggleAction : std::uint8_t {
    none,
    toggle,
    show_full_clear_requirement,
};

struct PassiveOverlayInputGate final {
    bool forward_actions{};
    bool forward_movement{};
    bool forward_descent{};
};

[[nodiscard]] bool passive_tree_can_open(
    const dungeon::DungeonSnapshot& snapshot) noexcept;
[[nodiscard]] PassiveTreeToggleAction passive_tree_toggle_action(
    const dungeon::DungeonSnapshot& snapshot,
    bool pressed,
    bool gameplay_input_available) noexcept;
[[nodiscard]] PassiveNodeProjection project_passive_node(
    passives::PassiveNodeId node, float width, float height) noexcept;
[[nodiscard]] std::optional<passives::PassiveNodeId> hit_test_passive_node(
    PassiveScreenPoint screen, float width, float height) noexcept;
[[nodiscard]] PassiveNodeVisualState passive_node_visual_state(
    const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node) noexcept;
[[nodiscard]] PassiveOverlayInputGate passive_overlay_input_gate(
    bool overlay_open) noexcept;

}  // namespace arpg::platform
