#include "passive_tree_view_math.hpp"

#include "allocation_probe.hpp"
#include "passives/passive_tree_catalog.hpp"
#include "shared_ui_font.hpp"
#include "test_framework.hpp"
#include "ui_text_catalog.hpp"
#include "ui_utf8.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>

namespace {

using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::RoomPhase;
using arpg::passives::PassiveNodeId;
using arpg::platform::PassiveNodeVisualState;
using arpg::platform::PassiveOverlayInputGate;
using arpg::platform::PassiveTreeToggleAction;

bool contains(Rectangle outer, Rectangle inner) noexcept {
    constexpr float kTolerance = 0.001F;
    return inner.x + kTolerance >= outer.x
        && inner.y + kTolerance >= outer.y
        && inner.x + inner.width <= outer.x + outer.width + kTolerance
        && inner.y + inner.height <= outer.y + outer.height + kTolerance;
}

bool overlaps(Rectangle left, Rectangle right) noexcept {
    return left.x < right.x + right.width
        && left.x + left.width > right.x
        && left.y < right.y + right.height
        && left.y + left.height > right.y;
}

bool has_ascii_letter(const char* text) noexcept {
    if (text == nullptr) return false;
    for (const unsigned char* cursor =
             reinterpret_cast<const unsigned char*>(text);
         *cursor != 0U; ++cursor) {
        if ((*cursor >= static_cast<unsigned char>('A')
                && *cursor <= static_cast<unsigned char>('Z'))
            || (*cursor >= static_cast<unsigned char>('a')
                && *cursor <= static_cast<unsigned char>('z'))) {
            return true;
        }
    }
    return false;
}

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

    struct Viewport final {
        int width{};
        int height{};
    };
    constexpr Viewport kViewports[] = {
        {800, 450}, {1280, 720}, {1920, 1080},
    };
    for (const Viewport viewport : kViewports) {
        const Rectangle screen{0.0F, 0.0F,
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.height)};
        const auto layout = arpg::platform::passive_tree_overlay_layout(
            viewport.width, viewport.height);
        ARPG_REQUIRE(layout.valid);
        ARPG_REQUIRE(contains(screen, layout.title));
        ARPG_REQUIRE(contains(screen, layout.points));
        ARPG_REQUIRE(contains(screen, layout.close));
        ARPG_REQUIRE(contains(screen, layout.status));
        ARPG_REQUIRE(arpg::test::near(
            layout.title.x + layout.title.width * 0.5F,
            screen.width * 0.5F));
        ARPG_REQUIRE(layout.points.x + layout.points.width
            <= layout.title.x);
        ARPG_REQUIRE(layout.close.x
            >= layout.title.x + layout.title.width);

        const auto tooltip = arpg::platform::passive_tree_tooltip_layout(
            viewport.width, viewport.height);
        ARPG_REQUIRE(tooltip.valid);
        ARPG_REQUIRE(contains(screen, tooltip.panel));
        ARPG_REQUIRE(contains(tooltip.panel, tooltip.name));
        ARPG_REQUIRE(contains(tooltip.panel, tooltip.type_state));
        ARPG_REQUIRE(contains(tooltip.panel, tooltip.benefit));
        ARPG_REQUIRE(contains(tooltip.panel, tooltip.cost));
        ARPG_REQUIRE(contains(tooltip.panel, tooltip.action));
        ARPG_REQUIRE(tooltip.name.y + tooltip.name.height
            <= tooltip.type_state.y);
        ARPG_REQUIRE(tooltip.type_state.y + tooltip.type_state.height
            <= tooltip.benefit.y);
        ARPG_REQUIRE(tooltip.benefit.y + tooltip.benefit.height
            <= tooltip.cost.y);
        ARPG_REQUIRE(tooltip.cost.y + tooltip.cost.height
            <= tooltip.action.y);

