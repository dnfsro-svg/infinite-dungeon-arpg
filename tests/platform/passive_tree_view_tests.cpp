#include "passive_tree_view_math.hpp"

#include "test_framework.hpp"

#include <cmath>
#include <cstring>
#include <optional>

namespace {

using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::RoomPhase;
using arpg::passives::PassiveNodeId;
using arpg::platform::PassiveNodeVisualState;
using arpg::platform::PassiveOverlayInputGate;
using arpg::platform::PassiveTreeToggleAction;

arpg::test::Failure overlay_only_opens_for_clean_awaiting_exit() noexcept {
    DungeonSnapshot snapshot{};
    snapshot.phase = RoomPhase::awaiting_exit;
    ARPG_REQUIRE(arpg::platform::passive_tree_can_open(snapshot));
    snapshot.passive_save_pending = true;
    ARPG_REQUIRE(!arpg::platform::passive_tree_can_open(snapshot));
    snapshot.passive_save_pending = false;
    snapshot.phase = RoomPhase::combat;
    ARPG_REQUIRE(!arpg::platform::passive_tree_can_open(snapshot));
    return {};
}

arpg::test::Failure combat_toggle_reports_full_clear_gate_only_for_live_input()
    noexcept {
    DungeonSnapshot combat{};
    combat.has_active_room = true;
    combat.phase = RoomPhase::combat;
    combat.remaining_targets = 7U;

    ARPG_REQUIRE(arpg::platform::passive_tree_toggle_action(
        combat, true, true)
        == PassiveTreeToggleAction::show_full_clear_requirement);
    ARPG_REQUIRE(arpg::platform::passive_tree_toggle_action(
        combat, false, true) == PassiveTreeToggleAction::none);
    ARPG_REQUIRE(arpg::platform::passive_tree_toggle_action(
        combat, true, false) == PassiveTreeToggleAction::none);

    combat.phase = RoomPhase::awaiting_exit;
    combat.remaining_targets = 0U;
    ARPG_REQUIRE(arpg::platform::passive_tree_toggle_action(
        combat, true, true) == PassiveTreeToggleAction::toggle);
    return {};
}

arpg::test::Failure node_projection_and_hit_test_are_stable() noexcept {
    const auto projected = arpg::platform::project_passive_node(
        static_cast<PassiveNodeId>(8U), 1280.0F, 720.0F);
    ARPG_REQUIRE(projected.visible);
    ARPG_REQUIRE(arpg::platform::hit_test_passive_node(
        projected.center, 1280.0F, 720.0F)
        == std::optional<PassiveNodeId>{static_cast<PassiveNodeId>(8U)});
    ARPG_REQUIRE(!arpg::platform::hit_test_passive_node(
        {-1.0F, -1.0F}, 1280.0F, 720.0F));
    return {};
}

arpg::test::Failure distinct_landmarks_keep_distinct_projections() noexcept {
    const auto origin = arpg::platform::project_passive_node(0U, 1280.0F, 720.0F);
    const auto cinder_vow = arpg::platform::project_passive_node(21U, 1280.0F, 720.0F);
    const auto blood_pact = arpg::platform::project_passive_node(63U, 1280.0F, 720.0F);
    ARPG_REQUIRE(origin.visible && cinder_vow.visible && blood_pact.visible);
    ARPG_REQUIRE(origin.center.x != cinder_vow.center.x || origin.center.y != cinder_vow.center.y);
    ARPG_REQUIRE(origin.center.x != blood_pact.center.x || origin.center.y != blood_pact.center.y);
    ARPG_REQUIRE(cinder_vow.center.x != blood_pact.center.x
        || cinder_vow.center.y != blood_pact.center.y);
    return {};
}

arpg::test::Failure route_nodes_have_unique_projections_and_hit_themselves() noexcept {
    struct Viewport final { float width{}; float height{}; };
    constexpr Viewport kViewports[] = {{1280.0F, 720.0F}, {800.0F, 450.0F}};
    constexpr float kHitMargin = 4.0F;
    for (const Viewport viewport : kViewports) {
        for (unsigned value = 8U; value < 64U; ++value) {
            const auto node = static_cast<PassiveNodeId>(value);
            const auto projection = arpg::platform::project_passive_node(
                node, viewport.width, viewport.height);
            ARPG_REQUIRE(projection.visible);
            ARPG_REQUIRE(arpg::platform::hit_test_passive_node(
                projection.center, viewport.width, viewport.height)
                == std::optional<PassiveNodeId>{node});
            const unsigned route = (value - 8U) / 14U;
            for (unsigned other_value = value + 1U; other_value < 64U; ++other_value) {
                const unsigned other_route = (other_value - 8U) / 14U;
                if (route == other_route) continue;
                const auto other = arpg::platform::project_passive_node(
                    static_cast<PassiveNodeId>(other_value), viewport.width, viewport.height);
                const float dx = projection.center.x - other.center.x;
                const float dy = projection.center.y - other.center.y;
                ARPG_REQUIRE(std::sqrt(dx * dx + dy * dy)
                    > projection.radius + other.radius + kHitMargin);
            }
        }
    }
    return {};
}

arpg::test::Failure visual_state_reports_locked_available_allocated_and_feedback() noexcept {
    DungeonSnapshot snapshot{};
    snapshot.phase = RoomPhase::awaiting_exit;
    snapshot.progression.level = 2U;
    snapshot.progression.unspent_passive_points = 1U;
    snapshot.passive_tree.allocated_bits = (1ULL << 0U) | (1ULL << 8U);
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 0U)
        == PassiveNodeVisualState::allocated);
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 9U)
        == PassiveNodeVisualState::available);
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 21U)
        == PassiveNodeVisualState::locked);
    snapshot.passive_tree_error = arpg::passives::PassiveTreeError::no_points;
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 0U)
        == PassiveNodeVisualState::allocated);
    snapshot.progression.unspent_passive_points = 0U;
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 9U)
        == PassiveNodeVisualState::locked);
    snapshot.passive_tree_error = arpg::passives::PassiveTreeError::none;
    snapshot.progression.unspent_passive_points = 1U;
    snapshot.passive_save_pending = true;
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 9U)
        == PassiveNodeVisualState::pending);
    return {};
}

