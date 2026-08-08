#include "test_framework.hpp"

#include "combat_renderer.hpp"
#include "dungeon_view_math.hpp"
#include "progression/progression_rules.hpp"

#include <array>
#include <cstring>

namespace {

using arpg::dungeon::DungeonEventKind;
using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;
using arpg::platform::DoorTheme;
using arpg::platform::DoorVisualMode;
using arpg::platform::EnvironmentHazardVisualMode;
using arpg::platform::HoleVisualMode;
using arpg::platform::Rgba8;
using arpg::platform::SaveIndicator;

DungeonSnapshot active_snapshot(
    std::uint64_t room_index,
    std::uint64_t room_seed,
    std::uint64_t room_instance_generation = 1U) noexcept {
    DungeonSnapshot snapshot{};
    snapshot.has_active_room = true;
    snapshot.room_index = room_index;
    snapshot.room_seed = room_seed;
    snapshot.room_instance_generation = room_instance_generation;
    snapshot.combat.emplace();
    return snapshot;
}

arpg::test::Failure door_modes_follow_room_lifecycle() noexcept {
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::locked, true, false)
        == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::combat, true, false)
        == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::cleared, true, true)
        == DoorVisualMode::open);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::awaiting_exit, true, true) == DoorVisualMode::open);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::committing, true, false) == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::committing, true, true) == DoorVisualMode::open);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::faulted, true, false) == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::transitioning, true, false) == DoorVisualMode::hidden);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::combat, false, false)
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

arpg::test::Failure directional_door_decisions_use_element_sprites_and_modes() noexcept {
    struct Expected final {
        ExitDirection direction;
        arpg::dungeon::DungeonElement element;
        arpg::platform::MaterialSpriteId sprite;
    };
    constexpr std::array<Expected, 4> kExpected{{
        {ExitDirection::up, arpg::dungeon::DungeonElement::fire,
            arpg::platform::MaterialSpriteId::environment_door_fire},
        {ExitDirection::down, arpg::dungeon::DungeonElement::water,
            arpg::platform::MaterialSpriteId::environment_door_water},
        {ExitDirection::left, arpg::dungeon::DungeonElement::lightning,
            arpg::platform::MaterialSpriteId::environment_door_lightning},
        {ExitDirection::right, arpg::dungeon::DungeonElement::chaos,
            arpg::platform::MaterialSpriteId::environment_door_chaos},
    }};
    for (const Expected& expected : kExpected) {
        const DoorTheme theme = arpg::platform::door_theme(expected.direction);
        const auto closed = arpg::platform::door_render_decision(
            DoorVisualMode::closed, expected.direction);
        const auto open = arpg::platform::door_render_decision(
            DoorVisualMode::open, expected.direction);
        ARPG_REQUIRE(theme.element == expected.element);
        ARPG_REQUIRE(closed.sprite == expected.sprite);
        ARPG_REQUIRE(open.sprite == expected.sprite);
        ARPG_REQUIRE(std::strcmp(closed.label, theme.label) == 0);
        ARPG_REQUIRE(rgba_equals(closed.text, theme.frame));
        ARPG_REQUIRE(rgba_equals(closed.body_tint, {150U, 150U, 150U, 255U}));
        ARPG_REQUIRE(rgba_equals(open.body_tint, {255U, 255U, 255U, 255U}));
        ARPG_REQUIRE(closed.draw_lock_marker);
        ARPG_REQUIRE(!open.draw_lock_marker);
    }
    return {};
}

