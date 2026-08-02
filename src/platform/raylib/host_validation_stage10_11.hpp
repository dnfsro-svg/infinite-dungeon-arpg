#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_types.hpp"

#include <cstdint>

namespace arpg::dungeon {
class DungeonSession;
}

namespace arpg::platform {
struct RaylibHostConfig;
enum class Stage10ValidationScenario : std::uint8_t;

namespace host_validation {

enum class Stage10GridRoutePhase : std::uint8_t {
    need_join,
    join_near_x,
    join_far_x,
    route,
};

struct Stage10GridRouteState final {
    Stage10GridRoutePhase phase{Stage10GridRoutePhase::need_join};
    std::uint8_t boundary_column{};
    std::uint8_t route_rejoins{};
    bool pending_movement_progress_check{};
    combat::Vec3 previous_position{};
};

struct Stage10ValidationState final {
    bool entered_abyss{};
    bool reset_requested{};
    bool descent_warning_seen{};
    bool normal_exit_started{};
    combat::MonsterOrdinal ranged_target_ordinal{
        combat::kInvalidMonsterOrdinal};
    combat::Vec3 ranged_stance{};
    combat::Facing ranged_facing{combat::Facing::right};
    combat::MonsterOrdinal stalled_target_ordinal{
        combat::kInvalidMonsterOrdinal};
    combat::Vec3 recovery_target{};
    bool recovery_target_valid{};
    std::uint8_t sweep_waypoint{};
    Stage10GridRouteState sweep_grid{};
    bool stance_reached{};
    bool close_for_light{};
    bool melee_chain{};
    bool recover_until_light_lane{};
    bool sweep_escape{};
    bool pending_stance_progress_check{};
    bool pending_close_progress_check{};
    float previous_stance_distance_squared{};
    combat::Vec3 previous_close_position{};
    std::uint32_t chaos_presented_frames{};
};

struct Stage11ValidationState final {
    bool entered_abyss{};
    bool saw_depth_two{};
    bool continue_requested{};
    std::uint32_t target_presented_frames{};
    Stage10GridRouteState sweep_grid{};
    Stage10ValidationState combat_driver{};
};

[[nodiscard]] bool stage10_validation_abyss_skills_enabled(
    Stage10ValidationScenario) noexcept;

[[nodiscard]] combat::MovementInput stage10_validation_input(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    const RaylibHostConfig&, Stage10ValidationState&) noexcept;
[[nodiscard]] combat::MovementInput stage11_validation_input(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    const RaylibHostConfig&, Stage11ValidationState&) noexcept;
[[nodiscard]] bool stage10_validation_reached(
    const dungeon::DungeonSnapshot&, const RaylibHostConfig&,
    const Stage10ValidationState&) noexcept;
[[nodiscard]] bool stage11_validation_reached(
    const dungeon::DungeonSnapshot&, const RaylibHostConfig&,
    const Stage11ValidationState&) noexcept;

}  // namespace host_validation
}  // namespace arpg::platform
