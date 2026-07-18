#include "test_framework.hpp"

#include "hud_notice_state.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {

namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;

constexpr float kTransientSeconds = 3.0F;

platform::ControlHints rebound_hints() noexcept {
    platform::ControlHints hints{};
    static constexpr char kSecondary[] =
        "J Light Attack  K Jump  L Launcher  Q Interact  O Inventory  "
        "T Passive Tree";
    static_assert(sizeof(kSecondary) - 1U < 160U);
    hints.secondary.fill('Z');
    std::memcpy(hints.secondary.data(), kSecondary, sizeof(kSecondary) - 1U);
    hints.revision = 17U;
    return hints;
}

dungeon::DungeonSnapshot baseline_snapshot() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.commit_generation = 7U;
    snapshot.room_index = 11U;
    snapshot.phase = dungeon::RoomPhase::combat;
    snapshot.has_active_room = true;
    snapshot.remaining_targets = 2U;
    snapshot.progression.level = 4U;
    return snapshot;
}

platform::DungeonRenderStatus saved_status() noexcept {
    platform::DungeonRenderStatus status{};
    status.indicator = platform::SaveIndicator::saved;
    return status;
}

platform::DungeonRenderStatus error_status() noexcept {
    platform::DungeonRenderStatus status{};
    status.indicator = platform::SaveIndicator::error;
    status.error = arpg::persistence::SaveError::write_failed;
    return status;
}

arpg::test::Failure priority_order_and_two_line_limit_are_deterministic() noexcept {
    const dungeon::DungeonSnapshot previous = baseline_snapshot();
    dungeon::DungeonSnapshot current = previous;
    current.phase = dungeon::RoomPhase::awaiting_exit;
    current.exits_open[0] = true;
    current.has_hole = true;
    current.abyss_exit_confirmation_armed = true;
    current.last_room_experience = 25U;
    current.progression.level = 5U;
    current.progression.unspent_passive_points = 1U;
    current.inventory_count = 1U;

    platform::HudNoticeState state{};
    state.observe(previous, current, error_status(), rebound_hints(), true);
    const platform::HudNoticeView view = state.view();
    ARPG_REQUIRE(view.primary.kind == platform::HudNoticeKind::save_error);
    ARPG_REQUIRE(view.secondary.kind
        == platform::HudNoticeKind::recovery_required);
    ARPG_REQUIRE(view.primary.priority == 110U);
    ARPG_REQUIRE(view.secondary.priority == 109U);
    ARPG_REQUIRE(std::isinf(view.primary.seconds_left));
    ARPG_REQUIRE(std::isinf(view.secondary.seconds_left));
    return {};
}

arpg::test::Failure complete_priority_order_is_exposed_by_each_trigger() noexcept {
    const dungeon::DungeonSnapshot previous = baseline_snapshot();
    const platform::ControlHints hints = rebound_hints();

    const auto primary_for = [&previous, &hints](
                                 dungeon::DungeonSnapshot current,
                                 platform::DungeonRenderStatus status,
                                 bool recovery) noexcept {
        platform::HudNoticeState state{};
        state.observe(previous, current, status, hints, recovery);
        return state.view().primary;
    };

    dungeon::DungeonSnapshot current = previous;
    ARPG_REQUIRE(primary_for(current, error_status(), false).priority == 110U);
    ARPG_REQUIRE(primary_for(current, saved_status(), true).priority == 109U);

    current.abyss_exit_confirmation_armed = true;
    ARPG_REQUIRE(primary_for(current, saved_status(), false).priority == 100U);
    current.abyss_exit_confirmation_armed = false;

    current.phase = dungeon::RoomPhase::awaiting_exit;
    current.has_hole = true;
    ARPG_REQUIRE(primary_for(current, saved_status(), false).priority == 90U);
    current.has_hole = false;
    current.exits_open[0] = true;
    ARPG_REQUIRE(primary_for(current, saved_status(), false).priority == 80U);

    current = previous;
    current.phase = dungeon::RoomPhase::cleared;
    current.remaining_targets = 0U;
    ARPG_REQUIRE(primary_for(current, saved_status(), false).priority == 70U);
    current.phase = dungeon::RoomPhase::combat;
    current.last_room_experience = 25U;
    ARPG_REQUIRE(primary_for(current, saved_status(), false).priority == 60U);
    current.last_room_experience = 0U;
    current.progression.level = 5U;
    ARPG_REQUIRE(primary_for(current, saved_status(), false).priority == 50U);
    current.progression.level = previous.progression.level;
    current.progression.unspent_passive_points = 1U;
    ARPG_REQUIRE(primary_for(current, saved_status(), false).priority == 40U);
    current.progression.unspent_passive_points = 0U;
    current.inventory_count = 1U;
    ARPG_REQUIRE(primary_for(current, saved_status(), false).priority == 30U);

    current.inventory_count = 0U;
    current.phase = dungeon::RoomPhase::awaiting_exit;
    current.exits_open.fill(false);
    current.progression.unspent_passive_points = 1U;
    const platform::HudNoticeView passive_view =
        [&previous, &current, &hints]() noexcept {
            platform::HudNoticeState state{};
            state.observe(previous, current, saved_status(), hints, false);
            return state.view();
        }();
    ARPG_REQUIRE(passive_view.primary.kind == platform::HudNoticeKind::passive_points);
    ARPG_REQUIRE(passive_view.secondary.kind == platform::HudNoticeKind::passive_tree);
    ARPG_REQUIRE(passive_view.secondary.priority == 20U);
    return {};
}

