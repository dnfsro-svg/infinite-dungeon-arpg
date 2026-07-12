#include "test_framework.hpp"

#include "dungeon_view_math.hpp"

#include <cstring>

namespace {

using arpg::dungeon::DungeonEventKind;
using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;
using arpg::platform::DoorTheme;
using arpg::platform::DoorVisualMode;
using arpg::platform::HoleVisualMode;
using arpg::platform::Rgba8;
using arpg::platform::SaveIndicator;

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
        RoomPhase::committing, true) == DoorVisualMode::open);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::faulted, true) == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::transitioning, true) == DoorVisualMode::hidden);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::combat, false)
        == DoorVisualMode::hidden);
    return {};
}

bool rgba_equals(Rgba8 lhs, Rgba8 rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b
        && lhs.a == rhs.a;
}

arpg::test::Failure door_themes_match_directional_elements() noexcept {
    const DoorTheme up = arpg::platform::door_theme(ExitDirection::up);
    const DoorTheme down = arpg::platform::door_theme(ExitDirection::down);
    const DoorTheme left = arpg::platform::door_theme(ExitDirection::left);
    const DoorTheme right = arpg::platform::door_theme(ExitDirection::right);
    ARPG_REQUIRE(up.element == arpg::dungeon::DungeonElement::fire);
    ARPG_REQUIRE(down.element == arpg::dungeon::DungeonElement::water);
    ARPG_REQUIRE(left.element == arpg::dungeon::DungeonElement::lightning);
    ARPG_REQUIRE(right.element == arpg::dungeon::DungeonElement::chaos);
    ARPG_REQUIRE(std::strcmp(up.arrow, "\xE2\x86\x91") == 0);
    ARPG_REQUIRE(std::strcmp(down.arrow, "\xE2\x86\x93") == 0);
    ARPG_REQUIRE(std::strcmp(left.arrow, "\xE2\x86\x90") == 0);
    ARPG_REQUIRE(std::strcmp(right.arrow, "\xE2\x86\x92") == 0);
    ARPG_REQUIRE(rgba_equals(up.frame, {236U, 92U, 54U, 255U}));
    ARPG_REQUIRE(rgba_equals(down.frame, {64U, 156U, 236U, 255U}));
    ARPG_REQUIRE(rgba_equals(left.frame, {236U, 218U, 72U, 255U}));
    ARPG_REQUIRE(rgba_equals(right.frame, {154U, 76U, 210U, 255U}));
    return {};
}

arpg::test::Failure ecosystem_tints_are_stable() noexcept {
    const Rgba8 fire = arpg::platform::ecosystem_tint(
        arpg::dungeon::DungeonElement::fire);
    const Rgba8 water = arpg::platform::ecosystem_tint(
        arpg::dungeon::DungeonElement::water);
    const Rgba8 lightning = arpg::platform::ecosystem_tint(
        arpg::dungeon::DungeonElement::lightning);
    const Rgba8 chaos = arpg::platform::ecosystem_tint(
        arpg::dungeon::DungeonElement::chaos);
    ARPG_REQUIRE(rgba_equals(fire, {88U, 43U, 34U, 255U}));
    ARPG_REQUIRE(rgba_equals(water, {35U, 70U, 104U, 255U}));
    ARPG_REQUIRE(rgba_equals(lightning, {96U, 90U, 34U, 255U}));
    ARPG_REQUIRE(rgba_equals(chaos, {73U, 43U, 99U, 255U}));
    return {};
}

arpg::test::Failure abyss_pulse_is_bounded_and_periodic() noexcept {
    const float negative = arpg::platform::abyss_pulse_alpha(-0.25F);
    const float at_start = arpg::platform::abyss_pulse_alpha(0.0F);
    const float at_cycle = arpg::platform::abyss_pulse_alpha(1.0F);
    const float huge = arpg::platform::abyss_pulse_alpha(100000000.0F);
    ARPG_REQUIRE(negative >= 0.0F && negative <= 1.0F);
    ARPG_REQUIRE(at_start >= 0.0F && at_start <= 1.0F);
    ARPG_REQUIRE(at_cycle >= 0.0F && at_cycle <= 1.0F);
    ARPG_REQUIRE(huge >= 0.0F && huge <= 1.0F);
    ARPG_REQUIRE(arpg::test::near(at_start, at_cycle, 1.0e-6));
    return {};
}

arpg::test::Failure hole_modes_follow_room_phase() noexcept {
    DungeonSnapshot snapshot = active_snapshot(1U, 2U);
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::hidden);
    snapshot.has_hole = true;
    snapshot.phase = RoomPhase::locked;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::sealed);
    snapshot.phase = RoomPhase::combat;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::sealed);
    snapshot.phase = RoomPhase::cleared;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::ready);
    snapshot.phase = RoomPhase::awaiting_exit;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::ready);
    snapshot.phase = RoomPhase::committing;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::busy);
    snapshot.phase = RoomPhase::faulted;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::faulted);
    return {};
}