        std::array<Rectangle, arpg::passives::kPassiveNodeCount> name_areas{};
        for (std::uint8_t raw{}; raw < arpg::passives::kPassiveNodeCount;
             ++raw) {
            const auto node_id = static_cast<PassiveNodeId>(raw);
            const auto name_bounds =
                arpg::platform::passive_node_name_bounds(
                    node_id, viewport.width, viewport.height);
            const auto node = arpg::platform::project_passive_node(
                node_id, screen.width, screen.height);
            ARPG_REQUIRE(name_bounds.valid);
            ARPG_REQUIRE(contains(screen, name_bounds.bounds));
            ARPG_REQUIRE(arpg::test::near(
                name_bounds.bounds.x + name_bounds.bounds.width * 0.5F,
                node.center.x,
                0.001));
            ARPG_REQUIRE(arpg::test::near(
                name_bounds.bounds.y + name_bounds.bounds.height * 0.5F,
                node.center.y,
                0.001));
            name_areas[raw] = name_bounds.bounds;
        }
        for (std::size_t left{}; left < name_areas.size(); ++left) {
            for (std::size_t right = left + 1U; right < name_areas.size();
                 ++right) {
                ARPG_REQUIRE(!overlaps(name_areas[left], name_areas[right]));
            }
        }
    }
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

    using arpg::platform::PassiveNodeStateMark;
    constexpr std::array<PassiveNodeVisualState, 4U> kStates{{
        PassiveNodeVisualState::locked,
        PassiveNodeVisualState::available,
        PassiveNodeVisualState::allocated,
        PassiveNodeVisualState::pending,
    }};
    constexpr const char* kLabels[] = {
        u8"锁定", u8"可分配", u8"已分配", u8"保存中",
    };
    constexpr PassiveNodeStateMark kMarks[] = {
        PassiveNodeStateMark::locked_cross,
        PassiveNodeStateMark::available_ring,
        PassiveNodeStateMark::allocated_dot,
        PassiveNodeStateMark::pending_hourglass,
    };
    for (std::size_t index{}; index < kStates.size(); ++index) {
        const auto cue = arpg::platform::passive_node_visual_cue(
            kStates[index]);
        ARPG_REQUIRE(std::strcmp(cue.text, kLabels[index]) == 0);
        ARPG_REQUIRE(cue.mark == kMarks[index]);
        ARPG_REQUIRE(arpg::platform::ui_utf8_is_valid(cue.text));
    }
    return {};
}