arpg::test::Failure pause_freezes_and_unpaused_time_saturates() noexcept {
    const dungeon::DungeonSnapshot previous = baseline_snapshot();
    dungeon::DungeonSnapshot current = previous;
    current.last_room_experience = 25U;
    platform::HudNoticeState state{};
    state.observe(previous, current, saved_status(), rebound_hints(), false);

    state.update(100.0F, true);
    ARPG_REQUIRE(arpg::test::near(
        state.view().primary.seconds_left, kTransientSeconds));
    state.update(1.25F, false);
    ARPG_REQUIRE(arpg::test::near(state.view().primary.seconds_left, 1.75F));
    state.update((std::numeric_limits<float>::infinity)(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
    return {};
}

arpg::test::Failure duplicate_commit_room_and_level_signals_do_not_requeue() noexcept {
    const dungeon::DungeonSnapshot previous = baseline_snapshot();
    dungeon::DungeonSnapshot current = previous;
    current.commit_generation = previous.commit_generation + 1U;
    current.room_index = previous.room_index + 1U;
    current.phase = dungeon::RoomPhase::cleared;
    current.remaining_targets = 0U;
    current.last_room_experience = 30U;
    current.progression.level = previous.progression.level + 1U;

    platform::HudNoticeState state{};
    state.observe(previous, current, saved_status(), rebound_hints(), false);
    state.update(10.0F, false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
    state.observe(previous, current, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
    return {};
}

arpg::test::Failure room_transition_and_explicit_clear_remove_room_context() noexcept {
    const dungeon::DungeonSnapshot previous = baseline_snapshot();
    dungeon::DungeonSnapshot contextual = previous;
    contextual.phase = dungeon::RoomPhase::awaiting_exit;
    contextual.has_hole = true;
    contextual.exits_open[0] = true;

    platform::HudNoticeState state{};
    state.observe(previous, contextual, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::hole_interact);
    state.clear_room_context();
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);

    state.observe(previous, contextual, saved_status(), rebound_hints(), false);
    dungeon::DungeonSnapshot next_room = contextual;
    ++next_room.room_index;
    next_room.phase = dungeon::RoomPhase::combat;
    next_room.has_hole = false;
    next_room.exits_open.fill(false);
    state.observe(contextual, next_room, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
    return {};
}

arpg::test::Failure full_queue_discards_lowest_priority_and_counts_drop() noexcept {
    const dungeon::DungeonSnapshot previous = baseline_snapshot();
    dungeon::DungeonSnapshot current = previous;
    current.phase = dungeon::RoomPhase::cleared;
    current.remaining_targets = 0U;
    current.last_room_experience = 25U;
    current.progression.level = previous.progression.level + 1U;
    current.progression.unspent_passive_points = 1U;
    current.inventory_count = 1U;

    platform::HudNoticeState state{};
    state.observe(previous, current, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.dropped_count() == 1U);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::room_clear);
    ARPG_REQUIRE(state.view().secondary.kind == platform::HudNoticeKind::reward);
    state.update(10.0F, false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
    return {};
}

arpg::test::Failure unchanged_context_does_not_repeat_overflow_drops() noexcept {
    const dungeon::DungeonSnapshot previous = baseline_snapshot();
    dungeon::DungeonSnapshot current = previous;
    current.phase = dungeon::RoomPhase::cleared;
    current.remaining_targets = 0U;
    current.last_room_experience = 25U;
    current.progression.level = previous.progression.level + 1U;
    current.progression.unspent_passive_points = 1U;
    current.inventory_count = 1U;

    platform::HudNoticeState state{};
    state.observe(previous, current, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.dropped_count() == 1U);
    state.observe(previous, current, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.dropped_count() == 1U);
    return {};
}

arpg::test::Failure save_error_and_recovery_are_persistent_and_state_owned() noexcept {
    const dungeon::DungeonSnapshot snapshot = baseline_snapshot();
    platform::HudNoticeState state{};
    state.observe(snapshot, snapshot, error_status(), rebound_hints(), true);
    state.update(1000.0F, false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::save_error);
    ARPG_REQUIRE(state.view().secondary.kind
        == platform::HudNoticeKind::recovery_required);

    state.observe(snapshot, snapshot, saved_status(), rebound_hints(), true);
    ARPG_REQUIRE(state.view().primary.kind
        == platform::HudNoticeKind::recovery_required);
    ARPG_REQUIRE(std::isinf(state.view().primary.seconds_left));
    state.observe(snapshot, snapshot, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
    return {};
}

arpg::test::Failure rebound_labels_and_non_terminated_arrays_are_bounded() noexcept {
    const dungeon::DungeonSnapshot previous = baseline_snapshot();
    const platform::ControlHints hints = rebound_hints();

    dungeon::DungeonSnapshot hole = previous;
    hole.phase = dungeon::RoomPhase::awaiting_exit;
    hole.has_hole = true;
    platform::HudNoticeState hole_state{};
    hole_state.observe(previous, hole, saved_status(), hints, false);
    ARPG_REQUIRE(std::strstr(hole_state.view().primary.text.bytes.data(), "Q")
        != nullptr);

    dungeon::DungeonSnapshot inventory = previous;
    inventory.inventory_count = 1U;
    platform::HudNoticeState inventory_state{};
    inventory_state.observe(previous, inventory, saved_status(), hints, false);
    ARPG_REQUIRE(std::strstr(inventory_state.view().primary.text.bytes.data(), "O")
        != nullptr);

    dungeon::DungeonSnapshot passive = previous;
    passive.phase = dungeon::RoomPhase::awaiting_exit;
    passive.progression.unspent_passive_points = 1U;
    platform::HudNoticeState passive_state{};
    passive_state.observe(previous, passive, saved_status(), hints, false);
    ARPG_REQUIRE(std::strstr(passive_state.view().secondary.text.bytes.data(), "T")
        != nullptr);
    ARPG_REQUIRE(hole_state.view().primary.text.bytes.back() == '\0');
    ARPG_REQUIRE(inventory_state.view().primary.text.bytes.back() == '\0');
    ARPG_REQUIRE(passive_state.view().secondary.text.bytes.back() == '\0');
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"priority order and two-line limit", &priority_order_and_two_line_limit_are_deterministic},
    {"complete priority order", &complete_priority_order_is_exposed_by_each_trigger},
    {"pause freezes notice timer", &pause_freezes_and_unpaused_time_saturates},
    {"duplicate commit room level suppression", &duplicate_commit_room_and_level_signals_do_not_requeue},
    {"room context clearing", &room_transition_and_explicit_clear_remove_room_context},
    {"bounded queue overflow", &full_queue_discards_lowest_priority_and_counts_drop},
    {"unchanged context has no repeat overflow", &unchanged_context_does_not_repeat_overflow_drops},
    {"persistent save and recovery ownership", &save_error_and_recovery_are_persistent_and_state_owned},
    {"rebound bounded control hints", &rebound_labels_and_non_terminated_arrays_are_bounded},
};

}  // namespace

arpg::test::TestSuite hud_notice_state_suite() noexcept {
    return arpg::test::make_suite("hud_notice_state", kCases);
}
