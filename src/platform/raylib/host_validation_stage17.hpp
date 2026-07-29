#pragma once

#include "combat/active_skill_runtime.hpp"
#include "skills/active_skill_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace arpg::dungeon {
struct DungeonSnapshot;
}

namespace arpg::settings {
struct SettingsData;
}

namespace arpg::platform {
class InventoryRenderer;
struct ActiveSkillDrawRuntimeStatus;
struct PhysicalKeySnapshot;
struct RaylibHostConfig;
struct SubmittedFrameActions;

namespace host_validation {

enum class Stage17ValidationStep : std::uint8_t {
    initial,
    empty_slot_3,
    empty_slot_4,
    empty_slot_5,
    approach_draw,
    draw_active,
    approach_storm,
    storm_active,
    open_inventory,
    open_skill_page,
    remove_slot_1,
    select_draw_inventory,
    equip_slot_5,
    swap_slots_2_5,
    close_inventory,
    cooldown_drain,
    restart_open_inventory,
    restart_open_skill_page,
    restart_capture,
    complete,
};

enum class Stage17Capture : std::uint8_t {
    none,
    initial,
    draw_windup,
    draw_hit,
    storm_array,
    storm_aerial,
    storm_finisher,
    restarted,
};

struct Stage17SkillDrawRuntimeEvidence final {
    std::uint32_t samples{};
    bool material_frame_drawn{};
    bool base_player_drawn{};
    std::size_t procedural_main_visual_peak{};
    bool valid{true};
};

struct Stage17SkillStonesValidationState final {
    Stage17ValidationStep step{Stage17ValidationStep::initial};
    Stage17Capture capture_pending{Stage17Capture::none};
    skills::SkillLoadoutState initial_loadout{};
    skills::SkillLoadoutState final_loadout{};
    combat::Vec3 storm_center{};
    combat::Vec3 storm_player_start{};
    combat::Vec3 storm_player_last{};
    combat::Vec3 storm_approach_center{};
    combat::Vec3 storm_approach_target_start{};
    combat::Vec3 storm_approach_player_start{};
    combat::Vec3 storm_approach_player_last{};
    std::array<combat::Vec3, 4U> storm_approach_samples{};
    std::array<bool, 4U> storm_approach_sampled{};
    std::array<std::uint16_t, combat::kStormStrikeCount>
        storm_targets_per_strike{};
    combat::MovementInput storm_threat_pull_movement{};
    std::uint16_t storm_approach_target_ordinal{
        (std::numeric_limits<std::uint16_t>::max)()};
    bool initial_recorded{};
    bool initial_captured{};
    bool draw_accepted{};
    bool draw_windup_captured{};
    bool draw_captured{};
    bool storm_accepted{};
    bool storm_approach_locked{};
    bool storm_isolation_invalidated{};
    bool storm_lock_recorded{};
    bool storm_threat_pull_locked{};
    bool storm_center_locked{true};
    bool storm_player_moved{};
    bool storm_array_captured{};
    bool storm_aerial_captured{};
    bool storm_finisher_captured{};
    bool restart_captured{};
    bool restart_persisted{};
    bool restart_cooldowns_zero{};
    bool public_input_path{};
    bool production_transactions{};
    bool production_inventory_closed{};
    bool production_cooldown_wait_started{};
    bool production_cooldowns_zero_before_shutdown{};
    bool clean_shutdown_exact_ready{};
    bool active_skill_atlases_ready{};
    bool storm_invulnerable_seen{};
    bool storm_finisher_phase_seen{};
    bool aborted_by_death{};
    bool renderer_status_failure_latched{};
    bool suspend_injection{};
    bool suppress_draw_captures{};
    Stage17SkillDrawRuntimeEvidence draw_runtime{};
    Stage17SkillDrawRuntimeEvidence storm_runtime{};
    std::array<bool, 3> empty_slots_none{};
    std::uint32_t draw_hit_count{};
    std::uint32_t storm_strike_hit_count{};
    std::uint32_t storm_finisher_hit_count{};
    std::uint8_t storm_strike_count{};
    std::uint8_t storm_sword_peak{};
    std::uint16_t production_cooldown_start_ticks{};
    std::uint64_t production_cooldown_wait_start_tick{};
    std::uint64_t production_cooldown_wait_end_tick{};
    std::uint8_t storm_sampled_strike_count{};
    std::uint16_t draw_frame_peak{};
    std::uint32_t presented_frames{};
    std::uint32_t storm_approach_frames{};
    std::uint32_t storm_approach_movement_injections{};
    std::uint32_t storm_isolation_invalidated_frame{};
    std::size_t storm_isolation_invalidated_target_count{};
    float storm_approach_initial_clearance{};
};

void observe_stage17_draw_runtime(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&, const dungeon::DungeonSnapshot&,
    const ActiveSkillDrawRuntimeStatus&) noexcept;
void observe_stage17_combat_event(Stage17SkillStonesValidationState*,
    const combat::CombatEvent&) noexcept;
[[nodiscard]] PhysicalKeySnapshot inject_stage17_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    const settings::SettingsData&, const dungeon::DungeonSnapshot&,
    Stage17SkillStonesValidationState&) noexcept;
void observe_stage17_submitted_actions(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&, const SubmittedFrameActions&) noexcept;
void observe_stage17_snapshot(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&,
    const dungeon::DungeonSnapshot&) noexcept;
void observe_stage17_inventory(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&, const InventoryRenderer&,
    const dungeon::DungeonSnapshot&) noexcept;
[[nodiscard]] std::optional<std::string> stage17_capture_path(
    const RaylibHostConfig&, Stage17SkillStonesValidationState&) noexcept;
void mark_stage17_capture_complete(Stage17SkillStonesValidationState&) noexcept;
[[nodiscard]] bool stage17_validation_complete(const RaylibHostConfig&,
    const Stage17SkillStonesValidationState&) noexcept;
void write_stage17_validation_summary(const RaylibHostConfig&,
    const Stage17SkillStonesValidationState&) noexcept;

}  // namespace host_validation
}  // namespace arpg::platform