arpg::test::Failure door_arrow_geometry_points_outward_inside_scaled_box() noexcept {
    constexpr float kCenterX = 240.0F;
    constexpr float kCenterY = 160.0F;
    constexpr float kScale = 2.0F;
    const auto inside = [](Vector2 point) noexcept {
        return point.x >= kCenterX - 24.0F * kScale
            && point.x <= kCenterX + 24.0F * kScale
            && point.y >= kCenterY - 24.0F * kScale
            && point.y <= kCenterY + 24.0F * kScale;
    };
    for (const ExitDirection direction : {ExitDirection::up,
             ExitDirection::down, ExitDirection::left, ExitDirection::right}) {
        const auto arrow = arpg::platform::door_arrow_geometry(
            direction, kCenterX, kCenterY, kScale);
        ARPG_REQUIRE(arrow.thickness > 0.0F);
        ARPG_REQUIRE(inside(arrow.tail));
        ARPG_REQUIRE(inside(arrow.tip));
        ARPG_REQUIRE(inside(arrow.head_left));
        ARPG_REQUIRE(inside(arrow.head_right));
        switch (direction) {
        case ExitDirection::up:
            ARPG_REQUIRE(arrow.tip.y < arrow.tail.y);
            break;
        case ExitDirection::down:
            ARPG_REQUIRE(arrow.tip.y > arrow.tail.y);
            break;
        case ExitDirection::left:
            ARPG_REQUIRE(arrow.tip.x < arrow.tail.x);
            break;
        case ExitDirection::right:
            ARPG_REQUIRE(arrow.tip.x > arrow.tail.x);
            break;
        case ExitDirection::none:
            ARPG_REQUIRE(false);
            break;
        }
    }
    return {};
}

arpg::test::Failure abyss_door_markers_follow_only_directional_preview() noexcept {
    DungeonSnapshot snapshot = active_snapshot(3U, 4U);
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::life_sacrifice;
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::high;
    for (const ExitDirection direction : {ExitDirection::up,
             ExitDirection::down, ExitDirection::left,
             ExitDirection::right, ExitDirection::none}) {
        ARPG_REQUIRE(!arpg::platform::abyss_door_marker(snapshot, direction));
    }

    for (const ExitDirection selected : {ExitDirection::up,
             ExitDirection::down, ExitDirection::left,
             ExitDirection::right}) {
        snapshot.abyss_doors.fill(false);
        snapshot.abyss_doors[static_cast<std::size_t>(selected)] = true;
        for (const ExitDirection direction : {ExitDirection::up,
                 ExitDirection::down, ExitDirection::left,
                 ExitDirection::right}) {
            ARPG_REQUIRE(arpg::platform::abyss_door_marker(snapshot, direction)
                == (direction == selected));
        }
        const auto closed = arpg::platform::door_render_decision(
            DoorVisualMode::closed, selected);
        const auto open = arpg::platform::door_render_decision(
            DoorVisualMode::open, selected);
        ARPG_REQUIRE(closed.draw_lock_marker);
        ARPG_REQUIRE(!open.draw_lock_marker);
    }
    return {};
}

arpg::test::Failure environment_hazards_use_snapshot_geometry_and_phase() noexcept {
    struct Expected final {
        arpg::combat::HazardKind kind;
        Rgba8 active_outline;
    };
    constexpr Expected cases[] = {
        {arpg::combat::HazardKind::thunderstorm,
            {132U, 211U, 255U, 235U}},
        {arpg::combat::HazardKind::hunting_flame,
            {255U, 91U, 48U, 235U}},
        {arpg::combat::HazardKind::chaos_expansion,
            {221U, 62U, 188U, 235U}},
    };
    float radius = 0.8F;
    for (const Expected& expected : cases) {
        arpg::combat::HazardSnapshot hazard{};
        hazard.active = true;
        hazard.source = arpg::combat::HazardSource::abyss_environment;
        hazard.kind = expected.kind;
        hazard.center = {2.25F, -3.5F, 0.0F};
        hazard.radius = radius;
        hazard.telegraph_ticks = 12U;
        auto visual = arpg::platform::environment_hazard_visual(hazard);
        ARPG_REQUIRE(visual.mode == EnvironmentHazardVisualMode::warning);
        ARPG_REQUIRE(arpg::test::near(visual.center.x, 2.25, 1.0e-6));
        ARPG_REQUIRE(arpg::test::near(visual.center.y, -3.5, 1.0e-6));
        ARPG_REQUIRE(arpg::test::near(visual.radius, radius, 1.0e-6));
        ARPG_REQUIRE(rgba_equals(visual.outline, {255U, 203U, 91U, 235U}));

        hazard.telegraph_ticks = 0U;
        hazard.active_ticks = 60U;
        visual = arpg::platform::environment_hazard_visual(hazard);
        ARPG_REQUIRE(visual.mode == EnvironmentHazardVisualMode::active);
        ARPG_REQUIRE(rgba_equals(visual.outline, expected.active_outline));
        ARPG_REQUIRE(arpg::test::near(visual.radius, radius, 1.0e-6));
        radius += 0.2F;
    }

    arpg::combat::HazardSnapshot monster_hazard{};
    monster_hazard.active = true;
    monster_hazard.source = arpg::combat::HazardSource::monster;
    monster_hazard.kind = arpg::combat::HazardKind::thunderstorm;
    ARPG_REQUIRE(arpg::platform::environment_hazard_visual(monster_hazard).mode
        == EnvironmentHazardVisualMode::hidden);
    return {};
}

