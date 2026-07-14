#include "passive_tree_view_math.hpp"

#include "test_framework.hpp"

#include <optional>

namespace {

using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::RoomPhase;
using arpg::passives::PassiveNodeId;
using arpg::platform::PassiveNodeVisualState;
using arpg::platform::PassiveOverlayInputGate;

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
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 9U)
        == PassiveNodeVisualState::rejected);
    snapshot.passive_tree_error = arpg::passives::PassiveTreeError::none;
    snapshot.passive_save_pending = true;
    ARPG_REQUIRE(arpg::platform::passive_node_visual_state(snapshot, 9U)
        == PassiveNodeVisualState::pending);
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
    {"projects and hits nodes", &node_projection_and_hit_test_are_stable},
    {"keeps landmarks distinct", &distinct_landmarks_keep_distinct_projections},
    {"reports visual states", &visual_state_reports_locked_available_allocated_and_feedback},
    {"captures gameplay input", &overlay_captures_all_gameplay_input},
};

}  // namespace

arpg::test::TestSuite passive_tree_view_suite() noexcept {
    return arpg::test::make_suite("passive_tree_view", kCases);
}