arpg::test::Failure feedback_explains_node_actions_and_rule_failures() noexcept {
    DungeonSnapshot snapshot{};
    snapshot.phase = RoomPhase::awaiting_exit;
    snapshot.progression.earned_passive_points = 2U;
    snapshot.progression.unspent_passive_points = 1U;
    snapshot.passive_tree.allocated_bits = (1ULL << 0U) | (1ULL << 8U);

    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 0U), "Starting node is permanent") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 8U), "Click to refund (autosaves)") == 0);
    snapshot.passive_tree.allocated_bits |= 1ULL << 9U;
    snapshot.progression.earned_passive_points = 3U;
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 8U), "Refund would disconnect the tree") == 0);
    snapshot.passive_tree.allocated_bits &= ~(1ULL << 9U);
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 9U), "Click to allocate (autosaves)") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 21U), "Allocate an adjacent node first") == 0);

    snapshot.passive_tree_error = arpg::passives::PassiveTreeError::not_adjacent;
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 0U)
        == PassiveNodeVisualState::allocated);
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 9U)
        == PassiveNodeVisualState::available);
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 21U)
        == PassiveNodeVisualState::locked);
    snapshot.passive_tree_error = arpg::passives::PassiveTreeError::none;

    snapshot.progression.unspent_passive_points = 0U;
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 9U), "No passive points available") == 0);
    snapshot.passive_save_pending = true;
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 9U), "Saving passive tree...") == 0);

    using arpg::platform::PassiveTreeStatusInput;
    using arpg::platform::PassiveTreeStatusTone;
    const auto ready = arpg::platform::passive_tree_status_view({});
    ARPG_REQUIRE(std::strcmp(ready.text, "Autosave READY") == 0);
    ARPG_REQUIRE(ready.tone == PassiveTreeStatusTone::ready);

    const auto saving = arpg::platform::passive_tree_status_view(
        PassiveTreeStatusInput{true, false,
            arpg::passives::PassiveTreeError::none});
    ARPG_REQUIRE(std::strcmp(saving.text, "Autosave SAVING") == 0);
    ARPG_REQUIRE(saving.tone == PassiveTreeStatusTone::saving);

    const auto save_error = arpg::platform::passive_tree_status_view(
        PassiveTreeStatusInput{true, true,
            arpg::passives::PassiveTreeError::not_adjacent});
    ARPG_REQUIRE(std::strcmp(save_error.text, "Autosave ERROR") == 0);
    ARPG_REQUIRE(save_error.tone == PassiveTreeStatusTone::error);

    struct RuleExpectation final {
        arpg::passives::PassiveTreeError error{};
        const char* text{};
    };
    constexpr RuleExpectation kRules[] = {
        {arpg::passives::PassiveTreeError::unknown_node,
            "Unknown passive node"},
        {arpg::passives::PassiveTreeError::already_allocated,
            "Node is already allocated"},
        {arpg::passives::PassiveTreeError::not_allocated,
            "Node cannot be refunded"},
        {arpg::passives::PassiveTreeError::no_points,
            "No passive points available"},
        {arpg::passives::PassiveTreeError::not_adjacent,
            "Allocate an adjacent node first"},
        {arpg::passives::PassiveTreeError::disconnects_tree,
            "Refund would disconnect the tree"},
        {arpg::passives::PassiveTreeError::invalid_state,
            "Passive tree state is invalid"},
    };
    for (const RuleExpectation& expected : kRules) {
        const auto view = arpg::platform::passive_tree_status_view(
            PassiveTreeStatusInput{false, false, expected.error});
        ARPG_REQUIRE(std::strcmp(view.text, expected.text) == 0);
        ARPG_REQUIRE(view.tone == PassiveTreeStatusTone::error);
        ARPG_REQUIRE(std::strcmp(view.text, "Autosave ERROR") != 0);
    }
    return {};
}

