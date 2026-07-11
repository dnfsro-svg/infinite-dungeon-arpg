#include "dungeon_view_math.hpp"

#include <algorithm>

namespace arpg::platform {

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

float transition_overlay_alpha(float seconds_left) noexcept {
    constexpr float kTransitionSeconds = 0.12F;
    return std::clamp(seconds_left / kTransitionSeconds, 0.0F, 1.0F);
}

}  // namespace arpg::platform
