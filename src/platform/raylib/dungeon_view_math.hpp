#pragma once

#include "dungeon/dungeon_types.hpp"
#include "progression/progression_rules.hpp"

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

struct ProgressionHudValues final {
    std::uint8_t level{};
    std::uint64_t experience{};
    std::uint64_t required_experience{};
    std::uint8_t unspent_passive_points{};
    std::uint64_t pending_room_experience{};
    bool maximum_level{};
};

enum class EnvironmentHazardVisualMode : std::uint8_t {
    hidden,
    warning,
    active,
};

struct EnvironmentHazardVisual final {
    EnvironmentHazardVisualMode mode{EnvironmentHazardVisualMode::hidden};
    combat::Vec3 center{};
    float radius{};
    Rgba8 fill{};
    Rgba8 outline{};
};

struct AbyssHudValues final {
    bool visible{};
    const char* danger_label{};
    const char* rule_label{};
    const char* effect_label{};
    std::uint8_t pending_rewards{};
    std::uint8_t unpicked_rewards{};
    bool confirmation_visible{};
    dungeon::TransitionKind confirmation_transition{
        dungeon::TransitionKind::none};
    dungeon::ExitDirection confirmation_direction{
        dungeon::ExitDirection::none};
    const char* confirmation_label{};
};

inline constexpr combat::Vec3 kHoleCenter{0.0F, 3.5F, 0.0F};
inline constexpr float kHoleInteractionRadius = 2.0F;

[[nodiscard]] DoorVisualMode door_visual_mode(
    dungeon::RoomPhase phase,
    bool has_active_room) noexcept;
[[nodiscard]] DoorTheme door_theme(
    dungeon::ExitDirection direction) noexcept;
[[nodiscard]] bool abyss_door_marker(
    const dungeon::DungeonSnapshot& snapshot,
    dungeon::ExitDirection direction) noexcept;
[[nodiscard]] EnvironmentHazardVisual environment_hazard_visual(
    const combat::HazardSnapshot& hazard) noexcept;
[[nodiscard]] AbyssHudValues abyss_hud_values(
    const dungeon::DungeonSnapshot& snapshot) noexcept;
[[nodiscard]] HoleVisualMode hole_visual_mode(
    const dungeon::DungeonSnapshot& snapshot) noexcept;
[[nodiscard]] bool player_in_hole_range(
    combat::Vec3 position,
    combat::Vec3 center,
    float radius) noexcept;
[[nodiscard]] bool can_prompt_descent(
    const dungeon::DungeonSnapshot& snapshot,
    combat::Vec3 player_position) noexcept;
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
[[nodiscard]] ProgressionHudValues progression_hud_values(
    const dungeon::DungeonSnapshot& snapshot,
    const progression::ProgressionRules& rules) noexcept;

}  // namespace arpg::platform
