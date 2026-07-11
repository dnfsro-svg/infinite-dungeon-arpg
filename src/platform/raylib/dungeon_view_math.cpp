#include "dungeon_view_math.hpp"

#include <algorithm>

namespace arpg::platform {
namespace {

constexpr float kTransitionSeconds = 0.12F;

}  // namespace

DoorVisualMode door_visual_mode(
    dungeon::RoomPhase phase,
    bool has_active_room) noexcept {
    if (!has_active_room || phase == dungeon::RoomPhase::transitioning) {
        return DoorVisualMode::hidden;
    }
    if (phase == dungeon::RoomPhase::cleared
        || phase == dungeon::RoomPhase::awaiting_exit) {
        return DoorVisualMode::open;
    }
    return DoorVisualMode::closed;
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
    case dungeon::RoomPhase::transitioning:
        return "TRANSITIONING";
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