arpg::test::Failure feedback_explains_node_actions_and_rule_failures() noexcept {
    DungeonSnapshot snapshot{};
    snapshot.phase = RoomPhase::awaiting_exit;
    snapshot.progression.earned_passive_points = 2U;
    snapshot.progression.unspent_passive_points = 1U;
    snapshot.passive_tree.allocated_bits = (1ULL << 0U) | (1ULL << 8U);

    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 0U), u8"起始节点不可退款") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 8U), u8"点击退款并保存") == 0);
    snapshot.passive_tree.allocated_bits |= 1ULL << 9U;
    snapshot.progression.earned_passive_points = 3U;
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 8U), u8"退款错误：将断链") == 0);
    snapshot.passive_tree.allocated_bits &= ~(1ULL << 9U);
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 9U), u8"点击分配并保存") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 21U), u8"请分配相邻节点") == 0);

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
        snapshot, 9U), u8"未分配点：0") == 0);
    snapshot.passive_save_pending = true;
    ARPG_REQUIRE(std::strcmp(arpg::platform::passive_node_action_text(
        snapshot, 9U), u8"被动星图保存中…") == 0);

    using arpg::platform::PassiveTreeStatusInput;
    using arpg::platform::PassiveTreeStatusTone;
    const auto ready = arpg::platform::passive_tree_status_view({});
    ARPG_REQUIRE(std::strcmp(ready.text, u8"保存状态：就绪") == 0);
    ARPG_REQUIRE(ready.tone == PassiveTreeStatusTone::ready);

    const auto saving = arpg::platform::passive_tree_status_view(
        PassiveTreeStatusInput{true, false,
            arpg::passives::PassiveTreeError::none});
    ARPG_REQUIRE(std::strcmp(saving.text, u8"保存状态：保存中") == 0);
    ARPG_REQUIRE(saving.tone == PassiveTreeStatusTone::saving);

    const auto save_error = arpg::platform::passive_tree_status_view(
        PassiveTreeStatusInput{true, true,
            arpg::passives::PassiveTreeError::not_adjacent});
    ARPG_REQUIRE(std::strcmp(save_error.text, u8"保存状态：错误") == 0);
    ARPG_REQUIRE(save_error.tone == PassiveTreeStatusTone::error);

    struct RuleExpectation final {
        arpg::passives::PassiveTreeError error{};
        const char* text{};
    };
    constexpr RuleExpectation kRules[] = {
        {arpg::passives::PassiveTreeError::unknown_node,
            u8"未知被动节点"},
        {arpg::passives::PassiveTreeError::already_allocated,
            u8"节点已分配"},
        {arpg::passives::PassiveTreeError::not_allocated,
            u8"节点不可退款"},
        {arpg::passives::PassiveTreeError::no_points,
            u8"未分配点：0"},
        {arpg::passives::PassiveTreeError::not_adjacent,
            u8"请分配相邻节点"},
        {arpg::passives::PassiveTreeError::disconnects_tree,
            u8"退款错误：将断链"},
        {arpg::passives::PassiveTreeError::invalid_state,
            u8"被动星图状态错误"},
    };
    for (const RuleExpectation& expected : kRules) {
        const auto view = arpg::platform::passive_tree_status_view(
            PassiveTreeStatusInput{false, false, expected.error});
        ARPG_REQUIRE(std::strcmp(view.text, expected.text) == 0);
        ARPG_REQUIRE(view.tone == PassiveTreeStatusTone::error);
        ARPG_REQUIRE(std::strcmp(view.text, u8"保存状态：错误") != 0);
        ARPG_REQUIRE(!has_ascii_letter(view.text));
    }

    const auto saving_over_rule = arpg::platform::passive_tree_status_view(
        PassiveTreeStatusInput{true, false,
            arpg::passives::PassiveTreeError::disconnects_tree});
    ARPG_REQUIRE(std::strcmp(
        saving_over_rule.text, u8"保存状态：保存中") == 0);

    const auto header = arpg::platform::passive_tree_header_text(7U, "P");
    ARPG_REQUIRE(header.valid);
    ARPG_REQUIRE(std::strcmp(header.points.data(), u8"未分配点：7") == 0);
    ARPG_REQUIRE(std::strcmp(header.close.data(), u8"P 关闭") == 0);

    const auto& catalog = arpg::passives::passive_nodes();
    const auto glyphs = arpg::platform::ui_glyph_plan();
    for (std::size_t index{}; index < catalog.size(); ++index) {
        const auto node = static_cast<PassiveNodeId>(index);
        const auto text = arpg::platform::passive_node_text_view(
            snapshot, node);
        ARPG_REQUIRE(text.valid);
        ARPG_REQUIRE(std::strcmp(text.name.data(),
            arpg::platform::ui_passive_node_name(node)) == 0);
        ARPG_REQUIRE(std::strcmp(text.name.data(), catalog[index].name) != 0);
        for (const char* value : {text.name.data(), text.type_state.data(),
                 text.benefit.data(), text.cost.data(), text.action.data()}) {
            ARPG_REQUIRE(arpg::platform::ui_utf8_is_valid(value));
            ARPG_REQUIRE(arpg::platform::ui_glyph_plan_covers_text(
                glyphs, value));
            ARPG_REQUIRE(!has_ascii_letter(value));
        }
    }

    DungeonSnapshot readable = snapshot;
    readable.passive_save_pending = false;
    readable.progression.unspent_passive_points = 1U;
    readable.passive_tree.allocated_bits = 1ULL;
    struct KeystoneExpectation final {
        PassiveNodeId node{};
        const char* benefit{};
        const char* cost{};
    };
    constexpr KeystoneExpectation kKeystones[] = {
        {21U, u8"收益：最终火焰伤害 ×1.30", u8"代价：-20% 水流减伤"},
        {35U, u8"收益：最终水流伤害 ×1.25，+24 最大护盾",
            u8"代价：最终近战伤害 ×0.85"},
        {49U, u8"收益：最终闪电伤害 ×1.25，移动速度/攻击速度 +12%",
            u8"代价：最终最大生命 ×0.80"},
        {63U, u8"收益：最终混沌伤害 ×1.30",
            u8"代价：最终承受伤害 ×1.15"},
    };
    for (const KeystoneExpectation expected : kKeystones) {
        const auto text = arpg::platform::passive_node_text_view(
            readable, expected.node);
        ARPG_REQUIRE(std::strcmp(
            text.benefit.data(), expected.benefit) == 0);
        ARPG_REQUIRE(std::strcmp(text.cost.data(), expected.cost) == 0);
    }

    std::uint64_t checksum{};
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    for (std::size_t iteration{}; iteration < 10'000U; ++iteration) {
        const auto node = static_cast<PassiveNodeId>(iteration % 64U);
        const auto text = arpg::platform::passive_node_text_view(
            readable, node);
        const auto frame = arpg::platform::passive_tree_overlay_layout(
            800 + static_cast<int>(iteration % 3U) * 560,
            450 + static_cast<int>(iteration % 3U) * 315);
        const auto tooltip = arpg::platform::passive_tree_tooltip_layout(
            800 + static_cast<int>(iteration % 3U) * 560,
            450 + static_cast<int>(iteration % 3U) * 315);
        const auto name = arpg::platform::passive_node_name_bounds(node,
            800 + static_cast<int>(iteration % 3U) * 560,
            450 + static_cast<int>(iteration % 3U) * 315);
        const auto dynamic_header = arpg::platform::passive_tree_header_text(
            static_cast<std::uint32_t>(iteration), "P");
        checksum += static_cast<std::uint64_t>(text.name[0])
            + static_cast<std::uint64_t>(frame.valid)
            + static_cast<std::uint64_t>(tooltip.valid)
            + static_cast<std::uint64_t>(name.valid)
            + static_cast<std::uint64_t>(dynamic_header.valid);
    }
    ARPG_REQUIRE(checksum != 0U);
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
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