arpg::test::Failure abyss_hud_uses_committed_snapshot_labels_and_counts() noexcept {
    DungeonSnapshot snapshot = active_snapshot(5U, 6U);
    auto hidden = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(!hidden.visible);

    snapshot.is_abyss = true;
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::abyss_fury;
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::high;
    snapshot.abyss_pending_rewards = 2U;
    snapshot.abyss_unpicked_rewards = 1U;
    const auto values = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(values.visible);
    ARPG_REQUIRE(std::strcmp(values.danger_label, "ABYSS HIGH") == 0);
    ARPG_REQUIRE(std::strcmp(values.rule_label, "ABYSS FURY") == 0);
    ARPG_REQUIRE(std::strcmp(values.effect_label,
        "Monsters: damage and attack speed x145%") == 0);
    ARPG_REQUIRE(values.pending_rewards == 2U);
    ARPG_REQUIRE(values.unpicked_rewards == 1U);
    ARPG_REQUIRE(!values.confirmation_visible);

    snapshot.abyss_danger = arpg::abyss::AbyssDanger::low;
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::thunderstorm;
    const auto low = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(std::strcmp(low.danger_label, "ABYSS LOW") == 0);
    ARPG_REQUIRE(std::strcmp(low.rule_label, "THUNDERSTORM") == 0);
    ARPG_REQUIRE(std::strcmp(low.effect_label,
        "Every 180t: warn 45t/radius 0.8, hit 15% max HP lightning") == 0);
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::medium;
    ARPG_REQUIRE(std::strcmp(
        arpg::platform::abyss_hud_values(snapshot).danger_label,
        "ABYSS MED") == 0);
    return {};
}

arpg::test::Failure abyss_confirmation_prompt_preserves_transition_and_direction() noexcept {
    DungeonSnapshot snapshot = active_snapshot(7U, 8U);
    snapshot.is_abyss = true;
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::hunting_flames;
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::medium;
    snapshot.abyss_pending_rewards = 1U;
    snapshot.abyss_unpicked_rewards = 2U;
    snapshot.abyss_exit_confirmation_armed = true;
    snapshot.abyss_exit_confirmation_transition =
        arpg::dungeon::TransitionKind::door;
    snapshot.abyss_exit_confirmation_direction = ExitDirection::left;
    auto values = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(values.confirmation_visible);
    ARPG_REQUIRE(values.confirmation_transition
        == arpg::dungeon::TransitionKind::door);
    ARPG_REQUIRE(values.confirmation_direction == ExitDirection::left);
    ARPG_REQUIRE(std::strcmp(values.confirmation_label,
        "Touch the SAME door again to abandon ALL remaining rewards") == 0);

    snapshot.abyss_exit_confirmation_transition =
        arpg::dungeon::TransitionKind::descent;
    snapshot.abyss_exit_confirmation_direction = ExitDirection::none;
    values = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(std::strcmp(values.confirmation_label,
        "Press E again to abandon remaining rewards and descend") == 0);

    snapshot.abyss_exit_confirmation_armed = false;
    ARPG_REQUIRE(!arpg::platform::abyss_hud_values(snapshot)
        .confirmation_visible);
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
    snapshot.exits_unlocked = true;
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
    ARPG_REQUIRE(arpg::platform::player_in_hole_range(center, center, 2.0F));
    ARPG_REQUIRE(arpg::platform::player_in_hole_range(
        Vec3{6.0F, -2.0F, -100.0F}, center, 2.0F));
    ARPG_REQUIRE(!arpg::platform::player_in_hole_range(
        Vec3{6.01F, -2.0F, 0.0F}, center, 2.0F));
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
    const DungeonSnapshot previous = active_snapshot(7U, 0x1234U, 9U);
    const DungeonSnapshot current = active_snapshot(7U, 0x1234U, 9U);
    ARPG_REQUIRE(arpg::platform::can_interpolate_room(previous, current));

    const DungeonSnapshot reconstructed = active_snapshot(7U, 0x1234U, 10U);
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        previous, reconstructed));

    DungeonSnapshot death_visible = current;
    death_visible.death.emplace();
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        current, death_visible));
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        death_visible, current));

    DungeonSnapshot death_pending = current;
    death_pending.phase = RoomPhase::death_pending;
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        current, death_pending));
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

