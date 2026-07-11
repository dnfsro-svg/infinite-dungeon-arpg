#pragma once

#include "dungeon/dungeon_types.hpp"

#include <cstdint>

namespace arpg::platform {

enum class DoorVisualMode : std::uint8_t {
    closed,
    open,
    hidden,
};

struct TransitionVisualState final {
    float seconds_left{};
    bool transition_phase_seen{};
};

[[nodiscard]] DoorVisualMode door_visual_mode(
    dungeon::RoomPhase phase,
    bool has_active_room) noexcept;
[[nodiscard]] bool can_interpolate_room(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current) noexcept;
[[nodiscard]] bool dungeon_event_clears_transients(
    dungeon::DungeonEventKind kind) noexcept;
[[nodiscard]] const char* room_phase_label(
    dungeon::RoomPhase phase) noexcept;
[[nodiscard]] const char* exit_direction_label(
    dungeon::ExitDirection direction) noexcept;
[[nodiscard]] TransitionVisualState transition_after_dungeon_event(
    TransitionVisualState state,
    dungeon::DungeonEventKind kind) noexcept;
[[nodiscard]] TransitionVisualState transition_after_room_phase(
    TransitionVisualState state,
    dungeon::RoomPhase phase) noexcept;
[[nodiscard]] TransitionVisualState advance_transition(
    TransitionVisualState state,
    float frame_seconds) noexcept;
[[nodiscard]] float transition_overlay_alpha(float seconds_left) noexcept;

}  // namespace arpg::platform
