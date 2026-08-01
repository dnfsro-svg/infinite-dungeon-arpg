#include "test_framework.hpp"

#include "dungeon_view_math.hpp"
#include "hud_notice_state.hpp"

#include <cstddef>

namespace {

using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;
using arpg::platform::DoorVisualMode;
using arpg::platform::HoleVisualMode;
using arpg::platform::HudNoticeKind;

DungeonSnapshot combat_snapshot(bool unlocked) noexcept {
    DungeonSnapshot snapshot{};
    snapshot.has_active_room = true;
    snapshot.phase = RoomPhase::combat;
    snapshot.has_hole = true;
    snapshot.exits_unlocked = unlocked;
    snapshot.exits_open.fill(unlocked);
    snapshot.combat.emplace();
    snapshot.remaining_targets = 7U;
    snapshot.commit_generation = unlocked ? 9U : 8U;
    snapshot.room_index = 3U;
    return snapshot;
}

arpg::test::Failure committed_combat_unlock_opens_four_doors_and_hole() noexcept {
    const DungeonSnapshot closed = combat_snapshot(false);
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(closed)
        == HoleVisualMode::sealed);
    ARPG_REQUIRE(!arpg::platform::can_prompt_descent(
        closed, arpg::platform::kHoleCenter));

    const DungeonSnapshot open = combat_snapshot(true);
    for (const ExitDirection direction : {ExitDirection::up,
             ExitDirection::down, ExitDirection::left,
             ExitDirection::right}) {
        const std::size_t index = static_cast<std::size_t>(direction);
        ARPG_REQUIRE(open.exits_open[index]);
        ARPG_REQUIRE(arpg::platform::door_visual_mode(
            open.phase, open.has_active_room, open.exits_open[index])
            == DoorVisualMode::open);
    }
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(open)
        == HoleVisualMode::ready);
    ARPG_REQUIRE(arpg::platform::can_prompt_descent(
        open, arpg::platform::kHoleCenter));
    static_assert(arpg::platform::kHoleInteractionRadius == 3.25F,
        "Task 6 must preserve the hole interaction radius");
    ARPG_REQUIRE(!arpg::platform::can_prompt_descent(open,
        {arpg::platform::kHoleCenter.x
                + arpg::platform::kHoleInteractionRadius + 0.01F,
            arpg::platform::kHoleCenter.y, 0.0F}));
    return {};
}

arpg::test::Failure combat_unlock_hud_announces_exit_without_passive_prompt() noexcept {
    const DungeonSnapshot previous = combat_snapshot(false);
    DungeonSnapshot current = combat_snapshot(true);
    current.progression.unspent_passive_points = 1U;
    arpg::platform::HudNoticeState notices{};
    arpg::platform::DungeonRenderStatus status{};
    arpg::platform::ControlHints hints{};
    notices.observe(previous, previous, status, hints, false);
    const auto locked_view = notices.view();
    ARPG_REQUIRE(locked_view.primary.kind == HudNoticeKind::none);
    ARPG_REQUIRE(locked_view.secondary.kind == HudNoticeKind::none);
    notices.observe(previous, current, status, hints, false);
    const auto view = notices.view();
    ARPG_REQUIRE(view.primary.kind == HudNoticeKind::hole_interact);
    ARPG_REQUIRE(view.secondary.kind == HudNoticeKind::exit_ready);
    ARPG_REQUIRE(view.primary.kind != HudNoticeKind::passive_tree);
    ARPG_REQUIRE(view.secondary.kind != HudNoticeKind::passive_tree);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"committed combat unlock opens four doors and hole",
        &committed_combat_unlock_opens_four_doors_and_hole},
    {"combat unlock hud announces exit without passive prompt",
        &combat_unlock_hud_announces_exit_without_passive_prompt},
};

}  // namespace

arpg::test::TestSuite task6_exit_unlock_view_suite() noexcept {
    return arpg::test::make_suite("task6_exit_unlock_view", kCases);
}