arpg::test::Failure player_hole_range_uses_xy_radius() noexcept {
    using arpg::combat::Vec3;
    const Vec3 center{4.0F, -2.0F, 5.0F};
    ARPG_REQUIRE(arpg::platform::player_in_hole_range(center, center, 1.2F));
    ARPG_REQUIRE(arpg::platform::player_in_hole_range(
        Vec3{5.2F, -2.0F, -100.0F}, center, 1.2F));
    ARPG_REQUIRE(!arpg::platform::player_in_hole_range(
        Vec3{5.21F, -2.0F, 0.0F}, center, 1.2F));
    ARPG_REQUIRE(!arpg::platform::player_in_hole_range(
        center, center, -1.0F));
    return {};
}

arpg::test::Failure indicators_and_phase_labels_are_stable() noexcept {
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::none), "") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::saving), "SAVING") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::saved), "SAVED") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::recovered), "RECOVERED") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::error), "SAVE ERROR") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::room_phase_label(
        RoomPhase::committing), "COMMITTING") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::room_phase_label(
        RoomPhase::faulted), "FAULTED") == 0);
    return {};
}

arpg::test::Failure recovery_requires_fault_and_n_press() noexcept {
    ARPG_REQUIRE(!arpg::platform::recovery_requested(false, false));
    ARPG_REQUIRE(!arpg::platform::recovery_requested(false, true));
    ARPG_REQUIRE(!arpg::platform::recovery_requested(true, false));
    ARPG_REQUIRE(arpg::platform::recovery_requested(true, true));
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
    using arpg::platform::TransitionVisualState;

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

    const TransitionVisualState destroyed =
        arpg::platform::transition_after_dungeon_event(
            {}, DungeonEventKind::room_destroyed);
    ARPG_REQUIRE(arpg::test::near(
        destroyed.seconds_left, 0.12, 1.0e-6));

    const TransitionVisualState first_transition =
        arpg::platform::transition_after_room_phase(
            {}, RoomPhase::transitioning);
    ARPG_REQUIRE(arpg::test::near(
        first_transition.seconds_left, 0.12, 1.0e-6));
    ARPG_REQUIRE(first_transition.transition_phase_seen);

    const TransitionVisualState advanced =
        arpg::platform::advance_transition(
            first_transition, 0.05F);
    ARPG_REQUIRE(arpg::test::near(
        advanced.seconds_left, 0.07, 1.0e-6));
    const TransitionVisualState repeated_transition =
        arpg::platform::transition_after_room_phase(
            advanced, RoomPhase::transitioning);
    ARPG_REQUIRE(arpg::test::near(
        repeated_transition.seconds_left, 0.07, 1.0e-6));

    const TransitionVisualState left_transition =
        arpg::platform::transition_after_room_phase(
            repeated_transition, RoomPhase::combat);
    ARPG_REQUIRE(!left_transition.transition_phase_seen);
    const TransitionVisualState reentered_transition =
        arpg::platform::transition_after_room_phase(
            left_transition, RoomPhase::transitioning);
    ARPG_REQUIRE(arpg::test::near(
        reentered_transition.seconds_left, 0.12, 1.0e-6));

    const TransitionVisualState reset =
        arpg::platform::transition_after_dungeon_event(
            reentered_transition, DungeonEventKind::room_reset);
    ARPG_REQUIRE(arpg::test::near(
        reset.seconds_left, 0.0, 1.0e-6));
    ARPG_REQUIRE(!reset.transition_phase_seen);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"door lifecycle modes", &door_modes_follow_room_lifecycle},
    {"same room interpolation", &same_room_snapshots_can_interpolate},
    {"cross and missing room rejection", &cross_room_and_missing_room_reject_interpolation},
    {"cleanup routing and labels", &cleanup_routing_and_labels_are_stable},
    {"transition fade alpha", &fade_alpha_clamps_to_transition_window},
    {"directional door themes", &door_themes_match_directional_elements},
    {"ecosystem tints", &ecosystem_tints_are_stable},
    {"abyss pulse bounds", &abyss_pulse_is_bounded_and_periodic},
    {"hole visual modes", &hole_modes_follow_room_phase},
    {"XY hole range", &player_hole_range_uses_xy_radius},
    {"save indicators and labels", &indicators_and_phase_labels_are_stable},
    {"recovery request gate", &recovery_requires_fault_and_n_press},
};

}  // namespace

arpg::test::TestSuite dungeon_view_math_suite() noexcept {
    return arpg::test::make_suite("dungeon_view_math", kCases);
}
