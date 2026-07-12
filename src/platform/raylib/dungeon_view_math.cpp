#include "dungeon_view_math.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::platform {
namespace {

constexpr float kTransitionSeconds = 0.12F;
constexpr float kTwoPi = 6.28318530717958647692F;

constexpr Rgba8 kFireFrame{236U, 92U, 54U, 255U};
constexpr Rgba8 kWaterFrame{64U, 156U, 236U, 255U};
constexpr Rgba8 kLightningFrame{236U, 218U, 72U, 255U};
constexpr Rgba8 kChaosFrame{154U, 76U, 210U, 255U};

}  // namespace

DoorVisualMode door_visual_mode(
    dungeon::RoomPhase phase,
    bool has_active_room) noexcept {
    if (!has_active_room || phase == dungeon::RoomPhase::transitioning) {
        return DoorVisualMode::hidden;
    }
    if (phase == dungeon::RoomPhase::cleared
        || phase == dungeon::RoomPhase::awaiting_exit
        || phase == dungeon::RoomPhase::committing) {
        return DoorVisualMode::open;
    }
    return DoorVisualMode::closed;
}

DoorTheme door_theme(dungeon::ExitDirection direction) noexcept {
    switch (direction) {
    case dungeon::ExitDirection::up:
        return {dungeon::DungeonElement::fire, "FIRE", "\xE2\x86\x91", kFireFrame};
    case dungeon::ExitDirection::down:
        return {dungeon::DungeonElement::water, "WATER", "\xE2\x86\x93", kWaterFrame};
    case dungeon::ExitDirection::left:
        return {dungeon::DungeonElement::lightning, "LIGHTNING", "\xE2\x86\x90", kLightningFrame};
    case dungeon::ExitDirection::right:
        return {dungeon::DungeonElement::chaos, "CHAOS", "\xE2\x86\x92", kChaosFrame};
    case dungeon::ExitDirection::none:
        return {dungeon::DungeonElement::chaos, "UNKNOWN", "?", kChaosFrame};
    }
    return {dungeon::DungeonElement::chaos, "UNKNOWN", "?", kChaosFrame};
}

HoleVisualMode hole_visual_mode(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    if (!snapshot.has_active_room || !snapshot.has_hole) {
        return HoleVisualMode::hidden;
    }
    switch (snapshot.phase) {
    case dungeon::RoomPhase::locked:
    case dungeon::RoomPhase::combat:
        return HoleVisualMode::sealed;
    case dungeon::RoomPhase::cleared:
    case dungeon::RoomPhase::awaiting_exit:
        return HoleVisualMode::ready;
    case dungeon::RoomPhase::committing:
        return HoleVisualMode::busy;
    case dungeon::RoomPhase::faulted:
        return HoleVisualMode::faulted;
    case dungeon::RoomPhase::transitioning:
        return HoleVisualMode::hidden;
    }
    return HoleVisualMode::hidden;
}

bool player_in_hole_range(
    combat::Vec3 position,
    combat::Vec3 center,
    float radius) noexcept {
    if (radius < 0.0F) {
        return false;
    }
    const float x = position.x - center.x;
    const float y = position.y - center.y;
    return x * x + y * y <= radius * radius;
}

Rgba8 ecosystem_tint(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire:
        return {88U, 43U, 34U, 255U};
    case dungeon::DungeonElement::water:
        return {35U, 70U, 104U, 255U};
    case dungeon::DungeonElement::lightning:
        return {96U, 90U, 34U, 255U};
    case dungeon::DungeonElement::chaos:
        return {73U, 43U, 99U, 255U};
    }
    return {73U, 43U, 99U, 255U};
}

float abyss_pulse_alpha(float elapsed_seconds) noexcept {
    if (!std::isfinite(elapsed_seconds)) {
        return 0.5F;
    }
    const float phase = std::fmod(elapsed_seconds, 1.0F);
    const float alpha = 0.5F + 0.5F * std::sin(kTwoPi * phase);
    return std::clamp(alpha, 0.0F, 1.0F);
}

const char* save_indicator_label(SaveIndicator indicator) noexcept {
    switch (indicator) {
    case SaveIndicator::none:
        return "";
    case SaveIndicator::saving:
        return "SAVING";
    case SaveIndicator::saved:
        return "SAVED";
    case SaveIndicator::recovered:
        return "RECOVERED";
    case SaveIndicator::error:
        return "SAVE ERROR";
    }
    return "";
}

bool recovery_requested(
    bool runtime_recovery_required,
    bool n_pressed) noexcept {
    return runtime_recovery_required && n_pressed;
}

bool can_interpolate_room(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current) noexcept {
    return previous.has_active_room && current.has_active_room
        && previous.combat.has_value() && current.combat.has_value()
        && previous.room_index == current.room_index
        && previous.room_seed == current.room_seed;
}

bool dungeon_event_clears_transients(
    dungeon::DungeonEventKind kind) noexcept {
    return kind == dungeon::DungeonEventKind::room_destroyed
        || kind == dungeon::DungeonEventKind::room_reset;
}

const char* room_phase_label(dungeon::RoomPhase phase) noexcept {
    switch (phase) {
    case dungeon::RoomPhase::locked:
        return "LOCKED";
    case dungeon::RoomPhase::combat:
        return "COMBAT";
    case dungeon::RoomPhase::cleared:
        return "CLEARED";
    case dungeon::RoomPhase::awaiting_exit:
        return "AWAITING EXIT";
    case dungeon::RoomPhase::committing:
        return "COMMITTING";
    case dungeon::RoomPhase::transitioning:
        return "TRANSITIONING";
    case dungeon::RoomPhase::faulted:
        return "FAULTED";
    }
    return "UNKNOWN";
}

const char* exit_direction_label(dungeon::ExitDirection direction) noexcept {
    switch (direction) {
    case dungeon::ExitDirection::up:
        return "UP";
    case dungeon::ExitDirection::down:
        return "DOWN";
    case dungeon::ExitDirection::left:
        return "LEFT";
    case dungeon::ExitDirection::right:
        return "RIGHT";
    case dungeon::ExitDirection::none:
        return "NONE";
    }
    return "UNKNOWN";
}

TransitionVisualState transition_after_dungeon_event(
    TransitionVisualState state,
    dungeon::DungeonEventKind kind) noexcept {
    if (kind == dungeon::DungeonEventKind::room_destroyed) {
        state.seconds_left = kTransitionSeconds;
    } else if (kind == dungeon::DungeonEventKind::room_reset) {
        state = {};
    }
    return state;
}

TransitionVisualState transition_after_room_phase(
    TransitionVisualState state,
    dungeon::RoomPhase phase) noexcept {
    const bool transitioning = phase == dungeon::RoomPhase::transitioning;
    if (transitioning && !state.transition_phase_seen) {
        state.seconds_left = kTransitionSeconds;
    }
    state.transition_phase_seen = transitioning;
    return state;
}

TransitionVisualState advance_transition(
    TransitionVisualState state,
    float frame_seconds) noexcept {
    state.seconds_left = std::max(
        0.0F,
        state.seconds_left - std::clamp(frame_seconds, 0.0F, 0.1F));
    return state;
}

float transition_overlay_alpha(float seconds_left) noexcept {
    return std::clamp(seconds_left / kTransitionSeconds, 0.0F, 1.0F);
}

}  // namespace arpg::platform
