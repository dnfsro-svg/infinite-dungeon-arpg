#include "test_framework.hpp"

#include "death_overlay_font.hpp"
#include "hud_font.hpp"
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
    current.exits_unlocked = true;
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
    current.exits_unlocked = true;
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
    current.exits_unlocked = false;
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
    contextual.exits_unlocked = true;
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
    next_room.exits_unlocked = false;
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

arpg::test::Failure room_clear_edge_does_not_require_commit_or_room_change() noexcept {
    const dungeon::DungeonSnapshot combat = baseline_snapshot();
    dungeon::DungeonSnapshot cleared = combat;
    cleared.phase = dungeon::RoomPhase::cleared;
    cleared.remaining_targets = 0U;

    platform::HudNoticeState state{};
    state.observe(combat, combat, saved_status(), rebound_hints(), false);
    state.observe(combat, cleared, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::room_clear);
    state.update(10.0F, false);
    state.observe(combat, cleared, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
    return {};
}

arpg::test::Failure room_zero_context_clears_on_next_room() noexcept {
    dungeon::DungeonSnapshot first_room = baseline_snapshot();
    first_room.room_index = 0U;
    dungeon::DungeonSnapshot contextual = first_room;
    contextual.phase = dungeon::RoomPhase::awaiting_exit;
    contextual.exits_unlocked = true;
    contextual.has_hole = true;

    platform::HudNoticeState state{};
    state.observe(first_room, contextual, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::hole_interact);

    dungeon::DungeonSnapshot next_room = contextual;
    next_room.room_index = 1U;
    next_room.phase = dungeon::RoomPhase::combat;
    next_room.exits_unlocked = false;
    next_room.has_hole = false;
    state.observe(contextual, next_room, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
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
    hole.exits_unlocked = true;
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

arpg::test::Failure production_visible_notices_are_chinese_first_and_font_covered() noexcept {
    const dungeon::DungeonSnapshot previous = baseline_snapshot();
    const platform::ControlHints hints = rebound_hints();
    const platform::HudFontPlan font = platform::hud_font_plan();
    const auto require_primary = [&](const dungeon::DungeonSnapshot& current,
                                     platform::DungeonRenderStatus status,
                                     bool recovery,
                                     const char* expected) noexcept {
        platform::HudNoticeState state{};
        state.observe(previous, current, status, hints, recovery);
        return std::strcmp(state.view().primary.text.bytes.data(), expected) == 0
            && platform::death_overlay_font_covers_text(font.shared, expected);
    };

    ARPG_REQUIRE(require_primary(previous, error_status(), false, u8"保存失败"));
    ARPG_REQUIRE(require_primary(previous, saved_status(), true, u8"需要恢复存档"));

    dungeon::DungeonSnapshot current = previous;
    current.is_abyss = true;
    current.abyss_rule = arpg::abyss::AbyssRuleId::abyss_fury;
    current.abyss_exit_confirmation_armed = true;
    current.abyss_exit_confirmation_transition = dungeon::TransitionKind::door;
    ARPG_REQUIRE(require_primary(current, saved_status(), false,
        u8"离开后再次触碰同一出口以放弃全部剩余奖励"));

    current = previous;
    current.phase = dungeon::RoomPhase::awaiting_exit;
    current.exits_unlocked = true;
    current.has_hole = true;
    ARPG_REQUIRE(require_primary(current, saved_status(), false, u8"Q 进入下一层"));

    current = previous;
    current.phase = dungeon::RoomPhase::awaiting_exit;
    current.exits_unlocked = true;
    current.exits_open[0] = true;
    ARPG_REQUIRE(require_primary(current, saved_status(), false, u8"进入出口"));

    current = previous;
    current.phase = dungeon::RoomPhase::cleared;
    current.remaining_targets = 0U;
    ARPG_REQUIRE(require_primary(current, saved_status(), false, u8"房间已清理"));

    current = previous;
    current.last_room_experience = 25U;
    ARPG_REQUIRE(require_primary(current, saved_status(), false, u8"奖励 +25 XP"));

    current = previous;
    current.progression.level = 5U;
    ARPG_REQUIRE(require_primary(current, saved_status(), false, u8"升级至 5 级"));

    current = previous;
    current.progression.unspent_passive_points = 1U;
    ARPG_REQUIRE(require_primary(current, saved_status(), false, u8"有未分配被动点"));

    current = previous;
    current.inventory_count = 1U;
    ARPG_REQUIRE(require_primary(current, saved_status(), false, u8"O 打开背包"));

    current = previous;
    current.phase = dungeon::RoomPhase::awaiting_exit;
    current.progression.unspent_passive_points = 1U;
    platform::HudNoticeState tree_state{};
    tree_state.observe(previous, current, saved_status(), hints, false);
    ARPG_REQUIRE(std::strcmp(tree_state.view().secondary.text.bytes.data(),
        u8"T 打开被动树") == 0);
    ARPG_REQUIRE(platform::death_overlay_font_covers_text(font.shared,
        tree_state.view().secondary.text.bytes.data()));
    return {};
}

arpg::test::Failure abyss_confirmation_between_presented_frames_enqueues_once() noexcept {
    const dungeon::DungeonSnapshot baseline = baseline_snapshot();
    platform::HudNoticeState state{};
    state.observe(baseline, baseline, saved_status(), rebound_hints(), false);

    dungeon::DungeonSnapshot armed = baseline;
    armed.is_abyss = true;
    armed.abyss_rule = arpg::abyss::AbyssRuleId::abyss_fury;
    armed.abyss_exit_confirmation_armed = true;
    state.observe(armed, armed, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::abyss_abandon);
    state.update(1.0F, false);
    const float after_one_second = state.view().primary.seconds_left;
    state.observe(armed, armed, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(arpg::test::near(
        state.view().primary.seconds_left, after_one_second));
    state.update(after_one_second, false);
    state.observe(armed, armed, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
    return {};
}

arpg::test::Failure progression_between_presented_frames_enqueues_each_edge_once() noexcept {
    const dungeon::DungeonSnapshot baseline = baseline_snapshot();
    platform::HudNoticeState state{};
    state.observe(baseline, baseline, saved_status(), rebound_hints(), false);

    dungeon::DungeonSnapshot progressed = baseline;
    progressed.progression.level = baseline.progression.level + 1U;
    progressed.progression.unspent_passive_points = 1U;
    state.observe(progressed, progressed, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::level_up);
    ARPG_REQUIRE(state.view().secondary.kind == platform::HudNoticeKind::passive_points);
    state.update(1.0F, false);
    const float level_time = state.view().primary.seconds_left;
    const float points_time = state.view().secondary.seconds_left;
    state.observe(progressed, progressed, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(arpg::test::near(state.view().primary.seconds_left, level_time));
    ARPG_REQUIRE(arpg::test::near(state.view().secondary.seconds_left, points_time));
    state.update(level_time, false);
    state.observe(progressed, progressed, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind == platform::HudNoticeKind::none);
    return {};
}

arpg::test::Failure pickup_feedback_is_reward_priority_and_keeps_abyss_style()
    noexcept {
    platform::HudText96 text{};
    static_cast<void>(std::snprintf(text.bytes.data(), text.bytes.size(),
        u8"已拾取：稀有 胸甲 · i24"));
    platform::HudNoticeState normal{};
    normal.publish_loot_pickup(text, false);
    ARPG_REQUIRE(normal.view().primary.kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(!normal.view().primary.abyss);
    ARPG_REQUIRE(normal.view().primary.priority == 60U);
    ARPG_REQUIRE(arpg::test::near(normal.view().primary.seconds_left, 3.0F));

    platform::HudNoticeState abyss{};
    abyss.publish_loot_pickup(text, true);
    ARPG_REQUIRE(abyss.view().primary.kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(abyss.view().primary.abyss);
    ARPG_REQUIRE(std::strcmp(abyss.view().primary.text.bytes.data(),
        text.bytes.data()) == 0);
    return {};
}

arpg::test::Failure pickup_feedback_survives_room_change_for_full_lifetime()
    noexcept {
    const dungeon::DungeonSnapshot baseline = baseline_snapshot();
    platform::HudNoticeState state{};
    state.observe(baseline, baseline, saved_status(), rebound_hints(), false);
    platform::HudText96 text{};
    static_cast<void>(std::snprintf(text.bytes.data(), text.bytes.size(),
        u8"已拾取：普通 Iron Blade · i1"));
    state.publish_loot_pickup(text, false);

    dungeon::DungeonSnapshot next = baseline;
    ++next.room_index;
    ++next.commit_generation;
    state.observe(baseline, next, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(state.view().primary.kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(arpg::test::near(state.view().primary.seconds_left, 3.0F));
    state.update(1.0F, false);
    ARPG_REQUIRE(arpg::test::near(state.view().primary.seconds_left, 2.0F));
    return {};
}

arpg::test::Failure pickup_feedback_priority_is_below_transaction_blockers()
    noexcept {
    platform::HudText96 text{};
    static_cast<void>(std::snprintf(text.bytes.data(), text.bytes.size(),
        u8"已拾取：稀有 Ward Coat · i24"));
    const auto baseline = baseline_snapshot();

    platform::HudNoticeState save{};
    save.publish_loot_pickup(text, false);
    save.observe(baseline, baseline, error_status(), rebound_hints(), false);
    ARPG_REQUIRE(save.view().primary.kind == platform::HudNoticeKind::save_error);
    ARPG_REQUIRE(save.view().primary.priority == 110U);
    ARPG_REQUIRE(save.view().secondary.kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(save.view().secondary.priority == 60U);

    platform::HudNoticeState recovery{};
    recovery.publish_loot_pickup(text, false);
    recovery.observe(baseline, baseline, saved_status(), rebound_hints(), true);
    ARPG_REQUIRE(recovery.view().primary.kind
        == platform::HudNoticeKind::recovery_required);
    ARPG_REQUIRE(recovery.view().primary.priority == 109U);
    ARPG_REQUIRE(recovery.view().secondary.kind
        == platform::HudNoticeKind::loot_pickup);

    dungeon::DungeonSnapshot abyss = baseline;
    abyss.abyss_exit_confirmation_armed = true;
    platform::HudNoticeState confirmation{};
    confirmation.publish_loot_pickup(text, true);
    confirmation.observe(baseline, abyss, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(confirmation.view().primary.kind
        == platform::HudNoticeKind::abyss_abandon);
    ARPG_REQUIRE(confirmation.view().primary.priority == 100U);
    ARPG_REQUIRE(confirmation.view().secondary.kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(confirmation.view().secondary.abyss);

    dungeon::DungeonSnapshot rewarded = baseline;
    rewarded.last_room_experience = 25U;
    platform::HudNoticeState reward{};
    reward.publish_loot_pickup(text, false);
    reward.observe(baseline, rewarded, saved_status(), rebound_hints(), false);
    ARPG_REQUIRE(reward.view().primary.kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(reward.view().secondary.kind == platform::HudNoticeKind::reward);
    ARPG_REQUIRE(reward.view().primary.priority == 60U);
    ARPG_REQUIRE(reward.view().secondary.priority == 60U);
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
    {"room clear edge without commit", &room_clear_edge_does_not_require_commit_or_room_change},
    {"room zero context transition", &room_zero_context_clears_on_next_room},
    {"persistent save and recovery ownership", &save_error_and_recovery_are_persistent_and_state_owned},
    {"rebound bounded control hints", &rebound_labels_and_non_terminated_arrays_are_bounded},
    {"production Chinese notice corpus", &production_visible_notices_are_chinese_first_and_font_covered},
    {"abyss confirmation presented edge once", &abyss_confirmation_between_presented_frames_enqueues_once},
    {"progression presented edges once", &progression_between_presented_frames_enqueues_each_edge_once},
    {"pickup feedback reward priority abyss style",
        &pickup_feedback_is_reward_priority_and_keeps_abyss_style},
    {"pickup feedback survives room change",
        &pickup_feedback_survives_room_change_for_full_lifetime},
    {"pickup feedback priority ordering",
        &pickup_feedback_priority_is_below_transaction_blockers},
};

}  // namespace

arpg::test::TestSuite hud_notice_state_suite() noexcept {
    return arpg::test::make_suite("hud_notice_state", kCases);
}