arpg::test::Failure overlay_captures_all_gameplay_input() noexcept {
    const PassiveOverlayInputGate open = arpg::platform::passive_overlay_input_gate(true);
    ARPG_REQUIRE(!open.forward_actions);
    ARPG_REQUIRE(!open.forward_movement);
    ARPG_REQUIRE(!open.forward_descent);
    const PassiveOverlayInputGate closed = arpg::platform::passive_overlay_input_gate(false);
    ARPG_REQUIRE(closed.forward_actions);
    ARPG_REQUIRE(closed.forward_movement);
    ARPG_REQUIRE(closed.forward_descent);
    return {};
}

const arpg::test::TestCase kCases[] = {
    {"opens only for clean exit", &overlay_only_opens_for_clean_awaiting_exit},
    {"combat toggle reports full-clear gate",
        &combat_toggle_reports_full_clear_gate_only_for_live_input},
    {"projects and hits nodes", &node_projection_and_hit_test_are_stable},
    {"keeps landmarks distinct", &distinct_landmarks_keep_distinct_projections},
    {"separates route nodes and hits each", &route_nodes_have_unique_projections_and_hit_themselves},
    {"reports visual states", &visual_state_reports_locked_available_allocated_and_feedback},
    {"explains node actions and rule failures",
        &feedback_explains_node_actions_and_rule_failures},
    {"captures gameplay input", &overlay_captures_all_gameplay_input},
};

}  // namespace

arpg::test::TestSuite passive_tree_view_suite() noexcept {
    return arpg::test::make_suite("passive_tree_view", kCases);
}
