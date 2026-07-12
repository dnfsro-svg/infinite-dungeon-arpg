#pragma once

#include "dungeon/dungeon_types.hpp"

#include <cstdint>

namespace arpg::platform {

struct Rgba8 final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};
};

struct DoorTheme final {
    dungeon::DungeonElement element{};
    const char* label{};
    const char* arrow{};
    Rgba8 frame{};
};

enum class HoleVisualMode : std::uint8_t {
    hidden,
    sealed,
    ready,
    busy,
    faulted,
};

enum class SaveIndicator : std::uint8_t {
    none,
    saving,
    saved,
    recovered,
    error,
};

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
[[nodiscard]] DoorTheme door_theme(
    dungeon::ExitDirection direction) noexcept;
[[nodiscard]] HoleVisualMode hole_visual_mode(
    const dungeon::DungeonSnapshot& snapshot) noexcept;
[[nodiscard]] bool player_in_hole_range(
    combat::Vec3 position,
    combat::Vec3 center,
    float radius) noexcept;
[[nodiscard]] Rgba8 ecosystem_tint(
    dungeon::DungeonElement element) noexcept;
[[nodiscard]] float abyss_pulse_alpha(float elapsed_seconds) noexcept;
[[nodiscard]] const char* save_indicator_label(
    SaveIndicator indicator) noexcept;
[[nodiscard]] bool recovery_requested(
    bool runtime_recovery_required,
    bool n_pressed) noexcept;
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
