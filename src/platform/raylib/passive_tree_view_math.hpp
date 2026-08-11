#pragma once

#include "dungeon/dungeon_types.hpp"
#include "passives/passive_tree_types.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
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
    pending,
};

enum class PassiveNodeStateMark : std::uint8_t {
    locked_cross,
    available_ring,
    allocated_dot,
    pending_hourglass,
};

struct PassiveNodeVisualCue final {
    const char* text{};
    PassiveNodeStateMark mark{PassiveNodeStateMark::locked_cross};
};

inline constexpr std::size_t kPassiveNodeNameCapacity = 64U;
inline constexpr std::size_t kPassiveNodeTypeStateCapacity = 64U;
inline constexpr std::size_t kPassiveNodeBenefitCapacity = 160U;
inline constexpr std::size_t kPassiveNodeCostCapacity = 128U;
inline constexpr std::size_t kPassiveNodeActionCapacity = 128U;

struct PassiveNodeTextView final {
    std::array<char, kPassiveNodeNameCapacity> name{};
    std::array<char, kPassiveNodeTypeStateCapacity> type_state{};
    std::array<char, kPassiveNodeBenefitCapacity> benefit{};
    std::array<char, kPassiveNodeCostCapacity> cost{};
    std::array<char, kPassiveNodeActionCapacity> action{};
    bool valid{};
};

struct PassiveTreeHeaderText final {
    std::array<char, 48U> points{};
    std::array<char, 64U> close{};
    bool valid{};
};

struct PassiveTreeOverlayLayout final {
    Rectangle title{};
    Rectangle points{};
    Rectangle close{};
    Rectangle status{};
    bool valid{};
};

struct PassiveTreeTooltipLayout final {
    Rectangle panel{};
    Rectangle name{};
    Rectangle type_state{};
    Rectangle benefit{};
    Rectangle cost{};
    Rectangle action{};
    bool valid{};
};

struct PassiveNodeNameBounds final {
    Rectangle bounds{};
    bool valid{};
};

enum class PassiveTreeStatusTone : std::uint8_t {
    ready,
    saving,
    error,
};

struct PassiveTreeStatusInput final {
    bool saving{};
    bool save_error{};
    passives::PassiveTreeError rule_error{passives::PassiveTreeError::none};
};

struct PassiveTreeStatusView final {
    const char* text{};
    PassiveTreeStatusTone tone{PassiveTreeStatusTone::ready};
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
[[nodiscard]] PassiveNodeVisualCue passive_node_visual_cue(
    PassiveNodeVisualState state) noexcept;
[[nodiscard]] const char* passive_node_action_text(
    const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node) noexcept;
[[nodiscard]] PassiveNodeTextView passive_node_text_view(
    const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node) noexcept;
[[nodiscard]] PassiveTreeHeaderText passive_tree_header_text(
    std::uint32_t unspent_points,
    const char* passive_tree_binding_label) noexcept;
[[nodiscard]] PassiveTreeOverlayLayout passive_tree_overlay_layout(
    int width, int height) noexcept;
[[nodiscard]] PassiveTreeTooltipLayout passive_tree_tooltip_layout(
    int width, int height) noexcept;
[[nodiscard]] PassiveNodeNameBounds passive_node_name_bounds(
    passives::PassiveNodeId node, int width, int height) noexcept;
[[nodiscard]] PassiveTreeStatusView passive_tree_status_view(
    PassiveTreeStatusInput input) noexcept;
[[nodiscard]] PassiveOverlayInputGate passive_overlay_input_gate(
    bool overlay_open) noexcept;

}  // namespace arpg::platform
