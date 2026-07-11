#include "test_framework.hpp"

#include "dungeon_view_math.hpp"

#include <cstring>

namespace {

using arpg::dungeon::DungeonEventKind;
using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;
using arpg::platform::DoorVisualMode;

DungeonSnapshot active_snapshot(
    std::uint64_t room_index,
    std::uint64_t room_seed) noexcept {
    DungeonSnapshot snapshot{};
    snapshot.has_active_room = true;
    snapshot.room_index = room_index;
    snapshot.room_seed = room_seed;
    snapshot.combat.emplace();
    return snapshot;
}

arpg::test::Failure door_modes_follow_room_lifecycle() noexcept {
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::locked, true)
        == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::combat, true)
        == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::cleared, true)
        == DoorVisualMode::open);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::awaiting_exit, true) == DoorVisualMode::open);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::transitioning, true) == DoorVisualMode::hidden);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::combat, false)
        == DoorVisualMode::hidden);
    return {};
}

arpg::test::Failure same_room_snapshots_can_interpolate() noexcept {
    const DungeonSnapshot previous = active_snapshot(7U, 0x1234U);
    const DungeonSnapshot current = active_snapshot(7U, 0x1234U);
    ARPG_REQUIRE(arpg::platform::can_interpolate_room(previous, current));
    return {};
}

arpg::test::Failure cross_room_and_missing_room_reject_interpolation() noexcept {
    const DungeonSnapshot room = active_snapshot(7U, 0x1234U);
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        room, active_snapshot(8U, 0x1234U)));
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        room, active_snapshot(7U, 0x5678U)));

    DungeonSnapshot missing_combat = room;
    missing_combat.combat.reset();
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        room, missing_combat));
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        DungeonSnapshot{}, room));
    return {};
}

arpg::test::Failure cleanup_routing_and_labels_are_stable() noexcept {
    ARPG_REQUIRE(arpg::platform::dungeon_event_clears_transients(
        DungeonEventKind::room_destroyed));
    ARPG_REQUIRE(arpg::platform::dungeon_event_clears_transients(
        DungeonEventKind::room_reset));
    ARPG_REQUIRE(!arpg::platform::dungeon_event_clears_transients(
        DungeonEventKind::room_entered));
    ARPG_REQUIRE(std::strcmp(
        arpg::platform::room_phase_label(RoomPhase::awaiting_exit),
        "AWAITING EXIT") == 0);
    ARPG_REQUIRE(std::strcmp(
        arpg::platform::exit_direction_label(ExitDirection::left),
        "LEFT") == 0);
    ARPG_REQUIRE(std::strcmp(
        arpg::platform::exit_direction_label(ExitDirection::none),
        "NONE") == 0);
    return {};
}

arpg::test::Failure fade_alpha_clamps_to_transition_window() noexcept {
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(-1.0F), 0.0, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(0.0F), 0.0, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(0.06F), 0.5, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(0.12F), 1.0, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(1.0F), 1.0, 1.0e-6));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"door lifecycle modes", &door_modes_follow_room_lifecycle},
    {"same room interpolation", &same_room_snapshots_can_interpolate},
    {"cross and missing room rejection", &cross_room_and_missing_room_reject_interpolation},
    {"cleanup routing and labels", &cleanup_routing_and_labels_are_stable},
    {"transition fade alpha", &fade_alpha_clamps_to_transition_window},
};

}  // namespace

arpg::test::TestSuite dungeon_view_math_suite() noexcept {
    return arpg::test::make_suite("dungeon_view_math", kCases);
}