arpg::test::Failure progression_hud_values_follow_snapshot() noexcept {
    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.progression = {37U, 42U, 36U, 36U};
    snapshot.pending_room_experience = 25U;
    const auto rules = arpg::progression::default_progression_rules();
    const auto values = arpg::platform::progression_hud_values(snapshot, rules);
    ARPG_REQUIRE(values.level == 37U);
    ARPG_REQUIRE(values.experience == 42U);
    ARPG_REQUIRE(values.required_experience == 100U);
    ARPG_REQUIRE(values.unspent_passive_points == 36U);
    ARPG_REQUIRE(values.pending_room_experience == 25U);
    ARPG_REQUIRE(!values.maximum_level);

    snapshot.progression = {100U, 0U, 99U, 99U};
    const auto maximum = arpg::platform::progression_hud_values(snapshot, rules);
    ARPG_REQUIRE(maximum.maximum_level);
    ARPG_REQUIRE(maximum.required_experience == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"door lifecycle modes", &door_modes_follow_room_lifecycle},
    {"same room interpolation", &same_room_snapshots_can_interpolate},
    {"cross and missing room rejection", &cross_room_and_missing_room_reject_interpolation},
    {"cleanup routing and labels", &cleanup_routing_and_labels_are_stable},
    {"transition fade alpha", &fade_alpha_clamps_to_transition_window},
    {"directional door themes", &door_themes_match_directional_elements},
    {"directional door sprites and modes",
        &directional_door_decisions_use_element_sprites_and_modes},
    {"directional door arrow geometry",
        &door_arrow_geometry_points_outward_inside_scaled_box},
    {"abyss door marker preview", &abyss_door_markers_follow_only_directional_preview},
    {"environment hazard snapshot visuals", &environment_hazards_use_snapshot_geometry_and_phase},
    {"abyss HUD snapshot values", &abyss_hud_uses_committed_snapshot_labels_and_counts},
    {"abyss confirmation prompt values", &abyss_confirmation_prompt_preserves_transition_and_direction},
    {"ecosystem tints", &ecosystem_tints_are_stable},
    {"abyss pulse bounds", &abyss_pulse_is_bounded_and_periodic},
    {"hole visual modes", &hole_modes_follow_room_phase},
    {"XY hole range", &player_hole_range_uses_xy_radius},
    {"save indicators and labels", &indicators_and_phase_labels_are_stable},
    {"recovery request gate", &recovery_requires_fault_and_n_press},
    {"progression HUD values", &progression_hud_values_follow_snapshot},
};

}  // namespace

arpg::test::TestSuite dungeon_view_math_suite() noexcept {
    return arpg::test::make_suite("dungeon_view_math", kCases);
}
